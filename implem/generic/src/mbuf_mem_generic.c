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

#include <media-buffers/mbuf_mem_generic.h>

#include "mbuf_mem_internal.h"

#include <errno.h>
#include <stdlib.h>

#define ULOG_TAG mbuf_mem_generic
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);


/* Generic implementation based on malloc/free */


/* Cookie is 'generic ' in ascii coding */
const uint64_t mbuf_mem_generic_cookie = UINT64_C(0x67656e6572696320);


static int gen_alloc(struct mbuf_mem *mem, void *specific)
{
	mem->data = malloc(mem->size);
	if (!mem->data)
		return -ENOMEM;
	mem->cookie = mbuf_mem_generic_cookie;
	return 0;
}


static void gen_free(struct mbuf_mem *mem, void *specific)
{
	ULOG_ERRNO_RETURN_IF(mem->cookie != mbuf_mem_generic_cookie, EINVAL);

	free(mem->data);
	return;
}


static struct mbuf_mem_implem impl = {
	.alloc = gen_alloc,
	.free = gen_free,
};


struct mbuf_mem_implem *mbuf_mem_generic_impl = &impl;


/* Generic "wrapper" implementation, based on existing pointer and a release
 * callback */


/* Cookie is 'genericw' in ascii coding */
const uint64_t mbuf_mem_generic_wrap_cookie = UINT64_C(0x67656e6572696377);


struct wrap_specific {
	mbuf_mem_generic_wrap_release_t release;
	void *userdata;
};


static void wrap_free(struct mbuf_mem *mem, void *specific)
{
	ULOG_ERRNO_RETURN_IF(mem->cookie != mbuf_mem_generic_wrap_cookie,
			     EINVAL);

	struct wrap_specific *ws = mem->specific;
	if (ws->release)
		ws->release(mem->data, mem->size, ws->userdata);

	free(ws);
	return;
}


static struct mbuf_mem_implem wrap_impl = {
	.free = wrap_free,
};


int mbuf_mem_generic_new(size_t capacity, struct mbuf_mem **ret_obj)
{
	ULOG_ERRNO_RETURN_ERR_IF(!ret_obj, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(capacity == 0, EINVAL);

	int ret;

	void *buffer = malloc(capacity);
	if (!buffer)
		return -ENOMEM;

	ret = mbuf_mem_generic_wrap(buffer,
				    capacity,
				    mbuf_mem_generic_releaser_free,
				    NULL,
				    ret_obj);
	if (ret != 0)
		free(buffer);
	return ret;
}


int mbuf_mem_generic_wrap(void *data,
			  size_t len,
			  mbuf_mem_generic_wrap_release_t release,
			  void *userdata,
			  struct mbuf_mem **ret_obj)
{
	ULOG_ERRNO_RETURN_ERR_IF(!ret_obj, EINVAL);

	struct mbuf_mem *mem = calloc(1, sizeof(*mem));
	if (!mem)
		return -ENOMEM;
	struct wrap_specific *ws = calloc(1, sizeof(*ws));
	if (!ws) {
		free(mem);
		return -ENOMEM;
	}

	atomic_init(&mem->refcount, 1);

	ws->release = release;
	ws->userdata = userdata;

	mem->cookie = mbuf_mem_generic_wrap_cookie;
	mem->implem = &wrap_impl;
	mem->specific = ws;
	mem->data = data;
	mem->size = len;

	*ret_obj = mem;

	return 0;
}


void mbuf_mem_generic_releaser_free(void *data, size_t len, void *userdata)
{
	free(data);
}
