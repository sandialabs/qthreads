#ifndef _QT_CNC_CONTEXT_H_
#define _QT_CNC_CONTEXT_H_

#ifdef DEBUG_COUNTERS
#include<qthread/qloop.h>
static void init_debug_counters(size_t start, size_t stop, void* key){
    	assert(DEBUG_COUNTERS > 0);
	int* ptr = (int*)malloc( DEBUG_COUNTERS * sizeof(int) );
	for(int i=0; i<DEBUG_COUNTERS; i++)
		ptr[i] = 0;
	pthread_setspecific(*((pthread_key_t*)key), ptr);
}
static void print_debug_counters(size_t start, size_t stop, void* key){
	for(int i=0; i<DEBUG_COUNTERS; i++)
		printf("%d: Counter on position %d is %d\n", (int)start, i, ((int*)pthread_getspecific( *((pthread_key_t*)key)  ))[i]);	
}
# endif


namespace CnC {

	

	
	template< class ContextTemplate >
	context< ContextTemplate >::context():context_base()
	{
		//printf("<Context> default constructor has prerun = %d on %p\n", *prerun, this);
		prerun = (int*)malloc(sizeof(int));
		*prerun = -1;
		printf("prerun should be 1? %d\n", *prerun);
		cnc_status = STARTED;
		qthread_initialize();
		sinc = qt_sinc_create(0, NULL, NULL, 0);
		# ifdef DEBUG_COUNTERS
			//Initialize DEBUG_COUNTERS counters inside thread-local-data for each thread
			int nr_cores = qthread_num_workers();
			pthread_key_create(&key, NULL);
			qt_loop_balance(0, nr_cores, init_debug_counters, &key);
		# endif
	}

	template< class ContextTemplate >
	context< ContextTemplate >::context(int value):context_base()
	{
		//printf("<Context> default constructor has prerun = %d on %p\n", *prerun, this);
		//prerun = (int*)malloc(sizeof(int));
		*prerun = value;
		printf("prerun(%p) SET should be 1? %d\n", prerun, *prerun);
		cnc_status = STARTED;
		qthread_initialize();
		sinc = qt_sinc_create(0, NULL, NULL, 0);
		# ifdef DEBUG_COUNTERS
			//Initialize DEBUG_COUNTERS counters inside thread-local-data for each thread
			int nr_cores = qthread_num_workers();
			pthread_key_create(&key, NULL);
			qt_loop_balance(0, nr_cores, init_debug_counters, &key);
		# endif
	}

	template< class ContextTemplate >
	error_type context< ContextTemplate >::wait()
	{
		printf("=====\nWait found prerun to be %d on %p\n", *prerun, this);
		if ((*prerun) == 1)
		{
				printf("Prerun phase was: enabled\n");
				printf("Flushing step collections...\n");
				typename  std::list<tag_collection_base*>::const_iterator i;
				printf("Step collections to flush %d\n", (int)tcs.size());
				for( i=tcs.begin(); i != tcs.end(); ++i) {
					//printf("Creating step instance...\n");
					(**i).flush();
				}
		}
		else
			printf("Prerun phase was: disabled\n");
		*prerun = 0;
		qt_sinc_wait(sinc, NULL);
		# ifdef DEBUG_COUNTERS
			int nr_cores = qthread_num_workers();
			qt_loop_balance(0, nr_cores, print_debug_counters, &key);
			//Clear thread local data
			pthread_key_delete(key);
		# endif

		qthread_finalize();
		cnc_status = ENDED;

		printf("CnC execution finalized:\n");
		#ifdef CNC_PRECOND
			printf("\t* preconditions: ON\n");
			#ifdef CNC_PRECOND_ONLY
				printf("\t* flexible preconditions: OFF\n");
			#else
				printf("\t* flexible preconditions: ON\n");
			#endif
		#else
			printf("\t* preconditions: OFF\n");
		#endif
		
		
		#ifdef DISABLE_GET_COUNTS
			printf("\t* getcounts: OFF\n");
		#else
			printf("\t* getcounts: ON\n");
		#endif
		
		return 0;
	}
} // namespace CnC

#endif // _QT_CNC_CONTEXT_H_
