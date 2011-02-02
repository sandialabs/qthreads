#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <hwloc.h>

#include "qthread_innards.h"
#include "qt_affinity.h"

static hwloc_topology_t topology;
static int shep_depth = -1;

static void qt_affinity_internal_hwloc_teardown(
    void)
{
    qthread_debug(ALL_DETAILS, "destroy hwloc topology handle\n");
    hwloc_topology_destroy(topology);
}

void qt_affinity_init(
    void)
{				       /*{{{ */
    qassert(hwloc_topology_init(&topology), 0);
    qassert(hwloc_topology_load(topology), 0);
    qthread_internal_cleanup(qt_affinity_internal_hwloc_teardown);
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
    /* the goal here is to basically pick the number of domains over which
     * memory access is the same cost (e.g. number of sockets, if all cores on
     * a given socket share top-level cache */
    shep_depth = hwloc_get_type_depth(topology, HWLOC_OBJ_CACHE);
    qthread_debug(ALL_DETAILS, "depth of OBJ_CACHE = %d\n", shep_depth);
    if (shep_depth == HWLOC_TYPE_DEPTH_UNKNOWN ||
	shep_depth == HWLOC_TYPE_DEPTH_MULTIPLE) {
	shep_depth = hwloc_get_type_depth(topology, HWLOC_OBJ_SOCKET);
	assert(hwloc_get_nbobjs_inside_cpuset_by_depth
	       (topology, hwloc_topology_get_allowed_cpuset(topology),
		shep_depth) > 0);
	qthread_debug(ALL_DETAILS, "depth of OBJ_SOCKET = %d\n", shep_depth);
    }
    if (shep_depth == HWLOC_TYPE_DEPTH_UNKNOWN ||
	shep_depth == HWLOC_TYPE_DEPTH_MULTIPLE) {
	qthread_debug(ALL_DETAILS,
		      "topology too weird... nbobjs_by_type HWLOC_OBJ_PU is %u\n",
		      (unsigned int)hwloc_get_nbobjs_by_depth(topology,
							      HWLOC_OBJ_PU));
	shep_depth = hwloc_get_type_depth(topology, HWLOC_OBJ_PU);
    }
#endif
}				       /*}}} */

qthread_shepherd_id_t guess_num_shepherds(
    void)
{				       /*{{{ */
    qthread_shepherd_id_t ret = 1;
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
    ret = hwloc_get_nbobjs_by_depth(topology, shep_depth);
    qthread_debug(ALL_DETAILS, "nbobjs_by_depth is %u, depth: %u\n",
		  (unsigned int)ret, shep_depth);
#else
    /* the goal here is to pick the number of independent processing units
     * (i.e. cores) */
    ret = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PU);
    qthread_debug(ALL_DETAILS, "nbobjs_by_type HWLOC_OBJ_PU is %u\n",
		  (unsigned int)ret);
#endif
    return ret;
}				       /*}}} */

#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
qthread_worker_id_t guess_num_workers_per_shep(
    qthread_shepherd_id_t nshepherds)
{				       /*{{{ */
    qthread_worker_id_t ret = 0;
    size_t total = 0;
    const size_t max_socket = hwloc_get_nbobjs_by_depth(topology, shep_depth);
    for (size_t socket = 0; socket < nshepherds && socket < max_socket;
	 ++socket) {
	hwloc_obj_t obj =
	    hwloc_get_obj_by_depth(topology, shep_depth, socket);
# if HWLOC_API_VERSION == 0x00010000
	hwloc_cpuset_t cpuset = obj->allowed_cpuset;
	unsigned int weight = hwloc_cpuset_weight(cpuset);
# else
	hwloc_bitmap_t cpuset = obj->allowed_cpuset;
	unsigned int weight = hwloc_bitmap_weight(cpuset);
# endif
	qthread_debug(ALL_DETAILS, "socket %u has %u weight\n",
		      (unsigned int)socket, weight);
	total += weight;
	if (socket == 0 || ret < weight) {
	    ret = weight;
	}
    }
    if (ret * nshepherds > total) {
	ret = total / nshepherds;
    }
    qthread_debug(ALL_DETAILS, "guessed %i workers for %i sheps\n", (int)ret,
		  (int)nshepherds);
    return ret;
}				       /*}}} */

