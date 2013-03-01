#include <utility>
#include <assert.h>

# ifndef _QT_CNC_ITEM_COLLECTION_H_
# define _QT_CNC_ITEM_COLLECTION_H_

namespace CnC {

	template<typename T>
	struct IsPointer
	{
		typedef NoType  Result;
	};
	template<typename T>
	struct IsPointer<T*>
	{
		typedef YesType Result;
	};


	template<typename Item  >
	class entry_t
	{
	public:
		aligned_t   sinc;
		const Item *value;
		int count;

		entry_t(const Item *pitem, int get_count) :
			sinc(0),
			value(pitem),
			count(get_count) {}

		~entry_t()
		{
			qthread_fill(&sinc);
			//if(value != NULL)
			//	delete value; //TODO test this
		}

		

	};

	// TODO: Currently, in the C++ version of the iterator there is no support for concurrent modifications
	// while the iterator iterated. Behavior is undefined in this case
	template< typename Tag, typename Item >
	class item_collection< Tag, Item >::const_iterator
	{
	public:
		typedef std::pair< Tag, Item * > value_type;

		// Constructors
		const_iterator(const item_collection< Tag, Item> *c, bool isEnd = false);
		const_iterator() : m_coll(NULL) {
			assert(0);
		}
		const_iterator(const const_iterator &i) :  dict_it(i.dict_it), m_coll(i.m_coll), m_val(i.m_val), no(0) {
			this->dict_it = qt_dictionary_iterator_copy(i.dict_it);
		}
		~const_iterator() // TODO test destructor! How is end() destroyed?
		{
			qt_dictionary_iterator_destroy(dict_it);
		}

		// Operators
		bool operator==(const const_iterator &i) const
		{
			return qt_dictionary_iterator_equals(i.dict_it, dict_it);
		}

		bool operator!=(const const_iterator &i) const
		{
			return !(*this == i);
		}

		const value_type &operator*() const;
		const value_type *operator->() const
		{
			return &operator*();
		}

		const_iterator &operator++();
		const_iterator operator++(int)
		{
			++(*this);
			return *this;
		}

		bool valid() const
		{
			return m_coll != NULL;
		}


		
	private:
		qt_dictionary_iterator            *dict_it;
		const item_collection< Tag, Item> *m_coll;
		value_type                         m_val;
		int                                no;
	};

	// item_collection::const_iterator - Constructor external implementation
	template< typename Tag, typename Item>item_collection< Tag, Item>::const_iterator::const_iterator(const item_collection< Tag, Item> *c,
																									  bool isEnd)
		: m_coll(c),
		  m_val(), no(0)
	{
		if(!isEnd) {
			dict_it = qt_dictionary_iterator_create(c->m_itemCollection);
			assert (dict_it != ERROR);
		} else {
			dict_it = qt_dictionary_end(c->m_itemCollection);
		}
	}

	// item_collection::const_iterator - Operators external implementations
	template< typename Tag, typename Item >
	const typename item_collection< Tag, Item >::const_iterator::value_type &item_collection< Tag, Item >
	::const_iterator::operator*() const
	{
		list_entry *entry = qt_dictionary_iterator_get(dict_it);

		return *(new std::pair<Tag, Item *>(*((Tag *)(entry->key)), ((Item *)(entry->value))));
	}

	template< typename Tag, typename Item >
	typename item_collection< Tag, Item >::const_iterator &item_collection< Tag, Item >::const_iterator
	::operator++()
	{
		list_entry* ret = qt_dictionary_iterator_next(dict_it);
		assert (ret != ERROR && "Incrementing the iterator returned an error");
		no++;
		return *this;
	}

	// item_collection - Constructor external implementation
	template< typename Tag, typename Item  >
	template< class ContextTemplate >item_collection< Tag, Item >::item_collection(context< ContextTemplate > &context)
	{
		m_itemCollection = qt_dictionary_create(tag_equals, tag_hashcode, cleanupTag );
		pcnc_status      = &(context.cnc_status);
		id = context.id++;
	}

	template< typename Tag, typename Item  >item_collection< Tag, Item >::~item_collection(){}

	// item_collection - Methods external implementations
	template< typename Tag, typename Item  >
	int item_collection< Tag, Item >::tag_equals(void *a,
												 void *b)
	{
		int r = cnc_tag_hash_compare<Tag>().equal(*((Tag *)(a)), *((Tag *)(b)));

		return r;
	}

	template< typename Tag, typename Item  >
	int item_collection< Tag, Item >::tag_hashcode(void *a)
	{
		return cnc_tag_hash_compare<Tag>().hash(*(Tag *)a);
	}

	template< typename Tag, typename Item  >
	void item_collection< Tag, Item >::cleanupTag(void* t, void* v) {
		typename IsPointer<Tag>::Result r;
		clearTag(&r, *( (Tag*) t) );
	}

	//Cleanup methods which differentiate between reference types
	//and primitive types, as only reference types need to be deallocated.
	// cleanup methods for items
	template< typename Tag, typename Item  >
	void item_collection< Tag, Item >::clearItem(YesType*, Item i)
	{
		delete i;
	}
	
	template< typename Tag, typename Item  >
	void item_collection< Tag, Item >::clearItem(NoType*, Item i) {}

	// cleanup methods for tag
	template< typename Tag, typename Item  >
	void item_collection< Tag, Item >::clearTag(YesType*, Tag t)
	{
		delete t;
	}
	
	template< typename Tag, typename Item  >
	void item_collection< Tag, Item >::clearTag(NoType*, Tag t) {}
	
	template< typename Tag, typename Item>
	typename item_collection< Tag, Item >::const_iterator item_collection< Tag, Item >::begin() const
	{
		const_iterator *_tmp = new item_collection< Tag, Item >::const_iterator(this, false);
		++(*_tmp);
		return *_tmp;
	}

