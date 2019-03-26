#include <qthread/qthread.h>
#include <qthread/tls.h>

#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

/* Obtain a backtrace and print it to stdout. */
void
print_trace (void)
{
  void *array[10];
  size_t size;
  char **strings;
  size_t i;

  size = backtrace (array, 10);
  strings = backtrace_symbols (array, size);

  fprintf (stderr, "Obtained %zd stack frames.\n", size);

  for (i = 0; i < size; i++)
     fprintf (stderr, "%s\n", strings[i]);

  free (strings);
}

struct qt_keypair_t {
	qthread_key_t key;
	void *value;
	void (*destructor)(void*);
};
typedef struct qt_keypair_t qt_keypair_t;

struct TLS {
	int initialized;
	long nkeys;
	qt_keypair_t key_pair[];
};
typedef struct TLS TLS;

int qthread_key_create(qthread_key_t *key, void (*destructor)(void*))
{
	int i;
	int size = qthread_size_tasklocal();
	size = 8192;
	TLS *t = (TLS *)qthread_get_tasklocal(size);
	qt_keypair_t *p;
	//print_trace();
	p = &t->key_pair[t->nkeys];
	t->nkeys++;
        *key = (qthread_key_t)t->nkeys;
	fprintf(stderr, "%s: t->initialized %d t->nkeys %d key %p destructor %p", __func__, t->initialized, t->nkeys, *key, destructor);
        p->key = (qthread_key_t)t->nkeys;
	fprintf(stderr, " p->key %p\n", p->key); 
	p->value = NULL;
	p->destructor = destructor;
	return 0;
}

int qthread_key_delete(qthread_key_t key)
{
	long i;
	int size = qthread_size_tasklocal();
	size = 8192;
	TLS *t = (TLS *)qthread_get_tasklocal(size);
	qt_keypair_t *p;
	//print_trace();
	for(i = 0; i < t->nkeys; i++) {	
 		p = &t->key_pair[i];
		fprintf(stderr, "%s: key %p", __func__, key); 
		fprintf(stderr, " p->key %p\n", p->key); 
		if(p->key == key) {
			p->key = 0;
			p->destructor(p->value);
			return 0;
		}
	}
	return 1;
}

void *qthread_getspecific(qthread_key_t key)
{
	long i = 0;
	int size = qthread_size_tasklocal();
	size = 8192;
	TLS *t = (TLS *)qthread_get_tasklocal(size);
	qt_keypair_t *p;
	//print_trace();
	for(i = 0; i < t->nkeys; i++) {
	        p = &t->key_pair[i];
		fprintf(stderr, "%s: key %p", __func__, key); 
		fprintf(stderr, " p->key %p\n", p->key); 
		if(p->key == key)
			return p->value;
        }
	return NULL;
}

int qthread_setspecific(qthread_key_t key, const void *value)
{
	long i = 0;
	int size = qthread_size_tasklocal();
	size = 8192;
	TLS *t = (TLS *)qthread_get_tasklocal(size);
	qt_keypair_t *p;
        if(key == NULL)
		return 1;
	for(i = 0; i < t->nkeys; i++) {
	//print_trace();
	p = &t->key_pair[i];
	fprintf(stderr, "%s: p->key %p key %p\n", __func__, p->key, key); 
		if(p->key == key) {
			p->value = (void*)value;
			return 0;
		}
	}
	return 1;
}
