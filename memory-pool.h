#ifndef FLAT_INCLUDES
#include <stddef.h>
#define FLAT_INCLUDES
#endif

typedef struct thread_memory_pool thread_memory_pool;

thread_memory_pool * thread_memory_pool_new(size_t item_size);
/**<
   Creates a new memory pool
*/

void * thread_memory_pool_calloc_from_pool(thread_memory_pool * pool);
/**<
   Allocates pre-zero'd and locked memory from a pool
*/

void * thread_memory_pool_calloc_from_peer(void * mem);
/**<
   Allocates pre-zero'd and locked memory of the given type
*/

void thread_memory_free(void * mem);
/**<
   Frees locked memory
*/

void thread_memory_pool_free(thread_memory_pool * pool);
/**<
   Frees a pool in which all allocated memory has already been freed using thread_memory_free
*/

void thread_memory_lock(void * mem);
/**<
   Locks some memory
*/

void thread_memory_unlock(void * mem);
/**<
   Unlocks some memory
*/

void thread_memory_wait(void * mem);
/**<
   Waits on some memory
*/

void thread_memory_signal(void * mem);
/**<
   Signals some memory
*/

void thread_memory_broadcast(void * mem);
/**<
   Broadcasts some memory
*/

#define thread_memory_pool_declare_types(name, type)			\
									\
    typedef struct name##_memory_pool name##_memory_pool;		\
									\
    typedef type name##_memory;						\
									\

#define thread_memory_pool_declare_alloc(name)				\
    									\
    inline static void name##_memory_pool_free(name##_memory_pool * pool) \
    {									\
	thread_memory_pool_free((thread_memory_pool*)pool);		\
    }									\
									\
    inline static name##_memory * name##_memory_calloc_from_pool(name##_memory_pool * pool) \
    {									\
	return thread_memory_pool_calloc_from_pool((thread_memory_pool*)pool);	\
    }									\
									\
    inline static name##_memory * name##_memory_calloc_from_peer(name##_memory * mem) \
    {									\
	return thread_memory_pool_calloc_from_peer(mem);		\
    }									\
    									\
    inline static void name##_memory_free(name##_memory * mem)		\
    {									\
	thread_memory_free(mem);					\
    }									\

#define thread_memory_pool_declare_default_alloc(name)	\
							\
    void name##_memory_calloc_init();			\
						\
    name##_memory * name##_memory_calloc();	\

#define thread_memory_pool_declare_pool_alloc(name)			\
									\
    name##_memory_pool * name##_memory_pool_new();			\

#define thread_memory_pool_declare_concurrency(name)			\
									\
    inline static void name##_memory_lock(name##_memory * mem)		\
    {									\
	thread_memory_lock(mem);					\
    }									\
									\
    inline static void name##_memory_unlock(name##_memory * mem)	\
    {									\
	thread_memory_unlock(mem);					\
    }									\
									\
    inline static void name##_memory_wait(name##_memory * mem)		\
    {									\
	thread_memory_wait(mem);					\
    }									\
									\
    inline static void name##_memory_signal(name##_memory * mem)	\
    {									\
	thread_memory_signal(mem);					\
    }									\
									\
    inline static void name##_memory_broadcast(name##_memory * mem)	\
    {									\
	thread_memory_broadcast(mem);					\
    }									\

#define thread_memory_pool_define_alloc(name)				\
									\
    name##_memory_pool * name##_memory_pool_new()			\
    {									\
	return (name##_memory_pool*)thread_memory_pool_new(sizeof(name##_memory)); \
    }									\
    
#define thread_memory_pool_declare(name,type)		\
    thread_memory_pool_declare_types(name,type);	\
    thread_memory_pool_declare_pool_alloc(name);	\
    thread_memory_pool_declare_alloc(name);		\
    thread_memory_pool_declare_concurrency(name);	\

#define thread_memory_pool_inherit(name,from,type)		\
    thread_memory_pool_declare_types(name,type);		\
    thread_memory_pool_declare_alloc(name);			\
    thread_memory_pool_declare_concurrency(name);		\
    inline static name##_memory_pool * name##_memory_pool_new()	\
    {								\
	return (name##_memory_pool*) from##_memory_pool_new();	\
    }								\

#define thread_memory_pool_define_default_alloc(name)				\
									\
    static name##_memory_pool * _##name##_memory_pool_default;		\
									\
    void name##_memory_pool_default_free()				\
    {									\
	name##_memory_pool_free(_##name##_memory_pool_default);		\
    }									\
									\
    void name##_memory_calloc_init()					\
    {									\
	assert(!_##name##_memory_pool_default);				\
	_##name##_memory_pool_default = name##_memory_pool_new();	\
	atexit(name##_memory_pool_default_free);			\
    }									\
									\
    name##_memory * name##_memory_calloc()				\
    {									\
	return name##_memory_calloc_from_pool(_##name##_memory_pool_default); \
    }									\

