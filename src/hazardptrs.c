#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* The API */
#include "qthread/qthread.h"

/* System Headers */
#include <stdlib.h>            /* for qsort(), malloc() and abort() */

/* Internal Headers */
#include "qt_hazardptrs.h"
#include "qt_shepherd_innards.h"
#include "qthread_innards.h"

static pthread_key_t ts_hazard_ptrs;
static uintptr_t    *hzptr_list     = NULL;
static aligned_t     hzptr_list_len = 0;

static void hazardptr_internal_teardown(void)
{
    pthread_key_delete(ts_hazard_ptrs);
    {
        while (hzptr_list != NULL) {
            uintptr_t *hzptr_tmp = hzptr_list;
            hzptr_list = (uintptr_t *)hzptr_tmp[HAZARD_PTRS_PER_SHEP];
            free(hzptr_tmp);
        }
    }
}

void INTERNAL initialize_hazardptrs(void)
{
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
    for (qthread_shepherd_id_t i = 0; i < qthread_num_shepherds(); ++i) {
        for (qthread_worker_id_t j = 0; j < qlib->nworkerspershep; ++j) {
            memset(qlib->shepherds[i].workers[j].hazard_ptrs, 0, sizeof(uintptr_t) * HAZARD_PTRS_PER_SHEP);
            memset(&qlib->shepherds[i].workers[j].hazard_free_list, 0, sizeof(hazard_freelist_t));
        }
    }
#else
    for (qthread_shepherd_id_t i = 0; i < qthread_num_shepherds(); ++i) {
        memset(qlib->shepherds[i].hazard_ptrs, 0, sizeof(uintptr_t) * HAZARD_PTRS_PER_SHEP);
        memset(&qlib->shepherds[i].hazard_free_list, 0, sizeof(hazard_freelist_t));
    }
#endif /* ifdef QTHREAD_MULTITHREADED_SHEPHERDS */
    qassert(pthread_key_create(&ts_hazard_ptrs, NULL), 0);
    qthread_internal_cleanup(hazardptr_internal_teardown);
}

void INTERNAL hazardous_ptr(unsigned int which,
                            void        *ptr)
{
    uintptr_t *hzptrs = pthread_getspecific(ts_hazard_ptrs);

    if (hzptrs == NULL) {
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
        {
            qthread_worker_t *wkr = qthread_internal_getworker();
            if (wkr == NULL) {
                hzptrs = calloc(sizeof(uintptr_t), HAZARD_PTRS_PER_SHEP + 1);
                assert(hzptrs);
                do {
                    hzptrs[HAZARD_PTRS_PER_SHEP] = (uintptr_t)hzptr_list;
                } while (qt_cas((void **)&hzptr_list, hzptrs[HAZARD_PTRS_PER_SHEP], hzptrs)
                         != (void *)hzptrs[HAZARD_PTRS_PER_SHEP]);
                (void)qthread_incr(&hzptr_list_len, 1);
            } else {
                hzptrs = wkr->hazard_ptrs;
            }
        }
#else   /* ifdef QTHREAD_MULTITHREADED_SHEPHERDS */
        {
            qthread_shepherd_t *shep = qthread_internal_getshep();
            if (shep == NULL) {
                hzptrs = calloc(sizeof(uintptr_t), HAZARD_PTRS_PER_SHEP + 1);
                assert(hzptrs);
                do {
                    hzptrs[HAZARD_PTRS_PER_SHEP] = (uintptr_t)hzptr_list;
                } while (qt_cas(&hzptr_list, hzptrs[HAZARD_PTRS_PER_SHEP], hzptrs)
                         != (void *)hzptrs[HAZARD_PTRS_PER_SHEP]);
                (void)qthread_incr(&hzptr_list_len, 1);
            } else {
                hzptrs = shep->hazard_ptrs;
            }
        }
#endif  /* ifdef QTHREAD_MULTITHREADED_SHEPHERDS */
        pthread_setspecific(ts_hazard_ptrs, hzptrs);
    }

    assert(hzptrs);
    assert(which < HAZARD_PTRS_PER_SHEP);
    hzptrs[which] = (uintptr_t)ptr;
}

static int void_cmp(const void *a,
                    const void *b)
{
    return (*(intptr_t *)a) - (*(intptr_t *)b);
}

static int binary_search(uintptr_t *list,
                         uintptr_t  findme,
                         size_t     len)
{
    size_t max  = len;
    size_t min  = 0;
    size_t curs = max / 2;

    while (list[curs] != findme) {
        if (list[curs] > findme) {
            max = curs;
        } else if (list[curs] < findme) {
            min = curs;
        }
        if (max == min + 1) { break; }
        curs = (max + min) / 2;
    }
    return (list[curs] == findme);
}

