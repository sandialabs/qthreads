#include <stdio.h>		       /* for printf() */
#include <stdlib.h>		       /* for strtol() */
#include <assert.h>		       /* for assert() */
#include <qthread/qthread.h>
#include <qthread/qloop.h>
#include <qthread/qtimer.h>
#include "argparsing.h"

size_t TEST_SELECTION = 0xffffffff;
size_t ITERATIONS = 1000000;
size_t MAXPARALLELISM = 256;
aligned_t incrementme = 0;
aligned_t *increments = NULL;

static void balanced_incr(
    qthread_t * me,
    const size_t startat,
    const size_t stopat,
    const size_t step,
    void *arg)
{
    size_t i;

    for (i = startat; i < stopat; i++) {
	qthread_incr((aligned_t *) arg, 1);
    }
}

static void balanced_falseshare(
    qthread_t * me,
    const size_t startat,
    const size_t stopat,
    const size_t step,
    void *arg)
{
    size_t i;
    qthread_shepherd_id_t shep = qthread_shep(me);
    aligned_t *myinc = increments + shep;

    for (i = startat; i < stopat; i++) {
	qthread_incr(myinc, 1);
    }
}

static void balanced_noncomp(
    qthread_t * me,
    const size_t startat,
    const size_t stopat,
    const size_t step,
    void *arg)
{
    size_t i;
    qthread_shepherd_id_t shep = qthread_shep(me);
    aligned_t myinc;

    assert(shep != NO_SHEPHERD);
    for (i = startat; i < stopat; i++) {
	qthread_incr(&myinc, 1);
    }
}

static aligned_t incrloop(
    qthread_t * me,
    void *arg)
{
    unsigned int i;

    for (i = 0; i < ITERATIONS; i++) {
	qthread_incr(&incrementme, 1);
    }
    return 0;
}

static aligned_t incrloop_falseshare(
    qthread_t * me,
    void *arg)
{
    unsigned int offset = (unsigned int)(intptr_t) arg;
    unsigned int i;
    aligned_t *myinc = increments + offset;

    for (i = 0; i < ITERATIONS; i++) {
	qthread_incr(myinc, 1);
    }
    return 0;
}

static aligned_t incrloop_nocompete(
    qthread_t * me,
    void *arg)
{
    unsigned int i;
    aligned_t myinc;

    for (i = 0; i < ITERATIONS; i++) {
	qthread_incr(&myinc, 1);
    }
    return 0;
}

static aligned_t incrstream(
    qthread_t * me,
    void *arg)
{
    unsigned int i;
    aligned_t *const myinc = (aligned_t *) arg;

    for (i = 0; i < ITERATIONS; i++) {
	qthread_incr(myinc + i, 1);
    }
    return 0;
}

static aligned_t addloop_falseshare(
    qthread_t * me,
    void *arg)
{
    unsigned int offset = (unsigned int)(intptr_t) arg;
    unsigned int i;
    aligned_t *myinc = increments + offset;

    for (i = 0; i < ITERATIONS; i++) {
	(*myinc)++;
    }
    return *myinc;
}

static aligned_t addloop_nocompete(
    qthread_t * me,
    void *arg)
{
    unsigned int i;
    aligned_t myinc = 0;

    for (i = 0; i < ITERATIONS; i++) {
	myinc++;
    }
    return myinc;
}

static void streaming_incr(
    qthread_t * me,
    const size_t startat,
    const size_t stopat,
    const size_t step,
    void *arg)
{
    size_t i;

    for (i = startat; i < stopat; i++) {
	qthread_incr(increments + i, 1);
    }
}

static void streaming_naincr(
    qthread_t * me,
    const size_t startat,
    const size_t stopat,
    const size_t step,
    void *arg)
{
    size_t i;

    for (i = startat; i < stopat; i++) {
	increments[i]++;
    }
}

static char *human_readable_rate(
    double rate)
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
	memset(readable_string, 0, 100 * sizeof(char));
    }
    return readable_string;
}

