#ifndef FUTURES_H
#define FUTURES_H

#include <qthread/qthread.h>

#ifdef __cplusplus
extern "C"
{
#endif
typedef qthread_t future_t;

future_t *future_create(qthread_t * qthr, void (*fptr) (qthread_t *, void *),
			void *arg);

void future_join_all(qthread_t * qthr, future_t ** fta, int ftc);

void future_init(qthread_t * qthr, int vp_per_loc, int loc_count);

void future_exit(qthread_t * qthr);

int future_yeild(qthread_t * qthr);
void future_acquire(qthread_t * qthr);

#ifdef __cplusplus
}
#include <qthread/loop_templates.hpp>
#endif

#endif
