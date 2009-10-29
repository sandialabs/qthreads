#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <assert.h>
#include <qt_hash.h>
#include <stdlib.h> /* for calloc() */
#include <stdio.h> /* debugging */
#include <pthread.h>
#include <qthread/qthread.h>
#ifdef HAVE_UNORDERED_MAP
# include <unordered_map>
typedef std::unordered_map<void*,void*> qtmap;
#elif defined(HAVE_TR1_UNORDERED_MAP)
# include <tr1/unordered_map>
typedef std::tr1::unordered_map<void*,void*> qtmap;
#else /* unordered_map */
# ifdef __GNUC__
#  if __GNUC__ < 3
#   include <hash_map.h>
namespace extension { using ::hash_map; }; // inherit globals
#  else
#   include <backward/hash_map>
#   if __GNUC__ == 3 && __GNUC_MINOR__ == 0
namespace extension = std; // GCC 3.0
#   else
namespace extension = ::__gnu_cxx; // GCC 3.1 and later
#   endif
#  endif
#  include <ext/hash_map>
# else
#  include <hash_map>
# endif
typedef std::hash_map<void*, void*> qtmap;
#endif /* unordered_map */

struct qt_hash_s {
    qtmap h;
    pthread_mutex_t lock;
    pthread_t lock_owner;
};

qt_hash qt_hash_create()
{
    qt_hash ret = new qt_hash_s;
    if (ret) {
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (pthread_mutex_init(&(ret->lock), &attr) != 0) {
	    delete ret;
	    ret = NULL;
	}
	pthread_mutexattr_destroy(&attr);
    }
    return ret;
}

void qt_hash_destroy(qt_hash h)
{
    /* better hope nobody's fooling with it! */
    if (h) {
	pthread_mutex_destroy(&(h->lock));
	delete h;
    }
}


/* This function destroys the hash and applies the given deallocator function
 * to each value stored in the hash */
void qt_hash_destroy_deallocate(qt_hash h, qt_hash_deallocator_fn f)
{
    if (h) {
	qtmap::iterator iter;
	pthread_mutex_lock(&(h->lock));
	for (iter = h->h.begin();
		iter != h->h.end();
		iter++) {
	    assert(iter->second != NULL);
	    f((void*)(iter->second));
	}
	pthread_mutex_unlock(&(h->lock));
	qt_hash_destroy(h);
    }
}

void *qt_hash_put(qt_hash h, void *const key, void *value)
{
    if (h) {
	pthread_mutex_lock(&(h->lock));
	h->h[key] = value;
	pthread_mutex_unlock(&(h->lock));
	return value;
    } else {
	return NULL;
    }
}

void *qt_hash_remove(qt_hash h, void *const key)
{
    if (h) {
	void * ret;
	qtmap::iterator iter;
	pthread_mutex_lock(&(h->lock));
	iter = h->h.find(key);
	if (iter != h->h.end()) {
	    h->h.erase(iter);
	    //h->h.erase(key);
	}
	pthread_mutex_unlock(&(h->lock));
	return ret;
    }
    return NULL;
}

void *qt_hash_get(qt_hash h, void *const key)
{
    if (h) {
	void * ret;
	qtmap::iterator iter;
	pthread_mutex_lock(&(h->lock));
	iter = h->h.find(key);
	if (iter == h->h.end()) {
	    ret = NULL;
	} else {
	    assert(iter->first == key);
	    ret = iter->second;
	}
	pthread_mutex_unlock(&(h->lock));
	return ret;
    } else {
	return NULL;
    }
}

void qt_hash_callback(qt_hash h, qt_hash_callback_fn f, void *arg)
{
    if (h) {
	qtmap::iterator iter;
	pthread_mutex_lock(&(h->lock));
	for (iter = h->h.begin();
		iter != h->h.end();
		iter++) {
	    f(iter->first, iter->second, arg);
	}
	pthread_mutex_unlock(&(h->lock));
    }
}

size_t qt_hash_count(qt_hash h)
{
    size_t s;
    pthread_mutex_lock(&(h->lock));
    s = h->h.size();
    pthread_mutex_lock(&(h->lock));
    return s;
}

void qt_hash_lock(qt_hash h)
{
    if (h) {
	pthread_mutex_lock(&(h->lock));
    }
}

void qt_hash_unlock(qt_hash h)
{
    if (h) {
	pthread_mutex_unlock(&(h->lock));
    }
}
