//#include <qthread.h> // for CAS operations
#include <qthread/dictionary.h>
#include <assert.h>
#include <stdio.h>
#include <qthread/qthread.h>
#include <qthread_innards.h>
#include <56reader-rwlock.h>


/* Prototype should NOT go in header, we don't want it public*/
void* qt_dictionary_put_helper(qt_dictionary* dict, void* key, void* value, 
			char put_type);


#ifdef DELETE_SUPPORT

#define rlock(a) \
	{ \
		int id = qthread_worker_unique(NULL); 		\
		/* this if protects against crash when using after qthreads was finalized*/ \
		/* if qthreads is finalized, use sequential semantics */ \
		if (id<MAX_READERS) 						\
			rwlock_rdlock(a, id); 					\
	}	

#define runlock(a) \
	{ \
		int id = qthread_worker_unique(NULL);		\
		if (id<MAX_READERS) 						\
			rwlock_rdunlock(a, id);					\
	}		
#define wlock(a) \
	{ \
		int id = qthread_worker_unique(NULL); 		\
		if (id<MAX_READERS) 						\
			rwlock_wrlock(a, id); 					\
	}	

#define wunlock(a) \
	{ \
		int id = qthread_worker_unique(NULL);		\
		if (id<MAX_READERS) 						\
			rwlock_wrunlock(a);						\
	}
#define rwlinit(a) \
	{ \
		/* fail instead of silent accepting more readers than we can handle*/ \
		/* setting the number of workers dynamically to be larger than MAX_READERS*/ \
		/* is not checked, so don't do it! */ \
		assert(qthread_num_workers()<MAX_READERS);		\
		a = (rwlock_t*) malloc (sizeof(rwlock_t));		\
		rwlock_init(a);									\
	}
#define rwlfree(a) \
	{ \
		free(a);								\
	}
#else
	#define rlock(a)
	#define runlock(a) 
	#define wlock(a) 
	#define wunlock(a) 
	#define rwlinit(a)
	#define rwlfree(a) 
#endif			






qt_dictionary* qt_dictionary_create(key_equals eq, hashcode hash) {
	qt_dictionary* ret = (qt_dictionary*) malloc (sizeof(qt_dictionary));
	ret -> op_equals = eq;
	ret -> op_hash = hash;
	ret -> content = (list_entry**) malloc ( NR_BUCKETS * sizeof(list_entry*) );
	
	int i;
	for (i=0; i< NR_BUCKETS; i++){
		ret->content[i] = NULL;
	}
	
	rwlinit(ret -> lock);
	return ret;
}

void qt_dictionary_destroy(qt_dictionary* d) {
	int i;
	for (i=0; i< NR_BUCKETS; i++) {
		list_entry* tmp, *top = d -> content[i];
		while (top != NULL){
			tmp = top;
			top = top -> next;
			free(tmp);
		}
	}
	free(d -> content);
	rwlfree(d -> lock);
	free(d);
}

#define PUT_ALWAYS 0
#define PUT_IF_ABSENT 1

void* qt_dictionary_put_helper(qt_dictionary* dict, void* key, void* value, 
			char put_type) {
	int bucket = ( dict -> op_hash(key) ) ;
	if(bucket < 0) bucket = -bucket;
	bucket = bucket % NR_BUCKETS;
	

	
	
	list_entry** crt = &(dict -> content[bucket]);
	assert(!(crt == NULL || dict -> content == NULL));
	list_entry* walk = *crt, *toadd = NULL;
	while(1){
		while(walk != NULL){
			if(dict -> op_equals(walk -> key, key)){
				if(toadd != NULL) free(toadd);
				
				if(put_type == PUT_ALWAYS){
					void **crt_val_adr = &(walk -> value);
					void *crt_val = walk->value;
					while( (uint64_t)(qthread_cas64( crt_val_adr, \
										   crt_val, value )) != (uint64_t)crt_val ){
						//try until succeeding to add
						crt_val = walk->value;
					}
				}
				rlock(dict -> lock);
				return walk->value;
			}
			crt = &(walk -> next);
			walk = walk -> next;
		}
		if(toadd == NULL){
			toadd = (list_entry*) malloc (sizeof(list_entry));
			if(toadd == NULL) {
				runlock(dict -> lock);
				return NULL;
			}
			toadd -> key = key;
			toadd -> value = value;
			toadd -> next = NULL;
		}
		void* code = qthread_cas64( crt, NULL, toadd );
		if(code == NULL) {//succeeded adding
			runlock(dict -> lock);
			return value;
		}
		walk = *crt;
	}
	
	runlock(dict -> lock);
	return NULL;
}

void* qt_dictionary_put(qt_dictionary* dict, void* key, void* value) {
	return qt_dictionary_put_helper(dict, key, value, PUT_ALWAYS);
}

