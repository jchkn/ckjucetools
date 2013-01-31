/*
  ==============================================================================

    Created: 16 Jan 2013 2:33:44pm
    Author:  Christian

  ==============================================================================
*/



// THIS IS A HEAP MEMORY RECYCLING MEMORY ALLOCATER
// To enable it please define USE_CACHINGALLOCATER

// Proof of concept, only tested with MS VS2008

// In Multi-threaded application, all threads using the same CriticalSection for locking the HEAP Memory, which can be a bottleneck and reason for spikes/clicks/pos/buffer-repeats while audio-playback.
// This allocator recycles memory-Blocks deallocated by "delete" in its own cache per Thread
// Its overloads the global new and delete operator (which is IMHO done by the linker, part off c++ standard), no header file is required
// This is proof of concept, the caches won't be cleaned up at the end, so it WILL LEAK AS HELL (But if a application already will be closed, this should makes no difference)
// Of cause with this setting you hard to find your true memory leaks.

// Every memory block will be classified into its own size category. 
// Every size category/thread use its own cache
// Size Categories	0 = 0..2 bytes
//					1 = 3..4 bytes
//					2 = 5..8 bytes
//					3 = 9..16 bytes
//					4 = 17..32 bytes
//					5 = 33..64 bytes 
//					6 = 65..128 bytes 
//					7 = 129..256 bytes 
//					8 = 257..512 bytes
//					9 = 513..1024 bytes and so on...
//					-1= > 2<<CK_PREALLOCATOR_RECYCLE_MEMORYBLOCKS_UP_TO_SIZE_CATEGORY


// Bigger MemoryBlocks will be just allocated by malloc/free


#include "JuceHeader.h"


#ifdef USE_CACHINGALLOCATER

// 8 means: blocks until 512 bytes will be cached
#define CK_PREALLOCATOR_RECYCLE_MEMORYBLOCKS_UP_TO_SIZE_CATEGORY 8
#define CK_PREALLOCATOR_MAX_NUMBER_OF_MEMORYBLOCKS_TO_HOLD_PER_SIZE_AND_THREAD 100



// A Simple Stack
template <class ObjectClass>
class    SimpleStack					
{
public:

	SimpleStack (int capacity) noexcept
		: pos(0)
	{
		jassert (capacity > 0);
		oarray.resize(capacity);
	}

	~SimpleStack() {};
	

	bool canWrite()	{		return pos<oarray.size(); };
	bool canRead()	{		return pos>0;	};
	
	bool write ( ObjectClass object)  noexcept
	{
		if (!canWrite())
		{
			return false;
		}

		oarray.set(pos,object);
		pos++;
		return true;
	}
	
	ObjectClass read ()  noexcept
	{
		
		if (!canRead())
		{
			return ObjectClass();
		}

		pos--;
		return oarray[pos];

	}
	
private:
	//==============================================================================

	int pos;
	Array <ObjectClass>  oarray;

	

};


// from JUCE-BigInteger
namespace BitFunctions
{
	inline int countBitsInInt32 (uint32 n) noexcept
	{
		n -= ((n >> 1) & 0x55555555);
		n =  (((n >> 2) & 0x33333333) + (n & 0x33333333));
		n =  (((n >> 4) + n) & 0x0f0f0f0f);
		n += (n >> 8);
		n += (n >> 16);
		return (int) (n & 0x3f);
	}

	inline int highestBitInInt (uint32 n) noexcept
	{
		jassert (n != 0); // (the built-in functions may not work for n = 0)

#if JUCE_GCC
		return 31 - __builtin_clz (n);
#elif JUCE_USE_INTRINSICS
		unsigned long highest;
		_BitScanReverse (&highest, n);
		return (int) highest;
#else
		n |= (n >> 1);
		n |= (n >> 2);
		n |= (n >> 4);
		n |= (n >> 8);
		n |= (n >> 16);
		return countBitsInInt32 (n >> 1);
#endif
	}
}

struct ThreadPreAllocater
{
	void init ()
	{

		initialisationDone=false;

		for (int i=0; i<=CK_PREALLOCATOR_RECYCLE_MEMORYBLOCKS_UP_TO_SIZE_CATEGORY ; i++)
		{
			cache[i]=new SimpleStack<void *>(CK_PREALLOCATOR_MAX_NUMBER_OF_MEMORYBLOCKS_TO_HOLD_PER_SIZE_AND_THREAD);
		};


		#ifdef JUCE_DEBUG
		for (int i=0; i<sizeof(statistic)/sizeof(int); i++)
		{
			statistic[i]=0;
		};
		#endif

		initialisationDone=true;
	}

