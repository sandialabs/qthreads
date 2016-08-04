#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* System Headers */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

/* Public Headers */
#include "qthread/qthread.h"
#include "qthread/cacheline.h"

/* Internal Headers */
#include "qt_visibility.h"
#include "qthread_innards.h"           /* for qlib */
#include "qt_shepherd_innards.h"
#include "qt_qthread_struct.h"
#include "qt_qthread_mgmt.h"
#include "qt_asserts.h"
#include "qt_prefetch.h"
#include "qt_threadqueues.h"
#include "qt_envariables.h"
#include "qt_debug.h"
#ifdef QTHREAD_USE_EUREKAS
#include "qt_eurekas.h" /* for qt_eureka_check() */
#endif /* QTHREAD_USE_EUREKAS */
#include "qt_expect.h"
#include "qt_subsystems.h"

// Non portable
typedef uint8_t cacheline[64];

/* Cutoff variables */
int max_backoff; 
int spinloop_backoff;
int condwait_backoff;
int steal_ratio;

/* Data Structures */
struct _qt_threadqueue_node {
  struct _qt_threadqueue_node *next;
  struct _qt_threadqueue_node *prev;
  qthread_t                   *value;
};

typedef struct {
  qt_threadqueue_node_t *head;
  qt_threadqueue_node_t *tail;
  long                 qlength;
  long                 numwaiters;
  pthread_cond_t      cond;
  pthread_mutex_t     cond_mut;
  QTHREAD_TRYLOCK_TYPE qlock;
  cacheline buf; // ensure internal nodes are a cacheline apart
} qt_threadqueue_internal;

typedef struct {
  size_t n;
  cacheline buf; //ensure
} w_ind;

struct _qt_threadqueue {
  qt_threadqueue_internal *t;
  size_t num_queues;
  w_ind* w_inds;
}; 

/* Memory Management and Initialization/Shutdown */
qt_threadqueue_pools_t generic_threadqueue_pools;

#define mycounter(q) (q->w_inds[qthread_worker(NULL) % (qlib->nshepherds * qlib->nworkerspershep)].n)
#define myqueue(q) (q->t + mycounter(q))

static qt_threadqueue_t* alloc_threadqueue(){
  qt_threadqueue_t* t = (qt_threadqueue_t *)qt_mpool_alloc(generic_threadqueue_pools.queues);
  t->num_queues = qlib->nworkerspershep; // Assumption built into api of constant number of workers per shepherd
  t->t = malloc(sizeof(qt_threadqueue_internal) * t->num_queues);
  t->w_inds = calloc(qlib->nshepherds * qlib->nworkerspershep,  sizeof(w_ind));  
  return t;
}

static void free_threadqueue(qt_threadqueue_t* t){
  free(t->t);
  free(t->w_inds);
  qt_mpool_free(generic_threadqueue_pools.queues, t);
}

static QINLINE qt_threadqueue_node_t *alloc_tqnode(void){                                     
  return (qt_threadqueue_node_t *)qt_mpool_alloc(generic_threadqueue_pools.nodes);
} 

static QINLINE void free_tqnode(qt_threadqueue_node_t* t){                                     
  qt_mpool_free(generic_threadqueue_pools.nodes, t);
} 

extern qt_mpool generic_qthread_pool;

static QINLINE qthread_t* alloc_qthread() {
  return (qthread_t*) qt_mpool_alloc(generic_qthread_pool);
}
static QINLINE void free_qthread(qthread_t* t){
  return qt_mpool_free(generic_qthread_pool, t);
}

qt_threadqueue_t INTERNAL *qt_threadqueue_new(void){   
  qt_threadqueue_t *qe = alloc_threadqueue();
  steal_ratio = qt_internal_get_env_num("STEAL_RATIO", 8, 0);
  spinloop_backoff = qt_internal_get_env_num("SPINLOOP_BACKOFF", 16, 0);
  condwait_backoff = qt_internal_get_env_num("CONDWAIT_BACKOFF", 22, 0);
  max_backoff = qt_internal_get_env_num("MAX_BACKOFF", 23, 0);
  for(int i=0; i<qe->num_queues; i++){
    qt_threadqueue_internal* q = qe->t + i; 
    if (q != NULL) {
      q->head              = NULL;
      q->tail              = NULL;
      q->qlength           = 0;
      q->numwaiters        = 0;
      QTHREAD_TRYLOCK_INIT(q->qlock);
      pthread_cond_init(&q->cond, NULL);
      pthread_mutex_init(&q->cond_mut, NULL);
    }
  }
  for(size_t i=0; i<qlib->nshepherds * qlib->nworkerspershep; i++){
    qe->w_inds[i].n = i % qe->num_queues;
  }
  return qe;
} 

