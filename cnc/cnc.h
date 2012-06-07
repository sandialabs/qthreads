#ifndef _QT_CNC_H_
#define _QT_CNC_H_

#include <stdio.h>
#include <iostream>
#include <list>
#include <qthread.h>
#include <qthread/dictionary.h>

// TODO: refactor this...
#define STARTED 1
#define ENDED 	2

template< class T >
struct cnc_tag_hash_compare
{
	size_t hash( const T & x ) const { 
		// Knuth's multiplicative hash
		return static_cast< size_t >( x ) * 2654435761;
	}
	bool equal( const T & a, const T & b ) const {
		return ( a == b );
	}
};

namespace CnC {
    typedef int error_type;

    template< class T > class context;
    struct debug;
    template< typename Tag > class tag_collection;
    template< typename Tag, typename Item > class item_collection;

    const int CNC_Success = 0;
    const int CNC_Failure = 1;

	//1) Step collection
    template< typename StepType >
    class step_collection 
    {
		public:
			template< typename ContextTemplate >
			step_collection( context< ContextTemplate > & ctxt );
			StepType get_step() const;
			
	 	private:
			const StepType _step;
    };
    
    class context_base {
    	public:
    		inline void markJoin();
    		inline void markFork();
    	protected:
    		qt_sinc_t* sinc;
        	int initial_value;
	};
	
	void context_base::markFork() { qt_sinc_willspawn(sinc, 1); }
	void context_base::markJoin() { qt_sinc_submit(sinc, NULL); }
	
}

#include <cnc/internal/step_launcher.h>

namespace CnC {

	//2) Tag collection
    template< typename Tag >
    class tag_collection
    {
		public:
			typedef Tag tag_type;
			template< class ContextTemplate >
			tag_collection( context< ContextTemplate > & ctxt );
			template< typename Step, typename ContextType >
			int prescribes( const step_collection< Step > &sc, ContextType & ctxt );
			void put( const Tag & t );
			size_t size();
			bool empty();
	
		private:
			context_base&  _context;
			//Need base type as no info on parametrizing step_lancher after Step and ContextType is available, only Tag
			std::list< step_launcher_base<Tag>* > prescribedStepCollections;
    };

	//3) Item collection
	template< typename Tag, typename Item >
    class item_collection
    {
		public:
			typedef Tag tag_ype;
			class const_iterator;
			template< class ContextTemplate >
			item_collection( context< ContextTemplate > & ctxt );
			~item_collection();
			void put( const Tag & tag, const Item & item );
			void get( const Tag & tag, Item & item ) const;
			const_iterator begin() const;
			const_iterator end() const;
			size_t size();
			bool empty();
			static int tag_equals(void* a, void* b);
			static int tag_hashcode(void* a);
		
			// TODO: public for debugging purposes only!
			int* pcnc_status;
			qt_dictionary* m_itemCollection;
			
		protected:
			friend class const_iterator;
    };
    
	
    template< class ContextTemplate >
    class context : public context_base
    {
		public:
			context();
			virtual ~context() {}
			error_type wait();
			int cnc_status;
		private:
			
			context( bool );
    };

} // namespace cnc

#include <cnc/internal/step_collection.h>
#include <cnc/internal/tag_collection.h>
#include <cnc/internal/item_collection.h>
#include <cnc/internal/context.h>

#endif // _QT_CNC_H_
