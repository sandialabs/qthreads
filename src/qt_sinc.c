#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "qthread/qthread.h"
#include "qthread/cacheline.h"
#include "qthread/qt_sinc.h"
#include "qt_visibility.h"

void INTERNAL *qt_sinc_value(qt_sinc_t *sinc)
{
    qthread_shepherd_id_t shep_id;
    const qthread_worker_id_t worker_id = qthread_worker(&shep_id);

    const size_t shep_offset = shep_id * sinc->sizeof_shep_value_part;
    const size_t worker_offset = shep_id == 0 ? 
        worker_id * sinc->sizeof_value : 
        (worker_id % shep_id) * sinc->sizeof_value;

    return sinc->values + shep_offset + worker_offset;
}

qt_sinc_count_t INTERNAL *qt_sinc_count(qt_sinc_t *sinc)
{
    qthread_shepherd_id_t shep_id;
    const qthread_worker_id_t worker_id = qthread_worker(&shep_id);

    const size_t shep_offset = shep_id * sinc->sizeof_shep_count_part;
    const size_t worker_offset = shep_id == 0 ? 
        worker_id * sinc->sizeof_count : 
        (worker_id % shep_id) * sinc->sizeof_count;

    return (qt_sinc_count_t *)(sinc->counts + shep_offset+worker_offset);
}

void INTERNAL qt_sinc_check(qt_sinc_t *sinc)
{
    qt_sinc_count_t sum      = 0;
    const size_t num_sheps   = qthread_readstate(TOTAL_SHEPHERDS);
    const size_t num_workers = qthread_readstate(TOTAL_WORKERS);

    for (size_t s = 0; s < num_sheps; s++) {
        for (size_t w = 0; w < (num_workers/num_sheps); w++) {
            const size_t shep_offset = s * sinc->sizeof_shep_count_part;
            const size_t worker_offset = s == 0 ? 
                w * sinc->sizeof_count : 
                (w % s) * sinc->sizeof_count;
            const size_t offset = shep_offset + worker_offset;
            sum += *(qt_sinc_count_t *)(sinc->counts + offset);
        }
    }

    if (0 == sum) {
        qthread_syncvar_writeF_const(&sinc->ready, 42);
    }
}

qt_sinc_t *qt_sinc_create(const size_t  sizeof_value,
                          const void   *initial_value,
                          qt_sinc_op_f  op,
                          const size_t  will_spawn)
{
    qt_sinc_t *sinc = malloc(sizeof(qt_sinc_t));
    assert(sinc);

    sinc->op = op;
    sinc->sizeof_value = sizeof_value;

    if (NULL != initial_value) {
        sinc->initial_value = malloc(sizeof_value);
        memcpy(sinc->initial_value, initial_value, sizeof_value);
    }

    // Create cacheline-buffered values and counts arrays
    const int    cacheline                = qthread_cacheline();
    const size_t num_sheps                = qthread_readstate(TOTAL_SHEPHERDS);
    const size_t num_workers              = qthread_readstate(TOTAL_WORKERS);
    const size_t num_workers_per_shepherd = num_workers / num_sheps;

    // Allocate values array
    if (sizeof_value > 0) {
        const size_t sizeof_shep_values =
            num_workers_per_shepherd * sizeof_value;
        const size_t num_lines_per_shep =
            ceil(sizeof_shep_values*1.0 / cacheline);
        const size_t num_lines =
            num_sheps * num_lines_per_shep;
        const size_t sizeof_shep_value_part = 
            num_lines_per_shep * cacheline;

        sinc->sizeof_shep_value_part = sizeof_shep_value_part;

        sinc->values = malloc(num_lines *cacheline);
        assert(sinc->values);

        // Initialize values
        for (size_t s = 0; s < num_sheps; s++) {
            const size_t shep_offset = s * sizeof_shep_value_part;
            for (size_t w = 0; w < (num_workers/num_sheps); w++) {
                const size_t worker_offset = s == 0 ? 
                    w * sinc->sizeof_value : 
                    (w % s) * sinc->sizeof_value;
                memcpy(sinc->values + shep_offset + worker_offset, 
                       initial_value, 
                       sizeof_value);
            }
        }
    } else {
        sinc->values = NULL;
        sinc->result = NULL;
    }

    // Allocate counts array
    const size_t sizeof_count = sizeof(qt_sinc_count_t);
    sinc->sizeof_count = sizeof_count;

    assert(cacheline >= sizeof_count); // TODO: decide if this is necessary
    assert(cacheline % sizeof_count == 0); // TODO: decide if this is necessary

    const size_t sizeof_shep_counts =
        num_workers_per_shepherd * sizeof_count;
    const size_t num_lines_per_shep =
        ceil(sizeof_shep_counts*1.0 / cacheline);
    const size_t sizeof_shep_count_part = 
        num_lines_per_shep * cacheline;
    const size_t num_count_array_lines =
        num_sheps * num_lines_per_shep;

    sinc->sizeof_shep_count_part = sizeof_shep_count_part;
    sinc->counts = calloc(num_count_array_lines, cacheline);
    assert(sinc->counts);

    // Initialize counts array
    if (will_spawn > 0) {
        const size_t num_per_worker = will_spawn / num_workers;
        size_t extras = will_spawn % num_workers;

        for (size_t s = 0; s < num_sheps; s++) {
            for (size_t w = 0; w < (num_workers/num_sheps); w++) {
                const size_t shep_offset = s * sinc->sizeof_shep_count_part;
                const size_t worker_offset = s == 0 ? 
                    w * sinc->sizeof_count : 
                    (w % s) * sinc->sizeof_count;
                const size_t offset = shep_offset + worker_offset;

                *(qt_sinc_count_t *)(sinc->counts + offset) = num_per_worker;
                if (extras > 0) {
                    *(qt_sinc_count_t *)(sinc->counts + offset) += 1;
                    extras--;
                }
            }
        }
    }

    sinc->ready = SYNCVAR_EMPTY_INITIALIZER;

    return sinc;
}

