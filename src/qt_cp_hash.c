#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifndef HAVE_CPROPS
# error This file requires cprops.
#endif
#include "qt_hash.h"

#ifndef QT_HASH_CAST
#define QT_HASH_CAST qt_key_t
#endif

qt_hash qt_hash_create(int needSync)
{
    if (needSync == 0) {
	return cp_hashlist_create_by_mode(COLLECTION_MODE_NOSYNC, 100, cp_hash_addr, cp_hash_compare_addr);
    } else {
	return cp_hashlist_create(100, cp_hash_addr, cp_hash_compare_addr);
    }
}

void qt_hash_destroy(qt_hash h)
{
    cp_hashlist_destroy(h);
}


/* This function destroys the hash and applies the given deallocator function
 * to each value stored in the hash */
void qt_hash_destroy_deallocate(qt_hash h, qt_hash_deallocator_fn f)
{
    cp_hashlist_destroy_custom(h, NULL, f);
}

void *qt_hash_put(qt_hash h, const qt_key_t key, void *value)
{
    return cp_hashlist_append(h, (void*)key, value);
}

void *qt_hash_put_locked(qt_hash h, const qt_key_t key, void *value)
{
    return cp_hashlist_append(h, (void*)key, value);
}

void *qt_hash_remove(qt_hash h, const qt_key_t key)
{
    return cp_hashlist_remove(h, (void*)key);
}

void *qt_hash_remove_locked(qt_hash h, const qt_key_t key)
{
    return cp_hashlist_remove(h, (void*)key);
}

void *qt_hash_get(qt_hash h, const qt_key_t key)
{
    return cp_hashlist_get(h, (void*)key);
}

void *qt_hash_get_locked(qt_hash h, const qt_key_t key)
{
    return cp_hashlist_get(h, (void*)key);
}

void qt_hash_callback(qt_hash h, qt_hash_callback_fn f, void *arg)
{
    cp_hashlist_callback(h, f, arg);
}

size_t qt_hash_count(qt_hash h)
{
    return cp_hashlist_item_count(h);
}

void qt_hash_lock(qt_hash h)
{
    cp_hashlist_wrlock(h);
}

void qt_hash_unlock(qt_hash h)
{
    cp_hashlist_unlock(h);
}
