#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_TMC_CPUS_H
# include <tmc/cpus.h>
#endif

#include "qt_affinity.h"

void qt_affinity_init(
    void)
{
}

qthread_shepherd_id_t guess_num_shepherds(
    void)
{
    cpu_set_t online_cpus;
    qassert(tmc_cpus_get_online_cpus(&online_cpus), 0);
    return tmc_cpus_count(&online_cpus);
}

void qt_affinity_set(
    int node)
{
    if (tmc_cpus_set_my_cpu(node) < 0) {
	perror("tmc_cpus_set_my_affinity() failed");
	fprintf(stderr,"\tnode = %i\n", (int)node);
    }
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
    cpu_set_t online_cpus;
    unsigned int *cpu_array;
    size_t cpu_count, offset;

    qassert(tmc_cpus_get_online_cpus(&online_cpus), 0);
    cpu_count = tmc_cpus_count(&online_cpus);
    assert(cpu_count > 0);
    /* assign nodes */
    cpu_array = malloc(sizeof(unsigned int) * cpu_count);
    assert(cpu_array != NULL);
    qassert(tmc_cpus_to_array(&online_cpus, cpu_array, cpu_count), cpu_count);
    offset = 0;
    for (i = 0; i < nshepherds; i++) {
	sheps[i].node = cpu_array[offset];
	offset++;
	offset *= (offset < cpu_count);
    }
    free(cpu_array);
    for (i = 0; i < nshepherds; i++) {
	size_t j, k;
	unsigned int ix, iy;
	sheps[i].shep_dists =
	    calloc(nshepherds, sizeof(unsigned int));
	assert(sheps[i].shep_dists);
	tmc_cpus_grid_cpu_to_tile(sheps[i].node, &ix, &iy);
	for (j = 0; j < nshepherds; j++) {
	    unsigned int jx, jy;
	    tmc_cpus_grid_cpu_to_tile(sheps[j].node, &jx, &jy);
	    sheps[i].shep_dists[j] =
		abs((int)ix - (int)jx) + abs((int)iy - (int)jy);
	}
	sheps[i].sorted_sheplist =
	    calloc(nshepherds - 1, sizeof(qthread_shepherd_id_t));
	assert(sheps[i].sorted_sheplist);
	for (j = k = 0; j < nshepherds; j++) {
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
}
