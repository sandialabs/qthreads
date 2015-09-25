#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/qloop.h>

static aligned_t hello_world(void *arg)
{
    printf("Hello World! This is task %i, running on shepherd %i, worker %i\n", qthread_id(), qthread_shep(), qthread_worker(NULL));
    return 0;
}

int main(int argc, char *argv[])
{
  assert(qthread_initialize() == 0);

  int i;
  aligned_t* rets = malloc(sizeof(aligned_t) * qthread_num_workers());
  
  printf("Number of shepherds: %d number of workers:%d\n", qthread_num_shepherds(), qthread_num_workers_local(0));

  for(i=0; i < qthread_num_shepherds(); i++){
    int ret = qthread_fork_clones_to_local_priority(hello_world, NULL, rets+i, i, qthread_num_workers_local(i) - 1);
    assert(ret == QTHREAD_SUCCESS);
  }

  for(i=0; i < qthread_num_shepherds(); i++){
    int ret = qthread_readFF(NULL, rets + i);
    assert(ret == QTHREAD_SUCCESS);
  }
/* 
  for(i=0; i < qthread_num_workers(); i++){
    int ret = qthread_spawn(hello_world, NULL, 0, rets+i, 0, NULL, 0, QTHREAD_SPAWN_SIMPLE);
    assert(ret == QTHREAD_SUCCESS);
  } 
  for(i=0; i < qthread_num_shepherds(); i++){
    int ret = qthread_readFF(NULL, rets + i);
    assert(ret == QTHREAD_SUCCESS);
  }
*/
}

/* vim:set expandtab */
