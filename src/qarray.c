#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>		       /* for calloc() */
#include <unistd.h>		       /* for getpagesize() */
#include <qthread/qarray.h>
#include "qthread_innards.h"	       /* for qthread_shepherd_count() */

struct qarray_s
{
    size_t unit_size;
    size_t count;
    size_t cluster_size;	/* units in a cluster */
    size_t cluster_bytes;	/* bytes per cluster (sometimes > unit_size*cluster_count) */
    char *base_ptr;
    distribution_t dist_type;
    qthread_shepherd_id_t dist_shep;	/* for ALL_SAME dist type */
};

static unsigned short pageshift = 0;
static aligned_t *chunk_distribution_tracker = NULL;

/* local funcs */
static QINLINE size_t qarray_gcd(size_t a, size_t b)
{				       /*{{{ */
    while (1) {
	if (a == 0)
	    return b;
	b %= a;
	if (b == 0)
	    return a;
	a %= b;
    }
}				       /*}}} */
static QINLINE size_t qarray_lcm(size_t a, size_t b)
{				       /*{{{ */
    size_t tmp = qarray_gcd(a, b);

    return (tmp != 0) ? (a * b / tmp) : 0;
}				       /*}}} */

qarray *qarray_create(const size_t count, const size_t unit_size,
		      const distribution_t d)
{
    size_t pagesize;
    size_t cluster_count;	/* number of clusters allocated */
    qarray *ret = calloc(1, sizeof(qarray));

    assert(count > 0);
    assert(unit_size > 0);

    if (pageshift == 0) {
	pagesize = getpagesize() - 1;
	while (pagesize != 0) {
	    pageshift++;
	    pagesize >>= 1;
	}
    }
    pagesize = 1 << pageshift;

    if (chunk_distribution_tracker == NULL) {
	chunk_distribution_tracker =
	    calloc(qthread_shepherd_count(), sizeof(aligned_t));
    }

    ret->count = count;

    /* so, here's the idea: memory is assigned to shepherds in units I'm
     * choosing to call "clusters" (chunk would also work, but that's overused
     * elsewhere in qthreads). Each cluster can have its own shepherd. Which
     * shepherd a cluster is assigned to is stored in the cluster itself
     * (otherwise we'd have to use a hash table, and we'd lose all our cache
     * benefits). In SOME cases, such as FIXED_HASH and ALL_SAME, this is
     * unnecessary, so we can be a little more efficient with things by NOT
     * storing a shepherd ID in the clusters. */
    /***************************
     * Choose allocation sizes *
     ***************************/
    switch (d) {
	case ALL_LOCAL:
	case ALL_RAND:
	case ALL_LEAST:
	case ALL_SAME:		       /* assumed equivalent to ALL_LOCAL */
	case FIXED_HASH:
	default:
	    ret->unit_size = unit_size;
	    if (unit_size > pagesize) {
		ret->cluster_bytes = qarray_lcm(unit_size, pagesize);
	    } else {
		ret->cluster_bytes = pagesize;
	    }
	    ret->cluster_size = ret->cluster_bytes / unit_size;
	    break;
	case DIST_REG_STRIPES:
	case DIST_REG_FIELDS:
	case DIST_RAND:
	case DIST_LEAST:
	case DIST:		       /* assumed equivalent to DIST_RAND */
	    /* since we will be storing a qthread_shepherd_id_t in each
	     * cluster, we need to leave space in the cluster for that data.
	     * The way we'll do this is that we'll just reduce the cluster_size
	     * by 1 (thus providing space for the shepherd identifier, as long
	     * as the unit-size is bigger than a shepherd identifier). */
	    ret->unit_size =
		(unit_size >
		 sizeof(qthread_shepherd_id_t)) ? unit_size :
		sizeof(qthread_shepherd_id_t);
	    ret->cluster_bytes = qarray_lcm(unit_size, pagesize);
	    ret->cluster_size = (ret->cluster_bytes / unit_size) - 1;
	    if (unit_size > (pagesize + sizeof(qthread_shepherd_id_t))) {
		ret->cluster_bytes = ret->cluster_size * unit_size;
		ret->cluster_bytes +=
		    pagesize - (ret->cluster_bytes % pagesize);
	    }
	    break;
    }

    /*****************
     * Set dist_type *
     *****************/
    switch (d) {
	case ALL_LOCAL:
	case ALL_RAND:
	case ALL_LEAST:
	case ALL_SAME:
	    ret->dist_type = ALL_SAME;
	    break;
	case FIXED_HASH:
	default:
	    ret->dist_type = FIXED_HASH;
	    break;
	case DIST_REG_STRIPES:
	case DIST_REG_FIELDS:
	case DIST_RAND:
	case DIST_LEAST:
	case DIST:
	    ret->dist_type = DIST;
	    break;
    }

    cluster_count =
	count / ret->cluster_size + ((count % ret->cluster_size) ? 1 : 0);

    ret->base_ptr = (char*) calloc(cluster_count, ret->cluster_bytes);
    if (ret->base_ptr == NULL) {
	free(ret);
	ret = NULL;
    }

    /********************************************
     * Assign locations, maintain cluster_count *
     ********************************************/
    switch (d) {
	case ALL_SAME:
	case ALL_LOCAL:
	    ret->dist_shep = qthread_shep(NULL);
	    qthread_incr(&chunk_distribution_tracker[ret->dist_shep],
			 cluster_count);
	    break;
	case ALL_RAND:
	    ret->dist_shep = random() % qthread_shepherd_count();
	    qthread_incr(&chunk_distribution_tracker[ret->dist_shep],
			 cluster_count);
	    break;
	case ALL_LEAST:
	{
	    qthread_shepherd_id_t i, least = 0;

	    for (i = 1; i < qthread_shepherd_count(); i++) {
		if (chunk_distribution_tracker[i] <
		    chunk_distribution_tracker[least]) {
		    least = i;
		}
	    }
	    ret->dist_shep = least;
	    qthread_incr(&chunk_distribution_tracker[least], cluster_count);
	}
	    break;
	case FIXED_HASH:
	default:
	{
	    size_t cluster;

	    for (cluster = 0; cluster < cluster_count; cluster++) {
		void *clusterhead =
		    qarray_elem_nomigrate(ret, cluster * ret->cluster_size);
		qthread_incr(&chunk_distribution_tracker
			     [(((uintptr_t) clusterhead) >> pageshift) %
			      qthread_shepherd_count()], 1);
	    }
	}
	    break;
	case DIST_REG_STRIPES:
	{
	    size_t cluster;
	    const qthread_shepherd_id_t max_sheps = qthread_shepherd_count();

	    for (cluster = 0; cluster < cluster_count; cluster++) {
		char *clusterhead =
		    qarray_elem_nomigrate(ret, cluster * ret->cluster_size);
		clusterhead += ret->cluster_size * unit_size;
		*(qthread_shepherd_id_t *) clusterhead = cluster % max_sheps;
		qthread_incr(&chunk_distribution_tracker[cluster % max_sheps],
			     1);
	    }
	}
	    break;
	case DIST_REG_FIELDS:
	{
	    size_t cluster;
	    const qthread_shepherd_id_t max_sheps = qthread_shepherd_count();
	    const size_t field_size = cluster_count / max_sheps;
	    qthread_shepherd_id_t cur_shep = 0;

	    for (cluster = 0; cluster < cluster_count; cluster++) {
		char *ptr =
		    qarray_elem_nomigrate(ret, cluster * ret->cluster_size);
		ptr += ret->cluster_size * unit_size;
		*(qthread_shepherd_id_t *) ptr = cur_shep;
		qthread_incr(&chunk_distribution_tracker[cur_shep], 1);
		if ((cluster % max_sheps) == (max_sheps - 1)) {
		    cur_shep++;
		    cur_shep *= (cur_shep != max_sheps);
		}
	    }
	}
	    break;
	case DIST:		       /* assumed equivalent to DIST_RAND */
	case DIST_RAND:
	{
	    size_t cluster;
	    const qthread_shepherd_id_t max_sheps = qthread_shepherd_count();

	    for (cluster = 0; cluster < cluster_count; cluster++) {
		char *ptr =
		    qarray_elem_nomigrate(ret, cluster * ret->cluster_size);
		ptr += ret->cluster_size * unit_size;
		*(qthread_shepherd_id_t *) ptr = random() % max_sheps;
		qthread_incr(&chunk_distribution_tracker
			     [*(qthread_shepherd_id_t *) ptr], 1);
	    }
	}
	    break;
	case DIST_LEAST:
	{
	    size_t cluster;
	    const qthread_shepherd_id_t max_sheps = qthread_shepherd_count();

	    for (cluster = 0; cluster < cluster_count; cluster++) {
		qthread_shepherd_id_t i, least = 0;
		char *ptr =
		    qarray_elem_nomigrate(ret, cluster * ret->cluster_size);
		ptr += ret->cluster_size * unit_size;

		for (i = 1; i < max_sheps; i++) {
		    if (chunk_distribution_tracker[i] <
			chunk_distribution_tracker[least]) {
			least = i;
		    }
		}
		*(qthread_shepherd_id_t *) ptr = least;
		qthread_incr(&chunk_distribution_tracker[least], 1);
	    }
	}
	    break;
    }
    return ret;
}

