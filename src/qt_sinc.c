#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#if (HAVE_MEMALIGN && HAVE_MALLOC_H)
# include <malloc.h>                   /* for memalign() */
#endif

#include "qthread/qthread.h"
#include "qthread/cacheline.h"
#include "qthread/qt_sinc.h"
#include "qthread_asserts.h"
#include "qt_shepherd_innards.h"
#include "qt_visibility.h"

static size_t       num_sheps;
static size_t       num_workers;
static size_t       num_wps;
static unsigned int cacheline;

#ifdef HAVE_MEMALIGN
#define ALIGNED_ALLOC(val, size, align) (val) = memalign((align), (size))
#elif defined(HAVE_POSIX_MEMALIGN)
#define ALIGNED_ALLOC(val, size, align) posix_memalign(&(val), (align), (size))
#elif defined(HAVE_WORKING_VALLOC)
#define ALIGNED_ALLOC(val, size, align) (val) = valloc((size))
#elif defined(HAVE_PAGE_ALIGNED_MALLOC)
#define ALIGNED_ALLOC(val, size, align) (val) = malloc((size))
#else
#define ALIGNED_ALLOC(val, size, align) (val) = valloc((size)) /* cross your fingers! */
#endif

qt_sinc_t *qt_sinc_create(const size_t sizeof_value,
                          const void  *initial_value,
                          qt_sinc_op_f op,
                          const size_t will_spawn)
{
    qt_sinc_t *sinc = malloc(sizeof(qt_sinc_t));

    assert(sinc);

    if (num_sheps == 0) {
        num_sheps   = qthread_readstate(TOTAL_SHEPHERDS);
        num_workers = qthread_readstate(TOTAL_WORKERS);
        num_wps     = num_workers / num_sheps;
        cacheline   = qthread_cacheline();
    }

    sinc->op           = op;
    sinc->sizeof_value = sizeof_value;

    assert((0 == sizeof_value && NULL == initial_value) ||
           (0 != sizeof_value && NULL != initial_value));

    // Allocate values array
    if (NULL != initial_value) {
        const size_t sizeof_shep_values     = num_wps * sizeof_value;
        const size_t num_lines_per_shep     = ceil(sizeof_shep_values * 1.0 / cacheline);
        const size_t num_lines              = num_sheps * num_lines_per_shep;
        const size_t sizeof_shep_value_part = num_lines_per_shep * cacheline;

        sinc->initial_value = malloc(sizeof_value);
        assert(sinc->initial_value);
        memcpy(sinc->initial_value, initial_value, sizeof_value);

        sinc->sizeof_shep_value_part = sizeof_shep_value_part;

        ALIGNED_ALLOC(sinc->values, num_lines * cacheline, cacheline);
        assert(sinc->values);

        // Initialize values
        for (size_t s = 0; s < num_sheps; s++) {
            const size_t shep_offset = s * sizeof_shep_value_part;
            for (size_t w = 0; w < num_wps; w++) {
                const size_t worker_offset = w * sinc->sizeof_value;
                memcpy((uint8_t *)sinc->values + shep_offset + worker_offset,
                       initial_value,
                       sizeof_value);
            }
        }
        sinc->result = malloc(sinc->sizeof_value);
        assert(sinc->result);
    } else {
        sinc->initial_value          = NULL;
        sinc->values                 = NULL;
        sinc->sizeof_shep_value_part = 0;
        sinc->result                 = NULL;
    }
    assert((sinc->result && sinc->initial_value) || (!sinc->result && !sinc->initial_value));

    // Allocate counts array
    const size_t sizeof_shep_counts     = num_wps * QTHREAD_SIZEOF_ALIGNED_T;
    const size_t num_lines_per_shep     = ceil(sizeof_shep_counts * 1.0 / cacheline);
    const size_t sizeof_shep_count_part = (num_lines_per_shep * cacheline) / QTHREAD_SIZEOF_ALIGNED_T;
    const size_t num_count_array_lines  = num_sheps * num_lines_per_shep;

    sinc->sizeof_shep_count_part = sizeof_shep_count_part;
    ALIGNED_ALLOC(sinc->counts, num_count_array_lines * cacheline, cacheline);
    assert(sinc->counts);
    memset(sinc->counts, 0, num_count_array_lines * cacheline);

#if defined(SINCS_PROFILE)
    ALIGNED_ALLOC(sinc->count_incrs, num_count_array_lines * cacheline, cacheline);
    assert(sinc->count_incrs);
    memset(sinc->counts, 0, num_count_array_lines * cacheline);
#endif /* defined(SINCS_PROFILE) */

    // Initialize counts array
    if (will_spawn > 0) {
        const size_t num_per_worker = will_spawn / num_workers;
        size_t       extras         = will_spawn % num_workers;

        // Set remaining count
        if (num_per_worker > 0) {
            sinc->remaining = num_workers;
        } else {
            sinc->remaining = extras;
        }

        for (size_t s = 0; s < num_sheps; s++) {
            for (size_t w = 0; w < num_wps; w++) {
                const size_t shep_offset = s * sinc->sizeof_shep_count_part;
                const size_t offset      = shep_offset + w;

                sinc->counts[offset] = num_per_worker;
                if (extras > 0) {
                    sinc->counts[offset]++;
                    extras--;
                }
            }
        }
    } else {
        sinc->remaining = 0;
    }

    sinc->ready = SYNCVAR_EMPTY_INITIALIZER;

    return sinc;
}

