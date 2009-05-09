#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>		       /* for calloc() */
#include <unistd.h>		       /* for getpagesize() */
#include <qthread/qarray.h>
#include "qthread_asserts.h"
#ifdef QTHREAD_HAVE_LIBNUMA
# include "qthread_innards.h"	       /* for shep_to_node */
# include <numa.h>
# include <sys/mman.h>
#elif HAVE_SYS_LGRP_USER_H
# ifdef HAVE_MADV_ACCESS_LWP
#  include <sys/types.h>
#  include <sys/mman.h>
# endif
# include <sys/lgrp_user.h>
#endif

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

static QINLINE qthread_shepherd_id_t *qarray_internal_segment_shep(const
								   qarray * a,
								   const void
								   *segment_head)
{				       /*{{{ */
    char *ptr = (((char *)segment_head) + (a->segment_size * a->unit_size));

    /* ensure that it's 4-byte aligned
     * (mandatory on Sparc, good idea elsewhere) */
    if (((uintptr_t) ptr) & 3)
	ptr += 4 - (((uintptr_t) ptr) & 3);
    /* first, do we have the space? */
    assert(((ptr + sizeof(qthread_shepherd_id_t) - 1) -
	    (const char *)segment_head) < a->segment_bytes);
    return (qthread_shepherd_id_t *) ptr;
}				       /*}}} */

static inline qthread_shepherd_id_t qarray_internal_shepof_ch(const qarray *
							      a,
							      const void
							      *segment_head)
{				       /*{{{ */
    switch (a->dist_type) {
	case ALL_SAME:
	    return a->dist_shep;
	case FIXED_HASH:
	default:
	    return ((((uintptr_t) segment_head) -
		     ((uintptr_t) a->base_ptr)) / a->segment_bytes) %
		qthread_num_shepherds();
	case DIST:
	    return *qarray_internal_segment_shep(a, segment_head);
	    break;
    }
}				       /*}}} */

static inline qthread_shepherd_id_t qarray_internal_shepof_shi(const qarray *
							       a,
							       const size_t
							       shi)
{				       /*{{{ */
    switch (a->dist_type) {
	case ALL_SAME:
	    return a->dist_shep;
	case FIXED_HASH:
	    return (shi / a->segment_size) % qthread_num_shepherds();
	case DIST:
	    return *qarray_internal_segment_shep(a,
						 qarray_elem_nomigrate(a,
								       shi));
	default:
	    assert(0);
	    return 0;
    }
}				       /*}}} */

