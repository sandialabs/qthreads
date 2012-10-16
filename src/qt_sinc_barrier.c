#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* System Headers */
#include <stdlib.h>
#include <stdio.h>

/* System Compatibility Header */
#include "qthread-int.h"

/* Installed Headers */
#include <qthread/qthread.h>
#include <qthread/feb_barrier.h>

/* Internal Headers */
#include "qt_atomics.h"
#include "qt_mpool.h"
#include "qthread_innards.h"
#include "qt_visibility.h"
#include "qt_debug.h"
#include "qthread_asserts.h"
#include "qthread/qt_sinc_barrier.h"

void API_FUNC qt_sinc_barrier_enter(qt_sinc_barrier_t *b);
void qt_sinc_barrier_init(qt_sinc_barrier_t *restrict  barrier,
			  size_t               expect);
void qt_sinc_barrier_change(qt_sinc_barrier_t *restrict  barrier,
			    size_t               expect);

void qt_sinc_resize(qt_sinc_t   *sinc_,
		    const size_t diff);

qt_sinc_barrier_t *barrier;

void qt_sinc_barrier_init(qt_sinc_barrier_t *restrict  barrier,
			  size_t               expect)
{
  barrier->count = expect;
  barrier->sinc_1 = qt_sinc_create(0, NULL, NULL, expect);
  barrier->sinc_2 = qt_sinc_create(0, NULL, NULL, expect);
  barrier->sinc_3 = qt_sinc_create(0, NULL, NULL, expect);
}
void qt_sinc_barrier_destroy(qt_sinc_barrier_t *restrict  barrier)
{
  if (barrier->sinc_1) qt_sinc_destroy(barrier->sinc_1);
  barrier->sinc_1 = NULL;
  if (barrier->sinc_2) qt_sinc_destroy(barrier->sinc_2);
  barrier->sinc_2 = NULL;
  if (barrier->sinc_3) qt_sinc_destroy(barrier->sinc_3);
  barrier->sinc_3 = NULL;
}

void qt_sinc_barrier_change(qt_sinc_barrier_t *restrict  barrier,
			  size_t               expect)
{
  int diff = expect - barrier->count;
  barrier->count = expect;

  // modify each internal sinc to countdown form new limit
  qt_sinc_resize(barrier->sinc_1, diff);
  qt_sinc_resize(barrier->sinc_2, diff);
  qt_sinc_resize(barrier->sinc_3, diff);
}

// feels like care with the reset should allow this to be done with only 2 not 3 sincs

void API_FUNC qt_sinc_barrier_enter(qt_sinc_barrier_t *barrier)
{
  qt_sinc_submit(barrier->sinc_1, NULL);
  qt_sinc_wait(barrier->sinc_1, NULL);
  qt_sinc_reset(barrier->sinc_3, barrier->count); // should be only 1 reset not all
  qt_sinc_submit(barrier->sinc_2, NULL);
  qt_sinc_wait(barrier->sinc_2, NULL);
  qt_sinc_reset(barrier->sinc_1, barrier->count); // should be only 1 reset not all
  qt_sinc_submit(barrier->sinc_3, NULL);
  qt_sinc_wait(barrier->sinc_3, NULL);
  qt_sinc_reset(barrier->sinc_2, barrier->count); // should be only 1 reset not all
 }

/* vim:set expandtab: */