void INTERNAL qt_threadqueue_free(qt_threadqueue_t *qe){   
  for(int i=0; i<qe->num_queues; i++){
    qt_threadqueue_internal* q = qe->t + i;
    pthread_cond_destroy(&q->cond);
    pthread_mutex_destroy(&q->cond_mut);
    if (q->head != q->tail) {
      qthread_t *t;
      QTHREAD_TRYLOCK_LOCK(&q->qlock);
      while (q->head != q->tail) {
        qt_threadqueue_node_t *node = q->tail;
        if (node != NULL) {
          q->tail = node->prev;
          if (q->tail == NULL) {
              q->head = NULL;
          } else {
              q->tail->next = NULL;
          }
          t = node->value;
          free_tqnode(node);
          free_qthread(t);
        }
      }
      assert(q->head == NULL);
      assert(q->tail == NULL);
      q->qlength           = 0;
      QTHREAD_TRYLOCK_UNLOCK(&q->qlock);
    }
    assert(q->head == q->tail);
    QTHREAD_TRYLOCK_DESTROY(q->qlock);
  }
  free_threadqueue(qe);
} 

static void qt_threadqueue_subsystem_shutdown(){   
  qt_mpool_destroy(generic_threadqueue_pools.nodes);
  qt_mpool_destroy(generic_threadqueue_pools.queues);
} 

void INTERNAL qt_threadqueue_subsystem_init(){   
  generic_threadqueue_pools.queues = qt_mpool_create_aligned(sizeof(qt_threadqueue_t),
                                                             qthread_cacheline());
  generic_threadqueue_pools.nodes = qt_mpool_create_aligned(sizeof(qt_threadqueue_node_t),
                                                            qthread_cacheline());
  qthread_internal_cleanup(qt_threadqueue_subsystem_shutdown);
}

ssize_t INTERNAL qt_threadqueue_advisory_queuelen(qt_threadqueue_t *q){   
  return myqueue(q)->qlength;
} 

/* Threadqueue operations 
 * We have 4 basic queue operations, enqueue and dequeue for head and tail */
void INTERNAL qt_threadqueue_enqueue_tail(qt_threadqueue_t *restrict qe,
                                          qthread_t *restrict        t){ 
  qt_threadqueue_internal* q = myqueue(qe);
  mycounter(qe) = (mycounter(qe) + 1) % qe->num_queues;
  qt_threadqueue_node_t *node = alloc_tqnode();
  node->value = t;
  node->next = NULL;

  QTHREAD_TRYLOCK_LOCK(&q->qlock);
  node->prev = q->tail;
  q->tail    = node;
  if (q->head == NULL) {
    q->head = node;
  } else {
    node->prev->next = node;
  }
  q->qlength++;
  QTHREAD_TRYLOCK_UNLOCK(&q->qlock);
  if (q->numwaiters > 0) pthread_cond_broadcast (&q->cond);
} 

void INTERNAL qt_threadqueue_enqueue_head(qt_threadqueue_t *restrict qe,
                                          qthread_t *restrict        t){   
  qt_threadqueue_internal* q = myqueue(qe);
  mycounter(qe) = (mycounter(qe) + 1) % qe->num_queues;
  qt_threadqueue_node_t *node = alloc_tqnode();
  node->value     = t;
  node->prev = NULL;

  QTHREAD_TRYLOCK_LOCK(&q->qlock);
  node->next = q->head;
  q->head    = node;
  if (q->tail == NULL) {
      q->tail = node;
  } else {
      node->next->prev = node;
  }
  q->qlength++;
  QTHREAD_TRYLOCK_UNLOCK(&q->qlock);
  if (q->numwaiters > 0) pthread_cond_broadcast (&q->cond);
} 

qt_threadqueue_node_t INTERNAL *qt_threadqueue_dequeue_tail(qt_threadqueue_t *qe){                                     
  qt_threadqueue_internal* q = myqueue(qe);
  mycounter(qe) = (mycounter(qe) + 1) % qe->num_queues;
  qt_threadqueue_node_t *node;
  
  // If there is no work or we can't get the lock, fail
  if (q->qlength == 0) return NULL;
  if (!QTHREAD_TRYLOCK_TRY(&q->qlock)) return NULL;
  if (q->qlength == 0){
    QTHREAD_TRYLOCK_UNLOCK(&q->qlock);
    return NULL;
  }
  
  node = (qt_threadqueue_node_t *)q->tail;
  q->tail = node->prev;
  if(q->tail) q->tail->next = NULL;
  if(q->head == node) q->head = NULL;
  q->qlength--;
  QTHREAD_TRYLOCK_UNLOCK(&q->qlock);

  return node;
}                                   

qt_threadqueue_node_t INTERNAL *qt_threadqueue_dequeue_head(qt_threadqueue_t *qe){                                     
  qt_threadqueue_internal* q = myqueue(qe);
  mycounter(qe) = (mycounter(qe) + 1) % qe->num_queues;
  qt_threadqueue_node_t *node;
  
  // If there is no work or we can't get the lock, fail
  if (q->qlength == 0) return NULL;
  if (!QTHREAD_TRYLOCK_TRY(&q->qlock)) return NULL;
  if (q->qlength == 0){
    QTHREAD_TRYLOCK_UNLOCK(&q->qlock);
    return NULL;
  }
  
  node = (qt_threadqueue_node_t *)q->head;
  q->head = node->next;
  if(q->head) q->head->prev = NULL;
  if(q->tail == node) q->tail = NULL;
  q->qlength--;
  QTHREAD_TRYLOCK_UNLOCK(&q->qlock);

  return node;
}                                   

