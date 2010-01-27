#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "qtimer.h"
#include "qthread_asserts.h"

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

struct qtimer_s
{
    struct timespec start, stop;
};

static struct timespec res;
static int res_set = 0;

void qtimer_start(qtimer_t q)
{
    assert(q);
    qassert(clock_gettime(CLOCK_MONOTONIC, &(q->start)), 0);
}

void qtimer_stop(qtimer_t q)
{
    assert(q);
    qassert(clock_gettime(CLOCK_MONOTONIC, &(q->stop)), 0);
}

double qtimer_secs(qtimer_t q)
{
    assert(q);
    return (q->stop.tv_sec + q->stop.tv_nsec*1e-9) - (q->start.tv_sec + q->start.tv_nsec*1e-9);
}

qtimer_t qtimer_new()
{
    qtimer_t ret = calloc(1, sizeof(struct qtimer_s));
    assert(ret);
    if (res_set == 0) {
	qassert(clock_getres(CLOCK_MONOTONIC, &res), 0);
	printf("res of %lu secs and %lu nanosecs\n", (long unsigned)ret.tv_sec, (long unsigned)ret.tv_nsec);
	res_set = 1;
    }
    return ret;
}

void qtimer_free(qtimer_t q)
{
    assert(q);
    free(q);
}