static inline qthread_shepherd_id_t *qarray_internal_cluster_shep(const qarray
								  * a,
								  const void
								  *cluster_head)
{
    return (qthread_shepherd_id_t *) (((char*)cluster_head) +
				      (a->cluster_size * a->unit_size));
}

void qarray_free(qarray * a)
{
    assert(a);
    assert(a->base_ptr);
    if (a) {
	if (a->base_ptr) {
	    switch (a->dist_type) {
		case DIST:
		{
		    size_t cluster;
		    const size_t cluster_count =
			a->count / a->cluster_size +
			((a->count % a->cluster_size) ? 1 : 0);
		    for (cluster = 0; cluster < cluster_count; cluster++) {
			char *clusterhead = qarray_elem_nomigrate(a,
								  cluster *
								  a->
								  cluster_size);
			clusterhead += a->cluster_size * a->unit_size;
			qthread_incr(&chunk_distribution_tracker
				     [*(qthread_shepherd_id_t *) clusterhead],
				     -1);
		    }
		}
		    break;
		default:
		case FIXED_HASH:
		{
		    size_t cluster;
		    const size_t cluster_count =
			a->count / a->cluster_size +
			((a->count % a->cluster_size) ? 1 : 0);
		    const qthread_shepherd_id_t max_shep =
			qthread_shepherd_count();
		    for (cluster = 0; cluster < cluster_count; cluster++) {
			void *clusterhead = qarray_elem_nomigrate(a,
								  cluster *
								  a->
								  cluster_size);
			qthread_incr(&chunk_distribution_tracker
				     [(((uintptr_t) clusterhead) >> pageshift)
				      % max_shep], -1);
		    }
		}
		    break;
		case ALL_SAME:
		    qthread_incr(&chunk_distribution_tracker[a->dist_shep],
				 -1 * (a->count / a->cluster_size +
				       ((a->count %
					 a->cluster_size) ? 1 : 0)));
		    break;
	    }
	    free(a->base_ptr);
	}
	free(a);
    }
}