static qarray *qarray_create_internal(const size_t count,
				      const size_t obj_size,
				      const distribution_t d,
				      const char tight, const int seg_pages)
{				       /*{{{ */
    size_t pagesize;
    size_t segment_count;	/* number of segments allocated */
    qarray *ret = calloc(1, sizeof(qarray));

    assert(count > 0);
    assert(obj_size > 0);

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
	    calloc(qthread_num_shepherds(), sizeof(aligned_t));
    }

    ret->count = count;
    /* make obj_size a multiple of 8 */
    if (!tight) {
	ret->unit_size =
	    obj_size + ((obj_size & 7) ? (8 - (obj_size & 7)) : 0);
    } else {
	ret->unit_size = obj_size;
    }

    /* so, here's the idea: memory is assigned to shepherds in units I'm
     * choosing to call "segments" (chunk would also work, but that's overused
     * elsewhere in qthreads). Each segment can have its own shepherd. Which
     * shepherd a segment is assigned to is stored in the segment itself
     * (otherwise we'd have to use a hash table, and we'd lose all our cache
     * benefits). In SOME cases, such as FIXED_HASH and ALL_SAME, this is
     * unnecessary, so we can be a little more efficient with things by NOT
     * storing a shepherd ID in the segments. */
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
	    if (seg_pages == 0) {
		ret->segment_bytes = 16 * pagesize;
		if (ret->unit_size > ret->segment_bytes) {
		    ret->segment_bytes = qarray_lcm(ret->unit_size, pagesize);
		}
	    } else {
		ret->segment_bytes = seg_pages * pagesize;
	    }
	    ret->segment_size = ret->segment_bytes / ret->unit_size;
	    break;
	case DIST_REG_STRIPES:
	case DIST_REG_FIELDS:
	case DIST_RAND:
	case DIST_LEAST:
	case DIST:		       /* assumed equivalent to DIST_RAND */
	    /* since we will be storing a qthread_shepherd_id_t in each
	     * segment, we need to leave space in the segment for that data.
	     * The way we'll do this is that we'll just reduce the segment_size
	     * by 1 (thus providing space for the shepherd identifier, as long
	     * as the unit-size is bigger than a shepherd identifier). */
	    if (seg_pages == 0) {
		ret->segment_bytes = 16 * pagesize;
	    } else {
		ret->segment_bytes = seg_pages * pagesize;
	    }
	    ret->segment_size = ret->segment_bytes / ret->unit_size;
	    if ((ret->segment_bytes - (ret->segment_size * ret->unit_size)) <
		4) {
		ret->segment_size--;
		/* avoid wasting too much memory */
		if (ret->unit_size > pagesize) {
		    ret->segment_bytes -=
			(ret->unit_size / pagesize) * pagesize;
		    if (ret->unit_size % pagesize == 0) {
			ret->segment_bytes += pagesize;
		    }
		}
	    }
	    assert(ret->segment_size > 0);
	    assert(ret->segment_bytes > 0);
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

    segment_count =
	count / ret->segment_size + ((count % ret->segment_size) ? 1 : 0);

#ifdef QTHREAD_HAVE_LIBNUMA
    switch (d) {
	case ALL_LOCAL:
	case ALL_RAND:
	case ALL_LEAST:
	case ALL_SAME:
	default:
	    if (qthread_internal_shep_to_node(ret->dist_shep) ==
		QTHREAD_NO_NODE) {
	case DIST_REG_STRIPES:
	case DIST_REG_FIELDS:
	case DIST_RAND:
	case DIST_LEAST:
	case DIST:
	case FIXED_HASH:
		ret->base_ptr =
		    (char *)numa_alloc(segment_count * ret->segment_bytes);
		break;
	    } else {
		ret->base_ptr =
		    (char *)numa_alloc_onnode(segment_count *
					      ret->segment_bytes,
					      qthread_internal_shep_to_node
					      (ret->dist_shep));
	    }
	    break;
    }
#else
    /* For speed, we want page-aligned memory, if we can get it */
# ifdef HAVE_WORKING_VALLOC
    ret->base_ptr = (char *)valloc(segment_count * ret->segment_bytes);
# elif HAVE_MEMALIGN
    ret->base_ptr =
	(char *)memalign(pagesize, segment_count * ret->segment_bytes);
# elif HAVE_POSIX_MEMALIGN
    posix_memalign(&(ret->base_ptr), pagesize,
		   segment_count * ret->segment_bytes);
# elif HAVE_PAGE_ALIGNED_MALLOC
    ret->base_ptr = (char *)malloc(segment_count * ret->segment_bytes);
# else
    /* just don't free it */
    ret->base_ptr = (char *)valloc(segment_count * ret->segment_bytes);
