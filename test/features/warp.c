#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <qthread/qthread.h>
#include <qthread/sinc.h>

typedef struct array_s {
  unsigned int size;
  float* arr;
} array;

typedef struct warp_arg_s {
  int begin_semaphore;
  int end_semaphore;
  array a;
  array b;
  array c;
} warp_arg;


array rand_array(int size){
  array r;
  r.size = size;
  r.arr = malloc(sizeof(float) * size);
  int i;
  for(i = 0; i < size; i++){
    r.arr[i] = (float)rand();
  }
  return r;
}

static aligned_t array_add_warp(void *argv)
{
  volatile warp_arg* arg = (warp_arg*) argv;
  __sync_fetch_and_sub(&arg->begin_semaphore, 1); 
  while (arg->begin_semaphore > 0) { __asm__("pause; "); }
  
  qthread_shepherd_id_t shep = qthread_shep();
  int id = qthread_worker_local(&shep);
  int block_size = arg->a.size / qthread_num_workers_local(shep);

//  printf("Worker %d on shepherd %d doing task %d\n", id, shep, qthread_id());

  int i;
  for(i=block_size * id; i < block_size * (id + 1); i++){
    arg->c.arr[i] = arg->a.arr[i] + arg->b.arr[i];
  }
  __sync_fetch_and_sub(&arg->end_semaphore, 1); 
  return 0;
}

int main(int argc, char *argv[])
{
  assert(qthread_initialize() == 0);

  int i,j;
  int array_size = qthread_num_workers_local(0) * 1024; // Must be a multiple of num_workers_per_shep

  warp_arg* args = malloc(sizeof(warp_arg) * qthread_num_shepherds());
   
  printf("Number of shepherds: %d number of workers per shepherd:%d\n", qthread_num_shepherds(), qthread_num_workers_local(0));
  printf("Spawning work... \n");

  for(i=0; i < qthread_num_shepherds(); i++){
    args[i].begin_semaphore = qthread_num_workers_local(i);
    args[i].end_semaphore = qthread_num_workers_local(i);
    args[i].a = rand_array(array_size);
    args[i].b = rand_array(array_size);
    args[i].c = rand_array(array_size);
    int ret = qthread_fork_clones_to_local_priority(array_add_warp, args + i, NULL, i, qthread_num_workers_local(i) - 1);
    assert(ret == QTHREAD_SUCCESS);
  }

  for(i=0; i < qthread_num_shepherds(); i++){
    while (args[i].end_semaphore > 0) { qthread_yield(); }
  }

  printf("Checking array add results\n");
  for(i=0; i < qthread_num_shepherds(); i++){
    for(j=0; j < args[i].a.size; j++){
      if(args[i].a.arr[j] + args[i].b.arr[j] != args[i].c.arr[j]){
        printf("array addition failed at shepherd %d, inded %d!\n", i, j);
      }
    }
  }
  printf("done\n");
  return 0;
}

/* vim:set expandtab */
