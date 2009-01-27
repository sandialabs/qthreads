#ifndef QTHREAD_QARRAY_H
#define QTHREAD_QARRAY_H

#include <qthread/qthread-int.h>
#include <qthread/qthread.h>
#include <qthread/qloop.h>

typedef struct qarray_s qarray;
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

qarray *qarray_create(const size_t count, const size_t unit_size,
		      const distribution_t d);
qthread_shepherd_id_t qarray_shepof(const qarray * a, const size_t index);
void *qarray_elem_nomigrate(const qarray * a, const size_t index);
void *qarray_elem(qthread_t * me, const qarray * a, const size_t index);
void qarray_iter(qthread_t * me, qarray * a, qthread_f func);
void qarray_iter_loop(qthread_t * me, qarray * a, qt_loop_f func);
void qarray_free(qarray * a);

#endif
