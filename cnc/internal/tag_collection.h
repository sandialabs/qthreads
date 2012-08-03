#ifndef _QT_CNC_TAG_COLLECTION_H_
#define _QT_CNC_TAG_COLLECTION_H_

namespace CnC {
	template< typename Tag  >
	template< class ContextTemplate >tag_collection< Tag  >::tag_collection(context< ContextTemplate > &context)
		: _context(reinterpret_cast< context_base & >(context)), prescribedStepCollections()  {
			id = context.id++;
		}

	template< typename Tag  >
	void tag_collection<Tag >::put(const Tag &t)
	{
		typedef step_launcher_base<Tag> *ptr_step_tag;
		typename std::list< ptr_step_tag>::const_iterator iterator;
		for (iterator = prescribedStepCollections.begin(); iterator != prescribedStepCollections.end(); ++iterator) {
			(*iterator)->create_step_instance(t);
		}
	}

	template< typename Tag >
	template< typename Step, typename ContextType >
	int tag_collection< Tag  >::prescribes(const step_collection< Step > &sc,
										   ContextType &                  ctxt)
	{
		typedef step_launcher< Tag, Step, ContextType > the_step_launcher;
		the_step_launcher *_sl = new the_step_launcher(_context, ctxt, sc);
		prescribedStepCollections.push_back(_sl);
		return 0;
	}

	// TODO: implement this
	template< typename Tag >
	size_t tag_collection< Tag >::size()
	{
		return 0;
	}

	template< typename Tag >
	bool tag_collection< Tag >::empty()
	{
		return size() == 0;
	}
} // namespace CnC

#endif // _QT_CNC_TAG_COLLECTION_H_