void* qt_dictionary_put_if_absent(qt_dictionary* dict, void* key, void* value) {
	return qt_dictionary_put_helper(dict, key, value, PUT_IF_ABSENT);
}

void* qt_dictionary_get(qt_dictionary* dict, void* key) {
	int bucket = ( dict -> op_hash(key) ) % NR_BUCKETS;
	
	rlock(dict -> lock);
	
	list_entry* walk = dict -> content[bucket];
	while(walk != NULL){
		if(dict -> op_equals(walk -> key, key)){
			runlock(dict -> lock);
			return walk -> value;
		}
		walk = walk -> next;
	}
	
	runlock(dict -> lock);
	return NULL;
}

void* qt_dictionary_delete(qt_dictionary* dict, void* key) {
	void* to_ret = NULL, *to_free = NULL;
	int bucket = ( dict -> op_hash(key) ) % NR_BUCKETS;
	
	wlock(dict -> lock);
	
	list_entry* walk = dict -> content[bucket];
	if(walk == NULL) ;//cannot remove an element not present in hash
	else if(dict -> op_equals(walk -> key, key)){
		//remove list head
		to_free = walk;
		to_ret = walk -> value;
		dict -> content[bucket] = walk -> next;
		free(to_free);
	}
	else while(walk -> next != NULL){
		if(dict -> op_equals(walk -> next-> key, key)){
			to_free = walk -> next;
			to_ret = walk -> next -> value;
			walk -> next = walk -> next -> next;
			free(to_free);
			break;
		}
		walk = walk -> next;
	}
	
	wunlock(dict -> lock);
	return to_ret;
}

qt_dictionary_iterator* qt_dictionary_iterator_create(qt_dictionary* dict) {
	if(dict -> content == NULL){
		return ERROR;
	}
	qt_dictionary_iterator* it = (qt_dictionary_iterator*) malloc (sizeof(qt_dictionary_iterator));
	it -> dict = dict;
	it -> bkt = -1;
	it -> crt = NULL;
	return it;
}

void qt_dictionary_iterator_destroy(qt_dictionary_iterator* it) {
	if(it == NULL) return;
	free(it);
}

list_entry* qt_dictionary_iterator_next(qt_dictionary_iterator* it) {
	if(it == NULL || it -> dict -> content == NULL){
		return ERROR;
	}
	//First call to next: search for the first non-empty bucket
	if(it -> bkt == -1){
		int i;
		for(i = 0; i < NR_BUCKETS; i++)
			if(it->dict->content[i] != NULL){
				it -> bkt = i;
				it -> crt = it->dict->content[i];
				return it -> crt;
			}
	}
	else{
		// start searching for next element starting with the last returned element
		list_entry* walk = it -> crt;
		// if item was deleted or there are no more elements in the list, return NULL
		if(walk == NULL){
			it -> bkt = -1;
			return ERROR;
		}
		
		walk = walk -> next;
		
		//search all buckets in dictionary
		while (it -> bkt < NR_BUCKETS){
			if(walk != NULL){
				it -> crt = walk;
				return it -> crt;
			}
			it -> bkt++;
			if(it -> bkt < NR_BUCKETS)
				walk = it -> dict -> content[it -> bkt];
		}
		// if dictionary has no more elements return NULL
		it -> crt = NULL;
		return NULL;
	}
	assert(0);
	return NULL;
}

list_entry* qt_dictionary_iterator_get(const qt_dictionary_iterator* it) {		
	if(it == NULL || it -> dict == NULL || it -> dict -> content == NULL){
		printf(" Inside dictionary get, found NULL, will return ERROR\n" );
		return ERROR;
	}
	
	return it->crt;
}

qt_dictionary_iterator* qt_dictionary_end(qt_dictionary* dict) {
	if(dict == NULL || dict -> content == NULL)
		return NULL;
	qt_dictionary_iterator* it = qt_dictionary_iterator_create(dict);
	it->crt = NULL;
	it->bkt = NR_BUCKETS;
	it->dict = dict;
	return it;
}

int qt_dictionary_iterator_equals(qt_dictionary_iterator*a, qt_dictionary_iterator* b) {
	if (a == NULL || b == NULL)
		return a == b;
	return (a -> crt == b ->crt) && (a -> dict == b -> dict) && (a -> bkt == b -> bkt);
}

void qt_dictionary_printbuckets(qt_dictionary* dict) {
	for(int bucket = 0; bucket < NR_BUCKETS; bucket++) {
		int no_el = 0;
		list_entry** crt = &(dict -> content[bucket]);
		if (crt != NULL && dict -> content != NULL) {
			list_entry* walk = *crt;
			while(walk != NULL){
				no_el++;
				crt = &(walk -> next);
				walk = walk -> next;
			}
			printf("Bucket %d has %d elements.\n", bucket, no_el);
			assert(no_el < 2);
			
		}
	}
}