void qt_affinity_set(
    qthread_worker_t * me)
{				       /*{{{ */
    hwloc_topology_t ltopology;
    qassert(hwloc_topology_init(&ltopology), 0);
    qassert(hwloc_topology_load(ltopology), 0);
    hwloc_const_cpuset_t allowed_cpuset = hwloc_topology_get_allowed_cpuset(ltopology);	// where am I allowed to run?
    qthread_shepherd_t *myshep = me->shepherd;
    hwloc_obj_t obj =
	hwloc_get_obj_inside_cpuset_by_depth(ltopology, allowed_cpuset,
					     shep_depth, myshep->node);
    size_t nbobjs =
	hwloc_get_nbobjs_inside_cpuset_by_type(ltopology, obj->allowed_cpuset,
					       HWLOC_OBJ_PU);
    qthread_debug(ALL_DETAILS, "shep %i worker %i, there are %i PUs\n",
		  me->shepherd->shepherd_id, me->worker_id, (int)nbobjs);
    hwloc_obj_t sub_obj =
	hwloc_get_obj_inside_cpuset_by_type(ltopology, obj->allowed_cpuset,
					    HWLOC_OBJ_PU,
					    me->worker_id % nbobjs);
#ifdef QTHREAD_DEBUG
    {
	char *str;
#if HWLOC_API_VERSION == 0x00010000
	hwloc_cpuset_asprintf(&str, sub_obj->allowed_cpuset);
#else
	hwloc_bitmap_asprintf(&str, sub_obj->allowed_cpuset);
#endif
	qthread_debug(ALL_DETAILS,
		      "binding shep %i worker %i (%i) to mask %s\n",
		      me->shepherd->shepherd_id, me->worker_id,
		      me->packed_worker_id, str);
	free(str);
    }
#endif
    if (hwloc_set_cpubind
	(ltopology, sub_obj->allowed_cpuset, HWLOC_CPUBIND_THREAD)) {
	char *str;
	int i = errno;
#ifdef __APPLE__
	if (i == ENOSYS) {
	    return;
	}
#endif
#if HWLOC_API_VERSION == 0x00010000
	hwloc_cpuset_asprintf(&str, sub_obj->allowed_cpuset);
#else
	hwloc_bitmap_asprintf(&str, sub_obj->allowed_cpuset);
#endif
	fprintf(stderr, "Couldn't bind to cpuset %s because %s (%i)\n", str,
		strerror(i), i);
	free(str);
    }
}				       /*}}} */
#else
void qt_affinity_set(
    qthread_shepherd_t * me)
{				       /*{{{ */
    hwloc_const_cpuset_t allowed_cpuset = hwloc_topology_get_allowed_cpuset(topology);	// where am I allowed to run?
    hwloc_obj_t obj =
	hwloc_get_obj_inside_cpuset_by_depth(topology, allowed_cpuset,
					     shep_depth, me->node);
    if (hwloc_set_cpubind
	(topology, obj->allowed_cpuset, HWLOC_CPUBIND_THREAD)) {
	char *str;
	int i = errno;
#ifdef __APPLE__
	if (i == ENOSYS) {
	    return;
	}
#endif
#if HWLOC_API_VERSION == 0x00010000
	hwloc_cpuset_asprintf(&str, obj->allowed_cpuset);
#else
	hwloc_bitmap_asprintf(&str, obj->allowed_cpuset);
#endif
	fprintf(stderr, "Couldn't bind to cpuset %s because %s (%i)\n", str,
		strerror(i), i);
	free(str);
    }
}				       /*}}} */
#endif

int qt_affinity_gendists(
    qthread_shepherd_t * sheps,
    qthread_shepherd_id_t nshepherds)
{				       /*{{{ */
    hwloc_const_cpuset_t allowed_cpuset = hwloc_topology_get_allowed_cpuset(topology);	// where am I allowed to run?
    size_t num_extant_objs;

#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
    num_extant_objs =
	hwloc_get_nbobjs_inside_cpuset_by_depth(topology, allowed_cpuset,
						shep_depth);
    for (size_t i = 0; i < nshepherds; ++i) {
	sheps[i].node = i % num_extant_objs;
    }
#else
    /* Find the number of bigger-than-PU objects in the CPU set */
    assert(hwloc_get_nbobjs_inside_cpuset_by_type
	   (topology, allowed_cpuset, HWLOC_OBJ_PU) >= 1);
    size_t *cpus_left_per_obj;	// handle heterogeneous core counts
    int over_subscribing = 0;
    shep_depth = hwloc_get_type_depth(topology, HWLOC_OBJ_PU);
    do {
	//printf("#objs at shep_depth %i is: %i\n", shep_depth, hwloc_get_nbobjs_inside_cpuset_by_depth(topology, allowed_cpuset, shep_depth));
	if (hwloc_get_nbobjs_inside_cpuset_by_depth
	    (topology, allowed_cpuset, shep_depth) <= 1) {
	    shep_depth++;
	    break;
	} else {
	    shep_depth--;
	}
    } while (1);
    //printf("top shep_depth is %i\n", shep_depth);
    num_extant_objs =
	hwloc_get_nbobjs_inside_cpuset_by_depth(topology, allowed_cpuset,
						shep_depth);
    cpus_left_per_obj = calloc(num_extant_objs, sizeof(size_t));
    /* Count how many PUs are in each obj */
    for (size_t i = 0; i < num_extant_objs; ++i) {
	unsigned int j;
	hwloc_obj_t obj =
	    hwloc_get_obj_inside_cpuset_by_depth(topology, allowed_cpuset,
						 shep_depth, i);
	hwloc_cpuset_t obj_cpuset = obj->allowed_cpuset;
	/* count how many PUs in this obj */
#if HWLOC_API_VERSION == 0x00010000
	hwloc_cpuset_foreach_begin(j, obj_cpuset)
	    cpus_left_per_obj[i]++;
	hwloc_cpuset_foreach_end();
#else
	hwloc_bitmap_foreach_begin(j, obj_cpuset)
	    cpus_left_per_obj[i]++;
	hwloc_bitmap_foreach_end();
#endif
	//printf("count[%i] = %i\n", (int)i, (int)cpus_left_per_obj[i]);
    }
    /* assign nodes by iterating over cpus_left_per_node array (which is of
     * size num_extant_nodes rather than of size nodes_i_can_use) */
    int obj = 0;
    for (size_t i = 0; i < nshepherds; ++i) {
	switch (over_subscribing) {
	    case 0:
	    {
		int count = 0;
		while (count < num_extant_objs && cpus_left_per_obj[obj] == 0) {
		    obj++;
		    obj *= (obj < num_extant_objs);
		    count++;
		}
		if (count < num_extant_objs) {
		    cpus_left_per_obj[obj]--;
		    break;
		}
	    }
		over_subscribing = 1;
	}
	qthread_debug(ALL_DETAILS, "setting shep %i to numa obj %i\n", (int)i,
		      (int)obj);
	sheps[i].node = obj;
	obj++;
	obj *= (obj < num_extant_objs);
    }
    free(cpus_left_per_obj);
#endif
    /* there does not seem to be a way to extract distances... <sigh> */
    return QTHREAD_SUCCESS;
}				       /*}}} */
