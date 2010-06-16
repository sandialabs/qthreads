#ifndef QTHREAD_INNARDS_H
#define QTHREAD_INNARDS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>

#ifdef QTHREAD_HAVE_HWLOC
# include <hwloc.h>
# if (HWLOC_API_VERSION < 0x00010000)
#  error HWLOC version unrecognized
# endif
#endif

#include "qthread_asserts.h"
#include "qt_atomics.h"

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
#include <pthread.h>
#include <qt_hash.h>

extern unsigned int QTHREAD_LOCKING_STRIPES;

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
    QTHREAD_FASTLOCK_TYPE max_thread_id_lock;

    /* round robin scheduler - can probably be smarter */
    aligned_t sched_shepherd;
    QTHREAD_FASTLOCK_TYPE sched_shepherd_lock;

#ifdef QTHREAD_HAVE_HWLOC
    hwloc_topology_t topology;
#endif

#if defined(QTHREAD_MUTEX_INCREMENT) || (QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC32)
    QTHREAD_FASTLOCK_TYPE *atomic_locks;
# ifdef QTHREAD_COUNT_THREADS
    aligned_t *atomic_stripes;
    QTHREAD_FASTLOCK_TYPE *atomic_stripes_locks;
# endif
#endif
    /* this is how we manage FEB-type locks
     * NOTE: this can be a major bottleneck and we should probably create
     * multiple hashtables to improve performance. The current hashing is a bit
     * of a hack, but improves the bottleneck a bit
     */
    qt_hash *locks;
#ifdef QTHREAD_COUNT_THREADS
    aligned_t *locks_stripes;
# ifdef QTHREAD_MUTEX_INCREMENT
    QTHREAD_FASTLOCK_TYPE *locks_stripes_locks;
# endif
#endif
    /* these are separated out for memory reasons: if you can get away with
     * simple locks, then you can use a little less memory. Subject to the same
     * bottleneck concerns as the above hashtable, though these are slightly
     * better at shrinking their critical section. FEBs have more memory
     * overhead, though. */
    qt_hash *FEBs;
#ifdef QTHREAD_COUNT_THREADS
    aligned_t *febs_stripes;
# ifdef QTHREAD_MUTEX_INCREMENT
    QTHREAD_FASTLOCK_TYPE *febs_stripes_locks;
# endif
#endif
    /* this is for holding syncvar waiters... it's not striped because there
     * isn't supposed to be much contention for this hash table */
    qt_hash syncvars;
}     *qlib_t;

#ifndef QTHREAD_SST_PRIMITIVES
extern qlib_t qlib;
#endif

/* These are the internal functions that futurelib should be allowed to get at */
unsigned int qthread_isfuture(const qthread_t * t);

void qthread_assertfuture(qthread_t * t);

void qthread_assertnotfuture(qthread_t * t);

int qthread_fork_future_to(const qthread_t * me, const qthread_f f,
			   const void *arg, aligned_t * ret,
			   const qthread_shepherd_id_t shepherd);
int qthread_fork_syncvar_future_to(const qthread_t * me, const qthread_f f,
			   const void *arg, syncvar_t * ret,
			   const qthread_shepherd_id_t shepherd);
unsigned int qthread_internal_shep_to_node(const qthread_shepherd_id_t shep);

#define QTHREAD_NO_NODE ((unsigned int)(-1))
#ifdef QTHREAD_SST_PRIMITIVES
# define qthread_shepherd_count() PIM_readSpecial(PIM_CMD_LOC_COUNT)
#else
# define qthread_shepherd_count() (qlib->nshepherds)
#endif

/* internal initialization functions */
void qt_feb_barrier_internal_init(void);
void qthread_internal_cleanup(void (*function)(void));

/* for debugging */
#ifdef QTHREAD_DEBUG
enum qthread_debug_levels
{ NONE = 0,
    THREAD_BEHAVIOR, LOCK_BEHAVIOR, ALL_CALLS, ALL_FUNCTIONS,
    THREAD_DETAILS, LOCK_DETAILS, ALL_DETAILS
};

extern enum qthread_debug_levels debuglevel;

extern QTHREAD_FASTLOCK_TYPE output_lock;

#ifdef HAVE_GNU_VAMACROS
#define qthread_debug(level, format, args...) qthread_debug_(level, "%s(%u): " format, __FUNCTION__, __LINE__, ##args)
static QINLINE void qthread_debug_(int level, char *format, ...)
#elif defined( HAVE_C99_VAMACROS )
#define qthread_debug(level, format, ...) qthread_debug_(level, "%s(%u): " format, __FUNCTION__, __LINE__, __VA_ARGS__)
static QINLINE void qthread_debug_(int level, char *format, ...)
#else
static QINLINE void qthread_debug(int level, char *format, ...)
#endif
{				       /*{{{ */
    va_list args;

    if (level <= debuglevel || level == 0) {
	static char buf[1024];	/* protected by the output_lock */
	char *head = buf;
	char ch;

	QTHREAD_FASTLOCK_LOCK(&output_lock);

	while (write(2, "QDEBUG: ", 8) != 8) ;

	va_start(args, format);
	/* avoiding the obvious method, to save on memory
	 * vfprintf(stderr, format, args); */
	while ((ch = *format++)) {
	    assert(head < (buf + 1024));
	    if (ch == '%') {
		ch = *format++;
		switch (ch) {
		    case 's':
		    {
			char *str = va_arg(args, char *);

			qassert(write(2, buf, head - buf), head - buf);
			head = buf;
			qassert(write(2, str, strlen(str)), strlen(str));
			break;
		    }
		    case 'p':
		    case 'x':
			*head++ = '0';
			*head++ = 'x';
		    case 'u':
		    case 'd':
		    case 'i':
		    {
			uintptr_t num;
			unsigned base;

			num = va_arg(args, uintptr_t);
			base = (ch == 'p' || ch == 'x') ? 16 : 10;
			if (!num) {
			    *head++ = '0';
			} else {
			    /* count places */
			    unsigned places = 0;
			    uintptr_t tmpnum = num;

			    /* yes, this is dumb, but its guaranteed to take
			     * less than 10 iterations on 32-bit numbers and
			     * doesn't involve floating point */
			    while (tmpnum >= base) {
				tmpnum /= base;
				places ++;
			    }
			    head += places;
			    places = 0;
			    while (num >= base) {
				uintptr_t tmp = num % base;
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
	qassert(write(2, buf, head - buf), head - buf);
	va_end(args);
	/*fflush(stderr); */

	QTHREAD_FASTLOCK_UNLOCK(&output_lock);
    }
}				       /*}}} */
#else
#define qthread_debug(...) do{ }while(0)
#endif

#endif
