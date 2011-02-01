#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt_affinity.h"

void qt_affinity_init(
    void)
{
}

qthread_shepherd_id_t guess_num_shepherds(
    void)
{
#if defined(HAVE_SYSCONF) && defined(_SC_NPROCESSORS_CONF) /* Linux */
	long ret = sysconf(_SC_NPROCESSORS_CONF);
	nshepherds = (ret > 0) ? ret : 1;
#elif defined(HAVE_SYSCTL) && defined(CTL_HW) && defined(HW_NCPU)
	int name[2] = { CTL_HW, HW_NCPU };
	uint32_t oldv;
	size_t oldvlen = sizeof(oldv);
	if (sysctl(name, 2, &oldv, &oldvlen, NULL, 0) >= 0) {
	    assert(oldvlen == sizeof(oldv));
	    nshepherds = (int)oldv;
	}
#endif
    return 1;
}

#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
void qt_affinity_set(
	qthread_worker_t *me)
{
}
#else
void qt_affinity_set(
	qthread_shepherd_t *me)
{
}
#endif

#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
unsigned int guess_num_workers_per_shep(
    qthread_shepherd_id_t nshepherds)
{
    return 1;
}
#endif

void qt_affinity_gendists(
    qthread_shepherd_t * sheps,
    qthread_shepherd_id_t nshepherds)
{
}
