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

#ifndef _MBUF_MEM_GENERIC_H_
#define _MBUF_MEM_GENERIC_H_

#include <media-buffers/mbuf_mem.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern MBUF_API const uint64_t mbuf_mem_generic_cookie;

extern MBUF_API const uint64_t mbuf_mem_generic_wrap_cookie;

extern MBUF_API struct mbuf_mem_implem *mbuf_mem_generic_impl;

/**
 * Release function prototype for mbuf_mem_generic_wrap().
 *
 * This function will be called when the memory is released.
 *
 * @param data: The data pointer passed to mbuf_mem_generic_wrap().
 * @param len: The len passed to mbuf_mem_generic_wrap().
 * @param userdata: The userdata passed to mbuf_mem_generic_wrap().
 */
typedef void (*mbuf_mem_generic_wrap_release_t)(void *data,
						size_t len,
						void *userdata);


/**
 * Create a new mbuf_mem from an internally malloc'd buffer.
 *
 * The returned memory does not belong to any pool, it is created by this call,
 * and destroyed when released.
 *
 * @param capacity: The required capacity of the memory object.
 * @param ret_obj: [out] Pointer to the memory.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_mem_generic_new(size_t capacity, struct mbuf_mem **ret_obj);

/**
 * Wrap an existing buffer into a mbuf_mem object.
 *
 * The returned memory does not belong to any pool, it is created by this call,
 * and destroyed when released. Before destroying the memory, the release
 * callback will be called, so the calling program can cleanup the buffer.
 *
 * @param data: The data to wrap.
 * @param len: The length of the data to wrap.
 * @param release: The callback called when the mbuf_mem will be released.
 *                 Optional, can be NULL.
 * @param userdata: Parameter passed to the release callback.
 * @param ret_obj: [out] Pointer to the memory.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_mem_generic_wrap(void *data,
				   size_t len,
				   mbuf_mem_generic_wrap_release_t release,
				   void *userdata,
				   struct mbuf_mem **ret_obj);


/**
 * Generic release function for malloc'd memory.
 *
 * This function simply calls free(data), but has the right prototype to be
 * given as a release function to mbuf_mem_generic_wrap().
 *
 * @note This function is not intended to be called directly.
 *
 * @param data: Pointer to free.
 * @param len: Ignored.
 * @param userdata: Ignored.
 */
MBUF_API void
mbuf_mem_generic_releaser_free(void *data, size_t len, void *userdata);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MBUF_MEM_GENERIC_H_ */
