#include <stdio.h>		       /* for printf() */
#include <stdlib.h> /* for strtol() */
#include <assert.h>		       /* for assert() */
#include <qthread/qthread.h>
#include <qthread/qloop.h>
#include "qtimer.h"

size_t ITERATIONS;
#define MAXPARALLELISM 256

aligned_t incrementme = 0;
aligned_t *increments = NULL;

void balanced_incr(qthread_t *me, const unsigned long startat, const unsigned long stopat, void *arg)
{
    size_t i;
    for (i = startat; i < stopat; i++) {
	qthread_incr(increments+i, 1);
    }
}

void balanced_shepincr(qthread_t *me, const unsigned long startat, const unsigned long stopat, void *arg)
{
    size_t i;
    qthread_shepherd_id_t shep = qthread_shep(me);
    aligned_t *myinc = increments + shep;

    for (i = startat; i < stopat; i++) {
	qthread_incr(myinc, 1);
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

aligned_t incrloop_nocompete(qthread_t * me, void *arg)
{
    unsigned int offset = (unsigned int)(intptr_t)arg;
    unsigned int i;
    aligned_t *myinc = increments+offset;

    for (i = 0; i < ITERATIONS; i++) {
	qthread_incr(myinc, 1);
    }
    return 0;
}

aligned_t addloop_nocompete(qthread_t *me, void *arg)
{
    unsigned int offset = (unsigned int)(intptr_t)arg;
    unsigned int i;
    aligned_t *myinc = increments+offset;

    for (i = 0; i < ITERATIONS; i++) {
	(*myinc) ++;
    }
    return *myinc;
}

char *human_readable_rate(double rate)
{
    static char readable_string[100] = { 0 };
    const double GB = 1024 * 1024 * 1024;
    const double MB = 1024 * 1024;
    const double kB = 1024;

    if (rate > GB) {
	snprintf(readable_string, 100, "(%'.1f GB/s)", rate / GB);
    } else if (rate > MB) {
	snprintf(readable_string, 100, "(%'.1f MB/s)", rate / MB);
    } else if (rate > kB) {
	snprintf(readable_string, 100, "(%'.1f kB/s)", rate / kB);
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
    int shepherds = 1;
    int interactive = 0;

    if (argc == 2) {
	shepherds = strtol(argv[1], NULL, 0);
	interactive = 1;
	if (shepherds <= 0) {
	    shepherds = 1;
	    interactive = 0;
	}
    }
    ITERATIONS = (interactive)?1000000:1000;

    /* setup */
    qthread_init(shepherds);

    /* BALANCED INCREMENT LOOP (strong scaling) */
    printf("\tBalanced increment loop: "); fflush(stdout);
    increments = (aligned_t*)calloc(MAXPARALLELISM*ITERATIONS, sizeof(aligned_t));
    qtimer_start(timer);
    qt_loop_balance(0, MAXPARALLELISM*ITERATIONS, balanced_incr, NULL);
    qtimer_stop(timer);
    for (i=0;i<MAXPARALLELISM*ITERATIONS; i++) assert(increments[i] == 1);
    free(increments);

    printf("%19g secs (%u-threads %u iters)\n", qtimer_secs(timer), shepherds, (unsigned)(ITERATIONS*MAXPARALLELISM));
    printf("\t + average increment time: %17g secs\n",
	   qtimer_secs(timer) / (ITERATIONS*MAXPARALLELISM));
    printf("\t = increment throughput: %'19f increments/sec\n",
	   (ITERATIONS*MAXPARALLELISM) / qtimer_secs(timer));
    rate = (ITERATIONS*MAXPARALLELISM * sizeof(aligned_t)) / qtimer_secs(timer);
    printf("\t = data throughput: %24g bytes/sec %s\n", rate,
	   human_readable_rate(rate));

    /* BALANCED INCREMENT LOOP (weak scaling) */
    printf("\tBalanced increment weak loop: "); fflush(stdout);
    increments = (aligned_t*)calloc(shepherds, sizeof(aligned_t));
    qtimer_start(timer);
    qt_loop_balance(0, ITERATIONS*256, balanced_shepincr, NULL);
    qtimer_stop(timer);
    free(increments);

    printf("%14g secs (%u-threads %u iters)\n", qtimer_secs(timer), shepherds, (unsigned)(ITERATIONS*256));
    printf("\t + average increment time: %17g secs\n",
	   qtimer_secs(timer) / (ITERATIONS*256));
    printf("\t = increment throughput: %'19f increments/sec\n",
	   (ITERATIONS*256) / qtimer_secs(timer));
    rate = (ITERATIONS*256* sizeof(aligned_t)) / qtimer_secs(timer);
    printf("\t = data throughput: %24g bytes/sec %s\n", rate,
	   human_readable_rate(rate));

    /* COMPETING INCREMENT LOOP */
    printf("\tCompeting increment loop: "); fflush(stdout);
    qtimer_start(timer);
    for (i = 0; i < MAXPARALLELISM; i++) {
	qthread_fork(incrloop, NULL, rets+i);
    }
    for (i = 0; i < MAXPARALLELISM; i++) {
	qthread_readFF(NULL, NULL, rets+i);
    }
    qtimer_stop(timer);
    assert(incrementme == ITERATIONS*MAXPARALLELISM);

    printf("%18g secs (%u-threads %u iters)\n", qtimer_secs(timer), MAXPARALLELISM, (unsigned)ITERATIONS);
    printf("\t + average increment time: %17g secs\n",
	   qtimer_secs(timer) / (ITERATIONS*MAXPARALLELISM));
    printf("\t = increment throughput: %'19f increments/sec\n",
	   (ITERATIONS*MAXPARALLELISM) / qtimer_secs(timer));
    rate = (ITERATIONS*MAXPARALLELISM * sizeof(aligned_t)) / qtimer_secs(timer);
    printf("\t = data throughput: %24g bytes/sec %s\n", rate,
	   human_readable_rate(rate));

    /* NON-COMPETING INCREMENT LOOP */
    printf("\tNon-competing increment loop: "); fflush(stdout);
    increments = (aligned_t*)calloc(MAXPARALLELISM, sizeof(aligned_t));
    qtimer_start(timer);
    for (i=0; i<MAXPARALLELISM; i++) {
	qthread_fork(incrloop_nocompete, NULL, rets+i);
    }
    for (i = 0; i < MAXPARALLELISM; i++) {
	qthread_readFF(NULL, NULL, rets+i);
    }
    qtimer_stop(timer);
    free(increments);

    printf("%14g secs (%u-threads %u iters)\n", qtimer_secs(timer), MAXPARALLELISM, (unsigned)ITERATIONS);
    printf("\t + average increment time: %17g secs\n",
	   qtimer_secs(timer) / (ITERATIONS*MAXPARALLELISM));
    printf("\t = increment throughput: %'19f increments/sec\n",
	   (ITERATIONS*MAXPARALLELISM) / qtimer_secs(timer));
    rate = (ITERATIONS*MAXPARALLELISM * sizeof(aligned_t)) / qtimer_secs(timer);
    printf("\t = data throughput: %24g bytes/sec %s\n", rate,
	   human_readable_rate(rate));

    /* NON-COMPETING INCREMENT LOOP (weak scaling) */
    printf("\tNon-competing weak-scaling loop: "); fflush(stdout);
    increments = (aligned_t*)calloc(shepherds, sizeof(aligned_t));
    qtimer_start(timer);
    for (i=0; i<shepherds; i++) {
	qthread_fork(incrloop_nocompete, NULL, rets+i);
    }
    for (i = 0; i < shepherds; i++) {
	qthread_readFF(NULL, NULL, rets+i);
    }
    qtimer_stop(timer);
    free(increments);

    printf("%11g secs (%u-threads %u iters)\n", qtimer_secs(timer), shepherds, (unsigned)ITERATIONS);
    printf("\t + average increment time: %17g secs\n",
	   qtimer_secs(timer) / (ITERATIONS*shepherds));
    printf("\t = increment throughput: %'19f increments/sec\n",
	   (ITERATIONS*shepherds) / qtimer_secs(timer));
    rate = (ITERATIONS*shepherds* sizeof(aligned_t)) / qtimer_secs(timer);
    printf("\t = data throughput: %24g bytes/sec %s\n", rate,
	   human_readable_rate(rate));

    /* NON-COMPETING ADD LOOP */
    printf("\tNon-competing add loop: "); fflush(stdout);
    increments = (aligned_t*)calloc(MAXPARALLELISM, sizeof(aligned_t));
    qtimer_start(timer);
    for (i=0; i<MAXPARALLELISM; i++) {
	qthread_fork(addloop_nocompete, NULL, rets+i);
    }
    for (i = 0; i < MAXPARALLELISM; i++) {
	qthread_readFF(NULL, NULL, rets+i);
    }
    qtimer_stop(timer);
    free(increments);

    printf("%20g secs (%u-way %u iters)\n", qtimer_secs(timer), MAXPARALLELISM, (unsigned)ITERATIONS);
    printf("\t + average increment time: %17g secs\n",
	   qtimer_secs(timer) / (ITERATIONS*MAXPARALLELISM));
    printf("\t = increment throughput: %'19f increments/sec\n",
	   (ITERATIONS*MAXPARALLELISM) / qtimer_secs(timer));
    rate = (ITERATIONS*MAXPARALLELISM * sizeof(aligned_t)) / qtimer_secs(timer);
    printf("\t = data throughput: %24g bytes/sec %s\n", rate,
	   human_readable_rate(rate));

    qtimer_free(timer);

    qthread_finalize();

    return 0;
}
