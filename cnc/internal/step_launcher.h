#ifndef _QT_CNC_STEP_LAUNCHER_H_
#define _QT_CNC_STEP_LAUNCHER_H_

#include <assert.h>
#include <stdarg.h>

namespace CnC {
	// Class launcher_base
	template< class Tag >
	class step_launcher_base
	{
	public:
		step_launcher_base(context_base &ctxt_base);
		virtual ~step_launcher_base();

		virtual void *create_step_instance(const Tag &) const = 0;
		context_base &context() const
		{
			return _context;
		}

	protected:
		context_base &_context;
	};

	template< class Tag>step_launcher_base< Tag >::step_launcher_base(context_base &ctxt_base) : _context(ctxt_base) {}

	template< class Tag >step_launcher_base< Tag >::~step_launcher_base() {}

	template< typename StepType >  class step_collection;

	// Struct pass_to_step
	template< class Tag, class Step, class ContextType>
	struct pass_to_step {
		const Tag                    *tag;
		ContextType                  *ctxt;
		const step_collection<Step > *sc;
	};

	// Class step_launcher : launcher_base
	template< class Tag, class Step, class ContextType>
	class step_launcher : public step_launcher_base< Tag >
	{
	public:
		typedef Tag tag_type;
		typedef Step step_type;
		typedef ContextType ctxt_type;
		typedef step_collection< Step > step_coll_type;

		step_launcher(context_base &                 ctxt_base,
					  ContextType &                  ctxt,
					  const step_collection< Step > &sc);
		~step_launcher();
		virtual void *   create_step_instance(const Tag &tag) const;
		static aligned_t call_step(void *arg);

	private:
		step_collection< Step > _stepCol;
		ContextType &           _ctxt;
	};

	template< class Tag, class Step, class ContextType >step_launcher< Tag, Step, ContextType >::step_launcher(context_base &                 ctxt_base,
																											   ContextType &                  ctxt,
																											   const step_collection< Step > &sc)
		: step_launcher_base< Tag >(ctxt_base), _stepCol(sc), _ctxt(ctxt) {}

	template< class Tag, class Step, class ContextType>step_launcher< Tag, Step, ContextType>::~step_launcher( ) /*TODO*/
	{                                                                         }


	



	template< class Tag, class Step, class ContextType >
	aligned_t step_launcher< Tag, Step, ContextType >::call_step(void *arg)
	{
		pass_to_step<Tag, Step, ContextType> *proper_arg = (pass_to_step<Tag, Step, ContextType> *)arg;
		const Tag                            *tag        = proper_arg->tag;
		const Step                            crt_step   = proper_arg->sc->get_step();
		// init task local storage used to store the head of the list of gets for getcounts
		void* tld = qthread_get_tasklocal(sizeof(pair_base*));
		*((pair_base**)tld) = NULL;
		
		tld = qthread_get_tasklocal(sizeof(pair_base*));
		assert ( *((pair_base**)tld) == NULL && "Task local data init to NULL!");
		
		int rez = crt_step.execute(*tag, *(proper_arg->ctxt));
		// decrement the getCounts of the items in the gets list whose head is in the task local storage
		tld = qthread_get_tasklocal(sizeof(pair_base*));
		//assert ( *((pair_base**)tld) == NULL && "Task local data should still be NULL!");
		pair_base *crt = *((pair_base**)tld), *tmp;
		while(crt != NULL){
			tmp = crt;
			crt -> decrement();
			crt = crt -> next;
			free(tmp);
		}

		
		
		proper_arg->ctxt->markJoin();
		delete proper_arg;
		
		return rez;
	}

	template< class Tag, class Step, class ContextType >
	void *step_launcher< Tag, Step, ContextType >::create_step_instance(const Tag &tag) const
	{
		pass_to_step<Tag, Step, ContextType> *pts = new pass_to_step<Tag, Step, ContextType>();
		pts->ctxt = &_ctxt;
		assert(pts->ctxt != NULL);
		pts->tag = new Tag(tag);
		pts->sc  = &_stepCol;

		_ctxt.markFork();
	# ifndef CNC_PRECOND
		# ifdef ASSERTS_ENABLED
			int ret =
		# endif
		qthread_fork(call_step, pts, NULL);
	# else


	
		# ifdef ASSERTS_ENABLED
			int ret;
		# endif
		int         no_of_dependences = -1;
		aligned_t **list_of_sincs     = _stepCol.get_step().get_dependences(tag, _ctxt, no_of_dependences);
		if(list_of_sincs != NULL) {
			assert(no_of_dependences > 0);
			# ifdef ASSERTS_ENABLED
					ret =
			# endif
			#ifdef CNC_PRECOND_ONLY
				qthread_fork_precond_simple(call_step, pts, NULL, (-1) * no_of_dependences, list_of_sincs);
			#else // CNC_PRECOND_ONLY
				qthread_fork_precond(call_step, pts, NULL, (-1) * no_of_dependences, list_of_sincs);
			#endif // CNC_PRECOND_ONLY
			free(list_of_sincs);                         // TODO: Double-check list is not used after getting the aligned_t* from it
		} else 
		{
			# ifdef ASSERTS_ENABLED
				ret =
			# endif
			qthread_fork(call_step, pts, NULL);
		}
	#endif // ifndef CNC_PRECOND

	#ifdef ASSERTS_ENABLED
		assert(ret == 0 && "Fork failed!");
	#endif

		
		return NULL;
	}
} // namespace CnC

#endif // _QT_CNC_STEP_LAUNCHER_H_