	template< typename Tag, typename Item >
	typename item_collection< Tag, Item >::const_iterator item_collection< Tag, Item >::end() const
	{
		const_iterator *_tmp = new item_collection< Tag, Item >::const_iterator(this, true);

		return *_tmp;
	}

	template< typename Tag, typename Item  >
	void item_collection< Tag, Item >::put(const Tag  & t,
										   const Item &i,
										   int        get_count)
	{
	# ifdef DISABLE_GET_COUNTS
		get_count = -1;
	# endif
		if(get_count == 0){
			printf("Produced unused item => not stored\n");
			return;
		}
		assert ((get_count == -1 || get_count >= 0) && 
			"Correct get_count argument is >= 0 (default to -1 for persistent items)");
		entry_t<Item> *entry = new entry_t<Item>(new Item(i), get_count);
		entry_t<Item> *ret   = (entry_t<Item> *)qt_dictionary_put_if_absent(
																			m_itemCollection,
																			const_cast < Tag * >(new Tag(t)), entry);
		if (ret != entry) {
			delete entry;
			entry        = ret;
			entry->value = new Item(i);
			entry->count = get_count;
	# ifdef ASSERTS_ENABLED
			int err =
	# endif
			qthread_fill(&entry->sinc);
	# ifdef ASSERTS_ENABLED
			assert(err == 0 && "Could not mark item as present!");
	# endif
		}
	}

	template< typename Tag, typename Item  >
	void item_collection< Tag, Item >::get(const Tag &t,
										   Item &     i) const
	{
		entry_t<Item> *ret;
		int stat = *pcnc_status;
		
	# ifndef CNC_PRECOND_ONLY
		// Flexible Preconds, aka, use readFF
		entry_t<Item> *entry = new entry_t<Item>(NULL, -1);
		assert(entry != NULL);
		if (stat == STARTED) {
			qthread_empty(&entry->sinc);                            // init to empty
		}
		ret = (entry_t<Item> *)qt_dictionary_put_if_absent(m_itemCollection,
																		  const_cast < Tag * >(new Tag(t)), entry);
		assert(ret != NULL);
		if (entry != ret) {
			delete entry;
		}
		
		if (stat == STARTED) {
			qthread_readFF(NULL, &ret->sinc);
		}
	# else // Use this case to check precond is called when _all_ dependences are known ahead of time.
	       // Revert to above case for _partial_ preconditions; Keep above case as default.
	       // ONLY Preconds - aka, do not use readFF
		ret = (entry_t<Item> *) qt_dictionary_get(m_itemCollection, const_cast < Tag * >(&t));
		assert (ret != NULL && "item collection entry is null!");
		assert (ret -> value != NULL && "item collection value is null!");
	# endif
	
		if (stat == STARTED){
			// record the get as performed to enable decrementing
			// the getCount of the item after the step is finished
			if(ret -> count > 0 ){
				item_id_pair<Tag, Item>* toadd = new item_id_pair<Tag, Item>(this, t);
				void* tld = qthread_get_tasklocal(sizeof(pair_base*));
				pair_base **p = (pair_base **)tld;
				toadd -> next = *p;
				*p = toadd;
			}
		}
		//else if(ret -> count != 1)
			//printf("Item read after graph finished has getcount != 1 ( = %d)\n", ret ->count);
		
		i = *(ret->value);
	}

	template< typename Tag, typename Item  >
	void item_collection< Tag, Item >::wait_on(const Tag &t,
											   aligned_t **i) const
	{
		entry_t<Item> *entry = new entry_t<Item>(NULL, -1);
		assert(entry != NULL);
		qthread_empty(&entry->sinc);                            // init to empty
		
		entry_t<Item> *ret = (entry_t<Item> *)qt_dictionary_put_if_absent(m_itemCollection,
																		  const_cast < Tag * >(new Tag(t)), entry);
		assert(ret != NULL);
		if (entry != ret) {
			delete entry;
		}
		
		*i = &ret->sinc;
	}
	
	template< typename Tag, typename Item  >
	void item_collection< Tag, Item >::decrement(const Tag &t) const
	{
		//printf("Decrementing dictionary entry for %d-%d\n", ((int*)(&t))[0], ((int*)(&t))[1] );
				
		entry_t<Item> *ret = (entry_t<Item> *) qt_dictionary_get(m_itemCollection, const_cast < Tag * >(&t));
		
		aligned_t old_val = qthread_incr(&(ret->count), -1);
		if(old_val == 1){
				ret = (entry_t<Item> *) qt_dictionary_delete(m_itemCollection, const_cast < Tag * >(&t));
				if (ret == NULL) 
					printf("Item tagged %d-%d-%d already collected!\n",((int*)(&t))[0], ((int*)(&t))[1], ((int*)(&t))[2]);
				assert (ret !=  NULL && "Error when deleting item from dictionary (not found)");
				if(ret != NULL){
					typename IsPointer<Item>::Result r;
					//printf("Item tagged %d-%d is getting deleted!\n",((int*)(&t))[0], ((int*)(&t))[1] );
					clearItem(&r, *(ret -> value));
					//delete *(ret -> value);
				}
				delete(ret);
		}
		//printf("Ending Decrement dictionary entry for %d-%d\n", ((int*)(&t))[0], ((int*)(&t))[1] );
		
	}

	// TODO: implement size
	template< typename Tag, typename Item  >
	size_t item_collection< Tag, Item >::size()
	{
		return 0;
	}

	template< typename Tag, typename Item  >
	bool item_collection< Tag, Item >::empty()
	{
		return size() == 0;
	}
} // namespace CnC

# endif // _QT_CNC_ITEM_COLLECTION_H_
