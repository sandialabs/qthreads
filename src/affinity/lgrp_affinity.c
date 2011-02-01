#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_PROCESSOR_BIND
# include <sys/types.h>
# include <sys/processor.h>
# include <sys/procset.h>
# ifdef HAVE_SYS_LGRP_USER_H
#  include <sys/lgrp_user.h>
# endif
#endif
#include <stdio.h>		       /* for perror() */
#include <stdlib.h>		       /* for malloc() */

#include "qthread_innards.h"
#include "qt_affinity.h"

#include "shepcomp.h"

static lgrp_cookie_t lgrp_cookie;

void qt_affinity_init(
    void)
{
    lgrp_cookie = lgrp_init(LGRP_VIEW_OS);
}

qthread_shepherd_id_t guess_num_shepherds(
    void)
{
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
# warning affinity logic is totally wrong for multithreaded shepherds
#else
    return lgrp_cpus(lgrp_cookie, lgrp_root(lgrp_cookie), NULL, 0,
		     LGRP_CONTENT_ALL);
#endif
}

#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
void qt_affinity_set(
    qthread_worker_t * me)
{
    if (lgrp_affinity_set
	(P_LWPID, P_MYID, me->shepherd->lgrp, LGRP_AFF_STRONG) != 0) {
	perror("lgrp_affinity_set");
    }
}
#else
void qt_affinity_set(
    qthread_shepherd_t * me)
{
    if (lgrp_affinity_set(P_LWPID, P_MYID, me->lgrp, LGRP_AFF_STRONG) != 0) {
	perror("lgrp_affinity_set");
    }
}
#endif

#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
unsigned int guess_num_workers_per_shep(
    qthread_shepherd_id_t nshepherds)
{
    return 1;
}
#endif

static int lgrp_walk(
    const lgrp_id_t lgrp,
    processorid_t ** cpus,
    lgrp_id_t * lgrp_ids,
    int cpu_grps)
{				       /*{{{ */
    int nchildren, ncpus =
	lgrp_cpus(lgrp_cookie, lgrp, NULL, 0, LGRP_CONTENT_DIRECT);

    if (ncpus == -1) {
	return cpu_grps;
    } else if (ncpus > 0) {
	processorid_t *cpuids = malloc((ncpus + 1) * sizeof(processorid_t));

	ncpus =
	    lgrp_cpus(lgrp_cookie, lgrp, cpuids, ncpus, LGRP_CONTENT_DIRECT);
	if (ncpus == -1) {
	    free(cpuids);
	    return cpu_grps;
	}
	cpuids[ncpus] = -1;
	cpus[cpu_grps] = cpuids;
	lgrp_ids[cpu_grps] = lgrp;
	cpu_grps++;
    }
    nchildren = lgrp_children(lgrp_cookie, lgrp, NULL, 0);
    if (nchildren == -1) {
	return cpu_grps;
    } else if (nchildren > 0) {
	int i;
	lgrp_id_t *children = malloc(nchildren * sizeof(lgrp_id_t));

	nchildren = lgrp_children(lgrp_cookie, lgrp, children, nchildren);
	if (nchildren == -1) {
	    free(children);
	    return cpu_grps;
	}
	for (i = 0; i < nchildren; i++) {
	    cpu_grps = lgrp_walk(children[i], cpus, lgrp_ids, cpu_grps);
	}
    }
    return cpu_grps;
}				       /*}}} */

