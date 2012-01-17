#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define QTHREAD_NO_ASSERTS 1

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

typedef struct {
    aligned_t c;
    uint8_t   pad[CACHELINE_WIDTH - sizeof(aligned_t)];
} qt_sinc_cache_count_t;

struct qt_sinc_s {
    void *restrict                  values;
    qt_sinc_cache_count_t *restrict counts;
    qt_sinc_op_f                    op;
    syncvar_t                       ready;
    void *restrict                  result;
    void *restrict                  initial_value;
    aligned_t                       remaining;
    size_t                          sizeof_value;
    size_t                          sizeof_shep_value_part;
    size_t                          sizeof_shep_count_part;
};

static size_t       num_sheps;
static size_t       num_workers;
static size_t       num_wps;
static unsigned int cacheline;

#ifdef HAVE_MEMALIGN
# define ALIGNED_ALLOC(val, size, align) (val) = memalign((align), (size))
#elif defined(HAVE_POSIX_MEMALIGN)
# define ALIGNED_ALLOC(val, size, align) posix_memalign((void **)&(val), (align), (size))
#elif defined(HAVE_WORKING_VALLOC)
# define ALIGNED_ALLOC(val, size, align) (val) = valloc((size))
#elif defined(HAVE_PAGE_ALIGNED_MALLOC)
# define ALIGNED_ALLOC(val, size, align) (val) = malloc((size))
#else
# define ALIGNED_ALLOC(val, size, align) (val) = valloc((size)) /* cross your fingers! */
#endif

#define SNZI_ASSIGN(a, b) do {   \
        if ((b) == 0) { a = 0; } \
        else { a = (b) + 1; }    \
} while (0)

