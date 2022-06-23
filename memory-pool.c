#include "memory-pool.h"
#include "../link1/def.h"
#include "../range/def.h"
#include "../window/def.h"
#include <assert.h>
#include "../window/alloc.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include "../log/log.h"
#include <string.h>

#define lock(target) pthread_mutex_lock(&(target)->mutex)
#define unlock(target) pthread_mutex_unlock(&(target)->mutex)
#define wait(target) pthread_cond_wait(&(target)->cond, &(target)->mutex)
#define signal(target) pthread_cond_signal(&(target)->cond)
#define broadcast(target) pthread_cond_broadcast(&(target)->cond)

typedef struct memory_pool_segment memory_pool_segment;
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool is_allocated;
    memory_pool_segment * segment;
    thread_memory_pool * pool;
}
    memory_pool_header;

range_typedef(memory_pool_header*,memory_pool_header_p);
window_typedef(memory_pool_header*,memory_pool_header_p);

struct memory_pool_segment {
    size_t count;
    window_memory_pool_header_p free;
    uint8_t begin[];
};

link1_typedef(memory_pool_segment, memory_pool_segment);
link1_funcdef(memory_pool_segment);

struct thread_memory_pool {
    pthread_mutex_t mutex;
    size_t alloc_size;
    size_t new_segment_count;
    link1_memory_pool_segment * segments;
};

inline static void * memory_header_end(thread_memory_pool * pool, memory_pool_header * header)
{
    return (uint8_t*)header + sizeof(memory_pool_header) + pool->alloc_size;
}

inline static void * memory_segment_end(memory_pool_segment * segment, thread_memory_pool * pool)
{
    return (memory_pool_header*)(segment->begin + segment->count * (sizeof(memory_pool_header) + pool->alloc_size));
}

inline static memory_pool_header * memory_segment_index(memory_pool_segment * segment, thread_memory_pool * pool, size_t index)
{
    memory_pool_header * memory_item = (memory_pool_header*)(segment->begin + index * (sizeof(memory_pool_header) + pool->alloc_size));
    assert(index < segment->count);
    assert((void*)memory_item >= (void*)segment->begin);
    assert(memory_header_end(pool, memory_item) <= memory_segment_end(segment, pool));
    return memory_item;
}

thread_memory_pool * thread_memory_pool_new(size_t item_size)
{
    thread_memory_pool * retval = calloc (1, sizeof(*retval));

    pthread_mutex_init(&retval->mutex, NULL);

    retval->new_segment_count = 1024;
    retval->alloc_size = item_size;
    
    return retval;
}

static memory_pool_segment * memory_pool_segment_add (thread_memory_pool * pool, size_t count)
{
    size_t memory_item_size = sizeof(memory_pool_header) + pool->alloc_size;
    
    link1_memory_pool_segment * new = calloc(1, sizeof(*new) + count * memory_item_size);

    new->child.count = count;
    
    memory_pool_header * memory_item;

    for (size_t i = 0; i < count; i++)
    {
	memory_item = memory_segment_index(&new->child, pool, i);
	assert((uint8_t*)(memory_item + 1) < (uint8_t*)new + sizeof(*new) + count * memory_item_size);
	pthread_mutex_init(&memory_item->mutex, NULL);
	pthread_cond_init(&memory_item->cond, NULL);
	memory_item->segment = &new->child;
	memory_item->pool = pool;
	*window_push(new->child.free) = memory_item;
    }
    
    link1_memory_pool_segment_insert(&pool->segments, new);

    assert ((size_t)range_count(new->child.free.region) == new->child.count);

    return &new->child;
}

static memory_pool_segment * choose_free_segment (thread_memory_pool * pool)
{
    link1_memory_pool_segment * i;

    memory_pool_segment * free_segment = NULL;
    
    for_link1(i, *pool->segments)
    {
	if (!range_is_empty(i->child.free.region))
	{
	    if (!free_segment || i->child.count > free_segment->count)
	    {
		free_segment = &i->child;
	    }
	}
	else
	{
	    assert(i->child.count);
	    size_t new_segment_count = i->child.count * 2;
	    if (pool->new_segment_count < new_segment_count)
	    {
		pool->new_segment_count = new_segment_count;
	    }
	}
    }

    return free_segment;
}