int qt_affinity_gendists(
    qthread_shepherd_t * sheps,
    qthread_shepherd_id_t nshepherds)
{
    unsigned int lgrp_offset;
    int lgrp_count_grps;
    processorid_t **cpus = NULL;
    lgrp_id_t *lgrp_ids = NULL;

    switch (lgrp_cookie) {
	case EINVAL:
	case ENOMEM:
	    qthread_debug(ALL_DETAILS, "lgrp_cookie is invalid!\n");
	    return QTHREAD_THIRD_PARTY_ERROR;
    }
    {
	size_t max_lgrps = lgrp_nlgrps(lgrp_cookie);

	if (max_lgrps <= 0) {
	    qthread_debug(ALL_DETAILS, "max_lgrps is <= zero! (%i)\n",
			  max_lgrps);
	    return QTHREAD_THIRD_PARTY_ERROR;
	}
	cpus = calloc(max_lgrps, sizeof(processorid_t *));
	assert(cpus);
	lgrp_ids = calloc(max_lgrps, sizeof(lgrp_id_t));
	assert(lgrp_ids);
    }
    lgrp_count_grps = lgrp_walk(lgrp_root(lgrp_cookie), cpus, lgrp_ids, 0);
    if (lgrp_count_grps <= 0) {
	qthread_debug(ALL_DETAILS, "lgrp_count_grps is <= zero ! (%i)\n",
		      lgrp_count_grps);
	return QTHREAD_THIRD_PARTY_ERROR;
    }
    for (qthread_shepherd_id_t i = 0; i < nshepherds; i++) {
	/* first, pick a lgrp/node */
	int cpu;
	unsigned int first_loff;

	first_loff = lgrp_offset = i % lgrp_count_grps;
	sheps[i].node = -1;
	sheps[i].lgrp = -1;
	/* now pick an available CPU */
	while (1) {
	    cpu = 0;
	    /* find an unused one */
	    while (cpus[lgrp_offset][cpu] != (processorid_t) (-1))
		cpu++;
	    if (cpu == 0) {
		/* if no unused ones... try the next lgrp */
		lgrp_offset++;
		lgrp_offset *= (lgrp_offset < lgrp_count_grps);
		if (lgrp_offset == first_loff) {
		    break;
		}
	    } else {
		/* found one! */
		cpu--;
		sheps[i].node = cpus[lgrp_offset][cpu];
		sheps[i].lgrp = lgrp_ids[lgrp_offset];
		cpus[lgrp_offset][cpu] = -1;
		break;
	    }
	}
    }
    for (qthread_shepherd_id_t i = 0; i < nshepherds; i++) {
	const unsigned int node_i = sheps[i].lgrp;
	size_t j;
	sheps[i].shep_dists = calloc(nshepherds, sizeof(unsigned int));
	assert(sheps[i].shep_dists);
	for (j = 0; j < nshepherds; j++) {
	    const unsigned int node_j = sheps[j].lgrp;

	    if (node_i != QTHREAD_NO_NODE && node_j != QTHREAD_NO_NODE) {
		int ret = lgrp_latency_cookie(lgrp_cookie, node_i, node_j,
					      LGRP_LAT_CPU_TO_MEM);

		if (ret < 0) {
		    assert(ret >= 0);
		    return QTHREAD_THIRD_PARTY_ERROR;
		} else {
		    sheps[i].shep_dists[j] = (unsigned int)ret;
		}
	    } else {
		/* XXX too arbitrary */
		if (i == j) {
		    sheps[i].shep_dists[j] = 12;
		} else {
		    sheps[i].shep_dists[j] = 18;
		}
	    }
	}
    }
    for (qthread_shepherd_id_t i = 0; i < nshepherds; i++) {
	size_t j, k = 0;

	sheps[i].sorted_sheplist =
	    calloc(nshepherds - 1, sizeof(qthread_shepherd_id_t));
	assert(sheps[i].sorted_sheplist);
	for (j = 0; j < nshepherds; j++) {
	    if (j != i) {
		sheps[i].sorted_sheplist[k++] = j;
	    }
	}
#if defined(HAVE_QSORT_R) && !defined(__linux__)
	qsort_r(sheps[i].sorted_sheplist, nshepherds - 1,
		sizeof(qthread_shepherd_id_t), (void *)(intptr_t) i,
		&qthread_internal_shepcomp);
#elif defined(HAVE_QSORT_R)
	qsort_r(sheps[i].sorted_sheplist, nshepherds - 1,
		sizeof(qthread_shepherd_id_t), &qthread_internal_shepcomp,
		(void *)(intptr_t) i);
#else
	shepcomp_src = (qthread_shepherd_id_t) i;
	qsort(sheps[i].sorted_sheplist, nshepherds - 1,
	      sizeof(qthread_shepherd_id_t), qthread_internal_shepcomp);
#endif
    }
    if (cpus) {
	for (int i = 0; i < lgrp_count_grps; i++) {
	    free(cpus[i]);
	}
	free(cpus);
    }
    if (lgrp_ids) {
	free(lgrp_ids);
    }
}