	inline int  calcSizeCategory(size_t size)
	{
		// will not work if somebody request a object with new >2GB (which would be somehow unlikely)
		// Size Category	0 = 0..2 bytes
		//					1 = 3..4 bytes
		//					2 = 5..8 bytes
		//					3 = 9..16 bytes
		//					4 = 17..32 bytes and so on
		return  size > 1 ? BitFunctions::highestBitInInt((uint32)size-1) : 0;
	};

	void* mallocWithHeader(int sizeCategory)
	{
		//myMalloc adds 4 bytes int with the sizeCategory
		int sizeToMalloc=2<<sizeCategory;

		void * a=malloc(sizeToMalloc+sizeof(int));
		*((int*)a)=sizeCategory;
		return (void*)((int*)a+1);
	};

	void* mallocWithHeaderBig(int size)
	{
		// Bigger blocks have Size-Categorie 
		void * a=malloc(size+sizeof(int));
		*((int*)a)=-1;
		return (void*)((int*)a+1);
	};


	void* allocate(size_t size)
	{
	
		if (size==0)
		{
			// Who wants a block with zero memory, no one!
			jassertfalse;
			return 0;
		}
		
		
		int sizeCagetory=calcSizeCategory(size);

		#ifdef JUCE_DEBUG
		statistic[sizeCagetory]++;
		#endif

		
		// Okay a big block, its done standard malloc
		if (sizeCagetory>CK_PREALLOCATOR_RECYCLE_MEMORYBLOCKS_UP_TO_SIZE_CATEGORY)
		{
			return mallocWithHeaderBig(size);
		}

		if (!initialisationDone)
		{
			return mallocWithHeader(sizeCagetory);
		} else
		{
			if (cache[sizeCagetory]->canRead())
			{
				void* p=cache[sizeCagetory]->read();
				return (void*)((int*)p+1);
			} else
			{
				return mallocWithHeader(sizeCagetory);
			}
		};

	};

	void deallocate( void * a )
	{
		if (a==nullptr)
		{
			// this can happen
			return;
		}
		
		void *p=(void*)((int*)a-1);
		
		int sizeCategory=*((int*)p);
		
		if ( sizeCategory==-1)
		{
			free(p);
			return;
		};
	
		if (sizeCategory>=0 && sizeCategory<=CK_PREALLOCATOR_RECYCLE_MEMORYBLOCKS_UP_TO_SIZE_CATEGORY)
		{
			if (cache[sizeCategory]->canWrite())
			{
				//write in to cache
				cache[sizeCategory]->write(p);
			} else
			{
				//Cache is full
				free(p);
			};
		} else
		{
			jassertfalse;
			// this shouldn't happen
		}
	}

	// Prevent using myself --> stackoverflow
	static void* operator new (size_t size)
	{
		return malloc(size);
	}; 

	// Prevent using myself --> stackoverflow
	static void operator delete (void *p)
	{ 
		free(p);	
	}; 

	#ifdef JUCE_DEBUG
	uint32 statistic[64];
	#endif
	
	bool initialisationDone;

	SimpleStack <void*> *cache[CK_PREALLOCATOR_RECYCLE_MEMORYBLOCKS_UP_TO_SIZE_CATEGORY+1];
};
	

struct GlobalPreAllocater
{
	static void *allocate(size_t size);
	static void deallocate(void* p);
};



// c++ Linker-magic - replaces new with new and delete with delete :-)

void* operator new(size_t size) throw(/*std::bad_alloc*/) { return GlobalPreAllocater::allocate(size); }
void operator delete(void* addr) throw() { GlobalPreAllocater::deallocate(addr); }

void* operator new[](size_t size) throw(/*std::bad_alloc*/) {  return GlobalPreAllocater::allocate(size); }
void operator delete[](void* addr) throw() { GlobalPreAllocater::deallocate(addr); }


static ThreadLocalValue<ThreadPreAllocater*> threadAllocator;

void * GlobalPreAllocater::allocate( size_t size )
{
	if (threadAllocator.get()==nullptr)
	{
		threadAllocator=new ThreadPreAllocater();threadAllocator.get()->init();
	}
	return threadAllocator.get()->allocate(size);
}

void GlobalPreAllocater::deallocate( void * p )
{
	if (threadAllocator.get()==nullptr)
	{
		threadAllocator=new ThreadPreAllocater();threadAllocator.get()->init();
	}
	return threadAllocator.get()->deallocate(p);
}

#endif