static inline qthread_shepherd_id_t qarray_internal_shepof_ch(const qarray *
							      a,
							      const void
							      *cluster_head)
{
    switch (a->dist_type) {
	case ALL_SAME:
	    return a->dist_shep;
	case FIXED_HASH:
	default:
	    return (((uintptr_t) cluster_head) >> pageshift) %
		qthread_shepherd_count();
	case DIST:
	    return *qarray_internal_cluster_shep(a, cluster_head);
	    break;
    }
}

qthread_shepherd_id_t qarray_shepof(const qarray * a, const size_t index)
{
    assert(a);
    assert(index < a->count);
    switch (a->dist_type) {
	case ALL_SAME:
	    return a->dist_shep;
	case FIXED_HASH:
	case DIST:
	default:
	{
	    const size_t cluster_num = index / a->cluster_size;	/* rounded down */
	    const void *cluster_head =
		a->base_ptr + (cluster_num * a->cluster_bytes);
	    return qarray_internal_shepof_ch(a, cluster_head);
	}
    }
}

void *qarray_elem_nomigrate(const qarray * a, const size_t index)
{
    void *ret;
    qthread_shepherd_id_t dest;

    assert(a);
    if (index > a->count)
	return NULL;

    {
	const size_t cluster_num = index / a->cluster_size;	/* rounded down */

	return a->base_ptr + ((cluster_num * a->cluster_bytes) +
			      ((index -
				cluster_num * a->cluster_size) *
			       a->unit_size));
    }
}