# endif
#endif
    if (ret->base_ptr == NULL) {
	free(ret);
	ret = NULL;
    }

    /********************************************
     * Assign locations, maintain segment_count *
     ********************************************/
    switch (d) {
	case ALL_SAME:
	case ALL_LOCAL:
	    ret->dist_shep = qthread_shep(NULL);
	    qthread_incr(&chunk_distribution_tracker[ret->dist_shep],
			 segment_count);
	    break;
	case ALL_RAND:
	    ret->dist_shep = random() % qthread_num_shepherds();
	    qthread_incr(&chunk_distribution_tracker[ret->dist_shep],
			 segment_count);
	    break;
	case ALL_LEAST:
	{
	    qthread_shepherd_id_t i, least = 0;

	    for (i = 1; i < qthread_num_shepherds(); i++) {
		if (chunk_distribution_tracker[i] <
		    chunk_distribution_tracker[least]) {
		    least = i;
		}
	    }
	    ret->dist_shep = least;
	    qthread_incr(&chunk_distribution_tracker[least], segment_count);
	}
	    break;
	case FIXED_HASH:
	default:
	{
	    size_t segment;

	    for (segment = 0; segment < segment_count; segment++) {
#ifdef QTHREAD_HAVE_LIBNUMA
		if (qthread_internal_shep_to_node
		    (segment % qthread_num_shepherds()) !=
		    (unsigned int)(-1)) {
		    char *seghead = qarray_elem_nomigrate(ret,
							  segment *
							  ret->segment_size);
		    numa_tonode_memory(seghead, ret->segment_bytes,
				       qthread_internal_shep_to_node(segment %
								     qthread_num_shepherds
								     ()));
		}
#endif
		qthread_incr(&chunk_distribution_tracker
			     [qarray_internal_shepof_shi
			      (ret, segment * ret->segment_size)], 1);
	    }
	}
	    break;
	case DIST_REG_STRIPES:
	{
	    size_t segment;
	    const qthread_shepherd_id_t max_sheps = qthread_num_shepherds();

	    for (segment = 0; segment < segment_count; segment++) {
		char *seghead =
		    qarray_elem_nomigrate(ret, segment * ret->segment_size);
		qthread_shepherd_id_t *ptr =
		    qarray_internal_segment_shep(ret, seghead);
		*ptr = segment % max_sheps;
#ifdef QTHREAD_HAVE_LIBNUMA
		if (qthread_internal_shep_to_node(segment % max_sheps) !=
		    QTHREAD_NO_NODE) {
		    numa_tonode_memory(seghead, ret->segment_bytes,
				       qthread_internal_shep_to_node(segment %
								     max_sheps));
		}
#endif
		qthread_incr(&chunk_distribution_tracker[segment % max_sheps],
			     1);
	    }
	}
	    break;
	case DIST_REG_FIELDS:
	{
	    size_t segment;
	    const qthread_shepherd_id_t max_sheps = qthread_num_shepherds();
	    const size_t field_size = segment_count / max_sheps;
	    size_t field_count = 0;
	    qthread_shepherd_id_t cur_shep = 0;

	    for (segment = 0; segment < segment_count; segment++) {
		char *seghead =
		    qarray_elem_nomigrate(ret, segment * ret->segment_size);
		qthread_shepherd_id_t *ptr =
		    qarray_internal_segment_shep(ret, seghead);
		*ptr = cur_shep;
#ifdef QTHREAD_HAVE_LIBNUMA
		if (qthread_internal_shep_to_node(cur_shep) !=
		    QTHREAD_NO_NODE) {
		    numa_tonode_memory(seghead, ret->segment_bytes,
				       qthread_internal_shep_to_node
				       (cur_shep));
		}
#endif
		qthread_incr(&chunk_distribution_tracker[cur_shep], 1);
		field_count++;
		if (field_count == field_size) {
		    cur_shep++;
		    cur_shep *= (cur_shep != max_sheps);
		    field_count = 0;
		}
	    }
	}
	    break;
	case DIST:		       /* assumed equivalent to DIST_RAND */
	case DIST_RAND:
	{
	    size_t segment;
	    const qthread_shepherd_id_t max_sheps = qthread_num_shepherds();

	    for (segment = 0; segment < segment_count; segment++) {
		char *seghead =
		    qarray_elem_nomigrate(ret, segment * ret->segment_size);
		qthread_shepherd_id_t *ptr =
		    qarray_internal_segment_shep(ret, seghead);
		*ptr = random() % max_sheps;
#ifdef QTHREAD_HAVE_LIBNUMA
		if (qthread_internal_shep_to_node(*ptr) != QTHREAD_NO_NODE) {
		    numa_tonode_memory(seghead, ret->segment_bytes,
				       qthread_internal_shep_to_node(*ptr));
		}
#endif
		qthread_incr(&chunk_distribution_tracker[*ptr], 1);
	    }
	}
	    break;
	case DIST_LEAST:
	{
	    size_t segment;
	    const qthread_shepherd_id_t max_sheps = qthread_num_shepherds();

	    for (segment = 0; segment < segment_count; segment++) {
		qthread_shepherd_id_t i, least = 0;
		char *seghead =
		    qarray_elem_nomigrate(ret, segment * ret->segment_size);
		qthread_shepherd_id_t *ptr =
		    qarray_internal_segment_shep(ret, seghead);

		for (i = 1; i < max_sheps; i++) {
		    if (chunk_distribution_tracker[i] <
			chunk_distribution_tracker[least]) {
			least = i;
		    }
		}
		*ptr = least;
#ifdef QTHREAD_HAVE_LIBNUMA
		if (qthread_internal_shep_to_node(least) != QTHREAD_NO_NODE) {
		    numa_tonode_memory(seghead, ret->segment_bytes,
				       qthread_internal_shep_to_node(least));
		}
#endif
		qthread_incr(&chunk_distribution_tracker[least], 1);
	    }
	}
	    break;
    }
