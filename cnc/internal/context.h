#ifndef _QT_CNC_CONTEXT_H_
#define _QT_CNC_CONTEXT_H_

namespace CnC {
	template< class ContextTemplate >
	inline context< ContextTemplate >::context()
	{
		cnc_status = STARTED;
		qthread_initialize();
		sinc = qt_sinc_create(0, NULL, NULL, 0);
	}

	template< class ContextTemplate >
	error_type context< ContextTemplate >::wait()
	{
		qt_sinc_wait(sinc, NULL);
		qthread_finalize();
		cnc_status = ENDED;

		printf("CnC execution finalized:\n");
		#ifdef CNC_PRECOND
			printf("\t* preconditions: ON\n");
			#ifdef CNC_PRECOND_ONLY
				printf("\t* flexible preconditions: OFF\n");
			#else
				printf("\t* preconditions: ON\n");
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
