#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "qthread_asserts.h"
#include <qt_hash.h>
#include <stdlib.h> /* for calloc() */
#include <stdio.h> /* debugging */
#include <pthread.h>
#include <qthread/qthread.h>
#ifdef HAVE_UNORDERED_MAP
# include <unordered_map>
typedef std::unordered_map<qt_key_t,void*> qtmap;
#elif defined(HAVE_TR1_UNORDERED_MAP)
# include <tr1/unordered_map>
typedef std::tr1::unordered_map<qt_key_t,void*> qtmap;
#else /* unordered_map */
# ifdef __GNUC__
#  if __GNUC__ < 3
#   include <hash_map.h>
namespace extension { using ::hash_map; }; // inherit globals
#  else
#  include <ext/hash_map>
//#   include <backward/hash_map>
#   if __GNUC__ == 3 && __GNUC_MINOR__ == 0
namespace extension = std; // GCC 3.0
typedef std::hash_map<qt_key_t, void*> qtmap;
#   else
namespace extension = ::__gnu_cxx; // GCC 3.1 and later
typedef __gnu_cxx::hash_map<uintptr_t, void*> qtmap;
#    define QT_HASH_CAST uintptr_t
#   endif
#  endif
# else
#  include <hash_map>
typedef std::hash_map<qt_key_t, void*> qtmap;
# endif
#endif /* unordered_map */

#ifndef QT_HASH_CAST
#define QT_HASH_CAST qt_key_t
#endif

struct qt_hash_s {
    qtmap h;
    pthread_mutex_t lock;
    pthread_t lock_owner;
};

qt_hash qt_hash_create()
{
    qt_hash ret = new qt_hash_s;
    if (ret) {
	if (pthread_mutex_init(&(ret->lock), NULL) != 0) {
	    delete ret;
	    ret = NULL;
	}
    }
    return ret;
}

void qt_hash_destroy(qt_hash h)
{
    /* better hope nobody's fooling with it! */
    assert(h);
    pthread_mutex_destroy(&(h->lock));
    delete h;
}


/* This function destroys the hash and applies the given deallocator function
 * to each value stored in the hash */
void qt_hash_destroy_deallocate(qt_hash h, qt_hash_deallocator_fn f)
{
    assert(h);
    pthread_mutex_lock(&(h->lock));
    for (qtmap::iterator iter(h->h.begin());
	    iter != h->h.end();
	    iter++) {
	assert(iter->second != NULL);
	f((void*)(iter->second));
    }
    pthread_mutex_unlock(&(h->lock));
    qt_hash_destroy(h);
}

void *qt_hash_put(qt_hash h, const qt_key_t key, void *value)
{
    assert(h);
    pthread_mutex_lock(&(h->lock));
    h->h[(QT_HASH_CAST)key] = value;
    pthread_mutex_unlock(&(h->lock));
    return value;
}

void *qt_hash_put_locked(qt_hash h, const qt_key_t key, void *value)
{
    assert(h);
    h->h[(QT_HASH_CAST)key] = value;
    return value;
}

void *qt_hash_remove(qt_hash h, const qt_key_t key)
{
    void * ret = NULL;
    assert(h);
    pthread_mutex_lock(&(h->lock));
    ret = qt_hash_remove_locked(h,key);
    pthread_mutex_unlock(&(h->lock));
    return ret;
}

void *qt_hash_remove_locked(qt_hash h, const qt_key_t key)
{
    void * ret = NULL;
    assert(h);
    {
	qtmap::iterator iter(h->h.find((QT_HASH_CAST)key));
	if (iter != h->h.end()) {
	    ret = iter->second;
	    h->h.erase(iter);
	}
    }
    return ret;
}

void *qt_hash_get(qt_hash h, const qt_key_t key)
{
    void * ret;
    assert(h);
    pthread_mutex_lock(&(h->lock));
    ret = qt_hash_get_locked(h,key);
    pthread_mutex_unlock(&(h->lock));
    return ret;
}

void *qt_hash_get_locked(qt_hash h, const qt_key_t key)
{
    assert(h);
    {
	qtmap::iterator iter(h->h.find((QT_HASH_CAST)key));
	if (iter == h->h.end()) {
	    return NULL;
	} else {
	    assert(iter->first == (QT_HASH_CAST)key);
	    return iter->second;
	}
    }
}

void qt_hash_callback(qt_hash h, qt_hash_callback_fn f, void *arg)
{
    assert(h);
    pthread_mutex_lock(&(h->lock));
    for (qtmap::iterator iter(h->h.begin());
	    iter != h->h.end();
	    iter++) {
	f((void*)iter->first, iter->second, arg);
    }
    pthread_mutex_unlock(&(h->lock));
}

size_t qt_hash_count(qt_hash h)
{
    size_t s;
    assert(h);
    pthread_mutex_lock(&(h->lock));
    s = h->h.size();
    pthread_mutex_unlock(&(h->lock));
    return s;
}

void qt_hash_lock(qt_hash h)
{
    assert(h);
    pthread_mutex_lock(&(h->lock));
}

void qt_hash_unlock(qt_hash h)
{
    assert(h);
    pthread_mutex_unlock(&(h->lock));
}
