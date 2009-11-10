#ifdef HAVE_CONFIG_H
# include "config.h"		       /* for _GNU_SOURCE */
#endif

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>		       /* for INT_MIN & friends (according to C89) */
#include <float.h>		       /* for DBL_EPSILON (according to C89) */
#include <math.h>		       /* for fabs() */
#include <assert.h>

#include <sys/time.h>		       /* for gettimeofday() */
#include <time.h>		       /* for gettimeofday() */

#include <qthread/qutil.h>

int interactive = 0;
struct timeval start, stop;

int main(int argc, char *argv[])
{
    aligned_t *ui_array;
    double *d_array;
    int threads = 1;
    size_t len = 1000000, i;

    if (argc >= 2) {
	threads = strtol(argv[1], NULL, 0);
	if (threads < 0)
	    threads = 1;
	interactive = 1;
    }
    if (argc >= 3) {
	len = strtoul(argv[2], NULL, 0);
    }

    qthread_init(threads);

    ui_array = calloc(len, sizeof(aligned_t));
    for (i = 0; i < len; i++) {
	ui_array[i] = random();
    }
    if (interactive) {
	printf("ui_array generated...\n");
    }
    gettimeofday(&start, NULL);
    qutil_aligned_qsort(qthread_self(), ui_array, len);
    gettimeofday(&stop, NULL);
    if (interactive == 1) {
	printf("done sorting, checking correctness...\n");
    }
    for (i = 0; i < len - 1; i++) {
	if (ui_array[i] > ui_array[i + 1]) {
	    /*
	     * size_t j;
	     *
	     * for (j = i-20; j < i+20; j++) {
	     * if (j % 8 == 0)
	     * printf("\n");
	     * printf("[%6u]=%2.5f ", j, d_array[j]);
	     * }
	     * printf("\n");
	     */
	    printf("out of order at %lu: %lu > %lu\n", (unsigned long)i,
		   (unsigned long)ui_array[i],
		   (unsigned long)ui_array[i + 1]);
	    abort();
	}
    }
    if (interactive == 1) {
	printf("sorting %lu aligned_ts took: %f seconds\n",
	       (unsigned long)len,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
    }
    free(ui_array);

    d_array = calloc(len, sizeof(double));
    for (i = 0; i < len; i++) {
	d_array[i] = random() / (double)RAND_MAX *10;
    }
    if (interactive) {
	printf("d_array generated...\n");
    }
    gettimeofday(&start, NULL);
    qutil_qsort(qthread_self(), d_array, len);
    gettimeofday(&stop, NULL);
    if (interactive) {
	printf("done sorting, checking correctness...\n");
    }
    for (i = 0; i < len - 1; i++) {
	if (d_array[i] > d_array[i + 1]) {
	    printf("out of order at %lu: %f > %f\n", (unsigned long)i,
		   d_array[i], d_array[i + 1]);
	    abort();
	}
    }
    if (interactive == 1) {
	printf("sorting %lu doubles took: %f seconds\n", (unsigned long)len,
	       (stop.tv_sec + (stop.tv_usec * 1.0e-6)) - (start.tv_sec +
							  (start.tv_usec *
							   1.0e-6)));
    }
    free(d_array);

    return 0;
}