static void hazardous_scan(hazard_freelist_t *hfl)
{
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
    const size_t num_hps = qthread_num_shepherds() * qthread_num_workers() * HAZARD_PTRS_PER_SHEP;
#else
    const size_t num_hps = qthread_num_shepherds() * HAZARD_PTRS_PER_SHEP;
#endif
    void            **plist = malloc(sizeof(void *) * (num_hps + hzptr_list_len));
    hazard_freelist_t tmpfreelist;

    assert(plist);
    do {
        tmpfreelist.count = 0;
        /* Stage 1: Collect hazardpointers */
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
        {
            qthread_shepherd_id_t i;
            for (i = 0; i < qthread_num_shepherds(); ++i) {
                for (qthread_worker_id_t j = 0; j < qlib->nworkerspershep; ++j) {
                    if (&(qlib->shepherds[i].workers[j].hazard_free_list) != hfl) {
                        memcpy(plist + (i * qlib->nworkerspershep * HAZARD_PTRS_PER_SHEP) + (j * HAZARD_PTRS_PER_SHEP),
                               qlib->shepherds[i].workers[j].hazard_ptrs,
                               sizeof(void *) * HAZARD_PTRS_PER_SHEP);
                    } else {
                        memset(plist + (i * qlib->nworkerspershep * HAZARD_PTRS_PER_SHEP) + (j * HAZARD_PTRS_PER_SHEP),
                               0,
                               sizeof(void *) * HAZARD_PTRS_PER_SHEP);
                    }
                }
            }
            uintptr_t *hzptr_tmp = hzptr_list;
            while (hzptr_tmp != NULL) {
                memcpy(plist + (i * qlib->nworkerspershep * HAZARD_PTRS_PER_SHEP),
                       hzptr_tmp,
                       sizeof(uintptr_t) * HAZARD_PTRS_PER_SHEP);
                hzptr_tmp = (uintptr_t *)hzptr_tmp[HAZARD_PTRS_PER_SHEP];
            }
        }
#else   /* ifdef QTHREAD_MULTITHREADED_SHEPHERDS */
        qthread_shepherd_id_t i;
        for (i = 0; i < qthread_num_shepherds(); ++i) {
            if (&(qlib->shepherds[i].hazard_free_list) != hfl) {
                memcpy(plist + (i * HAZARD_PTRS_PER_SHEP), qlib->shepherds[i].hazard_ptrs, sizeof(void *) * HAZARD_PTRS_PER_SHEP);
            } else {
                memset(plist + (i * HAZARD_PTRS_PER_SHEP), 0, sizeof(void *) * HAZARD_PTRS_PER_SHEP);
            }
        }
        {
            uintptr_t *hzptr_tmp = hzptr_list;
            while (hzptr_tmp != NULL) {
                memcpy(plist + (i * HAZARD_PTRS_PER_SHEP),
                       hzptr_tmp,
                       sizeof(uintptr_t) * HAZARD_PTRS_PER_SHEP);
                hzptr_tmp = (uintptr_t *)hzptr_tmp[HAZARD_PTRS_PER_SHEP];
            }
        }
#endif  /* ifdef QTHREAD_MULTITHREADED_SHEPHERDS */

        /* Stage 2: free pointers that are not in the set of hazardous pointers */
        qsort(plist, num_hps, sizeof(void *), void_cmp);
        for (size_t i = 0; i < FREELIST_DEPTH; ++i) {
            const uintptr_t ptr = (uintptr_t)hfl->freelist[i].ptr;
            if (ptr == 0) { break; }
            /* look for this ptr in the plist */
            if (binary_search((uintptr_t *)plist, ptr, num_hps)) {
                /* if found, cannot free it */
                tmpfreelist.freelist[tmpfreelist.count] = hfl->freelist[i];
                tmpfreelist.count++;
            } else {
                /* not found, therefore, we can free it */
                hfl->freelist[i].free((void *)ptr);
            }
        }
        if (tmpfreelist.count == 8) {
            /* This will ONLY happen under *extremely* heavy contention. */
            MACHINE_FENCE;
        }
    } while (tmpfreelist.count == 8);
    memcpy(&hfl->freelist, &tmpfreelist.freelist, tmpfreelist.count * sizeof(hazard_freelist_entry_t));
    hfl->count = tmpfreelist.count;
    free(plist);
}

void INTERNAL hazardous_release_node(void  (*freefunc)(void *),
                                     void *ptr)
{
#ifdef QTHREAD_MULTITHREADED_SHEPHERDS
    hazard_freelist_t *hfl = &(qthread_internal_getworker()->hazard_free_list);
#else
    hazard_freelist_t *hfl = &(qthread_internal_getshep()->hazard_free_list);
#endif
    uintptr_t *hzptrs = pthread_getspecific(ts_hazard_ptrs);

    assert(ptr != NULL);
    assert(freefunc != NULL);
    hfl->freelist[hfl->count].free = freefunc;
    hfl->freelist[hfl->count].ptr  = ptr;
    hfl->count++;
    if (hzptrs != NULL) {
        memset(hzptrs, 0, sizeof(uintptr_t) * HAZARD_PTRS_PER_SHEP);
    }
    if (hfl->count == FREELIST_DEPTH) {
        hazardous_scan(hfl);
    }
}

/* vim:set expandtab: */