void qt_sinc_reset(qt_sinc_t   *sinc,
                   const size_t will_spawn)
{
    // Reset values
    if (NULL != sinc->values) {
        const size_t sizeof_shep_value_part = sinc->sizeof_shep_value_part;
        const size_t sizeof_value           = sinc->sizeof_value;
        for (size_t s = 0; s < num_sheps; s++) {
            const size_t shep_offset = s * sizeof_shep_value_part;
            for (size_t w = 0; w < num_wps; w++) {
                const size_t worker_offset = w * sizeof_value;
                memcpy((uint8_t *)sinc->values + shep_offset + worker_offset,
                       sinc->initial_value,
                       sizeof_value);
            }
        }
    }

    // Reset counts
    const size_t num_per_worker = will_spawn / num_workers;
    size_t       extras         = will_spawn % num_workers;

    if (num_per_worker > 0) {
        sinc->remaining = num_workers;
    } else {
        sinc->remaining = extras;
    }

    const size_t sizeof_shep_count_part = sinc->sizeof_shep_count_part;
    for (size_t s = 0; s < num_sheps; s++) {
        for (size_t w = 0; w < num_wps; w++) {
            const size_t shep_offset = s * sizeof_shep_count_part;
            const size_t offset      = shep_offset + w;

            sinc->counts[offset] = num_per_worker;
            if (extras > 0) {
                extras--;
                sinc->counts[offset]++;
            }
        }
    }

    // Reset ready flag
    sinc->ready = SYNCVAR_EMPTY_INITIALIZER;
}

void qt_sinc_destroy(qt_sinc_t *sinc)
{
#if defined(SINCS_PROFILE)
    const size_t sizeof_shep_count_part = sinc->sizeof_shep_count_part;
    for (size_t s = 0; s < num_sheps; s++) {
        for (size_t w = 0; w < num_wps; w++) {
            const size_t shep_offset   = s * sizeof_shep_count_part;
            const size_t offset = shep_offset + w;

            fprintf(stderr, "CI %lu %lu %lu\n", s, w, (unsigned long)sinc->count_incrs[offset]);
        }
    }

    free(sinc->count_incrs);
#endif /* defined(SINCS_PROFILE) */

    assert(sinc);
    assert(sinc->counts);
    free(sinc->counts);
    if (sinc->result || sinc->values) {
        assert(sinc->result);
        free(sinc->result);
        assert(sinc->values);
        free(sinc->values);
    }
    free(sinc);
}

/* Adds a new participant to the sinc.
 * Pre:  sinc was created
 * Post: aggregate count is positive
 */
void qt_sinc_willspawn(qt_sinc_t *sinc,
                       size_t     count)
{
    assert(sinc);
    if (count > 0) {
        const qthread_worker_id_t worker_id = qthread_readstate(CURRENT_WORKER);
        const size_t              shep_id   = qthread_shep();
        qt_sinc_count_t *counts = sinc->counts
            + (shep_id * sinc->sizeof_shep_count_part)
            + worker_id;

        // Increment count
        qt_sinc_count_t old = qthread_incr(counts, count);

        // Increment remaining, if necessary
        if (old == 0) {
            (void)qthread_incr(&sinc->remaining, 1);
        }
    }
}

void *qt_sinc_tmpdata(qt_sinc_t *sinc)
{
    if (NULL != sinc->values) {
        const size_t shep_offset   = qthread_shep() * sinc->sizeof_shep_value_part;
        const size_t worker_offset = qthread_readstate(CURRENT_WORKER) * sinc->sizeof_value;
        return (uint8_t *)sinc->values + shep_offset + worker_offset;
    } else {
        return NULL;
    }
}