#if ( HAVE_MADVISE && HAVE_MADV_ACCESS_LWP )
    madvise(ret->base_ptr, segment_count * ret->segment_bytes,
	    MADV_ACCESS_LWP);
#endif
    return ret;
}				       /*}}} */

qarray *qarray_create(const size_t count, const size_t obj_size)
{
#if QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_32 || \
    QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64
    return qarray_create_internal(count, obj_size, DIST_REG_STRIPES, 0, 0);
#else
    return qarray_create_internal(count, obj_size, FIXED_HASH, 0, 0);
#endif
}

qarray *qarray_create_tight(const size_t count, const size_t obj_size)
{
#if QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_32 || \
    QTHREAD_ASSEMBLY_ARCH == QTHREAD_SPARCV9_64
    return qarray_create_internal(count, obj_size, DIST_REG_STRIPES, 1, 0);
#else
    return qarray_create_internal(count, obj_size, FIXED_HASH, 1, 1);
#endif
}

qarray *qarray_create_configured(const size_t count, const size_t obj_size,
				 const distribution_t d, const char tight,
				 const int seg_pages)
{
    return qarray_create_internal(count, obj_size, d, tight, seg_pages);
}

void qarray_destroy(qarray * a)
{				       /*{{{ */
    assert(a);
    assert(a->base_ptr);
    if (a) {
	if (a->base_ptr) {
	    switch (a->dist_type) {
		case DIST:
		{
		    size_t segment;
		    const size_t segment_count =
			a->count / a->segment_size +
			((a->count % a->segment_size) ? 1 : 0);
		    for (segment = 0; segment < segment_count; segment++) {
			char *segmenthead = qarray_elem_nomigrate(a,
								  segment *
								  a->
								  segment_size);
			qthread_incr(&chunk_distribution_tracker
				     [*qarray_internal_segment_shep
				      (a, segmenthead)], -1);
		    }
		}
		    break;
		default:
		case FIXED_HASH:
		{
		    size_t segment;
		    const size_t segment_count =
			a->count / a->segment_size +
			((a->count % a->segment_size) ? 1 : 0);
		    for (segment = 0; segment < segment_count; segment++) {
			qthread_incr(&chunk_distribution_tracker
				     [qarray_internal_shepof_shi
				      (a, segment * a->segment_size)], -1);
		    }
		}
		    break;
		case ALL_SAME:
		    qthread_incr(&chunk_distribution_tracker[a->dist_shep],
				 -1 * (a->count / a->segment_size +
				       ((a->count %
					 a->segment_size) ? 1 : 0)));
		    break;
	    }
#ifdef QTHREAD_HAVE_LIBNUMA
	    numa_free(a->base_ptr,
		      (a->count / a->segment_size +
		       ((a->count % a->segment_size) ? 1 : 0)));
#elif (HAVE_WORKING_VALLOC || HAVE_MEMALIGN || HAVE_POSIX_MEMALIGN || HAVE_PAGE_ALIGNED_MALLOC)
	    /* avoid freeing base ptr if we had to use a broken valloc */
	    free(a->base_ptr);
#endif
	}
	free(a);
    }
}				       /*}}} */

