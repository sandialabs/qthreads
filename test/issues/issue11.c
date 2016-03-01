#include <stdio.h>
#include <pthread.h>
#include <qthread/qthread.h>
#include <qthread/sinc.h>
#include <stdlib.h>

aligned_t qt_run(void *args){
  char *arg = (char *)args;
  printf("I am the qthread number\t\t%s\n",arg);
  free(arg);
  return 0;
}

void *run(void *arg){
  qt_sinc_t *sinc = qt_sinc_create(0, NULL, NULL, 0);
  for(int x=0;x<10;x++){
    char *arg = malloc(sizeof(char)*10);
    sprintf(arg,"pqt%d",x);
    qthread_spawn(qt_run,(void*)arg,0,sinc,0,NULL,NO_SHEPHERD,
                  QTHREAD_SPAWN_RET_SINC | QTHREAD_SPAWN_SIMPLE);
  }
  qt_sinc_expect(sinc,10);
  qt_sinc_wait(sinc, NULL);
  printf("done with qthreads in pthreads\n");
  return NULL;
}

int main(){
  printf("initing qthreads\n");
  if(qthread_initialize() != QTHREAD_SUCCESS){
    printf("Failed to init qthreads.\n");
    abort();
  }
  pthread_t thread0;
  pthread_create(&thread0,NULL,run,NULL);

  qt_sinc_t *sinc = qt_sinc_create(0, NULL, NULL, 0);
  for(int x=0;x<10;x++){
    char *arg = malloc(sizeof(char)*10);
    sprintf(arg,"qt%d",x);
    qthread_spawn(qt_run,(void*)arg,0,sinc,0,NULL,NO_SHEPHERD,
                  QTHREAD_SPAWN_RET_SINC | QTHREAD_SPAWN_SIMPLE);
  }
  qt_sinc_expect(sinc,10);
  qt_sinc_wait(sinc, NULL);
  printf("done with qthreads\n");
  pthread_join(thread0,NULL);
  printf("done with pthreads\n");
  qthread_finalize();

  return 0;
}
