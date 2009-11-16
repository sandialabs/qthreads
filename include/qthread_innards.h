#ifndef QTHREAD_INNARDS_H
#define QTHREAD_INNARDS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qthread_asserts.h"

#if defined(HAVE_UCONTEXT_H) && defined(HAVE_NATIVE_MAKECONTEXT)
# include <ucontext.h>		       /* for ucontext_t */
#else
# include "osx_compat/taskimpl.h"
#endif

#ifdef QTHREAD_DEBUG
# ifdef HAVE_UNISTD_H
#  include <unistd.h>		       /* for write() */
# endif
# include <stdarg.h>		       /* for va_start and va_end */
#endif
//#include <cprops/hashtable.h>
#include <pthread.h>
#include <qt_hash.h>

#ifdef __SUN__
# define STRIPECOUNT 128
#else
# define STRIPECOUNT 32
#endif

typedef struct qlib_s
{
    unsigned int nshepherds;
    struct qthread_shepherd_s *shepherds;

    unsigned qthread_stack_size;
    unsigned master_stack_size;
    unsigned max_stack_size;

    qthread_t *mccoy_thread;	/* free when exiting */

    void *master_stack;
    ucontext_t *master_context;
#ifdef QTHREAD_USE_VALGRIND
    unsigned int valgrind_masterstack_id;
#endif

    /* assigns a unique thread_id mostly for debugging! */
    aligned_t max_thread_id;
    pthread_mutex_t max_thread_id_lock;

    /* round robin scheduler - can probably be smarter */
    aligned_t sched_shepherd;
    pthread_mutex_t sched_shepherd_lock;

    /* this is how we manage FEB-type locks
     * NOTE: this can be a major bottleneck and we should probably create
     * multiple hashtables to improve performance. The current hashing is a bit
     * of a hack, but improves the bottleneck a bit
     */
    qt_hash locks[STRIPECOUNT];
#ifdef QTHREAD_COUNT_THREADS
    aligned_t locks_stripes[STRIPECOUNT];
    pthread_mutex_t locks_stripes_locks[STRIPECOUNT];
#endif
    /* these are separated out for memory reasons: if you can get away with
     * simple locks, then you can use a little less memory. Subject to the same
     * bottleneck concerns as the above hashtable, though these are slightly
     * better at shrinking their critical section. FEBs have more memory
     * overhead, though. */
    qt_hash FEBs[STRIPECOUNT];
#ifdef QTHREAD_COUNT_THREADS
    aligned_t febs_stripes[STRIPECOUNT];
    pthread_mutex_t febs_stripes_locks[STRIPECOUNT];
#endif
}     *qlib_t;

#ifndef SST
extern qlib_t qlib;
#endif

/* These are the internal functions that futurelib should be allowed to get at */
unsigned int qthread_isfuture(const qthread_t * t);

void qthread_assertfuture(qthread_t * t);

void qthread_assertnotfuture(qthread_t * t);

int qthread_fork_future_to(const qthread_t * me, const qthread_f f,
			   const void *arg, aligned_t * ret,
			   const qthread_shepherd_id_t shepherd);
unsigned int qthread_internal_shep_to_node(const qthread_shepherd_id_t shep);

#define QTHREAD_NO_NODE ((unsigned int)(-1))
#ifdef SST
# define qthread_shepherd_count() PIM_readSpecial(PIM_CMD_LOC_COUNT)
#else
# define qthread_shepherd_count() (qlib->nshepherds)
#endif

/*#define QTHREAD_DEBUG 1*/
/* for debugging */
#ifdef QTHREAD_DEBUG
enum qthread_debug_levels
{ NONE = 0,
    THREAD_BEHAVIOR, LOCK_BEHAVIOR, ALL_CALLS, ALL_FUNCTIONS,
    THREAD_DETAILS, LOCK_DETAILS, ALL_DETAILS
};

extern enum qthread_debug_levels debuglevel;

extern pthread_mutex_t output_lock;

static QINLINE void qthread_debug(int level, char *format, ...)
{				       /*{{{ */
    va_list args;

    if (level <= debuglevel) {
	static char buf[1024];	// protected by the output_lock

	char *head = buf;

	char ch;

	qassert(pthread_mutex_lock(&output_lock), 0);

	while (write(2, "QDEBUG: ", 8) != 8) ;

	va_start(args, format);
	/* avoiding the obvious method, to save on memory
	 * vfprintf(stderr, format, args); */
	while ((ch = *format++)) {
	    if (ch == '%') {
		ch = *format++;
		switch (ch) {
		    case 's':
		    {
			char *str = va_arg(args, char *);

			while (write(2, buf, head - buf) != head - buf) ;
			head = buf;
			while (write(2, str, strlen(str)) != strlen(str)) ;
			break;
		    }
		    case 'p':
		    case 'x':
		    {
			uintptr_t num = 0;

			unsigned base = 0;

			*head++ = '0';
			*head++ = 'x';
		    case 'u':
		    case 'd':
		    case 'i':
			num = va_arg(args, uintptr_t);
			base = (ch == 'p') ? 16 : 10;
			if (!num) {
			    *head++ = '0';
			} else {
			    /* count places */
			    uintptr_t tmp = num;

			    unsigned places = 0;

			    while (tmp >= base) {
				tmp /= base;
				places++;
			    }
			    head += places;
			    places = 0;
			    while (num >= base) {
				tmp = num % base;
				*(head - places) =
				    (tmp <
				     10) ? ('0' + tmp) : ('a' + tmp - 10);
				num /= base;
				places++;
			    }
			    num %= base;
			    *(head - places) =
				(num < 10) ? ('0' + num) : ('a' + num - 10);
			    head++;
			}
		    }
			break;
		    default:
			*head++ = '%';
			*head++ = ch;
		}
	    } else {
		*head++ = ch;
	    }
	}
	/* XXX: not checking for extra long values of head */
	while (write(2, buf, head - buf) != head - buf) ;
	va_end(args);
	/*fflush(stderr); */

	qassert(pthread_mutex_unlock(&output_lock), 0);
    }
}				       /*}}} */
#else
#define qthread_debug(...) do{ }while(0)
#endif

#endif
