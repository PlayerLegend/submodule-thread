#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "thread-pool.h"
#include "../range/def.h"
#include "../range/alloc.h"
#include "../window/def.h"
#include "../window/alloc.h"
#include "../log/log.h"
#include <string.h>

thread_memory_pool_declare_alloc(thread_job);

#define lock(target) pthread_mutex_lock(&(target)->mutex)
#define unlock(target) pthread_mutex_unlock(&(target)->mutex)
#define wait(target) pthread_cond_wait(&(target)->cond, &(target)->mutex)
#define signal(target) pthread_cond_signal(&(target)->cond)
#define broadcast(target) pthread_cond_broadcast(&(target)->cond)

range_typedef(pthread_t, pthread_t);
range_typedef(thread_job*,thread_job_p);
window_typedef(thread_job*,thread_job_p);

struct thread_pool {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool should_quit;
    window_thread_job_p jobs;
};

struct thread_job {
    thread_job_function function;
    size_t dependency_count;
    thread_job * parent;
    bool waited;
    bool finished;
};

static bool is_in(range_thread_job_p * jobs, thread_job * job)
{
    thread_job ** i;

    for_range(i, *jobs)
    {
	if (*i == job)
	{
	    return true;
	}
    }

    return false;
}

/*static thread_job * pop_random(range_thread_job_p * list)
{
    thread_job ** index = list->begin + rand() % range_count(*list);

    thread_job * retval = *index;

    list->end--;
    
    *index = *list->end;

    assert (!is_in(list, retval));

    return retval;
    }*/

static void start_parent(thread_pool * pool, thread_job * parent)
{
    if (!parent)
    {
	return;
    }
    
    thread_job_memory_lock(parent);
    if(parent->dependency_count == 1)
    {
	parent->dependency_count = 0;
		
	lock(pool);
	*window_push(pool->jobs) = parent;
	unlock(pool);
	signal(pool);
    }
    else if (parent->dependency_count > 1)
    {
	parent->dependency_count--;
    }
    else
    {
	log_error("Parent has 0 dependencies where it should have at least 1, did a job run twice?");
	abort();
    }
    thread_job_memory_unlock(parent);
}

static void thread_job_end(thread_job * job)
{
    if (job->waited)
    {
	job->finished = true;
	thread_job_memory_signal(job);
	thread_job_memory_unlock(job);
    }
    else
    {
	thread_job_memory_free(job);
    }
}

static void flush_jobs(window_thread_job_p * cache, thread_pool * pool)
{
    while (!range_is_empty(pool->jobs.region))
    {
	size_t take = range_count(pool->jobs.region);

	size_t take_cap = 100;
	
	if (take > take_cap)
	{
	    take = take_cap;
	}
	
	
	//printf("count %zu\n", take);
        
	window_rewrite(*cache);

	/*while (take > 0)
	{
	    *window_push(*cache) = pop_random(&pool->jobs.region);
	    take--;
	    }*/

	window_alloc(*cache, take);

	memcpy(cache->region.begin, pool->jobs.region.end - take, take * sizeof(*pool->jobs.region.begin));
	pool->jobs.region.end -= take;
	cache->region.end += take;
	assert(cache->region.end <= cache->alloc.end);
	
        unlock(pool);

	thread_job * job;
	thread_job ** i;

	for_range(i, cache->region)
	{
	    job = *i;
	    
	    thread_job_memory_lock(job);
	    
	    if (job->dependency_count)
	    {
		thread_job_memory_unlock(job);
		lock(pool);
		continue;
	    }
	    
	    job->function(job, job->parent, pool, job + 1, pool->should_quit);
	    
	    thread_job * parent = job->parent;
	    
	    thread_job_end(job);
	    
	    start_parent(pool, parent);
	}
	
        lock(pool);
    }
}

static void * worker_function(void * _pool)
{
    thread_pool * pool = _pool;

    //thread_job * job;

    lock(pool);

    window_thread_job_p cache = {0};
    
    while (true)
    {
	flush_jobs(&cache, pool);

	if (pool->should_quit)
	{
	    break;
	}
	
	wait(pool);
    }

    window_clear(cache);
    
    unlock(pool);

    return NULL;
}

size_t thread_pool_job_count(thread_pool * pool)
{
    lock(pool);
    size_t retval = range_count(pool->jobs.region);
    unlock(pool);
    return retval;
}

void thread_pool_add_job(thread_pool * pool, thread_job * job)
{
    lock(pool);
//    log_debug("push %p", job);
    *window_push(pool->jobs) = job;
    
    unlock(pool);
    thread_job_memory_unlock(job);
    
    signal(pool);
}

void thread_pool_host(size_t worker_count, thread_job * first_job)
{
    if (worker_count)
    {
	worker_count--;
    }
    
    thread_pool pool = {0};
    pthread_mutex_init(&pool.mutex,NULL);
    pthread_cond_init(&pool.cond,NULL);

    range_pthread_t workers;
    range_calloc(workers, worker_count);

    pthread_t * i;

    for_range(i, workers)
    {
	pthread_create(i, NULL, worker_function, &pool);
    }

    thread_pool_add_job(&pool, first_job);

    worker_function(&pool);

    for_range(i, workers)
    {
	pthread_join(*i, NULL);
    }

    assert(range_is_empty(pool.jobs.region));

    window_clear(pool.jobs);
    
    range_clear(workers);

    pthread_mutex_destroy(&pool.mutex);
    pthread_cond_destroy(&pool.cond);
}

void thread_pool_quit(thread_pool * pool)
{
    lock(pool);
    pool->should_quit = true;
    unlock(pool);
    broadcast(pool);
}

thread_job_memory_pool * thread_job_memory_pool_new(size_t arg_size)
{
    return (thread_job_memory_pool*) thread_memory_pool_new(sizeof(thread_job) + arg_size);
}

void thread_job_add_child(thread_job * parent, thread_job * child)
{
    assert(parent);
    assert(child);
    child->parent = parent;
    parent->dependency_count++;
}

void * thread_job_init(thread_job * child, thread_job_function function)
{
    assert (!child->function);
    
    child->function = function;

    return child + 1;
}

bool thread_pool_should_quit(thread_pool * pool)
{
    return pool->should_quit;
}

void thread_job_wait(thread_pool * pool, thread_job * job)
{
    job->waited = true;

    assert(!job->finished);

    if (!job->dependency_count)
    {
	assert(!is_in(&pool->jobs.region, job));
    
	lock(pool);
	*window_push(pool->jobs) = job;
	unlock(pool);
    }
    else
    {
	//assert(is_in(&pool->jobs.region, job));
    }
    
    while (!job->finished)
    {
	thread_job_memory_wait(job);
    }

    thread_job_memory_free(job);
}
