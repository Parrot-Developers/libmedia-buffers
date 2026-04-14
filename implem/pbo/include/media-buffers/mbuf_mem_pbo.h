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

#ifndef _MBUF_MEM_PBO_H_
#define _MBUF_MEM_PBO_H_

#include <media-buffers/mbuf_mem.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern MBUF_API const uint64_t mbuf_mem_pbo_cookie;


/* Memory implementation attributes */
struct mbuf_pbo_attrs {
	/* Target to which a buffer object is bound; should be
	 * GL_PIXEL_PACK_BUFFER or GL_PIXEL_UNPACK_BUFFER */
	int target;

	/* Expected usage pattern of the data store; e.g. GL_STREAM_DRAW or
	 * GL_STREAM_READ */
	int usage;

	/* Combination of access flags; e.g. GL_MAP_READ_BIT or
	 * GL_MAP_WRITE_BIT */
	int access;

	/* Size of each memory */
	size_t mem_size;
};


/**
 * Get a mbuf_mem_implem structure for the given attributes.
 *
 * The returned implem must be released by calling
 * mbuf_mem_pbo_release_implem() after all PBO buffers have been released.
 *
 * @note The attrs argument is copied internally and does not need to stay valid
 * until mbuf_mem_pbo_release_implem() is called.
 *
 * @param attrs: PBO attributes
 * @return The memory implementation structure, or NULL on error.
 */
MBUF_API struct mbuf_mem_implem *
mbuf_mem_pbo_get_implem(const struct mbuf_pbo_attrs *attrs);


/**
 * Release a mbuf_mem_implem structure.
 *
 * The implem must no longer be used after this call.
 *
 * @warning This call only works on implementations returned by
 * mbuf_mem_pbo_get_implem(), or a NULL pointer. Calling this function with
 * another implementation will cause undefined behavior.
 *
 * @param implem: The implem to release.
 */
MBUF_API void mbuf_mem_pbo_release_implem(struct mbuf_mem_implem *implem);


/**
 * Get the PBO name from a mbuf_mem object.
 *
 * @note this function checks whether the memory is effectively allocated with
 * this implementation.
 *
 * @param mem: The memory to use
 *
 * @return a valid PBO name that should be casted to GLuint,
 *         negative errno on error.
 */
MBUF_API int mbuf_mem_pbo_get_gl_name(struct mbuf_mem *mem);


/**
 * Map the PBO memory to the CPU.
 *
 * @note this function checks whether the memory is effectively allocated with
 * this implementation.
 *
 * @param mem: The memory to use
 *
 * @return 0 on succues, negative errno on error.
 */
MBUF_API int mbuf_mem_pbo_map(struct mbuf_mem *mem);


/**
 * Unmap the PBO memory from the CPU.
 *
 * @note this function checks whether the memory is effectively allocated with
 * this implementation.
 *
 * @param mem: The memory to use
 */
MBUF_API void mbuf_mem_pbo_unmap(struct mbuf_mem *mem);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MBUF_MEM_PBO_H_ */
