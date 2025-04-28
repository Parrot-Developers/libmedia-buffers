/**
 * Copyright (c) 2021 Parrot Drones SAS
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
 *     derived from this software without specific prior written permissSHM.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE PARROT DRONES SAS COMPANY BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTSHM) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MBUF_MEM_SHM_H_
#define _MBUF_MEM_SHM_H_

#include <media-buffers/mbuf_mem.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


extern MBUF_API const uint64_t mbuf_mem_shm_cookie;


/**
 * Memory implementation attributes
 */
struct mbuf_shm_attr {
	/* Address of the shared memory */
	const char *addr;
	/* Size of each memory */
	size_t mem_size;
	/* Maximum number of memories */
	size_t mem_count;
};


/**
 * Get a mbuf_mem_implem structure for the given attributes.
 *
 * The returned implem must be released by calling
 * mbuf_mem_shm_release_implem() after all SHM buffers have been released.
 *
 * @note The attrs argument is copied internally and does not need to stay valid
 * until mbuf_mem_shm_release_implem() is called.
 *
 * @param attrs: SHM heap attributes (see mbuf_shm_attr doc)
 * @return The memory implementation structure, or NULL on error.
 */
MBUF_API struct mbuf_mem_implem *
mbuf_mem_shm_get_implem(const struct mbuf_shm_attr *attrs);


/**
 * Release a mbuf_mem_implem structure.
 *
 * The implem must no longer be used after this call.
 *
 * @warning This call only works on implementations returned by
 * mbuf_mem_shm_get_implem(), or a NULL pointer. Calling this function with
 * another implementation will cause undefined behavior.
 *
 * @param implem: The implem to release.
 */
MBUF_API void mbuf_mem_shm_release_implem(struct mbuf_mem_implem *implem);


/**
 * Get the SHM handle / index from a mbuf_mem object.
 *
 * @note this function checks the memory is effectively allocated with this
 * implementation.
 *
 * @param mem: The memory to use
 *
 * @return a valid index (SHM handle), negative errno on error.
 */
MBUF_API int mbuf_mem_shm_get_index(struct mbuf_mem *mem);


/**
 * Get the SHM handle / index from a mbuf_mem_info object.
 *
 * @note this function checks the memory is effectively allocated with this
 * implementation.
 *
 * @param info: The memory info to use
 *
 * @return a valid index (SHM handle), negative errno on error.
 */
MBUF_API int mbuf_mem_shm_get_index_from_info(struct mbuf_mem_info *info);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* _MBUF_MEM_SHM_H_ */