qthread_shepherd_id_t qarray_shepof(const qarray * a, const size_t index)
{				       /*{{{ */
    assert(a);
    assert(index < a->count);
    switch (a->dist_type) {
	case ALL_SAME:
	    return a->dist_shep;
	case FIXED_HASH:
	case DIST:
	default:
	{
	    const size_t segment_num = index / a->segment_size;	/* rounded down */

	    return qarray_internal_shepof_shi(a,
					      segment_num * a->segment_size);
	}
    }
}				       /*}}} */

void *qarray_elem(qthread_t * me, const qarray * a, const size_t index)
{				       /*{{{ */
    void *ret;
    qthread_shepherd_id_t dest;

    assert(a);
    assert(me);
    if (index >= a->count) {
	return NULL;
    } else {
	const size_t segment_num = index / a->segment_size;	/* rounded down */
	char *segment_head = a->base_ptr + (segment_num + a->segment_bytes);

	ret =
	    segment_head +
	    ((index - segment_num * a->segment_size) * a->unit_size);
	dest = qarray_internal_shepof_ch(a, segment_head);
    }
    if (qthread_shep(me) != dest) {
	qthread_migrate_to(me, dest);
    }
    return ret;
}				       /*}}} */

struct qarray_func_wrapper_args
{
    union
    {
	qa_loop_f ql;
	qthread_f qt;
    } func;
    qarray *a;
    void *arg;
    volatile aligned_t *donecount;
    const size_t startat, stopat;
};
struct qarray_constfunc_wrapper_args
{
    const union
    {
	qa_cloop_f ql;
	qthread_f qt;
    } func;
    const qarray *a;
    void *arg;
    volatile aligned_t *donecount;
    const size_t startat, stopat;
};

static aligned_t qarray_strider(qthread_t * me,
				const struct qarray_func_wrapper_args *arg)
{				       /*{{{ */
    const size_t max_count = arg->stopat;
    const size_t segment_size = arg->a->segment_size;
    const distribution_t dist_type = arg->a->dist_type;
    size_t count = arg->startat;
    qthread_shepherd_id_t shep = qthread_shep(me);

    if (dist_type == ALL_SAME && shep != arg->a->dist_shep) {
	goto qarray_strider_exit;
    }
    if (count > 0 && qarray_shepof(arg->a, count) != shep) {
	/* jump to the next segment boundary */
	count += segment_size - (count % segment_size);
	if (count >= max_count) {
	    goto qarray_strider_exit;
	}
    }
    while (qarray_shepof(arg->a, count) != shep) {
	count += segment_size;
	if (count >= max_count) {
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
	     segment_size) ? segment_size : (max_count - count);

	for (inpage_offset = 0; inpage_offset < max_offset; inpage_offset++) {
	    void *ptr = qarray_elem_nomigrate(arg->a, count + inpage_offset);

	    assert(ptr != NULL);
	    arg->func.qt(me, ptr);
	}
	switch (dist_type) {
	    case ALL_SAME:
		count += segment_size;
		break;
	    case FIXED_HASH:
	    default:
		count += segment_size * qthread_num_shepherds();
		break;
	    case DIST:		       /* XXX: this is awful - slow and bad for cache */
		count += segment_size;
		if (count >= max_count) {
		    goto qarray_strider_exit;
		}
		while (qarray_shepof(arg->a, count) != shep) {
		    count += segment_size;
		    if (count >= max_count) {
			goto qarray_strider_exit;
		    }
		}
		break;
	}
	if (count >= max_count) {
	    goto qarray_strider_exit;
	}
    }
  qarray_strider_exit:
    qthread_incr(arg->donecount, 1);
    return 0;
}				       /*}}} */

