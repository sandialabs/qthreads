#include <stdio.h>		       /* for printf() */
#include <stdlib.h> /* for strtol() */
#include <assert.h>		       /* for assert() */
#include <qthread/qthread.h>
#include <qthread/qloop.h>
#include "qtimer.h"
#include "argparsing.h"

size_t ITERATIONS;
#define MAXPARALLELISM 256

aligned_t incrementme = 0;
aligned_t *increments = NULL;

void balanced_incr(qthread_t *me, const size_t startat, const size_t stopat, void *arg)
{
    size_t i;
    for (i = startat; i < stopat; i++) {
	qthread_incr(increments+i, 1);
    }
}

void balanced_falseshare(qthread_t *me, const size_t startat, const size_t stopat, void *arg)
{
    size_t i;
    qthread_shepherd_id_t shep = qthread_shep(me);
    aligned_t *myinc = increments + shep;

    for (i = startat; i < stopat; i++) {
	qthread_incr(myinc, 1);
    }
}

void balanced_noncomp(qthread_t *me, const size_t startat, const size_t stopat, void *arg)
{
    size_t i;
    qthread_shepherd_id_t shep = qthread_shep(me);
    aligned_t myinc;

    for (i = startat; i < stopat; i++) {
	qthread_incr(&myinc, 1);
    }
}

aligned_t incrloop(qthread_t * me, void *arg)
{
    unsigned int i;

    for (i = 0; i < ITERATIONS; i++) {
	qthread_incr(&incrementme, 1);
    }
    return 0;
}

aligned_t incrloop_falseshare(qthread_t * me, void *arg)
{
    unsigned int offset = (unsigned int)(intptr_t)arg;
    unsigned int i;
    aligned_t *myinc = increments+offset;

    for (i = 0; i < ITERATIONS; i++) {
	qthread_incr(myinc, 1);
    }
    return 0;
}

aligned_t incrloop_nocompete(qthread_t * me, void *arg)
{
    unsigned int i;
    aligned_t myinc;

    for (i = 0; i < ITERATIONS; i++) {
	qthread_incr(&myinc, 1);
    }
    return 0;
}

aligned_t addloop_falseshare(qthread_t *me, void *arg)
{
    unsigned int offset = (unsigned int)(intptr_t)arg;
    unsigned int i;
    aligned_t *myinc = increments+offset;

    for (i = 0; i < ITERATIONS; i++) {
	(*myinc) ++;
    }
    return *myinc;
}

aligned_t addloop_nocompete(qthread_t *me, void *arg)
{
    unsigned int i;
    aligned_t myinc = 0;

    for (i = 0; i < ITERATIONS; i++) {
	myinc ++;
    }
    return myinc;
}

char *human_readable_rate(double rate)
{
    static char readable_string[100] = { 0 };
    const double GB = 1024 * 1024 * 1024;
    const double MB = 1024 * 1024;
    const double kB = 1024;

    if (rate > GB) {
	snprintf(readable_string, 100, "(%.1f GB/s)", rate / GB);
    } else if (rate > MB) {
	snprintf(readable_string, 100, "(%.1f MB/s)", rate / MB);
    } else if (rate > kB) {
	snprintf(readable_string, 100, "(%.1f kB/s)", rate / kB);
    } else {
	memset(readable_string, 0, 100*sizeof(char));
    }
    return readable_string;
}

