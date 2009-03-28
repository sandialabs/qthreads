#ifndef QTHREAD_INNARDS_H
#define QTHREAD_INNARDS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qthread_asserts.h"

#ifdef QTHREAD_DEBUG
# ifdef HAVE_UNISTD_H
#  include <unistd.h> /* for write() */
# endif
# include <stdarg.h>		       /* for va_start and va_end */
#endif
#include <cprops/hashtable.h>

typedef struct qlib_s
{
    unsigned int nshepherds;
    struct qthread_shepherd_s *shepherds;

    unsigned qthread_stack_size;
    unsigned master_stack_size;
    unsigned max_stack_size;

    void* shep0_stack; /* free the shep0 stack when exiting */

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
    cp_hashtable *locks[32];
#ifdef QTHREAD_COUNT_THREADS
    aligned_t locks_stripes[32];
    pthread_mutex_t locks_stripes_locks[32];
#endif
    /* these are separated out for memory reasons: if you can get away with
     * simple locks, then you can use a little less memory. Subject to the same
     * bottleneck concerns as the above hashtable, though these are slightly
     * better at shrinking their critical section. FEBs have more memory
     * overhead, though. */
    cp_hashtable *FEBs[32];
#ifdef QTHREAD_COUNT_THREADS
    aligned_t febs_stripes[32];
    pthread_mutex_t febs_stripes_locks[32];
#endif
} *qlib_t;

#ifndef SST
extern qlib_t qlib;
#endif

/* These are the internal functions that futurelib should be allowed to get at */
unsigned int qthread_isfuture(const qthread_t * t);
void qthread_assertfuture(qthread_t * t);
void qthread_assertnotfuture(qthread_t * t);
int qthread_fork_future_to(const qthread_t *me, const qthread_f f, const void *arg,
			    aligned_t * ret,
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
extern int debuglevel;
extern pthread_mutex_t output_lock;

static QINLINE void qthread_debug(int level, char *format, ...)
{				       /*{{{ */
    va_list args;

    if (level <= debuglevel) {
	static char buf[1024]; // protected by the output_lock
	char *head = buf;
	char ch;

	qassert(pthread_mutex_lock(&output_lock), 0);

	write(2, "QDEBUG: ",8);

	va_start(args, format);
	/* avoiding the obvious method, to save on memory
	vfprintf(stderr, format, args);*/
	while ((ch = *format++)) {
	    if (ch == '%') {
		ch = *format++;
		switch (ch) {
		    case 's':
			{
			    char * str = va_arg(args, char*);
			    write(2, buf, head - buf);
			    head = buf;
			    write(2, str, strlen(str));
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
			    base = (ch == 'p')?16:10;
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
				    tmp = num%base;
				    *(head-places) = (tmp<10)?('0'+tmp):('a'+tmp-10);
				    num /= base;
				    places++;
				}
				num %= base;
				*(head-places) = (num<10)?('0'+num):('a'+num-10);
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
	write(2, buf, head - buf);
	va_end(args);
	/*fflush(stderr);*/

	qassert(pthread_mutex_unlock(&output_lock), 0);
    }
}				       /*}}} */
#else
#define qthread_debug(...) do{ }while(0)
#endif

#endif
