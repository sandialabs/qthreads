#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "qthread/qthread.h"
#include "qthread/cacheline.h"
#include "qthread/qt_sinc.h"
#include "qt_visibility.h"

static size_t       num_sheps;
static size_t       num_workers;
static size_t       num_wps;
static unsigned int cacheline;

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

    if (NULL != initial_value) {
        sinc->initial_value = malloc(sizeof_value);
        memcpy(sinc->initial_value, initial_value, sizeof_value);
    } else {
        sinc->initial_value = NULL;
        sinc->values = NULL;
        sinc->sizeof_shep_value_part = 0;
    }

    // Allocate values array
    if (NULL != initial_value) {
        const size_t sizeof_shep_values     = num_wps * sizeof_value;
        const size_t num_lines_per_shep     = ceil(sizeof_shep_values * 1.0 / cacheline);
        const size_t num_lines              = num_sheps * num_lines_per_shep;
        const size_t sizeof_shep_value_part = num_lines_per_shep * cacheline;

        sinc->sizeof_shep_value_part = sizeof_shep_value_part;

        sinc->values = malloc(num_lines * cacheline);
        assert(sinc->values);

        // Initialize values
        for (size_t s = 0; s < num_sheps; s++) {
            const size_t shep_offset = s * sizeof_shep_value_part;
            for (size_t w = 0; w < num_wps; w++) {
                const size_t worker_offset = s == 0 ?
                                             w * sinc->sizeof_value :
                                             (w % num_wps) * sinc->sizeof_value;
                memcpy((uint8_t *)sinc->values + shep_offset + worker_offset,
                       initial_value,
                       sizeof_value);
            }
        }
    }

    sinc->result = NULL;

    // Allocate counts array
    const size_t sizeof_count = sizeof(qt_sinc_count_t);
    sinc->sizeof_count = sizeof_count;

    const size_t sizeof_shep_counts     = num_wps * sizeof_count;
    const size_t num_lines_per_shep     = ceil(sizeof_shep_counts * 1.0 / cacheline);
    const size_t sizeof_shep_count_part = num_lines_per_shep * cacheline;
    const size_t num_count_array_lines  = num_sheps * num_lines_per_shep;

    sinc->sizeof_shep_count_part = sizeof_shep_count_part;
    sinc->counts                 = calloc(num_count_array_lines, cacheline);
    assert(sinc->counts);

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
                const size_t shep_offset   = s * sinc->sizeof_shep_count_part;
                const size_t worker_offset = s == 0 ?
                                             w * sinc->sizeof_count :
                                             (w % num_wps) * sinc->sizeof_count;
                const size_t offset = shep_offset + worker_offset;

                *(qt_sinc_count_t *)((uint8_t *)sinc->counts + offset) =
                    num_per_worker;
                if (extras > 0) {
                    *(qt_sinc_count_t *)((uint8_t *)sinc->counts + offset) += 1;
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
        for (size_t s = 0; s < num_sheps; s++) {
            const size_t shep_offset = s * sinc->sizeof_shep_value_part;
            for (size_t w = 0; w < num_wps; w++) {
                const size_t worker_offset = s == 0 ?
                                             w * sinc->sizeof_value :
                                             (w % num_wps) * sinc->sizeof_value;
                memcpy((uint8_t *)sinc->values + shep_offset + worker_offset,
                       sinc->initial_value,
                       sinc->sizeof_value);
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

    for (size_t s = 0; s < num_sheps; s++) {
        for (size_t w = 0; w < num_wps; w++) {
            const size_t shep_offset   = s * sinc->sizeof_shep_count_part;
            const size_t worker_offset = s == 0 ?
                                         w * sinc->sizeof_count :
                                         (w % num_wps) * sinc->sizeof_count;
            const size_t offset = shep_offset + worker_offset;

            *(qt_sinc_count_t *)((uint8_t *)sinc->counts + offset) =
                num_per_worker;
            if (extras > 0) {
                extras--;
                *(qt_sinc_count_t *)((uint8_t *)sinc->counts + offset) += 1;
            }
        }
    }

    // Reset ready flag
    sinc->ready = SYNCVAR_EMPTY_INITIALIZER;

    // Reset result
    if (NULL != sinc->result) {
        free(sinc->result);
        sinc->result = NULL;
    }
}

void qt_sinc_destroy(qt_sinc_t *sinc)
{
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
        qthread_shepherd_id_t     shep_id;
        const qthread_worker_id_t worker_id = qthread_worker(&shep_id);

        const size_t shep_offset   = shep_id * sinc->sizeof_shep_count_part;
        const size_t worker_offset = shep_id == 0 ?
                                     worker_id * sinc->sizeof_count :
                                     (worker_id % (num_workers / num_sheps)) * sinc->sizeof_count;
        qt_sinc_count_t *counts = (qt_sinc_count_t *)((uint8_t *)sinc->counts + shep_offset + worker_offset);

        // Increment count
        qt_sinc_count_t old;
        do {
            old = *counts;
        } while (old != qthread_cas(counts, old, old + count));

        // Increment remaining, if necessary
        if (old == 0) {
            aligned_t old;
            do {
                old = sinc->remaining;
            } while (old != qthread_cas(&sinc->remaining, old, old + 1));
        }
    }
}

void *qt_sinc_tmpdata(qt_sinc_t *sinc)
{
    if (NULL != sinc->values) {
        qthread_shepherd_id_t shep_id;
        qthread_worker_id_t   worker_id     = qthread_worker(&shep_id);
        const size_t          shep_offset   = shep_id * sinc->sizeof_shep_value_part;
        const size_t          worker_offset = (shep_id == 0) ?
                                              (worker_id * sinc->sizeof_value) :
                                              ((worker_id % (num_workers / num_sheps)) * sinc->sizeof_value);
        return (uint8_t *)sinc->values + shep_offset + worker_offset;
    } else {
        return NULL;
    }
}

void qt_sinc_submit(qt_sinc_t *sinc,
                    void      *value)
{
    assert(NULL != sinc->values || NULL == value);

    qthread_shepherd_id_t shep_id;
    qthread_worker_id_t   worker_id = qthread_worker(&shep_id);

    if (NULL != sinc->values && NULL != value)
    {
        const size_t shep_offset   = shep_id * sinc->sizeof_shep_value_part;
        const size_t worker_offset = (shep_id == 0) ?
                                     (worker_id * sinc->sizeof_value) :
                                     ((worker_id % (num_workers / num_sheps)) * sinc->sizeof_value);
        void *values = (uint8_t *)sinc->values + shep_offset + worker_offset;

        sinc->op(values, value);
    }

    while (1) {
        // Calculate offset in counts array
        const size_t shep_offset   = shep_id * sinc->sizeof_shep_count_part;
        const size_t worker_offset = (shep_id == 0) ?
                                     (worker_id * sinc->sizeof_count) :
                                     ((worker_id % (num_wps)) * sinc->sizeof_count);
        qt_sinc_count_t *count =
            (qt_sinc_count_t *)((uint8_t *)sinc->counts + shep_offset + worker_offset);

        // Try to decrement this worker's count
        qt_sinc_count_t old_count, new_count;
#ifndef PUREWS_SINCS
        do {
            old_count = *count;
            new_count = -1;
            if (old_count != 0) {
                new_count = qthread_cas(count, old_count, old_count - 1);
            }
        } while (old_count != 0 && new_count != old_count);

        // Decrement remaining if successfully decremented to zero, and stop
        if ((new_count == 1) && (old_count == 1)) {
            aligned_t old, new;
            do {
                old = sinc->remaining;
                new = -1;
                if (old != 0) {
                    new = qthread_cas(&sinc->remaining, old, old - 1);
                } else {
                    assert(0); // Two workers set remaining to zero
                }
            } while (old != 0 && new != old);

            if ((new == 1) && (old == 1)) {
                qthread_syncvar_writeF_const(&sinc->ready, 42);
            }

            break;
        }

        // Stop if successfully decremented
        if (new_count == old_count) {
            break;
        }

        // Try the next worker
        worker_id = (worker_id == num_workers - 1) ? 0 : worker_id + 1;

        // Find shep_id for this worker
        if (worker_id % num_wps == 0) {
            shep_id = (shep_id == num_sheps - 1) ? 0 : shep_id + 1;
            qthread_migrate_to(shep_id);
        }
#else   /* ifndef PUREWS_SINCS */
        if (*count > 0) {
            *count -= 1;
            break;
        } else {
            // need to migrate elsewhere
            // Find shep_id for this worker
            shep_id = (shep_id == num_sheps - 1) ? 0 : shep_id + 1;
            qthread_migrate_to(shep_id);
        }
#endif  /* ifndef PUREWS_SINCS */
    }
}

void qt_sinc_wait(qt_sinc_t *sinc,
                  void      *target)
{
    qthread_syncvar_readFE(NULL, &sinc->ready);

    if ((NULL != sinc->values) && (NULL != target)) {
        if (NULL == sinc->result) {
            sinc->result = malloc(sinc->sizeof_value);
            assert(sinc->result);
            memcpy(sinc->result, sinc->initial_value, sinc->sizeof_value);

            for (size_t s = 0; s < num_sheps; s++) {
                const size_t shep_offset = s * sinc->sizeof_shep_value_part;
                for (size_t w = 0; w < (num_workers / num_sheps); w++) {
                    const size_t worker_offset = s == 0 ?
                                                 w * sinc->sizeof_value :
                                                 (w % (num_workers / num_sheps)) * sinc->sizeof_value;
                    sinc->op(sinc->result,
                             (uint8_t *)sinc->values + shep_offset + worker_offset);
                }
            }
        }
        memcpy(target, sinc->result, sinc->sizeof_value);
    }
    qthread_syncvar_writeEF_const(&sinc->ready, 42);
}

/* vim:set expandtab: */
