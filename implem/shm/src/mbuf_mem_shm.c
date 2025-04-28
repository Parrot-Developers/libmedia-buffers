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

#include <media-buffers/mbuf_mem_shm.h>

#include "mbuf_mem_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>


#define ULOG_TAG mbuf_mem_shm
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);


/* SHM implementation memory specific */
struct mem_shm_specific {
	/* Index (handle) of the memory in the shared memory */
	int index;
};


/* SHM implementation implem specific */
struct impl_shm_specific {
	char *addr;
	int fd;
	void *base_addr;
	size_t mem_size;
	size_t mem_count;
	bool *slots;
};


/* Cookie is 'shm' in ascii coding */
const uint64_t mbuf_mem_shm_cookie = UINT64_C(0x73686D);


static void mbuf_shm_close(const char *addr, void *data, size_t size);


static int
mbuf_shm_open(const char *addr, size_t size, int *ret_fd, void **ret_base_addr)
{
	int ret;
	void *base_addr = NULL;
	int fd = -1;

	fd = shm_open(addr, O_RDONLY | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		ULOG_ERRNO("shm_open", errno);
		return -errno;
	}

	ret = ftruncate(fd, size);
	if (ret < 0) {
		ULOG_ERRNO("ftruncate", -ret);
		goto error;
	}

	base_addr = mmap(NULL, size, PROT_WRITE, MAP_SHARED, fd, 0);
	if (base_addr == MAP_FAILED) {
		ret = -errno;
		ULOG_ERRNO("mmap", -ret);
		goto error;
	}

	*ret_fd = fd;
	*ret_base_addr = base_addr;

	return 0;

error:
	mbuf_shm_close(addr, base_addr, size);
	return ret;
}


static void mbuf_shm_close(const char *addr, void *data, size_t size)
{
	int err;
	if (data != NULL && data != MAP_FAILED) {
		err = munmap(data, size);
		if (err == -1)
			ULOG_ERRNO("munmap", errno);
	}
	if (addr != NULL) {
		err = shm_unlink(addr);
		if (err == -1)
			ULOG_ERRNO("shm_unlink", errno);
	}
}


static int mbuf_mem_shm_alloc(struct mbuf_mem *mem, void *specific)
{
	struct impl_shm_specific *impl_specific = specific;
	struct mem_shm_specific *shm_mem = NULL;
	size_t index = SIZE_MAX;

	ULOG_ERRNO_RETURN_ERR_IF(mem->specific, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mem->data, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!impl_specific, EINVAL);

	/* Find a slot */
	for (size_t i = 0; i < impl_specific->mem_count; i++) {
		if (impl_specific->slots[i] == false) {
			index = i;
			break;
		}
	}
	/* All slots used */
	if (index == SIZE_MAX)
		return -ENOMEM;

	shm_mem = calloc(1, sizeof(*shm_mem));
	if (!shm_mem)
		return -ENOMEM;
	shm_mem->index = index;

	/* Mark slot as used */
	impl_specific->slots[index] = true;

	mem->data = (uint8_t *)impl_specific->base_addr +
		    (shm_mem->index * impl_specific->mem_size);
	mem->size = impl_specific->mem_size;
	mem->cookie = mbuf_mem_shm_cookie;
	mem->specific = shm_mem;

	return 0;
}


static void mbuf_mem_shm_free(struct mbuf_mem *mem, void *specific)
{
	struct mem_shm_specific *shm_mem = mem->specific;
	struct impl_shm_specific *impl_specific = specific;

	ULOG_ERRNO_RETURN_IF(mem->cookie != mbuf_mem_shm_cookie, EINVAL);
	ULOG_ERRNO_RETURN_IF(!shm_mem, EINVAL);
	ULOG_ERRNO_RETURN_IF(!impl_specific, EINVAL);

	/* Mark slot as unused */
	impl_specific->slots[shm_mem->index] = false;

	mem->specific = NULL;
	mem->data = NULL;

	free(shm_mem);
}


struct mbuf_mem_implem *
mbuf_mem_shm_get_implem(const struct mbuf_shm_attr *attrs)
{
	int ret;
	struct impl_shm_specific *impl_specific = NULL;
	struct mbuf_mem_implem *impl = NULL;
	void *data = NULL;
	int fd = -1;

	ULOG_ERRNO_RETURN_VAL_IF(!attrs, EINVAL, NULL);
	ULOG_ERRNO_RETURN_VAL_IF(!attrs->addr, EINVAL, NULL);
	ULOG_ERRNO_RETURN_VAL_IF(
		attrs->mem_size * attrs->mem_count == 0, EINVAL, NULL);

	ret = mbuf_shm_open(
		attrs->addr, attrs->mem_size * attrs->mem_count, &fd, &data);
	if (ret < 0) {
		ULOG_ERRNO("mbuf_shm_open", -ret);
		goto error;
	}

	impl_specific = calloc(1, sizeof(*impl_specific));
	if (!impl_specific) {
		ULOG_ERRNO("malloc", ENOMEM);
		goto error;
	}
	impl_specific->fd = fd;
	impl_specific->addr = strdup(attrs->addr);
	impl_specific->base_addr = data;
	impl_specific->mem_size = attrs->mem_size;
	impl_specific->mem_count = attrs->mem_count;

	impl_specific->slots =
		calloc(impl_specific->mem_count, sizeof(*impl_specific->slots));
	if (!impl_specific->slots) {
		ULOG_ERRNO("calloc", ENOMEM);
		goto error;
	}

	impl = calloc(1, sizeof(*impl));
	if (!impl) {
		ULOG_ERRNO("calloc", ENOMEM);
		goto error;
	}
	impl->alloc = mbuf_mem_shm_alloc;
	impl->free = mbuf_mem_shm_free;
	impl->specific = impl_specific;

	return impl;

error:
	mbuf_shm_close(attrs->addr, data, attrs->mem_size * attrs->mem_count);
	if (impl_specific)
		free(impl_specific->slots);
	free(impl_specific);
	free(impl);
	return NULL;
}


void mbuf_mem_shm_release_implem(struct mbuf_mem_implem *implem)
{
	if (!implem)
		return;

	struct impl_shm_specific *impl_specific = implem->specific;

	if (impl_specific != NULL) {
		mbuf_shm_close(impl_specific->addr,
			       impl_specific->base_addr,
			       impl_specific->mem_size *
				       impl_specific->mem_count);
		free(impl_specific->addr);
		free(impl_specific->slots);
	}
	free(implem->specific);
	free(implem);
}


int mbuf_mem_shm_get_index(struct mbuf_mem *mem)
{
	ULOG_ERRNO_RETURN_ERR_IF(!mem, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mem->specific, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mem->cookie != mbuf_mem_shm_cookie, EINVAL);

	return ((struct mem_shm_specific *)mem->specific)->index;
}


int mbuf_mem_shm_get_index_from_info(struct mbuf_mem_info *info)
{
	ULOG_ERRNO_RETURN_ERR_IF(!info, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!info->specific, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(info->cookie != mbuf_mem_shm_cookie, EINVAL);

	return ((struct mem_shm_specific *)info->specific)->index;
}
