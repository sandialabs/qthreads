#ifndef FUTURES_H
#define FUTURES_H

#include <qthread/qthread.h>

Q_STARTCXX			       /* */
void future_fork(qthread_f func,
		 void *arg,
		 aligned_t * retval);
void future_fork_to(qthread_f fptr,
		    void *arg,
		    aligned_t * retval,
		    qthread_shepherd_id_t shep);
void future_fork_syncvar_to(qthread_f fptr,
			    void *arg,
			    syncvar_t * retval,
			    qthread_shepherd_id_t shep);

void future_join_all(
    qthread_t * me,
    aligned_t * fta,
    int ftc);

void future_init(int vp_per_loc);

void future_exit(qthread_t * me);

int future_yield(qthread_t * me);
void future_acquire(qthread_t * me);

Q_ENDCXX			       /* */
#ifdef __cplusplus
#include <qthread/loop_templates.hpp>
#endif
#endif