static void qt_sinc_internal_collate(qt_sinc_t *sinc);

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
    // const size_t sizeof_shep_counts     = QTHREAD_SIZEOF_ALIGNED_T;
    // const size_t num_lines_per_shep     = 1;
    // const size_t sizeof_shep_count_part = cacheline / QTHREAD_SIZEOF_ALIGNED_T;
    // const size_t num_count_array_lines  = num_sheps;

    sinc->sizeof_shep_count_part = 1;
    assert(sizeof(qt_sinc_cache_count_t) <= cacheline);
    ALIGNED_ALLOC(sinc->counts, num_sheps * cacheline, cacheline);
    assert(sinc->counts);
    // memset(sinc->counts, 0, num_sheps * cacheline);
    // memset(sinc->counts, 0, QTHREAD_SIZEOF_ALIGNED_T * num_sheps);

    // Initialize counts array
    if (will_spawn > 0) {
        const size_t num_per_shep = will_spawn / num_sheps;
        size_t       extras       = will_spawn % num_sheps;

        // Set remaining count
        if (num_per_shep > 0) {
            sinc->remaining = num_sheps;
        } else {
            sinc->remaining = extras;
        }

        for (size_t s = 0; s < num_sheps; s++) {
#if defined(SINCS_PROFILE)
            sinc->count_spawns[s] = num_per_shep;
            if (extras > 0) {
                sinc->count_spawns[s]++;
            }
#endif      /* defined(SINCS_PROFILE) */
            if (extras > 0) {
                SNZI_ASSIGN(sinc->counts[s].c, num_per_shep + 1);
                extras--;
            } else {
                SNZI_ASSIGN(sinc->counts[s].c, num_per_shep);
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
    const size_t num_per_worker = will_spawn / num_sheps;
    size_t       extras         = will_spawn % num_sheps;

    if (num_per_worker > 0) {
        sinc->remaining = num_sheps;
    } else {
        sinc->remaining = extras;
    }

    for (size_t s = 0; s < num_sheps; s++) {
        if (extras > 0) {
            extras--;
            SNZI_ASSIGN(sinc->counts[s].c, num_per_worker + 1);
        } else {
            SNZI_ASSIGN(sinc->counts[s].c, num_per_worker);
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
            const size_t shep_offset = s * sizeof_shep_count_part;
            const size_t offset      = shep_offset + w;

            fprintf(stderr, "CI %lu %lu %lu\n", s, w, (unsigned long)sinc->count_incrs[offset]);
            fprintf(stderr, "CL %lu %lu %lu\n", s, w, (unsigned long)sinc->count_locals[offset]);
            fprintf(stderr, "CD %lu %lu %lu\n", s, w, (unsigned long)sinc->count_decrs[offset]);
            fprintf(stderr, "CR %lu %lu %lu\n", s, w, (unsigned long)sinc->count_remaining[offset]);
            fprintf(stderr, "CS %lu %lu %lu\n", s, w, (unsigned long)sinc->count_spawns[offset]);
            fprintf(stderr, "DM %lu %lu %lu\n", s, w, (unsigned long)sinc->dist_max[offset]);
            fprintf(stderr, "DT %lu %lu %lu\n", s, w, (unsigned long)sinc->dist_ttl[offset]);
            fprintf(stderr, "DC %lu %lu %lu\n", s, w, (unsigned long)sinc->dist_cnt[offset]);
            fprintf(stderr, "DA %lu %lu %f\n", s, w,
                    ((unsigned long)sinc->dist_ttl[offset] * 1.0) /
                    ((unsigned long)sinc->dist_cnt[offset] * 1.0));
        }
    }

    free(sinc->count_incrs);
    free(sinc->count_locals);
    free(sinc->count_decrs);
    free(sinc->count_remaining);
    free(sinc->count_spawns);
    free(sinc->dist_max);
    free(sinc->dist_ttl);
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

#define SNZI_HALF 1
#define SNZI_ONE  2

/* Adds a new participant to the sinc.
 * Pre:  sinc was created
 * Post: aggregate count is positive
 */
void qt_sinc_willspawn(qt_sinc_t *sinc,
                       size_t     count)
{
    assert(sinc);
    if (count > 0) {
        const size_t shep_id = qthread_shep();
        aligned_t   *C       = &sinc->counts[shep_id].c;
        int          succ    = 0;
        int          undoArr = 0;

        // Increment count
        do {
            aligned_t c = *C;
            if ((c >= SNZI_ONE) && (qthread_cas(C, c, c + count) == c)) {
                succ = 1;
            }
            if ((c == 0) && (qthread_cas(C, 0, SNZI_HALF) == 0)) {
                succ = 1;
                c    = SNZI_HALF;
            }
            if (c == SNZI_HALF) {
                qthread_incr(&sinc->remaining, 1);
                if (qthread_cas(C, SNZI_HALF, count + 1) != SNZI_HALF) {
                    undoArr--;
                } else {
                    succ = 1;
                }
            }
            COMPILER_FENCE;
        } while (!succ);
        if (undoArr != 0) {
            qthread_incr(&sinc->remaining, undoArr);
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
                    void *restrict      value)
{
    assert(NULL != sinc->values || NULL == value);
    assert((sinc->result && sinc->initial_value) || (!sinc->result && !sinc->initial_value));
    assert(sinc->counts);

    const size_t sizeof_shep_value_part = sinc->sizeof_shep_value_part;
    // const size_t sizeof_shep_count_part = sinc->sizeof_shep_count_part;
    const size_t sizeof_value = sinc->sizeof_value;

    qthread_shepherd_id_t     shep_id   = qthread_shep();
    const qthread_worker_id_t worker_id = qthread_readstate(CURRENT_WORKER);

    if (NULL != value) {
        const size_t shep_offset   = shep_id * sizeof_shep_value_part;
        const size_t worker_offset = worker_id * sizeof_value;
        void        *values        = (uint8_t *)sinc->values + shep_offset + worker_offset;

        sinc->op(values, value);
    }

    do {
        aligned_t *X = &sinc->counts[shep_id].c;
        aligned_t  x = *X;
        aligned_t  y;
        while (sinc->counts[shep_id].c == 0) {
            shep_id++;
            shep_id *= (shep_id < num_sheps);
        }
        if (x > SNZI_ONE) {
            y = x - 1;
        } else if (x == SNZI_ONE) {
            y = 0;
        } else {
            // shep_id has insufficient counts
            continue;
        }
        // printf("depart c=%i (%i)\n", (int)x, (int)(count_offset));
        assert(x >= SNZI_ONE);  // search for a good X?
        if (qthread_cas(X, x, y) == x) {
            if (x == SNZI_ONE) {
                x = sinc->remaining;
                // printf("snzi_depart x = %i (%p)\n", (int)x, sinc);
                assert(x >= 1);
                if (qthread_incr(&sinc->remaining, -1) == 1) {
                    qt_sinc_internal_collate(sinc);
                    return;
                }
                // printf("            x = %i (%p)\n", (int)x, sinc);
            }
            // qthread_cas(&sinc->remaining, x, x-1);
            return;
        }
    } while (1);
}

void qt_sinc_wait(qt_sinc_t *restrict sinc,
                  void *restrict      target)
{
    qthread_syncvar_readFF(NULL, &sinc->ready);

    if (target) {
        assert(sinc->sizeof_value > 0);
        memcpy(target, sinc->result, sinc->sizeof_value);
    }
}

/* vim:set expandtab: */
