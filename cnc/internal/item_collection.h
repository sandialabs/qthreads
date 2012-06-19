#include <utility>
#include <assert.h>
#ifndef _QT_CNC_ITEM_COLLECTION_H_
#define _QT_CNC_ITEM_COLLECTION_H_

namespace CnC {

    template<typename Item  >
	class entry_t {
		public:
			aligned_t sinc;
			const Item* value; 
			
			entry_t(const Item* pitem ):
				sinc(0),
				value(pitem) {};

			~entry_t() {
                qthread_fill(&sinc);
            }
		
	};

	// TODO: Currently, in the C++ version of the iterator there is no support for concurrent modifications
	// while the iterator iterated. Behavior is undefined in this case
    template< typename Tag, typename Item >
    class item_collection< Tag, Item >::const_iterator
    {
		public:
			typedef std::pair< Tag, Item * > value_type;
			
			//Constructors
			const_iterator( const item_collection< Tag, Item> * c, bool isEnd = false) ;
			const_iterator() : m_coll( NULL ) { assert(0);}
			const_iterator( const const_iterator & i ) : m_coll( i.m_coll ), m_val( i.m_val ), dict_it(i.dict_it), no(0) {}
			
			//Operators
			bool operator==( const const_iterator & i ) const { 
				return qt_dictionary_iterator_equals(i.dict_it, dict_it);
			}
			bool operator!=( const const_iterator & i ) const { 
				return !(*this == i); 
			}
			const value_type & operator*() const;
			const value_type * operator->() const { 
				return &operator*(); 
			}
			const_iterator & operator++();
			const_iterator operator++( int ) { 
				const_iterator _tmp = *this; 
				++(*this); 
				return _tmp; 
			}
			bool valid() const { return m_coll != NULL ; }
			
			value_type m_val;
			
			// TODO add destructor!
			
		private:
			qt_dictionary_iterator* dict_it;
				const item_collection< Tag, Item> * m_coll;
				int no;
    };  
    
    // item_collection::const_iterator - Constructor external implementation
    template< typename Tag, typename Item>
	item_collection< Tag, Item>::const_iterator::const_iterator(const item_collection< Tag, Item> * c, bool isEnd)
			: m_coll( c ),
			  m_val(), no(0) {
		if(!isEnd)
			dict_it = qt_dictionary_iterator_create(c->m_itemCollection);
		else
			dict_it = qt_dictionary_end(c->m_itemCollection);
	}
	
	// item_collection::const_iterator - Operators external implementations
    template< typename Tag, typename Item >
    const typename item_collection< Tag, Item >::const_iterator::value_type & item_collection< Tag, Item >
    		::const_iterator::operator*() const {
        list_entry* entry = qt_dictionary_iterator_get(dict_it);
        return *(new std::pair<Tag, Item*>(*((Tag*)(entry->key)), ((Item*)(entry->value))));
    }

    template< typename Tag, typename Item >
    typename item_collection< Tag, Item >::const_iterator & item_collection< Tag, Item >::const_iterator
    		::operator++() {
        qt_dictionary_iterator_next(dict_it);
        no++;
        return *this;
    }
    
    // item_collection - Constructor external implementation
    template< typename Tag, typename Item  >
    template< class ContextTemplate >
    item_collection< Tag, Item >::item_collection( context< ContextTemplate > & context)
    {
    	m_itemCollection = qt_dictionary_create(tag_equals, tag_hashcode);
    	pcnc_status = &(context.cnc_status);
    } 

    template< typename Tag, typename Item  >
    item_collection< Tag, Item >::~item_collection(){}
    
	// item_collection - Methods external implementations
    template< typename Tag, typename Item  >
	int item_collection< Tag, Item >::tag_equals(void* a, void* b) {
		int r = cnc_tag_hash_compare<Tag>().equal( *((Tag*)(a)), *((Tag*)(b)) );
		return r;
	}
	
	template< typename Tag, typename Item  >
	int item_collection< Tag, Item >::tag_hashcode(void* a) {
		return cnc_tag_hash_compare<Tag>().hash( *(Tag*)a );
	}
	
	template< typename Tag, typename Item>
    typename item_collection< Tag, Item >::const_iterator item_collection< Tag, Item >::begin() const
    {
    	const_iterator _tmp( this );
        ++_tmp;
        return _tmp;
    }
    
    template< typename Tag, typename Item >
    typename item_collection< Tag, Item >::const_iterator item_collection< Tag, Item >::end() const
    {
    	const_iterator  _tmp( this , true);
        return _tmp;
    }

    template< typename Tag, typename Item  >
    void item_collection< Tag, Item >::put( const Tag & t, const Item & i )
    {
    	entry_t<Item>* entry = new entry_t<Item>( new Item(i)); 
        entry_t<Item>* ret = (entry_t<Item>*) qt_dictionary_put_if_absent(
        										m_itemCollection, 
        										const_cast < Tag* >(new Tag(t)), entry);
		
        if (ret != entry) {
			//TODO: write destructor for entry that deletes the Item and sinc;
        	delete entry;
        	entry = ret;
            entry->value = new Item(i);
			#ifdef ASSERTS_ENABLED
				int err = 
            #endif
            qthread_fill(&entry->sinc);
            #ifdef ASSERTS_ENABLED
				assert(err == 0 && "Could not mark item as present!" );
            #endif
        } 
    }

    
	template< typename Tag, typename Item  >
	void item_collection< Tag, Item >::get( const Tag & t, Item & i ) const
    {
     	int stat = *pcnc_status;
        entry_t<Item>* entry = new entry_t<Item>( NULL);
        assert(entry != NULL);
        if (stat==STARTED) qthread_empty(&entry->sinc); //init to empty
        entry_t<Item>* ret = (entry_t<Item>*) qt_dictionary_put_if_absent(m_itemCollection,
                                                                                const_cast < Tag* >(new Tag(t)), entry);
        assert(ret != NULL);
        if (entry != ret) {
                delete entry;
        }

	if (stat==STARTED) { qthread_readFF(NULL, &ret->sinc); }
        i = *(ret->value);
    }


	//TODO: implement size
    template< typename Tag, typename Item  >
    size_t item_collection< Tag, Item >::size(){
        return 0;
    }

    template< typename Tag, typename Item  >
    bool item_collection< Tag, Item >::empty(){
        return size() == 0;
    }

} // namespace CnC

#endif // _QT_CNC_ITEM_COLLECTION_H_
