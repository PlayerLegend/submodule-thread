#include "../../thread-pool.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include "../../../log/log.h"
#include <math.h>

typedef struct {
    int count;
    pthread_mutex_t mutex;
}
    root_result;

root_result * root_result_new()
{
    root_result * retval = calloc(1, sizeof(*retval));

    pthread_mutex_init(&retval->mutex, NULL);

    return retval;
}

thread_job_declare(leaf);
thread_job_declare(root);
thread_job_define_arg(root, struct { root_result * result; });
thread_job_define_arg(leaf, struct { int count; root_result * result; });

#define lock(target) pthread_mutex_lock(&(target)->mutex)
#define unlock(target) pthread_mutex_unlock(&(target)->mutex)
#define wait(target) pthread_cond_wait(&(target)->cond, &(target)->mutex)
#define signal(target) pthread_cond_signal(&(target)->cond)
#define broadcast(target) pthread_cond_broadcast(&(target)->cond)

thread_job_define_function(leaf)
{
    assert(parent);
    
    lock(arg->result);
    arg->result->count++;
    unlock(arg->result);

    float f = 3.14159;
    
    for(int i = 0; i < 5; i++)
    {
	sqrt(f * i);
    }

    for (int i = 1; i <= arg->count; i++)
    {
	leaf_job * peer = leaf_job_memory_calloc_from_peer(self);
	*leaf_job_init(peer) = (leaf_job_arg){ arg->count - 1, arg->result };

	thread_job_memory_lock(parent);
	thread_job_add_child(parent, leaf_job_generic(peer));
	thread_job_memory_unlock(parent);

	leaf_job * child = leaf_job_memory_calloc_from_peer(self);

	*leaf_job_init(child) = (leaf_job_arg){ arg->count - 1, arg->result };

	leaf_job_add_child(peer, leaf_job_generic(child));
	
	leaf_job_memory_unlock(peer);

	thread_pool_add_leaf_job(pool, child);

	//leaf_job_memory_unlock(child);
    }
}

thread_job_define_function(root)
{
    printf("root %d\n", arg->result->count);
    pthread_mutex_destroy(&arg->result->mutex);
    free(arg->result);
    thread_pool_quit(pool);
}

int main()
{
    log_debug("start");

    root_job_memory_calloc_init();
    leaf_job_memory_calloc_init();
    
    root_job * root_job = root_job_memory_calloc();

    log_debug("allocated root job %p", root_job);
    
    root_result * result = root_result_new();
    
    log_debug("allocated result");

    *root_job_init(root_job) = (root_job_arg){ result };

    log_debug("initialized root");

    root_job_memory_unlock(root_job);

    log_debug("unlocked root");
    
    leaf_job * leaf_job = leaf_job_memory_calloc();

    log_debug("allocated leaf job");

    *leaf_job_init(leaf_job) = (leaf_job_arg){ 6, result };

    root_job_add_child(root_job, leaf_job_generic(leaf_job));

    log_debug("initialized leaf job");

    thread_pool_host(2, leaf_job_generic(leaf_job));
    
    return 0;
}