int main(int argc, char *argv[])
{
    qtimer_t timer = qtimer_new();
    double rate;
    unsigned int i;
    aligned_t rets[MAXPARALLELISM];
    unsigned int shepherds = 1;

    /* setup */
    assert(qthread_initialize() == QTHREAD_SUCCESS);

    CHECK_VERBOSE();
    ITERATIONS = (verbose)?1000000:1000;
    shepherds = qthread_num_shepherds();
    iprintf("%u shepherds...\n", shepherds);

    /* BALANCED INCREMENT LOOP (strong scaling) */
    iprintf("\tBalanced competing loop: ");
    increments = (aligned_t*)calloc(MAXPARALLELISM*ITERATIONS, sizeof(aligned_t));
    qtimer_start(timer);
    qt_loop_balance(0, MAXPARALLELISM*ITERATIONS, balanced_incr, NULL);
    qtimer_stop(timer);
    for (i=0;i<MAXPARALLELISM*ITERATIONS; i++) assert(increments[i] == 1);
    free(increments);
    increments = NULL;

    iprintf("%19g secs (%u-threads %u iters)\n", qtimer_secs(timer), shepherds, (unsigned)(ITERATIONS*MAXPARALLELISM));
    iprintf("\t + average increment time: %17g secs\n",
	    qtimer_secs(timer) / (ITERATIONS*MAXPARALLELISM));
    iprintf("\t = increment throughput: %19f increments/sec\n",
	    (ITERATIONS*MAXPARALLELISM) / qtimer_secs(timer));
    rate = (ITERATIONS*MAXPARALLELISM * sizeof(aligned_t)) / qtimer_secs(timer);
    iprintf("\t = data throughput: %24g bytes/sec %s\n", rate,
	    human_readable_rate(rate));

    /* BALANCED FALSE-SHARING LOOP */
    iprintf("\tBalanced false-sharing loop: ");
    increments = (aligned_t*)calloc(shepherds, sizeof(aligned_t));
    qtimer_start(timer);
    qt_loop_balance(0, ITERATIONS*256, balanced_falseshare, NULL);
    qtimer_stop(timer);
    free(increments);
    increments = NULL;

    iprintf("%15g secs (%u-threads %u iters)\n", qtimer_secs(timer), shepherds, (unsigned)(ITERATIONS*256));
    iprintf("\t + average increment time: %17g secs\n",
	    qtimer_secs(timer) / (ITERATIONS*256));
    iprintf("\t = increment throughput: %19f increments/sec\n",
	    (ITERATIONS*256) / qtimer_secs(timer));
    rate = (ITERATIONS*256* sizeof(aligned_t)) / qtimer_secs(timer);
    iprintf("\t = data throughput: %24g bytes/sec %s\n", rate,
	    human_readable_rate(rate));

    /* BALANCED INDEPENDENT LOOP */
    iprintf("\tBalanced independent loop: ");
    qtimer_start(timer);
    qt_loop_balance(0, ITERATIONS*256, balanced_noncomp, NULL);
    qtimer_stop(timer);

    iprintf("%17g secs (%u-threads %u iters)\n", qtimer_secs(timer), shepherds, (unsigned)(ITERATIONS*256));
    iprintf("\t + average increment time: %17g secs\n",
	    qtimer_secs(timer) / (ITERATIONS*256));
    iprintf("\t = increment throughput: %19f increments/sec\n",
	    (ITERATIONS*256) / qtimer_secs(timer));
    rate = (ITERATIONS*256* sizeof(aligned_t)) / qtimer_secs(timer);
    iprintf("\t = data throughput: %24g bytes/sec %s\n", rate,
	    human_readable_rate(rate));

    /* OVER-SUBSCRIBED COMPETING INCREMENT LOOP */
    iprintf("\tOver-subscribed competing loop: ");
    qtimer_start(timer);
    for (i = 0; i < MAXPARALLELISM; i++) {
	qthread_fork(incrloop, NULL, rets+i);
    }
    for (i = 0; i < MAXPARALLELISM; i++) {
	qthread_readFF(NULL, NULL, rets+i);
    }
    qtimer_stop(timer);
    assert(incrementme == ITERATIONS*MAXPARALLELISM);

    iprintf("%12g secs (%u-threads %u iters)\n", qtimer_secs(timer), MAXPARALLELISM, (unsigned)ITERATIONS);
    iprintf("\t + average increment time: %17g secs\n",
	    qtimer_secs(timer) / (ITERATIONS*MAXPARALLELISM));
    iprintf("\t = increment throughput: %19f increments/sec\n",
	    (ITERATIONS*MAXPARALLELISM) / qtimer_secs(timer));
    rate = (ITERATIONS*MAXPARALLELISM * sizeof(aligned_t)) / qtimer_secs(timer);
    iprintf("\t = data throughput: %24g bytes/sec %s\n", rate,
	    human_readable_rate(rate));

    /* OVER-SUBSCRIBED FALSE-SHARING INCREMENT LOOP */
    iprintf("\tOver-subscribed false-sharing loop: ");
    increments = (aligned_t*)calloc(MAXPARALLELISM, sizeof(aligned_t));
    qtimer_start(timer);
    for (i=0; i<MAXPARALLELISM; i++) {
	qthread_fork(incrloop_falseshare, (void*)(intptr_t)i, rets+i);
    }
    for (i = 0; i < MAXPARALLELISM; i++) {
	qthread_readFF(NULL, NULL, rets+i);
    }
    qtimer_stop(timer);
    free(increments);
    increments = NULL;

    iprintf("%8g secs (%u-threads %u iters)\n", qtimer_secs(timer), MAXPARALLELISM, (unsigned)ITERATIONS);
    iprintf("\t + average increment time: %17g secs\n",
	    qtimer_secs(timer) / (ITERATIONS*MAXPARALLELISM));
    iprintf("\t = increment throughput: %19f increments/sec\n",
	    (ITERATIONS*MAXPARALLELISM) / qtimer_secs(timer));
    rate = (ITERATIONS*MAXPARALLELISM * sizeof(aligned_t)) / qtimer_secs(timer);
    iprintf("\t = data throughput: %24g bytes/sec %s\n", rate,
	    human_readable_rate(rate));

    /* OVER-SUBSCRIBED INDEPENDENT INCREMENT LOOP */
    iprintf("\tOver-subscribed independent loop: ");
    qtimer_start(timer);
    for (i=0; i<MAXPARALLELISM; i++) {
	qthread_fork(incrloop_nocompete, (void*)(intptr_t)i, rets+i);
    }
    for (i = 0; i < MAXPARALLELISM; i++) {
	qthread_readFF(NULL, NULL, rets+i);
    }
    qtimer_stop(timer);

    iprintf("%10g secs (%u-threads %u iters)\n", qtimer_secs(timer), MAXPARALLELISM, (unsigned)ITERATIONS);
    iprintf("\t + average increment time: %17g secs\n",
	    qtimer_secs(timer) / (ITERATIONS*MAXPARALLELISM));
    iprintf("\t = increment throughput: %19f increments/sec\n",
	    (ITERATIONS*MAXPARALLELISM) / qtimer_secs(timer));
    rate = (ITERATIONS*MAXPARALLELISM * sizeof(aligned_t)) / qtimer_secs(timer);
    iprintf("\t = data throughput: %24g bytes/sec %s\n", rate,
	    human_readable_rate(rate));

    /* INDEPENDENT ADD LOOP */
    iprintf("\tNon-atomic false-sharing loop: ");
    increments = (aligned_t*)calloc(MAXPARALLELISM, sizeof(aligned_t));
    qtimer_start(timer);
    for (i=0; i<MAXPARALLELISM; i++) {
	qthread_fork(addloop_falseshare, (void*)(intptr_t)i, rets+i);
    }
    for (i = 0; i < MAXPARALLELISM; i++) {
	qthread_readFF(NULL, NULL, rets+i);
    }
    qtimer_stop(timer);
    free(increments);
    increments = NULL;

    iprintf("%13g secs (%u-way %u iters)\n", qtimer_secs(timer), MAXPARALLELISM, (unsigned)ITERATIONS);
    iprintf("\t + average increment time: %17g secs\n",
	    qtimer_secs(timer) / (ITERATIONS*MAXPARALLELISM));
    iprintf("\t = increment throughput: %19f increments/sec\n",
	    (ITERATIONS*MAXPARALLELISM) / qtimer_secs(timer));
    rate = (ITERATIONS*MAXPARALLELISM * sizeof(aligned_t)) / qtimer_secs(timer);
    iprintf("\t = data throughput: %24g bytes/sec %s\n", rate,
	    human_readable_rate(rate));

    /* INDEPENDENT NON-ATOMIC LOOP */
    iprintf("\tNon-atomic independent loop: ");
    qtimer_start(timer);
    for (i=0; i<MAXPARALLELISM; i++) {
	qthread_fork(addloop_nocompete, (void*)(intptr_t)i, rets+i);
    }
    for (i = 0; i < MAXPARALLELISM; i++) {
	qthread_readFF(NULL, NULL, rets+i);
    }
    qtimer_stop(timer);

    iprintf("%15g secs (%u-way %u iters)\n", qtimer_secs(timer), MAXPARALLELISM, (unsigned)ITERATIONS);
    iprintf("\t + average increment time: %17g secs\n",
	    qtimer_secs(timer) / (ITERATIONS*MAXPARALLELISM));
    iprintf("\t = increment throughput: %19f increments/sec\n",
	    (ITERATIONS*MAXPARALLELISM) / qtimer_secs(timer));
    rate = (ITERATIONS*MAXPARALLELISM * sizeof(aligned_t)) / qtimer_secs(timer);
    iprintf("\t = data throughput: %24g bytes/sec %s\n", rate,
	    human_readable_rate(rate));

    qtimer_free(timer);

    return 0;
}