static void qt_sinc_internal_collate(qt_sinc_t *sinc)
{
    if (sinc->values) {
        // step 1: collate results
        const size_t sizeof_value           = sinc->sizeof_value;
        const size_t sizeof_shep_value_part = sinc->sizeof_shep_value_part;

        memcpy(sinc->result, sinc->initial_value, sizeof_value);
        for (qthread_shepherd_id_t s = 0; s < num_sheps; ++s) {
            const size_t shep_offset = s * sizeof_shep_value_part;
            for (size_t w = 0; w < num_wps; ++w) {
                sinc->op(sinc->result,
                        (uint8_t *)sinc->values + shep_offset + (w * sizeof_value));
            }
        }
    }
    // step 2: release waiters
    qthread_syncvar_writeF_const(&sinc->ready, 42);
}

void qt_sinc_submit(qt_sinc_t *restrict sinc,
                    void      *restrict value)
{
    assert(NULL != sinc->values || NULL == value);
    assert((sinc->result && sinc->initial_value) || (!sinc->result && !sinc->initial_value));

    const size_t sizeof_shep_value_part = sinc->sizeof_shep_value_part;
    const size_t sizeof_shep_count_part = sinc->sizeof_shep_count_part;
    const size_t sizeof_value           = sinc->sizeof_value;
    qthread_shepherd_t *this_shep = qthread_internal_getshep();
    assert(this_shep);

    qthread_shepherd_id_t shep_id   = qthread_shep();
    qthread_worker_id_t   worker_id = qthread_readstate(CURRENT_WORKER);

    if (NULL != value) {
        const size_t shep_offset   = shep_id * sizeof_shep_value_part;
        const size_t worker_offset = worker_id * sizeof_value;
        void        *values        = (uint8_t *)sinc->values + shep_offset + worker_offset;

        sinc->op(values, value);
    }

    // first check just this shepherd
    {
        const size_t shep_offset = shep_id * sizeof_shep_count_part;
        for (qthread_worker_id_t wkr_delta = 0; wkr_delta < num_wps; ++wkr_delta) {
            qthread_worker_id_t cur_wkr = (worker_id + wkr_delta) % num_wps;
            qt_sinc_count_t *count = sinc->counts + shep_offset + cur_wkr;
            // Try to decrement this worker's count
            qt_sinc_count_t old_count = *count;
            if (old_count > 0) {
                old_count = qthread_incr(count, -1);
                if (old_count < 1) {
                    (void)qthread_incr(count, 1);
                    old_count = 0;
                }
            } else {
                old_count = 0;
            }
            if (old_count == 1) {
                /* My counter went to zero, therefore I ned to decrement the global
                 * count of workers with non-zero counts (aka "remaining") */
                aligned_t oldr = qthread_incr(&sinc->remaining, -1);
                assert(oldr > 0);
                if (oldr == 1) qt_sinc_internal_collate(sinc);
                return;
            } else if (old_count != 0) {
                return;
            }
        }
    }
    // now check other shepherds
    for (qthread_shepherd_id_t shep_delta = 0; shep_delta < (num_sheps-1); ++shep_delta) {
        qthread_shepherd_id_t cur_shep = this_shep->sorted_sheplist[shep_delta];
        const size_t shep_offset = cur_shep * sizeof_shep_count_part;
        for (qthread_worker_id_t wkr = 0; wkr < num_wps; ++wkr) {
            qt_sinc_count_t *count = sinc->counts + shep_offset + wkr;
#if defined(SINCS_PROFILE)
            qt_sinc_count_t *count_incr =  sinc->count_incrs + shep_offset + wkr;
#endif /* defined(SINCS_PROFILE) */
            // Try to decrement this worker's count
            qt_sinc_count_t old_count = *count;
            if (old_count > 0) {
                old_count = qthread_incr(count, -1);
#if defined(SINCS_PROFILE)
                (void)qthread_incr(count_incr, 1);
#endif /* defined(SINCS_PROFILE) */
                if (old_count < 1) {
                    (void)qthread_incr(count, 1);
#if defined(SINCS_PROFILE)
                    (void)qthread_incr(count_incr, 1);
#endif /* defined(SINCS_PROFILE) */
                    old_count = 0;
                }
            } else {
                old_count = 0;
            }
            if (old_count == 1) {
                /* My counter went to zero, therefore I ned to decrement the global
                 * count of workers with non-zero counts (aka "remaining") */
                aligned_t oldr = qthread_incr(&sinc->remaining, -1);
                assert(oldr > 0);
                if (oldr == 1) qt_sinc_internal_collate(sinc);
                return;
            } else if (old_count != 0) {
                return;
            }
        }
    }
}

void qt_sinc_wait(qt_sinc_t *restrict sinc,
                  void      *restrict target)
{
    qthread_syncvar_readFF(NULL, &sinc->ready);

    if (target) {
        assert(sinc->sizeof_value > 0);
        memcpy(target, sinc->result, sinc->sizeof_value);
    }
}

/* vim:set expandtab: */
