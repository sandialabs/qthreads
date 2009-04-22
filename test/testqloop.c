#ifdef HAVE_CONFIG_H
# include "config.h" /* for _GNU_SOURCE */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>		       /* for INT_MIN & friends (according to C89) */
#include <float.h>		       /* for DBL_MIN & friends (according to C89) */
#include <sys/time.h>
#include <time.h>
#include <qthread/qloop.h>

//#define BIGLEN 200000000U
#define BIGLEN 1000000U

int main(int argc, char *argv[])
{
    size_t i;
    struct timeval start, stop;
    long int threads = 3;
    unsigned int interactive = 0;

    if (argc > 1) {
	char * endptr = NULL;
	threads = strtol(argv[1], &endptr, 10);
	if (threads == LONG_MIN || threads == LONG_MAX || *endptr != 0) {
	    threads = 1;
	}
	interactive = 1;
    }
    qthread_init(threads);
    future_init(128);

    {
	aligned_t *uia, uitmp, uisum = 0, uiprod = 1, uimax = 0, uimin =
	    UINT_MAX;

	uia = malloc(sizeof(aligned_t) * BIGLEN);
	assert(uia);
	for (i = 0; i < BIGLEN; i++) {
	    uia[i] = random();
	}
	gettimeofday(&start, NULL);
	for (i = 0; i < BIGLEN; i++)
	    uisum += uia[i];
	gettimeofday(&stop, NULL);
	if (interactive) {
	    printf("[testqloop] summing-serial   %u uints took %g seconds\n",
		    BIGLEN, (stop.tv_sec + (stop.tv_usec * 1.0e-6)) -
		    (start.tv_sec + (start.tv_usec * 1.0e-6)));
	}
	fflush(stdout);
	gettimeofday(&start, NULL);
	uitmp = qt_uint_sum(uia, BIGLEN, 0);
	gettimeofday(&stop, NULL);
	if (interactive) {
	    printf("[testqloop] summing-parallel %u uints took %g seconds\n",
		    BIGLEN, (stop.tv_sec + (stop.tv_usec * 1.0e-6)) -
		    (start.tv_sec + (start.tv_usec * 1.0e-6)));
	}
	fflush(stdout);
	assert(uitmp == uisum);

	gettimeofday(&start, NULL);
	for (i = 0; i < BIGLEN; i++)
	    uiprod *= uia[i];
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] multiplying-serial   %u uints took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	fflush(stdout);
	gettimeofday(&start, NULL);
	uitmp = qt_uint_prod(uia, BIGLEN, 0);
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] multiplying-parallel %u uints took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	fflush(stdout);
	assert(uitmp == uiprod);

	gettimeofday(&start, NULL);
	for (i = 0; i < BIGLEN; i++)
	    if (uia[i] > uimax)
		uimax = uia[i];
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] findmax-serial   %u uints took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	fflush(stdout);
	gettimeofday(&start, NULL);
	uitmp = qt_uint_max(uia, BIGLEN, 0);
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] findmax-parallel %u uints took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	fflush(stdout);
	assert(uimax == uitmp);

	gettimeofday(&start, NULL);
	for (i = 0; i < BIGLEN; i++)
	    if (uia[i] < uimin)
		uimin = uia[i];
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] findmin-serial   %u uints took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	fflush(stdout);
	gettimeofday(&start, NULL);
	uitmp = qt_uint_min(uia, BIGLEN, 0);
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] findmin-parallel %u uints took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	fflush(stdout);
	assert(uitmp == uimin);
	free(uia);
    }

    {
	saligned_t *ia, itmp, isum = 0, iprod = 1, imax = INT_MIN, imin = INT_MAX;

	ia = malloc(sizeof(saligned_t) * BIGLEN);
	assert(ia);
	for (i = 0; i < BIGLEN; i++) {
	    ia[i] = random();
	}
	gettimeofday(&start, NULL);
	for (i = 0; i < BIGLEN; i++)
	    isum += ia[i];
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] summing-serial   %u ints took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	fflush(stdout);
	gettimeofday(&start, NULL);
	itmp = qt_int_sum(ia, BIGLEN, 0);
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] summing-parallel %u ints took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	fflush(stdout);
	assert(itmp == isum);

	gettimeofday(&start, NULL);
	for (i = 0; i < BIGLEN; i++)
	    iprod *= ia[i];
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] multiplying-serial   %u ints took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	fflush(stdout);
	gettimeofday(&start, NULL);
	itmp = qt_int_prod(ia, BIGLEN, 0);
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] multiplying-parallel %u ints took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	fflush(stdout);
	assert(itmp == iprod);

	gettimeofday(&start, NULL);
	for (i = 0; i < BIGLEN; i++)
	    if (ia[i] > imax)
		imax = ia[i];
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] findmax-serial   %u ints took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	fflush(stdout);
	gettimeofday(&start, NULL);
	itmp = qt_int_max(ia, BIGLEN, 0);
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] findmax-parallel %u ints took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	fflush(stdout);
	assert(imax == itmp);

	gettimeofday(&start, NULL);
	for (i = 0; i < BIGLEN; i++)
	    if (ia[i] < imin)
		imin = ia[i];
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] findmin-serial   %u ints took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	fflush(stdout);
	gettimeofday(&start, NULL);
	itmp = qt_int_min(ia, BIGLEN, 0);
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] findmin-parallel %u ints took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	fflush(stdout);
	assert(itmp == imin);
	free(ia);
    }

    {
	double *da, dtmp, dsum = 0.0, dprod = 1.0, dmin = DBL_MAX, dmax =
	    DBL_MIN;

	da = malloc(sizeof(double) * BIGLEN);
	assert(da);
	srandom(0xdeadbeef);
	for (i = 0; i < BIGLEN; i++) {
	    da[i] = random() / (double)RAND_MAX *10.0;
	}
	gettimeofday(&start, NULL);
	for (i = 0; i < BIGLEN; i++)
	    dsum += da[i];
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] summing-serial   %u doubles took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	assert(dsum > 0);
	fflush(stdout);
	gettimeofday(&start, NULL);
	dtmp = qt_double_sum(da, BIGLEN, 0);
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] summing-parallel %u doubles took %g second\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	assert(dtmp > 0);
	fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0; i < BIGLEN; i++)
	    dprod *= da[i];
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] multiplying-serial   %u doubles took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	assert(dprod > 0);
	fflush(stdout);
	gettimeofday(&start, NULL);
	dtmp = qt_double_prod(da, BIGLEN, 0);
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] multiplying-parallel %u doubles took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	assert(dtmp > 0);
	fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0; i < BIGLEN; i++)
	    if (da[i] > dmax)
		dmax = da[i];
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] findmax-serial   %u doubles took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	fflush(stdout);
	gettimeofday(&start, NULL);
	dtmp = qt_double_max(da, BIGLEN, 0);
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] findmax-parallel %u doubles took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	fflush(stdout);
	assert(dmax == dtmp);

	gettimeofday(&start, NULL);
	for (i = 0; i < BIGLEN; i++)
	    if (da[i] < dmin)
		dmin = da[i];
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] findmin-serial   %u doubles took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	fflush(stdout);
	gettimeofday(&start, NULL);
	dtmp = qt_double_min(da, BIGLEN, 0);
	gettimeofday(&stop, NULL);
	if (interactive) {
	printf("[testqloop] findmin-parallel %u doubles took %g seconds\n", BIGLEN,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
	}
	fflush(stdout);
	assert(dtmp == dmin);
	free(da);
    }

    qthread_finalize();
    return 0;
}
