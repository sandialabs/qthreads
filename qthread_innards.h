#ifndef QTHREAD_INNARDS_H
#define QTHREAD_INNARDS_H

/* These are the internal functions that users should be allowed to get at */
unsigned int qthread_isfuture(const qthread_t * t);
void qthread_assertfuture(qthread_t * t);
void qthread_assertnotfuture(qthread_t * t);
void qthread_fork_future_to(const qthread_f f, const void *arg,
			    aligned_t * ret,
			    const qthread_shepherd_id_t shepherd);

#endif
