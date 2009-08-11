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
#include "qtimer.h"

int dcmp(const void *a, const void *b) {
    return(*(double*)a - *(double*)b);
}

int acmp(const void *a, const void *b) {
    return(*(aligned_t*)a - *(aligned_t*)b);
}

int main(int argc, char *argv[])
{
    aligned_t *ui_array, *ui_array2;
    double *d_array, *d_array2;
    int threads = 1;
    size_t len = 1000000, i;
    qtimer_t timer = qtimer_new();
    double cumulative_time=0.0;
    int interactive = 0;
    int using_doubles = 0;
    unsigned long iterations = 10;

    if (argc >= 2) {
	threads = strtol(argv[1], NULL, 0);
	if (threads < 0)
	    threads = 1;
	interactive = 1;
	printf("%i threads\n", threads);
    }
    if (argc >= 3) {
	len = strtoul(argv[2], NULL, 0);
	printf("len = %lu\n", (unsigned long)len);
    }
    if (argc >= 4) {
	iterations = strtoul(argv[3], NULL, 0);
	printf("%lu iterations\n", (unsigned long)iterations);
    }
    if (argc >= 5) {
	using_doubles = strtoul(argv[4], NULL, 0);
	printf("using %s\n", using_doubles?"doubles":"aligned_ts");
    }

    qthread_init(threads);

    if (using_doubles) {
	d_array = calloc(len, sizeof(double));
	for (i = 0; i < len; i++) {
	    d_array[i] = ((double)random())/((double)RAND_MAX) + random();
	}
	d_array2 = calloc(len, sizeof(double));
	if (interactive) {
	    printf("double array generated...\n");
	}
	for (unsigned int i=0;i<iterations;i++) {
	    memcpy(d_array2, d_array, len*sizeof(double));
	    qtimer_start(timer);
	    qutil_qsort(qthread_self(), d_array2, len);
	    qtimer_stop(timer);
	    cumulative_time += qtimer_secs(timer);
	    if (interactive == 1) {
		printf("\t%u: sorting %lu doubles with qutil took: %f seconds\n",
			i, (unsigned long)len,
			qtimer_secs(timer));
	    }
	}
	if (interactive == 1) {
	    printf("sorting %lu doubles with qutil took: %f seconds (avg)\n",
		    (unsigned long)len,
		    cumulative_time/(double)iterations);
	}
	cumulative_time = 0;
	for (unsigned int i=0;i<iterations;i++) {
	    memcpy(d_array2, d_array, len*sizeof(double));
	    qtimer_start(timer);
	    qsort(d_array2, len, sizeof(double), dcmp);
	    qtimer_stop(timer);
	    cumulative_time += qtimer_secs(timer);
	    if (interactive == 1) {
		printf("\t%u: sorting %lu doubles with libc took: %f seconds\n",
			i, (unsigned long)len,
			qtimer_secs(timer));
	    }
	}
	if (interactive == 1) {
	    printf("sorting %lu doubles with libc took: %f seconds\n",
		    (unsigned long)len,
		    cumulative_time/(double)iterations);
	}
	free(d_array);
	free(d_array2);
    } else {
	ui_array = calloc(len, sizeof(aligned_t));
	for (i = 0; i < len; i++) {
	    ui_array[i] = random();
	}
	ui_array2 = calloc(len, sizeof(aligned_t));
	if (interactive) {
	    printf("ui_array generated...\n");
	}
	for (int i=0;i<10;i++) {
	    memcpy(ui_array2, ui_array, len*sizeof(aligned_t));
	    qtimer_start(timer);
	    qutil_aligned_qsort(qthread_self(), ui_array2, len);
	    qtimer_stop(timer);
	    cumulative_time += qtimer_secs(timer);
	}
	if (interactive == 1) {
	    printf("sorting %lu aligned_ts with qutil took: %f seconds\n",
		    (unsigned long)len,
		    cumulative_time/(double)iterations);
	}
	cumulative_time = 0;
	for (int i=0;i<10;i++) {
	    memcpy(ui_array2, ui_array, len*sizeof(aligned_t));
	    qtimer_start(timer);
	    qsort(ui_array2, len, sizeof(double), acmp);
	    qtimer_stop(timer);
	    cumulative_time += qtimer_secs(timer);
	}
	if (interactive == 1) {
	    printf("sorting %lu aligned_ts with libc took: %f seconds (avg)\n",
		    (unsigned long)len,
		    cumulative_time/(double)iterations);
	}
	free(ui_array);
	free(ui_array2);
    }

    qtimer_free(timer);

    qthread_finalize();
    return 0;
}