void *qarray_elem(qthread_t * me, const qarray * a, const size_t index)
{
    void *ret;
    qthread_shepherd_id_t dest;

    assert(a);
    assert(me);
    if (index > a->count) {
	return NULL;
    } else {
	const size_t cluster_num = index / a->cluster_size;	/* rounded down */
	char *cluster_head = a->base_ptr + (cluster_num + a->cluster_bytes);

	ret =
	    cluster_head +
	    ((index - cluster_num * a->cluster_size) * a->unit_size);
	dest = qarray_internal_shepof_ch(a, cluster_head);
    }
    if (qthread_shep(me) != dest) {
	qthread_migrate_to(me, dest);
    }
    return ret;
}

struct qarray_func_wrapper_args
{
    union
    {
	qthread_f qt;
	qt_loop_f ql;
    } func;
    qarray *a;
    volatile aligned_t *donecount;
};

static aligned_t qarray_strider(qthread_t * me,
				const struct qarray_func_wrapper_args *arg)
{
    const size_t max_count = arg->a->count;
    const size_t cluster_size = arg->a->cluster_size;
    const distribution_t dist_type = arg->a->dist_type;
    size_t count = 0;
    qthread_shepherd_id_t shep = qthread_shep(me);

    if (dist_type == ALL_SAME && shep != arg->a->dist_shep) {
	goto qarray_strider_exit;
    }
    while (qarray_shepof(arg->a, count) != shep) {
	count += cluster_size;
	if (count > max_count) {
	    goto qarray_strider_exit;
	}
    }
    /* at this point...
     * 1. cursor points to the first element of the array associated with this CPU
     * 2. count is the index of that element
     */
    while (1) {
	size_t inpage_offset;
	const size_t max_offset =
	    ((max_count - count) >
	     cluster_size) ? cluster_size : (max_count - count);

	for (inpage_offset = 0; inpage_offset < max_offset; inpage_offset++) {
	    void *ptr = qarray_elem_nomigrate(arg->a, count + inpage_offset);

	    assert(ptr != NULL);
	    arg->func.qt(me, ptr);
	}
	switch (dist_type) {
	    case ALL_SAME:
		count += cluster_size;
		break;
	    case FIXED_HASH:
	    default:
		count += cluster_size * qthread_shepherd_count();
		break;
	    case DIST:		       /* XXX: this is awful - slow and bad for cache */
		count += cluster_size;
		if (count > max_count) {
		    goto qarray_strider_exit;
		}
		while (qarray_shepof(arg->a, count) != shep) {
		    count += cluster_size;
		    if (count > max_count) {
			goto qarray_strider_exit;
		    }
		}
		break;
	}
	if (count > max_count) {
	    goto qarray_strider_exit;
	}
    }
  qarray_strider_exit:
    qthread_incr(arg->donecount, 1);
    return 0;
}

