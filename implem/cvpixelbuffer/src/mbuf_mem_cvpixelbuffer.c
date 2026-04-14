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

#include <media-buffers/mbuf_mem_cvpixelbuffer.h>

#include "mbuf_mem_internal.h"

#include <errno.h>
#include <stdlib.h>

#define ULOG_TAG mbuf_mem_cvpixelbuffer
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);


/* CVPixelBuffer implementation memory specific */
struct mem_cvpixelbuffer_specific {
	CVPixelBufferRef buf_ref;
	bool locked;
	struct mbuf_cvpixelbuffer_attrs attrs;
};


/* CVPixelBuffer implementation implem specific */
struct impl_cvpixelbuffer_specific {
	struct mbuf_cvpixelbuffer_attrs attrs;
};


/* Cookie is 'cvpixbuf' in ascii coding */
const uint64_t mbuf_mem_cvpixelbuffer_cookie = UINT64_C(0x6376706978627566);


static int cvpixelbuffer_alloc(struct mbuf_mem *mem, void *specific)
{
	struct impl_cvpixelbuffer_specific *impl_specific = specific;
	struct mem_cvpixelbuffer_specific *cvpixelbuffer_mem = NULL;
	int ret = 0;
	CVReturn err;
	CFDictionaryRef empty = NULL;
	CFMutableDictionaryRef attrs = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(mem->specific, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mem->data, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!impl_specific, EINVAL);

	cvpixelbuffer_mem = calloc(1, sizeof(*cvpixelbuffer_mem));
	if (!cvpixelbuffer_mem)
		return -ENOMEM;
	cvpixelbuffer_mem->attrs = impl_specific->attrs;

	empty = CFDictionaryCreate(kCFAllocatorDefault,
				   NULL,
				   NULL,
				   0,
				   &kCFTypeDictionaryKeyCallBacks,
				   &kCFTypeDictionaryValueCallBacks);
	if (empty == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("CFDictionaryCreate", -ret);
		goto out;
	}

	attrs = CFDictionaryCreateMutable(kCFAllocatorDefault,
					  1,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
	if (attrs == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("CFDictionaryCreateMutable", -ret);
		goto out;
	}

	CFDictionarySetValue(
		attrs, kCVPixelBufferIOSurfacePropertiesKey, empty);

	err = CVPixelBufferCreate(kCFAllocatorDefault,
				  cvpixelbuffer_mem->attrs.width,
				  cvpixelbuffer_mem->attrs.height,
				  cvpixelbuffer_mem->attrs.format,
				  attrs,
				  &cvpixelbuffer_mem->buf_ref);
	if (err != kCVReturnSuccess) {
		ret = -ENOMEM;
		ULOG_ERRNO("CVPixelBufferCreate(%d)", -ret, err);
		goto out;
	}

	mem->size = CVPixelBufferGetDataSize(cvpixelbuffer_mem->buf_ref);
	mem->cookie = mbuf_mem_cvpixelbuffer_cookie;
	mem->specific = cvpixelbuffer_mem;

out:
	if ((ret != 0) && (cvpixelbuffer_mem->buf_ref != NULL)) {
		CVPixelBufferRelease(cvpixelbuffer_mem->buf_ref);
		cvpixelbuffer_mem->buf_ref = NULL;
	}
	if (attrs != NULL)
		CFRelease(attrs);
	if (empty != NULL)
		CFRelease(empty);

	return ret;
}


static void cvpixelbuffer_free(struct mbuf_mem *mem, void *specific)
{
	struct mem_cvpixelbuffer_specific *cvpixelbuffer_mem = mem->specific;
	struct impl_cvpixelbuffer_specific *impl_specific = specific;

	ULOG_ERRNO_RETURN_IF(mem->cookie != mbuf_mem_cvpixelbuffer_cookie,
			     EINVAL);
	ULOG_ERRNO_RETURN_IF(!cvpixelbuffer_mem, EINVAL);
	ULOG_ERRNO_RETURN_IF(!impl_specific, EINVAL);

	if (cvpixelbuffer_mem->locked) {
		CVReturn err = CVPixelBufferUnlockBaseAddress(
			cvpixelbuffer_mem->buf_ref,
			cvpixelbuffer_mem->attrs.map_read_only
				? kCVPixelBufferLock_ReadOnly
				: 0);
		if (err != kCVReturnSuccess) {
			int ret = -EPROTO;
			ULOGW_ERRNO(-ret,
				    "CVPixelBufferUnlockBaseAddress(%d)",
				    err);
		}
	}

	CVPixelBufferRelease(cvpixelbuffer_mem->buf_ref);
	cvpixelbuffer_mem->buf_ref = NULL;

	mem->specific = NULL;
	mem->data = NULL;

	free(cvpixelbuffer_mem);

	return;
}


struct mbuf_mem_implem *
mbuf_mem_cvpixelbuffer_get_implem(const struct mbuf_cvpixelbuffer_attrs *attrs)
{
	struct impl_cvpixelbuffer_specific *impl_specific = NULL;
	struct mbuf_mem_implem *impl = NULL;

	ULOG_ERRNO_RETURN_VAL_IF(!attrs, EINVAL, NULL);
	ULOG_ERRNO_RETURN_VAL_IF(attrs->width == 0, EINVAL, NULL);
	ULOG_ERRNO_RETURN_VAL_IF(attrs->height == 0, EINVAL, NULL);

	impl_specific = calloc(1, sizeof(*impl_specific));
	if (!impl_specific) {
		ULOG_ERRNO("calloc", ENOMEM);
		goto error;
	}
	impl_specific->attrs = *attrs;

	impl = calloc(1, sizeof(*impl));
	if (!impl) {
		ULOG_ERRNO("calloc", ENOMEM);
		goto error;
	}
	impl->alloc = cvpixelbuffer_alloc;
	impl->free = cvpixelbuffer_free;
	impl->specific = impl_specific;

	return impl;

error:
	free(impl_specific);
	free(impl);
	return NULL;
}


void mbuf_mem_cvpixelbuffer_release_implem(struct mbuf_mem_implem *implem)
{
	if (!implem)
		return;

	free(implem->specific);
	free(implem);
}


CVPixelBufferRef mbuf_mem_cvpixelbuffer_get_buffer_ref(struct mbuf_mem *mem)
{
	ULOG_ERRNO_RETURN_VAL_IF(!mem, EINVAL, NULL);
	ULOG_ERRNO_RETURN_VAL_IF(!mem->specific, EINVAL, NULL);
	ULOG_ERRNO_RETURN_VAL_IF(
		mem->cookie != mbuf_mem_cvpixelbuffer_cookie, EINVAL, NULL);

	struct mem_cvpixelbuffer_specific *cvpixelbuffer_mem = mem->specific;

	return cvpixelbuffer_mem->buf_ref;
}


CVPixelBufferRef mbuf_mem_cvpixelbuffer_get_buffer_ref_specific(void *specific)
{
	ULOG_ERRNO_RETURN_VAL_IF(!specific, EINVAL, NULL);

	struct mem_cvpixelbuffer_specific *cvpixelbuffer_mem = specific;

	return cvpixelbuffer_mem->buf_ref;
}


int mbuf_mem_cvpixelbuffer_map(struct mbuf_mem *mem)
{

	ULOG_ERRNO_RETURN_ERR_IF(!mem, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mem->specific, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mem->cookie != mbuf_mem_cvpixelbuffer_cookie,
				 EINVAL);

	struct mem_cvpixelbuffer_specific *cvpixelbuffer_mem = mem->specific;

	if (cvpixelbuffer_mem->locked)
		return 0;

	CVReturn err = CVPixelBufferLockBaseAddress(
		cvpixelbuffer_mem->buf_ref,
		cvpixelbuffer_mem->attrs.map_read_only
			? kCVPixelBufferLock_ReadOnly
			: 0);
	if (err != kCVReturnSuccess) {
		int ret = -EPROTO;
		ULOG_ERRNO("CVPixelBufferLockBaseAddress(%d)", -ret, err);
		return ret;
	}
	cvpixelbuffer_mem->locked = true;

	mem->data = CVPixelBufferGetBaseAddress(cvpixelbuffer_mem->buf_ref);
	if (mem->data == NULL) {
		int ret = -EADDRNOTAVAIL;
		ULOG_ERRNO("CVPixelBufferGetBaseAddress", -ret);
		return ret;
	}

	return 0;
}


void mbuf_mem_cvpixelbuffer_unmap(struct mbuf_mem *mem)
{
	ULOG_ERRNO_RETURN_IF(!mem, EINVAL);
	ULOG_ERRNO_RETURN_IF(!mem->specific, EINVAL);
	ULOG_ERRNO_RETURN_IF(mem->cookie != mbuf_mem_cvpixelbuffer_cookie,
			     EINVAL);

	struct mem_cvpixelbuffer_specific *cvpixelbuffer_mem = mem->specific;

	if (!cvpixelbuffer_mem->locked)
		return;

	CVReturn err = CVPixelBufferUnlockBaseAddress(
		cvpixelbuffer_mem->buf_ref,
		cvpixelbuffer_mem->attrs.map_read_only
			? kCVPixelBufferLock_ReadOnly
			: 0);
	if (err != kCVReturnSuccess) {
		int ret = -EPROTO;
		ULOG_ERRNO("CVPixelBufferUnlockBaseAddress(%d)", -ret, err);
		return;
	}
	cvpixelbuffer_mem->locked = false;

	mem->data = NULL;

	return;
}
