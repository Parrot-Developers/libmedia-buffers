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

#ifndef _MBUF_MEM_H_
#define _MBUF_MEM_H_

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* To be used for all public API */
#ifdef MBUF_API_EXPORTS
#	ifdef _WIN32
#		define MBUF_API __declspec(dllexport)
#	else /* !_WIN32 */
#		define MBUF_API __attribute__((visibility("default")))
#	endif /* !_WIN32 */
#else /* !MBUF_API_EXPORTS */
#	define MBUF_API
#endif /* !MBUF_API_EXPORTS */


struct mbuf_mem;
struct mbuf_mem_implem;
struct mbuf_pool;


/**
 * Pool behavior when a memory chunk is requested while the pool is empty.
 * For all policies, if the number of buffers is already equal to the maximum,
 * the pool will return -EAGAIN (and issue a warning log).
 */
enum mbuf_pool_grow_policy {
	/* Do not allocate a new memory, return -EAGAIN. */
	MBUF_POOL_NO_GROW = 0,

	/* Allocate a new memory and keep it in the pool until the pool is
	 * destroyed */
	MBUF_POOL_GROW,

	/* Allocate a new memory, but release one from the pool if the number
	 * of available memories goes over the initial capacity of the pool */
	MBUF_POOL_SMART_GROW,

	/* Allocate a new memory, but release one from the pool as soon as
	 * possible while the pool size is greater than its capacity */
	MBUF_POOL_LOW_MEM_GROW,
};


/**
 * Structure filled by the various mbuf_xxx_frame_get_zzz_mem_info() functions.
 */
struct mbuf_mem_info {
	uint64_t cookie;
	void *specific;
};


/**
 * Create a new memory pool with the given parameters.
 *
 * @param implem: Memory implementation for the pool.
 * @param mem_size: Size of a memory chunk in the pool.
 * @param mem_count: Initial number of memory chunks in the pool.
 * @param grow_policy: Grow policy of the pool.
 * @param max_mem_count: Maximum number of memory chunks in the pool.
 *   Only relevant if grow_policy is not MBUF_POOL_NO_GROW. 0 means no maximum.
 * @param name: Optional name of the pool. If NULL, the pool will be named
 *   "default".
 * @param ret_obj: [out] Pointer to the new pool object.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_pool_new(const struct mbuf_mem_implem *implem,
			   size_t mem_size,
			   size_t mem_count,
			   enum mbuf_pool_grow_policy grow_policy,
			   size_t max_mem_count,
			   const char *name,
			   struct mbuf_pool **ret_obj);


/**
 * Get the name of a pool.
 *
 * The returned pointer has the same lifetime as the pool. It should not be
 * freed by the caller.
 *
 * @param pool: The memory pool.
 *
 * @return The pool name, or NULL if the pool is invalid.
 */
MBUF_API const char *mbuf_pool_get_name(struct mbuf_pool *pool);


/**
 * Get a memory from the pool.
 *
 * This function never blocks. If no buffers are available in the pool, this
 * function either returns -EAGAIN or allocates a new memory, depending on the
 * pool grow_policy parameter. If the allocation of new memory fails, the
 * function returns -ENOMEM.
 *
 * @param pool: The memory pool.
 * @param ret_obj: [out] Pointer to the memory.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_pool_get(struct mbuf_pool *pool, struct mbuf_mem **ret_obj);


/**
 * Get the current number of memory chunks in the pool.
 *
 * @param pool: The memory pool.
 * @param count: [out] Total number of chunks in the pool, can be NULL.
 * @param free: [out] Number of free chunks in the pool, can be NULL.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int
mbuf_pool_get_count(struct mbuf_pool *pool, size_t *count, size_t *free);


/**
 * Destroy a memory pool.
 *
 * If a pool is destroyed while not all memories are free, the library will
 * send a warning log, but still delete all the memories. This usually leads
 * to undefined behavior, and denotes a bug in the calling program.
 *
 * @param pool: The memory pool.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_pool_destroy(struct mbuf_pool *pool);


/**
 * Increment the reference count of a memory chunk.
 *
 * Note that a memory returned by mbuf_pool_get() already has a refcount of 1.
 * This call is only needed if more than one object keep a reference on the
 * memory at the same time.
 *
 * @param mem: The memory to reference.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_mem_ref(struct mbuf_mem *mem);


/**
 * Decrement the reference count of a memory chunk.
 *
 * When the reference count reaches zero, the memory will be returned to the
 * pool. All memory needs to be returned to the pool before the pool is
 * destroyed.
 *
 * @param mem: The memory to unreference.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_mem_unref(struct mbuf_mem *mem);


/**
 * Get a read-write view of the memory.
 *
 * A memory consist of 'capacity' bytes of contiguous memory, at address 'data'.
 *
 * When a memory is attached to a frame (regardless of its type), it should no
 * longer be modified.
 * This API is only intended for mbuf_xxx_frame providers to fill the memories
 * before finalizing the frame.
 *
 * @param mem: The memory.
 * @param data: [out] The memory base pointer.
 * @param capacity: [out] The memory capacity.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int
mbuf_mem_get_data(struct mbuf_mem *mem, void **data, size_t *capacity);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MBUF_MEM_H_ */