int main(
    int argc,
    char *argv[])
{
    qtimer_t timer = qtimer_create();
    double rate;
    unsigned int i;
    aligned_t *rets;
    unsigned int shepherds = 1;

    /* setup */
    assert(qthread_initialize() == QTHREAD_SUCCESS);

    CHECK_VERBOSE();
    NUMARG(ITERATIONS, "ITERATIONS");
    NUMARG(MAXPARALLELISM, "MAXPARALLELISM");
    NUMARG(TEST_SELECTION, "TEST_SELECTION");
    shepherds = qthread_num_shepherds();
    printf("%u shepherds...\n", shepherds);
    rets = malloc(sizeof(aligned_t) * MAXPARALLELISM);
    assert(rets);

    /* BALANCED INCREMENT LOOP (strong scaling) */
    if (TEST_SELECTION & 1) {
	printf("\tBalanced competing loop: ");
	fflush(stdout);
	increments = (aligned_t *) calloc(1, sizeof(aligned_t));
	qtimer_start(timer);
	qt_loop_balance(0, MAXPARALLELISM * ITERATIONS, balanced_incr,
			increments);
	qtimer_stop(timer);
	assert(*increments == MAXPARALLELISM * ITERATIONS);
	free(increments);
	increments = NULL;

	printf("%19g secs (%u-threads %u iters)\n", qtimer_secs(timer),
	       shepherds, (unsigned)(ITERATIONS * MAXPARALLELISM));
	iprintf("\t + average increment time: %17g secs\n",
		qtimer_secs(timer) / (ITERATIONS * MAXPARALLELISM));
	printf("\t = increment throughput: %19f increments/sec\n",
	       (ITERATIONS * MAXPARALLELISM) / qtimer_secs(timer));
	rate =
	    (ITERATIONS * MAXPARALLELISM * sizeof(aligned_t)) /
	    qtimer_secs(timer);
	printf("\t = data throughput: %24g bytes/sec %s\n", rate,
	       human_readable_rate(rate));
    }

    /* BALANCED FALSE-SHARING LOOP */
    if (TEST_SELECTION & (1 << 1)) {
	printf("\tBalanced false-sharing loop: ");
	fflush(stdout);
	increments = (aligned_t *) calloc(shepherds, sizeof(aligned_t));
	qtimer_start(timer);
	qt_loop_balance(0, ITERATIONS * 256, balanced_falseshare, NULL);
	qtimer_stop(timer);
	free(increments);
	increments = NULL;

	printf("%15g secs (%u-threads %u iters)\n", qtimer_secs(timer),
	       shepherds, (unsigned)(ITERATIONS * 256));
	iprintf("\t + average increment time: %17g secs\n",
		qtimer_secs(timer) / (ITERATIONS * 256));
	printf("\t = increment throughput: %19f increments/sec\n",
	       (ITERATIONS * 256) / qtimer_secs(timer));
	rate = (ITERATIONS * 256 * sizeof(aligned_t)) / qtimer_secs(timer);
	printf("\t = data throughput: %24g bytes/sec %s\n", rate,
	       human_readable_rate(rate));
    }

    if (TEST_SELECTION & (1 << 2)) {
	/* BALANCED INDEPENDENT LOOP */
	printf("\tBalanced independent loop: ");
	fflush(stdout);
	qtimer_start(timer);
	qt_loop_balance(0, ITERATIONS * 256, balanced_noncomp, NULL);
	qtimer_stop(timer);

	printf("%17g secs (%u-threads %u iters)\n", qtimer_secs(timer),
	       shepherds, (unsigned)(ITERATIONS * 256));
	iprintf("\t + average increment time: %17g secs\n",
		qtimer_secs(timer) / (ITERATIONS * 256));
	printf("\t = increment throughput: %19f increments/sec\n",
	       (ITERATIONS * 256) / qtimer_secs(timer));
	rate = (ITERATIONS * 256 * sizeof(aligned_t)) / qtimer_secs(timer);
	printf("\t = data throughput: %24g bytes/sec %s\n", rate,
	       human_readable_rate(rate));
    }

    if (TEST_SELECTION & (1 << 3)) {
	/* OVER-SUBSCRIBED COMPETING INCREMENT LOOP */
	printf("\tOver-subscribed competing loop: ");
	fflush(stdout);
	qtimer_start(timer);
	for (i = 0; i < MAXPARALLELISM; i++) {
	    qthread_fork(incrloop, NULL, rets + i);
	}
	for (i = 0; i < MAXPARALLELISM; i++) {
	    qthread_readFF(NULL, NULL, rets + i);
	}
	qtimer_stop(timer);
	assert(incrementme == ITERATIONS * MAXPARALLELISM);

	printf("%12g secs (%u-threads %u iters)\n", qtimer_secs(timer),
	       (unsigned)MAXPARALLELISM, (unsigned)ITERATIONS);
	iprintf("\t + average increment time: %17g secs\n",
		qtimer_secs(timer) / (ITERATIONS * MAXPARALLELISM));
	printf("\t = increment throughput: %19f increments/sec\n",
	       (ITERATIONS * MAXPARALLELISM) / qtimer_secs(timer));
	rate =
	    (ITERATIONS * MAXPARALLELISM * sizeof(aligned_t)) /
	    qtimer_secs(timer);
	printf("\t = data throughput: %24g bytes/sec %s\n", rate,
	       human_readable_rate(rate));
    }

    if (TEST_SELECTION & (1 << 4)) {
	/* OVER-SUBSCRIBED FALSE-SHARING INCREMENT LOOP */
	printf("\tOver-subscribed false-sharing loop: ");
	fflush(stdout);
	increments = (aligned_t *) calloc(MAXPARALLELISM, sizeof(aligned_t));
	qtimer_start(timer);
	for (i = 0; i < MAXPARALLELISM; i++) {
	    qthread_fork(incrloop_falseshare, (void *)(intptr_t) i, rets + i);
	}
	for (i = 0; i < MAXPARALLELISM; i++) {
	    qthread_readFF(NULL, NULL, rets + i);
	}
	qtimer_stop(timer);
	free(increments);
	increments = NULL;

	printf("%8g secs (%u-threads %u iters)\n", qtimer_secs(timer),
	       (unsigned)MAXPARALLELISM, (unsigned)ITERATIONS);
	iprintf("\t + average increment time: %17g secs\n",
		qtimer_secs(timer) / (ITERATIONS * MAXPARALLELISM));
	printf("\t = increment throughput: %19f increments/sec\n",
	       (ITERATIONS * MAXPARALLELISM) / qtimer_secs(timer));
	rate =
	    (ITERATIONS * MAXPARALLELISM * sizeof(aligned_t)) /
	    qtimer_secs(timer);
	printf("\t = data throughput: %24g bytes/sec %s\n", rate,
	       human_readable_rate(rate));
    }

    if (TEST_SELECTION & (1 << 5)) {
	/* OVER-SUBSCRIBED INDEPENDENT INCREMENT LOOP */
	printf("\tOver-subscribed independent loop: ");
	fflush(stdout);
	qtimer_start(timer);
	for (i = 0; i < MAXPARALLELISM; i++) {
	    qthread_fork(incrloop_nocompete, (void *)(intptr_t) i, rets + i);
	}
	for (i = 0; i < MAXPARALLELISM; i++) {
	    qthread_readFF(NULL, NULL, rets + i);
	}
	qtimer_stop(timer);

	printf("%10g secs (%u-threads %u iters)\n", qtimer_secs(timer),
	       (unsigned)MAXPARALLELISM, (unsigned)ITERATIONS);
	iprintf("\t + average increment time: %17g secs\n",
		qtimer_secs(timer) / (ITERATIONS * MAXPARALLELISM));
	printf("\t = increment throughput: %19f increments/sec\n",
	       (ITERATIONS * MAXPARALLELISM) / qtimer_secs(timer));
	rate =
	    (ITERATIONS * MAXPARALLELISM * sizeof(aligned_t)) /
	    qtimer_secs(timer);
	printf("\t = data throughput: %24g bytes/sec %s\n", rate,
	       human_readable_rate(rate));
    }

    if (TEST_SELECTION & (1 << 6)) {
	/* INDEPENDENT ADD LOOP */
	printf("\tNon-atomic false-sharing loop: ");
	fflush(stdout);
	increments = (aligned_t *) calloc(MAXPARALLELISM, sizeof(aligned_t));
	qtimer_start(timer);
	for (i = 0; i < MAXPARALLELISM; i++) {
	    qthread_fork(addloop_falseshare, (void *)(intptr_t) i, rets + i);
	}
	for (i = 0; i < MAXPARALLELISM; i++) {
	    qthread_readFF(NULL, NULL, rets + i);
	}
	qtimer_stop(timer);
	free(increments);
	increments = NULL;

	printf("%13g secs (%u-way %u iters)\n", qtimer_secs(timer),
	       (unsigned)MAXPARALLELISM, (unsigned)ITERATIONS);
	iprintf("\t + average increment time: %17g secs\n",
		qtimer_secs(timer) / (ITERATIONS * MAXPARALLELISM));
	printf("\t = increment throughput: %19f increments/sec\n",
	       (ITERATIONS * MAXPARALLELISM) / qtimer_secs(timer));
	rate =
	    (ITERATIONS * MAXPARALLELISM * sizeof(aligned_t)) /
	    qtimer_secs(timer);
	printf("\t = data throughput: %24g bytes/sec %s\n", rate,
	       human_readable_rate(rate));
    }

    if (TEST_SELECTION & (1 << 7)) {
	/* INDEPENDENT NON-ATOMIC LOOP */
	printf("\tNon-atomic independent loop: ");
	fflush(stdout);
	qtimer_start(timer);
	for (i = 0; i < MAXPARALLELISM; i++) {
	    qthread_fork(addloop_nocompete, (void *)(intptr_t) i, rets + i);
	}
	for (i = 0; i < MAXPARALLELISM; i++) {
	    qthread_readFF(NULL, NULL, rets + i);
	}
	qtimer_stop(timer);

	printf("%15g secs (%u-way %u iters)\n", qtimer_secs(timer),
	       (unsigned)MAXPARALLELISM, (unsigned)ITERATIONS);
	iprintf("\t + average increment time: %17g secs\n",
		qtimer_secs(timer) / (ITERATIONS * MAXPARALLELISM));
	printf("\t = increment throughput: %19f increments/sec\n",
	       (ITERATIONS * MAXPARALLELISM) / qtimer_secs(timer));
	rate =
	    (ITERATIONS * MAXPARALLELISM * sizeof(aligned_t)) /
	    qtimer_secs(timer);
	printf("\t = data throughput: %24g bytes/sec %s\n", rate,
	       human_readable_rate(rate));
    }

    if (TEST_SELECTION & (1 << 8)) {
	printf("\tBalanced streaming loop: ");
	fflush(stdout);
	increments =
	    (aligned_t *) calloc(MAXPARALLELISM * ITERATIONS,
				 sizeof(aligned_t));
	qtimer_start(timer);
	qt_loop_balance(0, MAXPARALLELISM * ITERATIONS, streaming_incr, NULL);
	qtimer_stop(timer);
	for (i = 0; i < MAXPARALLELISM * ITERATIONS; i++)
	    assert(increments[i] == 1);
	free(increments);
	increments = NULL;

	printf("%19g secs (%u-threads %u iters)\n", qtimer_secs(timer),
	       shepherds, (unsigned)(ITERATIONS * MAXPARALLELISM));
	iprintf("\t + average increment time: %17g secs\n",
		qtimer_secs(timer) / (ITERATIONS * MAXPARALLELISM));
	printf("\t = increment throughput: %19f increments/sec\n",
	       (ITERATIONS * MAXPARALLELISM) / qtimer_secs(timer));
	rate =
	    (ITERATIONS * MAXPARALLELISM * sizeof(aligned_t)) /
	    qtimer_secs(timer);
	printf("\t = data throughput: %24g bytes/sec %s\n", rate,
	       human_readable_rate(rate));
    }

    if (TEST_SELECTION & (1 << 9)) {
	printf("\tOver-subscribed streaming loop: ");
	fflush(stdout);
	increments =
	    (aligned_t *) calloc(MAXPARALLELISM * ITERATIONS,
				 sizeof(aligned_t));
	qtimer_start(timer);
	for (i = 0; i < MAXPARALLELISM; i++) {
	    qthread_fork(incrstream, increments + (i * ITERATIONS), rets + i);
	}
	for (i = 0; i < MAXPARALLELISM; i++) {
	    qthread_readFF(NULL, NULL, rets + i);
	}
	qtimer_stop(timer);
	for (i = 0; i < MAXPARALLELISM * ITERATIONS; i++)
	    assert(increments[i] == 1);
	free(increments);
	increments = NULL;

	printf("%6g secs (%u-threads %u iters)\n", qtimer_secs(timer),
	       shepherds, (unsigned)(ITERATIONS * MAXPARALLELISM));
	iprintf("\t + average increment time: %17g secs\n",
		qtimer_secs(timer) / (ITERATIONS * MAXPARALLELISM));
	printf("\t = increment throughput: %19f increments/sec\n",
	       (ITERATIONS * MAXPARALLELISM) / qtimer_secs(timer));
	rate =
	    (ITERATIONS * MAXPARALLELISM * sizeof(aligned_t)) /
	    qtimer_secs(timer);
	printf("\t = data throughput: %24g bytes/sec %s\n", rate,
	       human_readable_rate(rate));
    }

    if (TEST_SELECTION & (1 << 10)) {
	printf("\tNon-atomic bal. streaming loop: ");
	fflush(stdout);
	increments =
	    (aligned_t *) calloc(MAXPARALLELISM * ITERATIONS,
				 sizeof(aligned_t));
	qtimer_start(timer);
	qt_loop_balance(0, MAXPARALLELISM * ITERATIONS, streaming_naincr,
			NULL);
	qtimer_stop(timer);
	for (i = 0; i < MAXPARALLELISM * ITERATIONS; i++)
	    assert(increments[i] == 1);
	free(increments);
	increments = NULL;

	printf("%6g secs (%u-threads %u iters)\n", qtimer_secs(timer),
	       shepherds, (unsigned)(ITERATIONS * MAXPARALLELISM));
	iprintf("\t + average increment time: %17g secs\n",
		qtimer_secs(timer) / (ITERATIONS * MAXPARALLELISM));
	printf("\t = increment throughput: %19f increments/sec\n",
	       (ITERATIONS * MAXPARALLELISM) / qtimer_secs(timer));
	rate =
	    (ITERATIONS * MAXPARALLELISM * sizeof(aligned_t)) /
	    qtimer_secs(timer);
	printf("\t = data throughput: %24g bytes/sec %s\n", rate,
	       human_readable_rate(rate));
    }

    qtimer_destroy(timer);
    free(rets);

    return 0;
}