void qt_sinc_reset(qt_sinc_t *sinc,
                   const size_t will_spawn)
{
    const size_t num_sheps   = qthread_readstate(TOTAL_SHEPHERDS);
    const size_t num_workers = qthread_readstate(TOTAL_WORKERS);

    // Reset values
    for (size_t s = 0; s < num_sheps; s++) {
        const size_t shep_offset = s * sinc->sizeof_shep_value_part;
        for (size_t w = 0; w < (num_workers/num_sheps); w++) {
            const size_t worker_offset = s == 0 ? 
                w * sinc->sizeof_value : 
                (w % s) * sinc->sizeof_value;
            memcpy(sinc->values + shep_offset + worker_offset, 
                   sinc->initial_value, 
                   sinc->sizeof_value);
        }
    }

    // Reset counts
    const size_t num_per_worker = will_spawn / num_workers;
    size_t extras = will_spawn % num_workers;

    for (size_t s = 0; s < num_sheps; s++) {
        for (size_t w = 0; w < (num_workers/num_sheps); w++) {
            const size_t shep_offset = s * sinc->sizeof_shep_count_part;
            const size_t worker_offset = s == 0 ? 
                w * sinc->sizeof_count : 
                (w % s) * sinc->sizeof_count;
            const size_t offset = shep_offset + worker_offset;

            *(qt_sinc_count_t *)(sinc->counts + offset) = num_per_worker;
            if (extras > 0) {
                extras--;
                *(qt_sinc_count_t *)(sinc->counts + offset) += 1;
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
    free(sinc->counts);
    free(sinc->result);
    free(sinc->values);
    free(sinc);
}

/* Adds a new participant to the sinc.
 * Pre:  sinc was created
 * Post: aggregate count is positive
 */
void qt_sinc_willspawn(qt_sinc_t *sinc,
                       size_t     count)
{
    *(qt_sinc_count(sinc)) += count;
}

void qt_sinc_submit(qt_sinc_t *sinc,
                    void      *value)
{
    if (NULL != sinc->values && NULL != value) {
        sinc->op(qt_sinc_value(sinc), value);
    }
    *(qt_sinc_count(sinc)) += -1;

    qt_sinc_check(sinc);
}

void qt_sinc_wait(qt_sinc_t *sinc,
                  void      *target)
{
    qthread_syncvar_readFE(NULL, &sinc->ready);

    if (NULL != sinc->values && NULL != target) {
        if (NULL == sinc->result) {
            sinc->result = malloc(sinc->sizeof_value);
            assert(sinc->result);
            memcpy(sinc->result, sinc->initial_value, sinc->sizeof_value);

            const size_t num_sheps = qthread_readstate(TOTAL_SHEPHERDS);
            const size_t num_workers = qthread_readstate(TOTAL_WORKERS);
            for (size_t s = 0; s < num_sheps; s++) {
                for (size_t w = 0; w < (num_workers/num_sheps); w++) {
                    const size_t shep_offset = s * sinc->sizeof_shep_value_part;
                    const size_t worker_offset = s == 0 ? 
                        w * sinc->sizeof_value : 
                        (w % s) * sinc->sizeof_value;

                    sinc->op(sinc->result, 
                             sinc->values + shep_offset + worker_offset);
                }
            }
        }
        memcpy(target, sinc->result, sinc->sizeof_value);
    }
    qthread_syncvar_writeEF_const(&sinc->ready, 42);
}

/* vim:set expandtab: */