void INTERNAL qt_threadqueue_enqueue(qt_threadqueue_t *restrict q,
                                     qthread_t *restrict        t){
  return qt_threadqueue_enqueue_tail(q, t);
}

void INTERNAL qt_threadqueue_enqueue_yielded(qt_threadqueue_t *restrict q,
                                             qthread_t *restrict        t){
  return qt_threadqueue_enqueue_head(q, t);
}

/* Unsupported operations */
qthread_t INTERNAL * qt_threadqueue_dequeue_specific(qt_threadqueue_t * q,
                                                     void             * value){
    return NULL;
}

qthread_t INTERNAL *qt_threadqueue_private_dequeue(qt_threadqueue_private_t *c){
    return NULL;
} 

void INTERNAL qt_threadqueue_enqueue_cache(qt_threadqueue_t         *q,
                                           qt_threadqueue_private_t *cache)
{}

void INTERNAL qt_threadqueue_private_filter(qt_threadqueue_private_t *restrict c,
                                            qt_threadqueue_filter_f            f)
{}

int INTERNAL qt_threadqueue_private_enqueue(qt_threadqueue_private_t *restrict pq,
                                            qt_threadqueue_t *restrict         q,
                                            qthread_t *restrict                t)
{ return 0; }

int INTERNAL qt_threadqueue_private_enqueue_yielded(qt_threadqueue_private_t *restrict q,
                                                    qthread_t *restrict                t)
{ return 0; } 

// We try and dequeue locally, if that fails we should do some stealing
qthread_t INTERNAL *qt_scheduler_get_thread(qt_threadqueue_t         *qe,
                                            qt_threadqueue_private_t *qc,
                                            uint_fast8_t              active){   
  qt_threadqueue_internal * q= myqueue(qe);
  qt_threadqueue_node_t *node = NULL;
  qthread_t* t;
  int numwaits = 0;
  qthread_shepherd_t *my_shepherd = qthread_internal_getshep();

  // Simple exponential backoff that switches to cond_wait spinloop_backoff waits
  while(!node){
    node = qt_threadqueue_dequeue_tail(qe);

    // If we've done QT_STEAL_RATIO waits on local queue, try to steal 
    if(!node && steal_ratio > 0 && numwaits % steal_ratio == 0) {
      for(int i=0; i < qlib->nshepherds; i++){
        qt_threadqueue_t *victim_queue = qlib->shepherds[i].ready;
        node = qt_threadqueue_dequeue_head(victim_queue);
        if (node){
          t = node->value;
          free_tqnode(node);
          return t;
        }
      }
    }
   
    if(!node){
      if(numwaits > condwait_backoff){
        struct timespec t; 
        struct timeval n; 
        gettimeofday(&n, NULL); 
        t.tv_nsec = (n.tv_usec * 1000) + (1 << (numwaits > max_backoff ? max_backoff : numwaits)); 
        t.tv_sec = n.tv_sec + ((t.tv_nsec >= 1000000000)?1:0); 
        t.tv_nsec -= ((t.tv_nsec >= 1000000000)?1000000000:0); 
        pthread_mutex_lock(&q->cond_mut);
        qthread_incr(&q->numwaiters, 1);
        pthread_cond_timedwait(&q->cond, &q->cond_mut, &t);
        qthread_incr(&q->numwaiters, -1);
        pthread_mutex_unlock(&q->cond_mut);
      } else if (numwaits < spinloop_backoff) {
        for(int i=0; i<1<<numwaits; i++){
          SPINLOCK_BODY();
        }
      }
      numwaits++;
    }
  }
  t = node->value;
  free_tqnode(node);
  return t;
} 

void INTERNAL qthread_steal_enable(){     
}   

void INTERNAL qthread_steal_disable(){     
}     

qthread_shepherd_id_t INTERNAL qt_threadqueue_choose_dest(qthread_shepherd_t * curr_shep){
  qthread_shepherd_id_t dest_shep_id = 0;
  if (curr_shep) {
    dest_shep_id               = curr_shep->sched_shepherd++ % qlib->nshepherds;
    curr_shep->sched_shepherd *= (qlib->nshepherds > (dest_shep_id + 1));
  } else {
    dest_shep_id = 
        (qthread_shepherd_id_t)qthread_internal_incr_mod(
            &qlib->sched_shepherd,
            qlib->nshepherds,
            &qlib->sched_shepherd_lock);
  }

  return dest_shep_id;
}

size_t INTERNAL qt_threadqueue_policy(const enum threadqueue_policy policy){
  switch (policy) {
    default:
      return THREADQUEUE_POLICY_UNSUPPORTED;
  }
}

/* vim:set expandtab: */
