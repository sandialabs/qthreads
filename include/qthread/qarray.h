#ifndef QTHREAD_QARRAY_H
#define QTHREAD_QARRAY_H

#include <qthread/qthread-int.h>
#include <qthread/qthread.h>
#include <qthread/qloop.h>

Q_STARTCXX /* */

typedef enum
{
    /* the default, used both as input and after creation */
    FIXED_HASH = 0,
    /* these are used only after it's created */
    ALL_SAME, DIST,
    /* types of DIST... only used for input to qarray_create() */
    DIST_REG_STRIPES, DIST_REG_FIELDS, DIST_RAND, DIST_LEAST,
    /* types of ALL_SAME... only used for input to qarray_create() */
    ALL_LOCAL, ALL_RAND, ALL_LEAST
} distribution_t;
typedef struct qarray_s
{
    size_t unit_size;
    size_t count;
    size_t segment_size;	/* units in a segment */
    size_t segment_bytes;	/* bytes per segment (sometimes > unit_size*segment_count) */
    char *base_ptr;
    distribution_t dist_type;
    qthread_shepherd_id_t dist_shep;	/* for ALL_SAME dist type */
} qarray;

typedef void (*qa_loop_f) (qthread_t * me, const size_t startat,
			   const size_t stopat, qarray * array,
			   void * arg);
typedef void (*qa_cloop_f) (qthread_t * me, const size_t startat,
			    const size_t stopat, const qarray * array,
			    void * arg);

qarray *qarray_create(const size_t count, const size_t unit_size);
qarray *qarray_create_tight(const size_t count, const size_t unit_size);
qarray *qarray_create_configured(const size_t count, const size_t unit_size,
				 const distribution_t d, const char tight,
				 const int seg_pages);

void qarray_destroy(qarray * a);

void qarray_iter(qthread_t * me, qarray * a, const size_t startat,
		 const size_t stopat, qthread_f func);
void qarray_iter_loop(qthread_t * me, qarray * a, const size_t startat,
		      const size_t stopat, qa_loop_f func,
		      void * arg);
void qarray_iter_constloop(qthread_t * me, const qarray * a,
			   const size_t startat, const size_t stopat,
			   qa_cloop_f func, void * arg);

qthread_shepherd_id_t qarray_shepof(const qarray * a, const size_t index);

void *qarray_elem(qthread_t * me, const qarray * a, const size_t index);
QINLINE static void *qarray_elem_nomigrate(const qarray * a,
					   const size_t index)
{
    if (a == NULL || index > a->count)
	return NULL;

    {
	const size_t segment_num = index / a->segment_size;	/* rounded down */

	return a->base_ptr + ((segment_num * a->segment_bytes) +
			      ((index -
				segment_num * a->segment_size) *
			       a->unit_size));
    }
}

Q_ENDCXX
#endif