static aligned_t qarray_loop_strider(qthread_t * me,
				     const struct qarray_func_wrapper_args
				     *arg)
{
    const size_t max_count = arg->a->count;
    const size_t cluster_size = arg->a->cluster_size;
    const distribution_t dist_type = arg->a->dist_type;
    size_t count = 0;
    qthread_shepherd_id_t shep = qthread_shep(me);

    if (dist_type == ALL_SAME && shep != arg->a->dist_shep) {
	goto qarray_loop_strider_exit;
    }
    while (qarray_shepof(arg->a, count) != shep) {
	count += cluster_size;
	if (count > max_count) {
	    goto qarray_loop_strider_exit;
	}
    }
    assert(count < max_count);
    /* at this point...
     * 1. cursor points to the first element of the array associated with this CPU
     * 2. count is the index of that element
     */
    while (1) {
	{
	    const size_t max_offset =
		((max_count - count) >
		 cluster_size) ? cluster_size : (max_count - count);
	    void *ptr = qarray_elem_nomigrate(arg->a, count);

	    assert(ptr != NULL);
	    arg->func.ql(me, 0, max_offset, ptr);
	}
	switch (dist_type) {
	    case ALL_SAME:
		count += cluster_size;
		break;
	    case FIXED_HASH:
	    default:
		count += cluster_size * qthread_shepherd_count();
		break;
	    case DIST:		       /* XXX: this is awful - slow and bad for cache */
		count += cluster_size;
		if (count > max_count) {
		    goto qarray_loop_strider_exit;
		}
		while (qarray_shepof(arg->a, count) != shep) {
		    count += cluster_size;
		    if (count > max_count) {
			goto qarray_loop_strider_exit;
		    }
		}
		break;
	}
	if (count > max_count) {
	    goto qarray_loop_strider_exit;
	}
    }
  qarray_loop_strider_exit:
    qthread_incr(arg->donecount, 1);
    return 0;
}

void qarray_iter(qthread_t * me, qarray * a, qthread_f func)
{
    qthread_shepherd_id_t i;
    volatile aligned_t donecount = 0;
    struct qarray_func_wrapper_args qfwa;

    qfwa.func.qt = func;
    qfwa.a = a;
    qfwa.donecount = &donecount;
    switch (a->dist_type) {
	case ALL_SAME:
	    qthread_fork_to((qthread_f) qarray_strider, &qfwa, NULL,
			    a->dist_shep);
	    while (donecount == 0) {
		qthread_yield(me);
	    }
	    break;
	default:
	    for (i = 0; i < qthread_shepherd_count(); i++) {
		qthread_fork_to((qthread_f) qarray_strider, &qfwa, NULL, i);
	    }
	    while (donecount < qthread_shepherd_count()) {
		qthread_yield(me);
	    }
	    break;
    }
}

void qarray_iter_loop(qthread_t * me, qarray * a, qt_loop_f func)
{
    qthread_shepherd_id_t i;
    volatile aligned_t donecount = 0;
    struct qarray_func_wrapper_args qfwa;

    qfwa.func.ql = func;
    qfwa.a = a;
    qfwa.donecount = &donecount;
    switch (a->dist_type) {
	case ALL_SAME:
	    qthread_fork_to((qthread_f) qarray_loop_strider, &qfwa, NULL,
			    a->dist_shep);
	    while (donecount == 0) {
		qthread_yield(me);
	    }
	    break;
	default:
	    for (i = 0; i < qthread_shepherd_count(); i++) {
		qthread_fork_to((qthread_f) qarray_loop_strider, &qfwa, NULL,
				i);
	    }
	    while (donecount < qthread_shepherd_count()) {
		qthread_yield(me);
	    }
	    break;
    }
}
