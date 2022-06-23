#ifndef FLAT_INCLUDES
#include <stddef.h>
#include <stdbool.h>
#include "memory-pool.h"
#endif

typedef struct thread_pool thread_pool;
typedef struct thread_job thread_job;

thread_memory_pool_declare_types(thread_job, thread_job);
thread_memory_pool_declare_concurrency(thread_job);

typedef void (*thread_job_function)(thread_job * self, thread_job * parent, thread_pool * pool, void * arg, bool should_quit);

void thread_pool_host(size_t worker_count, thread_job * first_job);
void thread_pool_quit(thread_pool * pool);
thread_job_memory_pool * thread_job_memory_pool_new(size_t arg_size);
void thread_pool_add_job(thread_pool * pool, thread_job * job);
void * thread_job_init(thread_job * child, thread_job_function function);
size_t thread_pool_job_count(thread_pool * pool);
void thread_job_wait(thread_pool * pool, thread_job * job);
void thread_job_add_child(thread_job * parent, thread_job * child);

#define thread_job_declare(name)					\
									\
    typedef struct name##_job name##_job;				\
									\
    inline static void thread_pool_add_##name##_job(thread_pool * pool, name##_job * job) \
    {									\
	thread_pool_add_job(pool, (thread_job*) job);			\
    }									\
									\
    inline static thread_job * name##_job_generic(name##_job * job)	\
    {									\
	return (thread_job*)job;					\
    }									\
									\
    void name##_job_add_child(name##_job * parent, thread_job * child)	\
    {									\
	thread_job_add_child(name##_job_generic(parent), child);	\
    }									\
									\
    void name##_job_add_parent(name##_job * child, thread_job * parent)	\
    {									\
	thread_job_add_child(parent, name##_job_generic(child));	\
    }									\
									\
    thread_memory_pool_declare_types(name##_job, name##_job);		\
    thread_memory_pool_declare_concurrency(name##_job);			\
    thread_memory_pool_declare_alloc(name##_job);			\
    thread_memory_pool_declare_default_alloc(name##_job);		\
    thread_memory_pool_declare_pool_alloc(name##_job);			\

#define thread_job_define_arg(name, ...)				\
									\
    typedef __VA_ARGS__ name##_job_arg;					\
									\
    void name##_job_function(thread_job * self, thread_job * parent, thread_pool * pool, void * arg, bool should_quit); \
									\
    name##_job_arg * name##_job_init(name##_job * child) \
    {									\
	return thread_job_init((thread_job*)child, name##_job_function); \
    }									\
									\
    name##_job_memory_pool * name##_job_memory_pool_new()		\
    {									\
	return (name##_job_memory_pool*) thread_job_memory_pool_new(sizeof(name##_job_arg)); \
    }									\
									\
    thread_memory_pool_define_default_alloc(name##_job);		\

#define thread_job_define_function(name)				\
									\
    inline static void name##_job_function_internal(name##_job * self, thread_job * parent, thread_pool * pool, name##_job_arg * arg, bool should_quit); \
									\
    void name##_job_function(thread_job * self, thread_job * parent, thread_pool * pool, void * arg, bool should_quit) \
    {									\
	name##_job_function_internal((name##_job*)self, parent, pool, arg, should_quit); \
    }									\
									\
    inline static void name##_job_function_internal(name##_job * self, thread_job * parent, thread_pool * pool, name##_job_arg * arg, bool should_quit) \
    
