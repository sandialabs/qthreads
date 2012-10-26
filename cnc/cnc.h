#ifndef _QT_CNC_H_
#define _QT_CNC_H_

#include <stdio.h>
#include <iostream>
#include <list>
#include <qthread.h>
#include <qthread/sinc.h>
#include <qthread/dictionary.h>

#define ASSERTS_ENABLED
// TODO: refactor this...
#define STARTED 1
#define ENDED   2





template< class T >
struct cnc_tag_hash_compare {
    size_t hash(const T &x) const
    {
        // Knuth's multiplicative hash
        return static_cast< size_t >(x) * 2654435761;
    }

    bool equal(const T &a,
               const T &b) const
    {
        return (a == b);
    }
};

namespace CnC {
	typedef int error_type;

	template< class T > class context;
	struct debug;
	template< typename Tag > class tag_collection;
	template< typename Tag, typename Item > class item_collection;

	class tag_collection_base
	{
		public:
		virtual void   flush() = 0;
	};

	
	const int CNC_Success = 0;
	const int CNC_Failure = 1;

	// 1) Step collection
	template< typename StepType >
	class step_collection
	{
	public:
		int id;
		template< typename ContextTemplate >step_collection(context< ContextTemplate > &ctxt);
		StepType get_step() const;

	private:
		const StepType _step;
	};

	//context_base
	class context_base
	{
	public:
		std::list<tag_collection_base*> tcs;
		int id; // incrementing counter that counts the number of collections used in this context
		inline void registerTagCollection(tag_collection_base* tc);
		inline void markJoin();
		inline void markFork();
		context_base(context_base& cb){
			id = cb.id;
			prerun = cb.prerun;
		}
		
		context_base(){
			prerun = new int;
			*prerun = 0;
		}
		int* prerun;
	protected:
		qt_sinc_t *sinc;
		int        initial_value;
	};

	void context_base::registerTagCollection(tag_collection_base* tc)
	{
		tcs.push_back(tc);
	}

	void context_base::markFork()
	{
		qt_sinc_expect(sinc, 1);
	}

	void context_base::markJoin()
	{
		qt_sinc_submit(sinc, NULL);
	}
	
	//pair_base
	class pair_base{
	public:
		pair_base(){}
		virtual void decrement(){}
		pair_base *next;
	};
}

#include <cnc/internal/step_launcher.h>

namespace CnC {

	// 2) Tag collection
	template< typename Tag >
	class tag_collection : tag_collection_base
	{
	public:
	
		int id;
		typedef Tag tag_type;
		template< class ContextTemplate >tag_collection(context< ContextTemplate > &ctxt);
		
		template< typename Step, typename ContextType >
		int prescribes(const step_collection< Step > &sc,
					   ContextType &                  ctxt);
					   
		void   put(const Tag &t);
		void   flush();
		size_t size();
		bool   empty();

	private:
		context_base &_context;
		// Need base type as no info on parametrizing step_lancher after Step and ContextType is available, only Tag
		std::list< step_launcher_base<Tag> * > prescribedStepCollections;
	};

	typedef char YesType;
	typedef char NoType[2];





	
	// 3) Item collection
	template< typename Tag, typename Item >
	class item_collection
	{
	public:
		int id;
		typedef Tag tag_ype;
		class const_iterator;
		template< class ContextTemplate >item_collection(context< ContextTemplate > &ctxt);
		~item_collection();
		void put(const Tag  & tag,
				 const Item & item,
				 int          get_count = -1);
		void get(const Tag &tag,
				 Item &     item) const;
		void wait_on(const Tag & t,
					 aligned_t **i) const;
		void decrement(const Tag &tag) const;
		const_iterator begin() const;
		const_iterator end() const;
		size_t         size();
		bool           empty();
		static int     tag_equals(void *a,
								  void *b);
		static int tag_hashcode(void *a);

		// TODO: public for debugging purposes only!
		int           *pcnc_status;
		qt_dictionary *m_itemCollection;

	protected:
		friend class const_iterator;
	private:
		static void clearItem(YesType*, Item i);
		static void clearItem(NoType*, Item i);
		static void clearTag(YesType*, Tag i);
		static void clearTag(NoType*, Tag i);
		static void cleanupTag(void* t);
	};

	template< class ContextTemplate >
	class context : public context_base
	{
	public:
		context();
		context(int);
		virtual ~context() {}
		error_type wait();
		int cnc_status;
	# ifdef DEBUG_COUNTERS
                pthread_key_t key;
	# endif

	};



	
	//item_id_pair
	template< typename Tag, typename Item >
	class item_id_pair : public pair_base 
	{
	public:
		item_id_pair(const item_collection<Tag, Item>* ic_arg, Tag t_arg) : 
			ic(ic_arg), t(t_arg) {}
		void decrement()
		{
			ic -> decrement(t);
		}
	private:
		const item_collection<Tag, Item>* ic;
		Tag t;
	};
	
} // namespace cnc

#include <cnc/internal/step_collection.h>
#include <cnc/internal/tag_collection.h>
#include <cnc/internal/item_collection.h>
#include <cnc/internal/context.h>

#endif // _QT_CNC_H_