static aligned_t qarray_loop_strider(qthread_t * me,
				     const struct qarray_func_wrapper_args
				     *arg)
{				       /*{{{ */
    const size_t max_count = arg->stopat;
    const size_t segment_size = arg->a->segment_size;
    const distribution_t dist_type = arg->a->dist_type;
    size_t count = arg->startat;
    qthread_shepherd_id_t shep = qthread_shep(me);
    const qa_loop_f ql = arg->func.ql;

    if (dist_type == ALL_SAME && shep != arg->a->dist_shep) {
	goto qarray_loop_strider_exit;
    }
    if (count > 0 && qarray_shepof(arg->a, count) != shep) {
	/* jump to the next segment boundary */
	count += segment_size - (count % segment_size);
	if (count >= max_count) {
	    goto qarray_loop_strider_exit;
	}
    }
    while (qarray_shepof(arg->a, count) != shep) {
	count += segment_size;
	if (count >= max_count) {
	    goto qarray_loop_strider_exit;
	}
    }
    assert(count < max_count);
    /* at this point...
     * 1. cursor points to the first element of the array associated with this CPU
     * 2. count is the index of that element
     */
    if (dist_type == ALL_SAME) {
	ql(me, count, max_count, arg->a, arg->arg);
	goto qarray_loop_strider_exit;
    }
    while (1) {
	{
	    const size_t max_offset =
		((max_count - count) >
		 segment_size) ? segment_size : (max_count - count);
	    /*void *ptr = qarray_elem_nomigrate(arg->a, count);
	     * 
	     * assert(ptr != NULL); */
	    ql(me, count, count + max_offset, arg->a, arg->arg);
	}
	switch (dist_type) {
	    default:
		assert(0);
		break;
	    case ALL_SAME:
		count += segment_size;
		break;
	    case FIXED_HASH:
		count += segment_size * qthread_num_shepherds();
		break;
	    case DIST:		       /* XXX: this is awful - slow and bad for cache */
		count += segment_size;
		if (count >= max_count) {
		    goto qarray_loop_strider_exit;
		}
		while (qarray_shepof(arg->a, count) != shep) {
		    count += segment_size;
		    if (count >= max_count) {
			goto qarray_loop_strider_exit;
		    }
		}
		break;
	}
	if (count >= max_count) {
	    goto qarray_loop_strider_exit;
	}
    }
  qarray_loop_strider_exit:
    qthread_incr(arg->donecount, 1);
    return 0;
}				       /*}}} */

void qarray_iter(qthread_t * me, qarray * a, const size_t startat,
		 const size_t stopat, qthread_f func)
{				       /*{{{ */
    volatile aligned_t donecount = 0;
    struct qarray_func_wrapper_args qfwa =
	{ {NULL}, a, NULL, &donecount, startat, stopat };

    qfwa.func.qt = func;
    switch (a->dist_type) {
	case ALL_SAME:
	    qthread_fork_to((qthread_f) qarray_strider, &qfwa, NULL,
			    a->dist_shep);
	    while (donecount == 0) {
		qthread_yield(me);
	    }
	    break;
	default:
	    /* it'd be NICE to avoid spawning if not all sheps are represented
	     * in the range (esp if the range is small), but because of
	     * DIST_RAND, that's impossible to do without checking all the
	     * ranges ahead of time. By spawning threads that check their own
	     * ranges, we essentially parallelize the task of figuring out
	     * which threads to spawn (bizarre way of thinking about it, I
	     * know). */
	    if (stopat - startat < a->segment_size) {
		qthread_fork_to((qthread_f) qarray_loop_strider, &qfwa, NULL,
				qarray_shepof(a, startat));
		while (donecount == 0) {
		    qthread_yield(me);
		}
	    } else {
		qthread_shepherd_id_t i;

		for (i = 0; i < qthread_num_shepherds(); i++) {
		    qthread_fork_to((qthread_f) qarray_strider, &qfwa, NULL,
				    i);
		}
		while (donecount < qthread_num_shepherds()) {
		    qthread_yield(me);
		}
	    }
	    break;
    }
}				       /*}}} */