static memory_pool_segment * choose_or_alloc_free_segment (thread_memory_pool * pool)
{
    memory_pool_segment * free_segment = choose_free_segment(pool);

    if (free_segment)
    {
	return free_segment;
    }
    else
    {
	free_segment = memory_pool_segment_add(pool, pool->new_segment_count);
	assert (!range_is_empty(free_segment->free.region));
	return free_segment;
    }
}

void * thread_memory_pool_calloc_from_pool(thread_memory_pool * pool)
{
    assert(pool);
    
    memory_pool_header * return_header = NULL;
    
    lock(pool);

    memory_pool_segment * parent = choose_or_alloc_free_segment (pool);
    
    assert (!range_is_empty(parent->free.region));

    parent->free.region.end--;

    return_header = *parent->free.region.end;

    lock(return_header);

    assert((void*)return_header >= (void*)parent->begin);
    assert(memory_header_end(pool, return_header) <= memory_segment_end(parent, pool));
    assert(return_header->pool == pool);
    assert(return_header->is_allocated == false);

    return_header->is_allocated = true;

    void * retval = return_header + 1;

    assert (retval < memory_segment_end(parent, pool));

    memset(retval, 0, pool->alloc_size);

    assert(return_header->is_allocated);
    
    unlock(pool);

    //log_debug("alloc %p", retval);
    
    return retval;
}

void thread_memory_free(void * mem)
{
    assert(mem);

    //log_debug("free %p", mem);
    
    memory_pool_header * mem_header = (memory_pool_header*)mem - 1;

    thread_memory_pool * pool = mem_header->pool;
    memory_pool_segment * segment = mem_header->segment;

    lock(pool);
    
    if (!mem_header->is_allocated)
    {
	log_error("Double free");
	abort();
    }

    *window_push(segment->free) = mem_header;
    mem_header->is_allocated = false;

    unlock(pool);
    unlock(mem_header);
}

void thread_memory_pool_free(thread_memory_pool * pool)
{
    assert(pool);

    lock(pool);

    link1_memory_pool_segment * segment_link;
    memory_pool_header ** header;
    memory_pool_header * header_ref;

    while (pool->segments)
    {
	segment_link = link1_memory_pool_segment_pop(&pool->segments);
	
	if ((size_t)range_count(segment_link->child.free.region) != segment_link->child.count)
	{
	    log_error("Freed nonempty pool");
	    abort();
	}

	for_range(header, segment_link->child.free.region)
	{
	    header_ref = *header;
	    assert(header_ref >= (memory_pool_header*)segment_link->child.begin);
	    assert(header_ref < (memory_pool_header*)(segment_link->child.begin + segment_link->child.count * (sizeof(memory_pool_header) + pool->alloc_size)));
	    assert(header_ref->is_allocated == false);
	    pthread_mutex_destroy(&header_ref->mutex);
	    pthread_cond_destroy(&header_ref->cond);
	}
	
	window_clear(segment_link->child.free);

	free(segment_link);
    }
    
    unlock(pool);

    pthread_mutex_destroy(&pool->mutex);

    free(pool);
}

void thread_memory_lock(void * mem)
{
    assert(mem);
    
    memory_pool_header * header = (memory_pool_header*)mem - 1;
    lock(header);
//    log_debug("Locked %p", mem);
    assert(header->is_allocated);
}

void thread_memory_unlock(void * mem)
{
    assert(mem);
    
    memory_pool_header * header = (memory_pool_header*)mem - 1;
//    log_debug("Unlocking %p", mem);
    assert(header->is_allocated);
    unlock(header);
}

void thread_memory_wait(void * mem)
{
    assert(mem);
    
    memory_pool_header * header = (memory_pool_header*)mem - 1;
    wait(header);
    assert(header->is_allocated);
}

void thread_memory_signal(void * mem)
{
    assert(mem);
    
    memory_pool_header * header = (memory_pool_header*)mem - 1;
    signal(header);
}

void thread_memory_broadcast(void * mem)
{
    assert(mem);
    
    memory_pool_header * header = (memory_pool_header*)mem - 1;
    broadcast(header);
}

void * thread_memory_pool_calloc_from_peer(void * mem)
{
    assert(mem);
    
    memory_pool_header * header = (memory_pool_header*)mem - 1;
    return thread_memory_pool_calloc_from_pool(header->pool);
}
