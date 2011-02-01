#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_SYSCTL
# ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
# endif
# ifdef HAVE_SYS_SYSCTL_H
#  include <sys/sysctl.h>
# endif
#endif
#if defined(HAVE_SYSCONF) && defined(HAVE_UNISTD_H)
# include <unistd.h>
#endif

#include <plpa.h>

#include "qt_affinity.h"

void qt_affinity_init(
    void)
{
}

qthread_shepherd_id_t guess_num_shepherds(
    void)
{
    qthread_shepherd_id_t nshepherds = 1;
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
    return nshepherds;
}

void qt_affinity_set(
    int node)
{
    plpa_cpu_set_t *cpuset =
	(plpa_cpu_set_t *) malloc(sizeof(plpa_cpu_set_t));
    PLPA_CPU_ZERO(cpuset);
    PLPA_CPU_SET(me->shepherd_id, cpuset);
    if (plpa_sched_setaffinity(0, sizeof(plpa_cpu_set_t), cpuset) < 0 &&
	    errno != EINVAL) {
	perror("plpa setaffinity");
    }
    free(cpuset);
}

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
    /* there is no inherent way to detect distances, so unfortunately we must assume that they're all equidistant */
}
