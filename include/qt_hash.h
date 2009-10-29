#ifndef QT_HASH_H
#define QT_HASH_H

#include <stddef.h> /* for size_t (according to C89) */
//#include <stdint.h> /* for uintptr_t (according to C99) */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct qt_hash_s *qt_hash;
typedef void (*qt_hash_deallocator_fn)(void*);
typedef void (*qt_hash_callback_fn)(void *, void *, void *);

qt_hash qt_hash_create();
void qt_hash_destroy(qt_hash h);
void qt_hash_destroy_deallocate(qt_hash h, qt_hash_deallocator_fn f);
void *qt_hash_put(qt_hash h, void *key, void *value);
void *qt_hash_remove(qt_hash h, void *key);
void *qt_hash_get(qt_hash h, void *key);
size_t qt_hash_count(qt_hash h);
void qt_hash_callback(qt_hash h, qt_hash_callback_fn f, void *arg);

void qt_hash_lock(qt_hash h);
void qt_hash_unlock(qt_hash h);

#ifdef __cplusplus
}
#endif

#endif