void qarray_iter_loop(qthread_t * me, qarray * a, const size_t startat,
		      const size_t stopat, qa_loop_f func, void *arg)
{				       /*{{{ */
    volatile aligned_t donecount = 0;
    struct qarray_func_wrapper_args qfwa =
	{ {func}, a, arg, &donecount, startat, stopat };

    switch (a->dist_type) {
	case ALL_SAME:
	    qthread_fork_to((qthread_f) qarray_loop_strider, &qfwa, NULL,
			    a->dist_shep);
	    while (donecount == 0) {
		qthread_yield(me);
	    }
	    break;
	default:
	    /* it'd be NICE to avoid spawning if not all sheps are represented
	     * in the range (esp if the range is small), but because of
	     * DIST_RAND, that's impossible to do without checking all the
	     * ranges ahead of time. By spawning threads that check their own
	     * ranges, we essentially parallelize the task of figuring out
	     * which threads to spawn (bizarre way of thinking about it, I
	     * know). */
	    if (stopat - startat < a->segment_size) {
		qthread_fork_to((qthread_f) qarray_loop_strider, &qfwa, NULL,
				qarray_shepof(a, startat));
		while (donecount == 0) {
		    qthread_yield(me);
		}
	    } else {
		qthread_shepherd_id_t i;

		for (i = 0; i < qthread_num_shepherds(); i++) {
		    qthread_fork_to((qthread_f) qarray_loop_strider, &qfwa,
				    NULL, i);
		}
		while (donecount < qthread_num_shepherds()) {
		    qthread_yield(me);
		}
	    }
	    break;
    }
}				       /*}}} */

void qarray_iter_constloop(qthread_t * me, const qarray * a,
			   const size_t startat, const size_t stopat,
			   qa_cloop_f func, void *arg)
{				       /*{{{ */
    volatile aligned_t donecount = 0;
    const struct qarray_constfunc_wrapper_args qfwa =
	{ {func}, a, arg, &donecount, startat, stopat };

    switch (a->dist_type) {
	case ALL_SAME:
	    qthread_fork_to((qthread_f) qarray_loop_strider, &qfwa, NULL,
			    a->dist_shep);
	    while (donecount == 0) {
		qthread_yield(me);
	    }
	    break;
	default:
	    /* it'd be NICE to avoid spawning if not all sheps are represented
	     * in the range (esp if the range is small), but because of
	     * DIST_RAND, that's impossible to do without checking all the
	     * ranges ahead of time. By spawning threads that check their own
	     * ranges, we essentially parallelize the task of figuring out
	     * which threads to spawn (bizarre way of thinking about it, I
	     * know). */
	    if (stopat - startat < a->segment_size) {
		qthread_fork_to((qthread_f) qarray_loop_strider, &qfwa, NULL,
				qarray_shepof(a, startat));
		while (donecount == 0) {
		    qthread_yield(me);
		}
	    } else {
		qthread_shepherd_id_t i;

		for (i = 0; i < qthread_num_shepherds(); i++) {
		    qthread_fork_to((qthread_f) qarray_loop_strider, &qfwa,
				    NULL, i);
		}
		while (donecount < qthread_num_shepherds()) {
		    qthread_yield(me);
		}
	    }
	    break;
    }
}				       /*}}} */
