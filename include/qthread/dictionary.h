#ifndef QT_DICTIONARY_H
#define QT_DICTIONARY_H

#include <stdlib.h>
//#include <qthread_innards.h>
//#include <56reader-rwlock.h>

#ifdef __cplusplus
extern "C" {
#endif

//#define DELETE_SUPPORT
#define ERROR ((void*)(-1))
#define NR_BUCKETS (10000)


struct tlrw_lock;

typedef int (*key_equals)(void*, void*);
typedef int (*hashcode)(void*);

typedef struct list_entry{
	void* value;
	void* key;
	struct list_entry* next;
} list_entry;

typedef struct {
	key_equals op_equals;
	hashcode op_hash;
	list_entry** content;
	#ifdef DELETE_SUPPORT
	struct tlrw_lock* lock;
	#endif
} qt_dictionary;

typedef struct {
	qt_dictionary* dict;
	list_entry* crt;
	int bkt;
} qt_dictionary_iterator;

qt_dictionary* qt_dictionary_create(key_equals eq, hashcode hash);
void qt_dictionary_destroy(qt_dictionary* d);

/*
	inserts a key, value pair in the dictionary
	returns void*:
			ADDR - address of item found in the hashmap after the current put
			NULL - if the insert failed because of an error
			
*/
void* qt_dictionary_put(qt_dictionary* dict, void* key, void* value);

/*
	inserts a key, value pair in the dictionary
	returns void*:
			ADDR - address of item found in the hashmap after the current put
			(if "key" was present in the dictionary, the address of the old
			entry is returned, as opposed to "value")
			NULL - if the insert failed because of an error
			
*/
void* qt_dictionary_put_if_absent(qt_dictionary* dict, void* key, void* value);

/*
	gets a value from the dictionary for a given key
	returns:
			item - if get was successful and 	
				item was present in the dictionary
			NULL - if item was not present in the dictionary or error
			
*/
void* qt_dictionary_get(qt_dictionary* dict, void* key);

/*
	removes a key,value pair from the dictionary
	returns:
			item - if the item was present and successfully removed
			NULL - if the item identified by key was not in the hash
			
*/
void* qt_dictionary_delete(qt_dictionary* dict, void* key);


/*
	creates a new iterator on the dictionary dict
	returns:
			addr - address of new iterator if creation was successful
			ERROR - if an error occurred
	
*/
qt_dictionary_iterator* qt_dictionary_iterator_create(qt_dictionary* dict);

/*
	destroys the iterator it
	
*/
void qt_dictionary_iterator_destroy(qt_dictionary_iterator* it);

/*
	advances the iterator and retrieves the next entry
	returns:
			addr - address of the next entry in the hash, if one was found
			NULL -  if there are no more elements in hash
			ERROR -  if an error occurred (e.g. iterator was destroyed)
	Note: For handling concurrency, if an item was deleted while using the iterator,
		  the behavior is undefined (can return an error, signal empty hash or 
		  correctly return the next entry)
*/
list_entry* qt_dictionary_iterator_next(qt_dictionary_iterator* it);

/*
	retrieves the curent entry in iterator it
	returns:
			addr - address of the next entry in the hash, if one was found
			NULL -  if there are no more elements in hash
			ERROR -  if an error occurred (e.g. iterator was destroyed)
	Note: For handling concurrency, if an item was deleted while using the iterator,
		  the behavior is undefined (can return an error, signal empty hash or 
		  correctly return the next entry)
*/
list_entry* qt_dictionary_iterator_get(const qt_dictionary_iterator* it);

/*
	creates a new iterator, pointing to the item past the end of the list (which is NULL)
	returns:
		addr - address of the newly created iterator
		NULL - if dictionary is null or not allocated properly
*/
qt_dictionary_iterator* qt_dictionary_end(qt_dictionary* dict);

/*
	tests equality for two iterators
	requires that they are both walking the same dictionaries and are located on the same positions
*/
int qt_dictionary_iterator_equals(qt_dictionary_iterator*a, qt_dictionary_iterator* b);


/*
 * displays debugging info for dictionary buckets 
 */ 
void qt_dictionary_printbuckets(qt_dictionary* dict);


#ifdef __cplusplus
}
#endif

#endif //QT_DICTIONARY_H


