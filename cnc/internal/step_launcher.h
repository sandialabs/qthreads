#ifndef _QT_CNC_STEP_LAUNCHER_H_
#define _QT_CNC_STEP_LAUNCHER_H_

namespace CnC {

	//Class launcher_base
	template< class Tag >
	class step_launcher_base {
		public:
			step_launcher_base( context_base & ctxt_base );
			virtual ~step_launcher_base();
	
			virtual void* create_step_instance( const Tag & ) const = 0;
			context_base & context() const { return _context; }
	
		protected:
			context_base & _context;
	};

	template< class Tag>
	step_launcher_base< Tag >::step_launcher_base( context_base & ctxt_base ) : _context( ctxt_base ) {}

	template< class Tag >
	step_launcher_base< Tag >::~step_launcher_base() {}
		
    template< typename StepType >  class step_collection;
    
	//Struct pass_to_step
	template< class Tag, class Step, class ContextType>
    struct pass_to_step {
    	const Tag* tag;
    	ContextType* ctxt;
    	const step_collection<Step >* sc;
    };

    //Class step_launcher : launcher_base
    template< class Tag, class Step, class ContextType>
    class step_launcher : public step_launcher_base< Tag > {
    	public:
            typedef Tag tag_type;
            typedef Step step_type;
            typedef ContextType ctxt_type;
            typedef step_collection< Step > step_coll_type;
            
        	step_launcher( 	context_base & ctxt_base, ContextType & ctxt, const step_collection< Step > & sc );
        	~step_launcher();
        	virtual void* create_step_instance( const Tag & tag ) const;
        	static aligned_t call_step(void *arg);
        	
        private:
        	step_collection< Step > _stepCol;
        	ContextType & _ctxt;
    };
    
	template< class Tag, class Step, class ContextType >
	step_launcher< Tag, Step, ContextType >::step_launcher( context_base & ctxt_base, ContextType & ctxt,
													const step_collection< Step > & sc) 
		: step_launcher_base< Tag >( ctxt_base ), _stepCol( sc ), _ctxt( ctxt ) {}

	template< class Tag, class Step, class ContextType>
	step_launcher< Tag, Step, ContextType>::~step_launcher( ) {/*TODO*/}
	
	template< class Tag, class Step, class ContextType >
	aligned_t step_launcher< Tag, Step, ContextType >::call_step(void *arg) {
		pass_to_step<Tag, Step, ContextType>* proper_arg = (pass_to_step<Tag, Step, ContextType>*)arg;
		const Tag* tag = proper_arg -> tag;
		const Step crt_step = proper_arg -> sc -> get_step();
		int rez = crt_step.execute(*tag, *(proper_arg -> ctxt));
		proper_arg -> ctxt -> markJoin();
		
		return rez;
	}

	template< class Tag, class Step, class ContextType >
	void* step_launcher< Tag, Step, ContextType >::create_step_instance( const Tag & tag ) const {
		aligned_t t;
		aligned_t copy;
		
		pass_to_step<Tag, Step, ContextType>* pts = new pass_to_step<Tag, Step, ContextType>();
		
		pts->ctxt = &_ctxt;
		pts->tag = new Tag(tag);
		pts->sc = &_stepCol;
		
		_ctxt.markFork();
		int ret = qthread_fork(call_step, pts, &t);
		return NULL;
	}
} // namespace CnC

#endif //_QT_CNC_STEP_LAUNCHER_H_
