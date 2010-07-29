#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifndef HAVE_JUDY
# error This file requires Judy.
#else
# include <Judy.h>
#endif
#include "qt_hash.h"
#include "qthread_asserts.h"
#include "qt_atomics.h"

#ifndef QT_HASH_CAST
#define QT_HASH_CAST qt_key_t
#endif

struct qt_hash_s {
    Pvoid_t hash;
    QTHREAD_FASTLOCK_TYPE *lock;
};

qt_hash qt_hash_create(int needSync)
{
    qt_hash ret = malloc(sizeof(struct qt_hash_s));
    if (ret) {
	if (needSync) {
	    ret->lock = malloc(sizeof(QTHREAD_FASTLOCK_TYPE));
	    QTHREAD_FASTLOCK_INIT_PTR(ret->lock);
	} else {
	    ret->lock = NULL;
	}
	ret->hash = NULL;
    }
    return ret;
}

void qt_hash_destroy(qt_hash h)
{
    Word_t a;
    assert(h);
    if (h->lock) {
	QTHREAD_FASTLOCK_DESTROY_PTR(h->lock);
	free(h->lock);
    }
    JLFA(a, h->hash);
}


/* This function destroys the hash and applies the given deallocator function
 * to each value stored in the hash */
void qt_hash_destroy_deallocate(qt_hash h, qt_hash_deallocator_fn f)
{
    Word_t *pv=NULL;
    Word_t Index = 0;
    assert(h);
    if (h->lock) {
	QTHREAD_FASTLOCK_LOCK(h->lock);
    }
    JLF(pv, h->hash, Index);
    while (pv != NULL) {
	f(pv);
	JLN(pv, h->hash, Index);
    }
    if (h->lock) {
	QTHREAD_FASTLOCK_UNLOCK(h->lock);
    }
    JLFA(Index, h->hash);
}

void *qt_hash_put(qt_hash h, const qt_key_t key, void *value)
{
    Word_t *PValue;
    assert(h);
    if (h->lock) {
	QTHREAD_FASTLOCK_LOCK(h->lock);
    }
    JLI(PValue, h->hash, (Word_t)key);
    if (PValue != PJERR) {
	*PValue = (Word_t)value;
    } else {
	PValue = NULL;
    }
    if (h->lock) {
	QTHREAD_FASTLOCK_UNLOCK(h->lock);
    }
    return PValue;
}

void *qt_hash_put_locked(qt_hash h, const qt_key_t key, void *value)
{
    Word_t *PValue;
    assert(h);
    JLI(PValue, h->hash, (Word_t)key);
    if (PValue != PJERR) {
	*PValue = (Word_t)value;
	return PValue;
    } else {
	return NULL;
    }
}

void *qt_hash_remove(qt_hash h, const qt_key_t key)
{
    Word_t *PValue;
    Word_t ret;
    int rc;
    assert(h);
    if (h->lock) {
	QTHREAD_FASTLOCK_LOCK(h->lock);
    }
    JLG(PValue, h->hash, (Word_t)key);
    ret = *PValue;
    JLD(rc, h->hash, (Word_t)key);
    if (h->lock) {
	QTHREAD_FASTLOCK_UNLOCK(h->lock);
    }
    return (void*)ret;
}

void *qt_hash_remove_locked(qt_hash h, const qt_key_t key)
{
    Word_t *PValue;
    Word_t ret;
    int rc;
    assert(h);
    JLG(PValue, h->hash, (Word_t)key);
    ret = *PValue;
    JLD(rc, h->hash, (Word_t)key);
    return (void*)ret;
}

void *qt_hash_get(qt_hash h, const qt_key_t key)
{
    Word_t *PValue;
    Word_t ret;
    assert(h);
    if (h->lock) {
	QTHREAD_FASTLOCK_LOCK(h->lock);
    }
    JLG(PValue, h->hash, (Word_t)key);
    ret = *PValue;
    if (h->lock) {
	QTHREAD_FASTLOCK_UNLOCK(h->lock);
    }
    return (void*)ret;
}

void *qt_hash_get_locked(qt_hash h, const qt_key_t key)
{
    Word_t *PValue;
    assert(h);
    JLG(PValue, h->hash, (Word_t)key);
    if (PValue == PJERR || PValue == NULL) {
	return NULL;
    } else {
	return (void*)*PValue;
    }
}

void qt_hash_callback(qt_hash h, qt_hash_callback_fn f, void *arg)
{
    Word_t *pv=NULL;
    Word_t Index = 0;
    assert(h);
    if (h->lock) {
	QTHREAD_FASTLOCK_LOCK(h->lock);
    }
    JLF(pv, h->hash, Index);
    while (pv != NULL) {
	f((qt_key_t)Index, (void*)pv, arg);
	JLN(pv, h->hash, Index);
    }
    if (h->lock) {
	QTHREAD_FASTLOCK_UNLOCK(h->lock);
    }
}

size_t qt_hash_count(qt_hash h)
{
    Word_t ct;
    assert(h);
    if (h->lock) {
	QTHREAD_FASTLOCK_LOCK(h->lock);
    }
    JLC(ct, h->hash, 0, -1);
    if (h->lock) {
	QTHREAD_FASTLOCK_UNLOCK(h->lock);
    }
    return ct;
}

void qt_hash_lock(qt_hash h)
{
    assert(h);
    if (h->lock) {
	QTHREAD_FASTLOCK_LOCK(h->lock);
    }
}

void qt_hash_unlock(qt_hash h)
{
    assert(h);
    if (h->lock) {
	QTHREAD_FASTLOCK_UNLOCK(h->lock);
    }
}
