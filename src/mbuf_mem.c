/**
 * Copyright (c) 2019 Parrot Drones SAS
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Parrot Drones SAS Company nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE PARROT DRONES SAS COMPANY BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <media-buffers/mbuf_mem.h>

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#define ULOG_TAG mbuf_mem
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);

#include "internal/mbuf_mem_internal.h"

#define MBUF_POOL_DEFAULT_NAME "default"


static int call_alloc(const struct mbuf_mem_implem *implem,
		      struct mbuf_mem *mem)
{
	if (!implem->alloc)
		return 0;
	return implem->alloc(mem, implem->specific);
}


static int call_pool_get(const struct mbuf_mem_implem *implem,
			 struct mbuf_mem *mem)
{
	if (!implem->pool_get)
		return 0;
	return implem->pool_get(mem, implem->specific);
}


static void call_pool_put(const struct mbuf_mem_implem *implem,
			  struct mbuf_mem *mem)
{
	if (!implem->pool_put)
		return;
	implem->pool_put(mem, implem->specific);
}


static void call_free(const struct mbuf_mem_implem *implem,
		      struct mbuf_mem *mem)
{
	if (!implem->free)
		return;
	implem->free(mem, implem->specific);
}


int mbuf_pool_new(const struct mbuf_mem_implem *implem,
		  size_t mem_size,
		  size_t mem_count,
		  enum mbuf_pool_grow_policy grow_policy,
		  size_t max_mem_count,
		  const char *name,
		  struct mbuf_pool **ret_obj)
{
	struct mbuf_pool *pool;
	int ret = 0;

	ULOG_ERRNO_RETURN_ERR_IF(!implem, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!ret_obj, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(max_mem_count > 0 && max_mem_count < mem_count,
				 EINVAL);

	pool = calloc(1, sizeof(*pool));
	if (!pool)
		return -ENOMEM;

	list_init(&pool->memories);
	pool->implem = implem;
	pool->initial_mem_count = mem_count;
	pool->mem_count = mem_count;
	pool->mem_size = mem_size;
	pool->max_mem_count = max_mem_count;
	pool->mem_free = mem_count;
	pool->policy = grow_policy;
	pool->name = name ? strdup(name) : strdup(MBUF_POOL_DEFAULT_NAME);
	if (!pool->name) {
		ret = -ENOMEM;
		goto error;
	}

	ret = pthread_mutex_init(&pool->lock, NULL);
	if (ret != 0) {
		ret = -ret;
		goto error;
	}
	pool->lock_created = true;
	for (size_t i = 0; i < pool->mem_count; i++) {
		struct mbuf_mem *mem = calloc(1, sizeof(*mem));
		if (!mem) {
			ret = -ENOMEM;
			goto error;
		}
		mem->size = mem_size;
		mem->pool = pool;
		ret = call_alloc(pool->implem, mem);
		if (ret != 0) {
			free(mem);
			goto error;
		}
		list_add_before(&pool->memories, &mem->node);
	}

	*ret_obj = pool;
	return 0;

error:
	mbuf_pool_destroy(pool);
	return ret;
}


int mbuf_pool_get(struct mbuf_pool *pool, struct mbuf_mem **ret_obj)
{
	struct mbuf_mem *mem, *tmp;
	unsigned int zero;
	int ret;

	ULOG_ERRNO_RETURN_ERR_IF(!pool, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!ret_obj, EINVAL);

	ret = pthread_mutex_lock(&pool->lock);

	list_walk_entry_forward_safe(&pool->memories, mem, tmp, node)
	{
		zero = 0;
		if (atomic_compare_exchange_strong(&mem->refcount, &zero, 1)) {
			/* We got a free memory from the pool, use it and
			 * reduce the free memory count */
			*ret_obj = mem;
			ret = call_pool_get(pool->implem, mem);
			if (ret != 0)
				atomic_store(&mem->refcount, 0);
			else
				pool->mem_free--;
			goto exit;
		}
	}
	if (pool->policy == MBUF_POOL_NO_GROW) {
		ret = -EAGAIN;
		goto exit;
	}
	if (pool->max_mem_count > 0 && pool->mem_count >= pool->max_mem_count) {
		ret = -EAGAIN;
		goto exit;
	}
	mem = calloc(1, sizeof(*mem));
	if (!mem) {
		ret = -ENOMEM;
		goto exit;
	}
	mem->size = pool->mem_size;
	mem->pool = pool;
	ret = call_alloc(pool->implem, mem);
	if (ret != 0) {
		free(mem);
		goto exit;
	}
	list_add_before(&pool->memories, &mem->node);
	pool->mem_count++;
	atomic_store(&mem->refcount, 1);
	ret = call_pool_get(pool->implem, mem);
	if (ret != 0) {
		call_free(pool->implem, mem);
		free(mem);
		goto exit;
	}
	*ret_obj = mem;
