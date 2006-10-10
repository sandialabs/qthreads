#include <stdio.h>
#include <pthread.h>

int x = 0;
pthread_mutex_t x_lock = PTHREAD_MUTEX_INITIALIZER;

int id = 0;
pthread_mutex_t id_lock = PTHREAD_MUTEX_INITIALIZER;

int up[4] = {1,0,0,0};
pthread_mutex_t up_locks[4] = {PTHREAD_MUTEX_INITIALIZER,PTHREAD_MUTEX_INITIALIZER,PTHREAD_MUTEX_INITIALIZER,PTHREAD_MUTEX_INITIALIZER};

void *thread(void * arg)
{
    int me = 0;
    int next = 0;
    int i;

    pthread_mutex_lock(&id_lock);
    me = id++;
    pthread_mutex_unlock(&id_lock);
    next = (me + 1) % 4;
    printf("thread(%i): next %i\n", me, next); fflush(stdout);

    for (i = 0; i< 10; i++) {
	while (1) {
	    pthread_mutex_lock(&(up_locks[me]));
	    if (up[me] != 0) {
		up[me] = 0;
		pthread_mutex_unlock(&(up_locks[me]));
		break;
	    }
	    pthread_mutex_unlock(&(up_locks[me]));
	}
	pthread_mutex_lock(&x_lock);
	printf("thread(%i): x=%d\n", me, x);
	x++;
	pthread_mutex_unlock(&x_lock);
	pthread_mutex_lock(&(up_locks[next]));
	up[next]++;
	pthread_mutex_unlock(&(up_locks[next]));
    }
}

int main (int argc, char *argv[])
{
    pthread_t a, b, c, d;

    pthread_create(&a, NULL, thread, NULL);
    pthread_create(&b, NULL, thread, NULL);
    pthread_create(&c, NULL, thread, NULL);
    pthread_create(&d, NULL, thread, NULL);

    pthread_join(a, NULL);
    pthread_join(b, NULL);
    pthread_join(c, NULL);
    pthread_join(d, NULL);

    return 0;
}
