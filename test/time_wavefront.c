#include <stdlib.h>
#include <stdio.h>
#include <qthread/qthread.h>
#include <qthread/wavefront.h>
#include <qtimer.h>

static size_t ASIZE = 10;

void avg(const void *restrict left, const void *restrict leftdown,
	 const void *restrict down, void *restrict out)
{
    *(double*)out = (*(double*)left + *(double*)leftdown + *(double*)down) / 3.0;
}

void assignrand(qthread_t * me, const size_t startat, const size_t stopat,
	     qarray * a, void *arg)
{
    double *ptr = qarray_elem_nomigrate(a, startat);
    const size_t max = stopat - startat;

    for (size_t i = 0; i < max; i++) {
	/*long r = random();
	memcpy(&(ptr[i]), &r, sizeof(long));*/
	ptr[i] = (double)(i+startat);
    }
}

int main(int argc, char *argv[])
{
    int threads = 0;
    int interactive = 0;
    qthread_t *me;
    qarray *v, *h;
    qt_wavefront_lattice *L;
    qtimer_t timer = qtimer_new();

    if (argc >= 2) {
	threads = strtol(argv[1], NULL, 0);
	if (threads < 0) {
	    threads = 0;
	} else {
	    interactive = 1;
	    printf("%i threads\n", threads);
	}
    }
    if (argc >= 3) {
	ASIZE = strtol(argv[2], NULL, 0);
	printf("ASIZE: %i\n", (int)ASIZE);
    }
    qthread_init(threads);

    me = qthread_self();
    v=qarray_create_configured(ASIZE, sizeof(double), FIXED_HASH, 1, 1);
    h=qarray_create_configured(ASIZE+1, sizeof(double), FIXED_HASH, 1, 1);

    qarray_iter_loop(me, h, 1, ASIZE+1, assignrand, NULL);
    qarray_iter_loop(me, v, 0, ASIZE, assignrand, NULL);
    printf("v items per seg: %i\n", (int)v->segment_size);

    /* do stuff */
    qtimer_start(timer);
    L = qt_wavefront(v, h, avg);
    qtimer_stop(timer);

    if (L) {
	printf("wavefront secs: %f\n", qtimer_secs(timer));
	//qt_wavefront_print_lattice(L);
    } else {
	printf("wavefront returned NULL!\n");
    }
    qarray_destroy(v);
    qarray_destroy(h);
    qt_wavefront_destroy_lattice(L);
    return 0;
}
