#include "../../memory-pool.h"
#include "../../../range/def.h"
#include "../../../window/def.h"
#include <assert.h>
#include "../../../window/alloc.h"
#include "../../../log/log.h"

thread_memory_pool_declare(test, size_t);
thread_memory_pool_define_alloc(test);
range_typedef(size_t*,size_t_p);
window_typedef(size_t*,size_t_p);

void test_iteration(test_memory_pool * pool, size_t size)
{
    size_t * ref;

    window_size_t_p have = {0};

    for(size_t i = 0; i < size; i++)
    {
	ref = test_memory_calloc_from_pool(pool);
	assert (!*ref);
	*ref = i;
	*window_push(have) = ref;
	thread_memory_unlock(ref);
    }

    for(size_t i = 0; i < size; i++)
    {
	ref = have.region.begin[i];
	thread_memory_lock(ref);
	assert(*ref == i);
	thread_memory_unlock(ref);
    }
    
    size_t half = size / 2;

    size_t quarter = half / 2;

    assert ((size_t)range_count(have.region) == size);

    for(size_t i = quarter; i < quarter + half; i++)
    {
	assert(i < (size_t)range_count(have.region));
	
	ref = have.region.begin[i];
	
	thread_memory_lock(ref);
	assert (*ref == i);
	thread_memory_free(ref);
    }

    for(size_t i = 0; i < quarter; i++)
    {
	ref = have.region.begin[i];
	thread_memory_lock(ref);
	assert(*ref == i);
	thread_memory_unlock(ref);
    }
    
    for(size_t i = quarter + half; i < size; i++)
    {
	ref = have.region.begin[i];
	thread_memory_lock(ref);
	assert(*ref == i);
	thread_memory_unlock(ref);
    }
    
    for(size_t i = quarter; i < quarter + half; i++)
    {
	have.region.begin[i] = ref = test_memory_calloc_from_pool(pool);

	*ref = i;
	thread_memory_unlock(ref);
    }

    for(size_t i = 0; i < size; i++)
    {
	ref = have.region.begin[i];
	thread_memory_lock(ref);
	assert(*ref == i);
	thread_memory_unlock(ref);
    }
    
    for(size_t i = 0; i < size; i++)
    {
	ref = have.region.begin[i];
	thread_memory_lock(ref);
	assert(*ref == i);
	thread_memory_free(ref);
    }

    window_clear(have);
}

int main()
{
    test_memory_pool * pool = test_memory_pool_new();

    test_iteration(pool, 512);
    test_iteration(pool, 1024);
    test_iteration(pool, 10240);
    test_iteration(pool, 65536);

    test_memory_pool_free(pool);
    
    return 0;
}
