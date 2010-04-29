#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <limits.h>		       /* for INT_MAX */
#include <qthread/qthread.h>
#include <qthread/futurelib.h>

/* prototypes */
void qthread_wrapper(const qthread_f f, const void *arg, aligned_t * ret);
void qthread_future_wrapper(const qthread_f f, const void *arg,
			    aligned_t * ret);
int qthread_fork_future_to(const qthread_f f, const void *arg,
			   aligned_t * ret, qthread_shepherd_id_t shep);
void qthread_assertnotfuture(qthread_t * t);
unsigned int qthread_isfuture(const qthread_t * t);

void qthread_wrapper(const qthread_f f, const void *arg, aligned_t * ret)
{
    if (ret == NULL) {
	f(qthread_self(), (void *)arg);
    } else {
	aligned_t retval = f(qthread_self(), (void *)arg);

	qthread_writeEF_const(qthread_self(), ret, retval);
    }
}

void qthread_future_wrapper(const qthread_f f, const void *arg,
			    aligned_t * ret)
{
    PIM_writeSpecial(PIM_CMD_SET_FUTURE, 1);
    if (ret == NULL) {
	f(qthread_self(), (void *)arg);
    } else {
	qthread_writeEF_const(qthread_self(), ret,
			      f(qthread_self(), (void *)arg));
    }
    future_exit(qthread_self());
    PIM_threadExitFree();	       // Why here and not in qthread_wrapper() ?
}

int qthread_fork_to(const qthread_f f, const void *arg, aligned_t * ret,
		    qthread_shepherd_id_t shep)
{
    if (ret) {
	PIM_feb_empty(ret);
    }
    PIM_loadAndSpawnToLocaleStack(shep == NO_SHEPHERD ? -1 : shep,
					 (void *)qthread_wrapper, (void *)f,
					 (void *)arg, ret, NULL, NULL);
    return 0;
}

int qthread_fork_future_to(const qthread_f f, const void *arg,
			   aligned_t * ret, qthread_shepherd_id_t shep)
{
    if (ret) {
	PIM_feb_empty(ret);
    }
    return PIM_loadAndSpawnToLocaleStack(shep == NO_SHEPHERD ? -1 : shep,
					(void *)qthread_future_wrapper,
					(void *)f, (void *)arg, ret, NULL,
					NULL);
}

qthread_t *qthread_prepare_for(const qthread_f f, const void *arg,
			       aligned_t * ret,
			       const qthread_shepherd_id_t shep)
{
    if (ret)
	PIM_feb_empty(ret);
    /*printf
     * ("2shep: %i, qthread_wrapper: %p, arg1(f): %p, arg2(arg): %p, arg3(ret): %p\n",
     * (shep == NO_SHEPHERD ? INT_MAX : shep), qthread_wrapper, f, arg,
     * ret); fflush(NULL); */
    return (qthread_t *) PIM_loadAndSpawnToLocaleStackStopped(shep ==
							      NO_SHEPHERD ?
							      INT_MAX : shep,
							      (void *)
							      qthread_wrapper,
							      (void *)f,
							      (void *)arg,
							      ret, NULL,
							      NULL);
}

void qthread_assertnotfuture(qthread_t * t)
{
/* XXX: this does NOTHING! This is a bug! */
}

unsigned int qthread_isfuture(const qthread_t * t)
{
    /* XXX: this does NOTHING! This is a bug! */
    return 0;
}
