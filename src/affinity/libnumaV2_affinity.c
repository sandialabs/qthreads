#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <numa.h>

#include "qt_affinity.h"

void qt_affinity_init(
    void)
{
}

qthread_shepherd_id_t guess_num_shepherds(
    void)
{
    qthread_shepherd_id_t nshepherds = 1;
    if (numa_available() != 1) {
	unsigned long bmask = 0;
	unsigned long count = 0;
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
	/* this is (probably) correct if/when we have multithreaded shepherds,
	 * ... BUT ONLY IF ALL NODES HAVE CPUS!!!!!! */
	nshepherds = numa_max_node() + 1;
	qthread_debug(ALL_DETAILS, "numa_max_node() returned %i\n",
		      nshepherds);
#else
# ifdef HAVE_NUMA_NUM_THREAD_CPUS
	/* note: not numa_num_configured_cpus(), just in case an
	 * artificial limit has been imposed. */
	nshepherds = numa_num_thread_cpus();
	qthread_debug(ALL_DETAILS, "numa_num_thread_cpus returned %i\n",
		      nshepherds);
# elif defined(HAVE_NUMA_BITMASK_NBYTES)
	for (size_t b = 0; b < numa_bitmask_nbytes(numa_all_cpus_ptr) * 8;
	     b++) {
	    nshepherds += numa_bitmask_isbitset(numa_all_cpus_ptr, b);
	}
	qthread_debug(ALL_DETAILS,
		      "after checking through the all_cpus_ptr, I counted %i cpus\n",
		      nshepherds);
# else
	nshepherds = numa_max_node() + 1;
	qthread_debug(ALL_DETAILS, "numa_max_node() returned %i\n",
		      nshepherds);
# endif
#endif /* MULTITHREADED */
    }
    return nshepherds;
}

#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
void qt_affinity_set(
	qthread_worker_t *me)
{
    if (numa_run_on_node(me->shepherd->node) != 0) {
	numa_error("setting thread affinity");
    }
    numa_set_preferred(me->shepherd->node);
}
#else
void qt_affinity_set(
	qthread_shepherd_t *me)
{
    if (numa_run_on_node(me->node) != 0) {
	numa_error("setting thread affinity");
    }
    numa_set_preferred(me->node);
}
#endif

#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
unsigned int guess_num_workers_per_shep(
    qthread_shepherd_id_t nshepherds)
{
    return 1;
}
#endif

static void assign_nodes(
    qthread_shepherd_t * sheps,
    size_t nsheps)
{				       /*{{{ */
    const size_t num_extant_nodes = numa_max_node() + 1;
    struct bitmask *nmask = numa_get_run_node_mask();
    struct bitmask *cmask = numa_allocate_cpumask();
    size_t *cpus_left_per_node = calloc(num_extant_nodes, sizeof(size_t));	// handle heterogeneous core counts
    int over_subscribing = 0;

    assert(cmask);
    assert(nmask);
    assert(cpus_left_per_node);
    numa_bitmask_clearall(cmask);
    /* get the # cpus for each node */
    for (size_t i = 0; i < numa_bitmask_nbytes(nmask) * 8; ++i) {
	if (numa_bitmask_isbitset(nmask, i)) {
	    numa_node_to_cpus(i, cmask);
	    for (size_t j = 0; j < numa_bitmask_nbytes(cmask) * 8; j++) {
		cpus_left_per_node[i] +=
		    numa_bitmask_isbitset(cmask, j) ? 1 : 0;
	    }
	    qthread_debug(ALL_DETAILS, "there are %i CPUs on node %i\n",
			  (int)cpus_left_per_node[i], (int)i);
	}
    }
    /* assign nodes by iterating over cpus_left_per_node array (which is of
     * size num_extant_nodes rather than of size nodes_i_can_use) */
    int node = 0;
    for (size_t i = 0; i < nsheps; ++i) {
	switch (over_subscribing) {
	    case 0:
	    {
		int count = 0;
		while (count < num_extant_nodes &&
		       cpus_left_per_node[node] == 0) {
		    node++;
		    node *= (node < num_extant_nodes);
		    count++;
		}
		if (count < num_extant_nodes) {
		    cpus_left_per_node[node]--;
		    break;
		}
	    }
		over_subscribing = 1;
	}
	qthread_debug(ALL_DETAILS, "setting shep %i to numa node %i\n",
		      (int)i, (int)node);
	sheps[i].node = node;
	node++;
	node *= (node < num_extant_nodes);
    }
    numa_bitmask_free(nmask);
    numa_bitmask_free(cmask);
    free(cpus_left_per_node);
}				       /*}}} */

void qt_affinity_gendists(
    qthread_shepherd_t * sheps,
    qthread_shepherd_id_t nshepherds)
{
    if (numa_available() == -1) {
	return;
    }
    assign_nodes(sheps, nshepherds);
# ifdef HAVE_NUMA_DISTANCE
    /* truly ancient versions of libnuma (in the changelog, this is
     * considered "pre-history") do not have numa_distance() */
    for (i = 0; i < nshepherds; i++) {
	const unsigned int node_i = sheps[i].node;
	size_t j, k;
	sheps[i].shep_dists = calloc(nshepherds, sizeof(unsigned int));
	assert(sheps[i].shep_dists);
	for (j = 0; j < nshepherds; j++) {
	    const unsigned int node_j = sheps[j].node;

	    if (node_i != QTHREAD_NO_NODE && node_j != QTHREAD_NO_NODE) {
		sheps[i].shep_dists[j] = numa_distance(node_i, node_j);
	    } else {
		/* XXX too arbitrary */
		if (i == j) {
		    sheps[i].shep_dists[j] = 0;
		} else {
		    sheps[i].shep_dists[j] = 20;
		}
	    }
	}
	sheps[i].sorted_sheplist =
	    calloc(nshepherds - 1, sizeof(qthread_shepherd_id_t));
	assert(sheps[i].sorted_sheplist);
	k = 0;
	for (j = 0; j < nshepherds; j++) {
	    if (j != i) {
		sheps[i].sorted_sheplist[k++] = j;
	    }
	}
#  if defined(HAVE_QSORT_R) && defined(QTHREAD_QSORT_BSD)
	assert(sheps[i].sorted_sheplist);
	qsort_r(sheps[i].sorted_sheplist, nshepherds - 1,
		sizeof(qthread_shepherd_id_t), (void *)(intptr_t) i,
		&qthread_internal_shepcomp);
#  elif defined(HAVE_QSORT_R) && defined(QTHREAD_QSORT_GLIBC)
	/* what moron in the linux community decided to implement BSD's
	 * qsort_r with the arguments reversed??? */
	assert(sheps[i].sorted_sheplist);
	qsort_r(sheps[i].sorted_sheplist, nshepherds - 1,
		sizeof(qthread_shepherd_id_t), &qthread_internal_shepcomp,
		(void *)(intptr_t) i);
#  else
	shepcomp_src = (qthread_shepherd_id_t) i;
	qsort(sheps[i].sorted_sheplist, nshepherds - 1,
	      sizeof(qthread_shepherd_id_t), qthread_internal_shepcomp);
#  endif
    }
# endif
}
