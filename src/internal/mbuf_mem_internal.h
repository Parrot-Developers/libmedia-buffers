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

#ifndef _MBUF_MEM_INTERNAL_H_
#define _MBUF_MEM_INTERNAL_H_

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include <futils/list.h>
#include <media-buffers/mbuf_mem.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct mbuf_mem_implem {
	/* MBuf Memory implementation. All functions are optional */

	/*
	 * This function is called when the memory is created, either during
	 * pool creation, or because of a pool grow.
	 * This function should set the implementation cookie, and allocate any
	 * value that does not changes during the memory life.
	 *
	 * If this function returns an error, the pool_new/pool_get call will
	 * be aborted.
	 *
	 * @param mem: The memory to initialize
	 * @param specific: The implementation specific data.
	 * @return 0 on success, negative errno on error.
	 */
	int (*alloc)(struct mbuf_mem *mem, void *specific);


	/*
	 * This function is called when the memory is taken out of its pool.
	 * This function allows late binding of memory (so an in-pool memory
	 * does not actually consumes memory)
	 *
	 * If this function returns an error, the pool_get call will be aborted.
	 *
	 * @param mem: The memory to initialize
	 * @param specific: The implementation specific data.
	 * @return 0 on success, negative errno on error.
	 */
	int (*pool_get)(struct mbuf_mem *mem, void *specific);


	/*
	 * This function is called when the memory is returned to its pool.
	 * This function cannot fail.
	 *
	 * @param mem: The memory to deinitialize
	 * @param specific: The implementation specific data.
	 */
	void (*pool_put)(struct mbuf_mem *mem, void *specific);


	/*
	 * This function is called when the memory is destroyed.
	 * This function cannot fail.
	 *
	 * @param mem: The memory to deinitialize
	 * @param specific: The implementation specific data.
	 */
	void (*free)(struct mbuf_mem *mem, void *specific);


	/* Pointer passed to all previous functions */
	void *specific;
};

struct mbuf_mem {
	/* Content */
	void *data;
	size_t size;

	/* Implementation details */
	uint64_t cookie;
	void *specific;
	const struct mbuf_mem_implem *implem;

	/* Reference count + originating pool */
	atomic_uint refcount;
	struct mbuf_pool *pool;
	struct list_node node;
};

struct mbuf_pool {
	const struct mbuf_mem_implem *implem;
	enum mbuf_pool_grow_policy policy;
	size_t mem_size;
	size_t initial_mem_count;
	size_t max_mem_count;
	size_t mem_count;
	size_t mem_free;
	struct list_node memories;
	char *name;

	pthread_mutex_t lock;
	bool lock_created;
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MBUF_MEM_INTERNAL_H_ */
