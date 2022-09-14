#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* The API */
#include "qthread/qthread.h"
#include "qt_feb.h"

/* Internal Headers */
#include "qt_visibility.h"

/* functions to implement FEB-ish locking/unlocking*/

int API_FUNC qthread_lock_init(const aligned_t *a, const bool is_recursive)
{                      /*{{{ */
    return qthread_feb_adr_init(a, is_recursive);
}                      /*}}} */

int API_FUNC qthread_lock_destroy(aligned_t *a)
{                      /*{{{ */
    return qthread_feb_adr_remove(a);
}                      /*}}} */

int API_FUNC qthread_lock(const aligned_t *a)
{                      /*{{{ */
    return qthread_readFE(NULL, a);
}                      /*}}} */

const int API_FUNC qthread_trylock(const aligned_t *a)
{                      /*{{{ */
    return qthread_readFE_nb(NULL, a);
}                      /*}}} */

int API_FUNC qthread_unlock(const aligned_t *a)
{                      /*{{{ */
    return qthread_fill(a);
}                      /*}}} */

/* vim:set expandtab: */