exit:
	pthread_mutex_unlock(&pool->lock);
	return ret;
}


const char *mbuf_pool_get_name(struct mbuf_pool *pool)
{
	ULOG_ERRNO_RETURN_VAL_IF(!pool, EINVAL, NULL);

	return pool->name;
}


int mbuf_pool_get_count(struct mbuf_pool *pool, size_t *count, size_t *free)
{
	ULOG_ERRNO_RETURN_ERR_IF(!pool, EINVAL);

	pthread_mutex_lock(&pool->lock);

	if (count)
		*count = pool->mem_count;
	if (free)
		*free = pool->mem_free;

	pthread_mutex_unlock(&pool->lock);
	return 0;
}


int mbuf_pool_destroy(struct mbuf_pool *pool)
{
	struct mbuf_mem *mem, *tmp;
	unsigned int zero;

	if (!pool)
		return 0;

	if (pool->lock_created)
		pthread_mutex_lock(&pool->lock);

	list_walk_entry_forward_safe(&pool->memories, mem, tmp, node)
	{
		zero = 0;
		if (!atomic_compare_exchange_strong(&mem->refcount, &zero, 1))
			ULOGW("pool %s: memory %p not released",
			      pool->name,
			      mem);
		list_del(&mem->node);
		call_free(pool->implem, mem);
		free(mem);
	}

	if (pool->lock_created) {
		pthread_mutex_unlock(&pool->lock);
		pthread_mutex_destroy(&pool->lock);
	}
	free(pool->name);
	free(pool);
	return 0;
}


int mbuf_mem_ref(struct mbuf_mem *mem)
{
	ULOG_ERRNO_RETURN_ERR_IF(!mem, EINVAL);
	atomic_fetch_add(&mem->refcount, 1);
	return 0;
}


static void mbuf_pool_put(struct mbuf_pool *pool, struct mbuf_mem *mem)
{
	pool->mem_free++;

	call_pool_put(pool->implem, mem);

	bool release;
	switch (pool->policy) {
	case MBUF_POOL_NO_GROW:
	case MBUF_POOL_GROW:
		release = false;
		break;
	case MBUF_POOL_SMART_GROW:
		release = pool->mem_free > pool->initial_mem_count;
		break;
	case MBUF_POOL_LOW_MEM_GROW:
		release = pool->mem_count > pool->initial_mem_count;
		break;
	default:
		ULOGW("pool %s: unknown policy %d", pool->name, pool->policy);
		release = false;
		break;
	}

	if (!release)
		return;

	list_del(&mem->node);
	pool->mem_count--;
	pool->mem_free--;
	call_free(pool->implem, mem);
	free(mem);
}


static bool mbuf_mem_unref_internal(struct mbuf_mem **mem_)
{
	bool ok;
	struct mbuf_mem *mem = *mem_;
	if (!mem) {
		return true;
	};
	pthread_mutex_t *lock;
	unsigned int curr = atomic_load(&mem->refcount);

	/* Handle the easy case first: unref, but not release */
	if (curr > 1) {
		unsigned int next = curr - 1;
		return atomic_compare_exchange_weak(
			&mem->refcount, &curr, next);
	}

	/* If the ref count should reach zero, check if the memory does not
	 * belong to a pool. In this case, free it directly */
	if (!mem->pool) {
		ok = atomic_compare_exchange_weak(&mem->refcount, &curr, 0);
		if (!ok)
			return false;
		call_free(mem->implem, mem);
		free(mem);
		*mem_ = NULL;
		return true;
	}

	/* Otherwise, the release must be done while holding the pool lock */
	lock = &mem->pool->lock;
	pthread_mutex_lock(lock);

	ok = atomic_compare_exchange_weak(&mem->refcount, &curr, 0);
	if (!ok)
		goto exit;
	mbuf_pool_put(mem->pool, mem);
	*mem_ = NULL;

exit:
	pthread_mutex_unlock(lock);
	return ok;
}


int mbuf_mem_unref(struct mbuf_mem *mem)
{
	ULOG_ERRNO_RETURN_ERR_IF(!mem, EINVAL);
	bool ok;

	do {
		ok = mbuf_mem_unref_internal(&mem);
	} while (!ok);

	return 0;
}


int mbuf_mem_get_data(struct mbuf_mem *mem, void **data, size_t *capacity)
{
	ULOG_ERRNO_RETURN_ERR_IF(!mem, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!data, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!capacity, EINVAL);

	*data = mem->data;
	*capacity = mem->size;
	return 0;
}


int mbuf_mem_get_info(struct mbuf_mem *mem, struct mbuf_mem_info *info)
{
	ULOG_ERRNO_RETURN_ERR_IF(!mem, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!info, EINVAL);

	info->cookie = mem->cookie;
	info->specific = mem->specific;
	return 0;
}
