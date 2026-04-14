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

#include <futils/futils.h>
#include <media-buffers/mbuf_mem.h>

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#define ULOG_TAG mbuf_mem_impl
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);


/* Put preferred implementation first for autoselection */
static const enum mbuf_mem_implem_type supported_implems[] = {
#ifdef BUILD_LIBMEDIA_BUFFERS_MEMORY_ION
	MBUF_MEM_IMPLEM_TYPE_ION,
#endif
#ifdef BUILD_LIBMEDIA_BUFFERS_MEMORY_VACQ
	MBUF_MEM_IMPLEM_TYPE_VACQ,
#endif
#ifdef BUILD_LIBMEDIA_BUFFERS_MEMORY_HISI
	MBUF_MEM_IMPLEM_TYPE_HISI,
#endif
#ifdef BUILD_LIBMEDIA_BUFFERS_MEMORY_CVPIXELBUFFER
	MBUF_MEM_IMPLEM_TYPE_CVPIXELBUFFER,
#endif
#ifdef BUILD_LIBMEDIA_BUFFERS_MEMORY_PBO
	MBUF_MEM_IMPLEM_TYPE_PBO,
#endif
#ifdef BUILD_LIBMEDIA_BUFFERS_MEMORY_SHM
	MBUF_MEM_IMPLEM_TYPE_SHM,
#endif
#ifdef BUILD_LIBMEDIA_BUFFERS_MEMORY_GENERIC
	MBUF_MEM_IMPLEM_TYPE_GENERIC,
#endif
};


static const int supported_implems_count =
	sizeof(supported_implems) / sizeof(supported_implems[0]);


static int mbuf_get_implem(enum mbuf_mem_implem_type *implem)
{
	ULOG_ERRNO_RETURN_ERR_IF(implem == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!supported_implems_count, ENOSYS);

	if (*implem == MBUF_MEM_IMPLEM_TYPE_AUTO) {
		*implem = supported_implems[0];
		return 0;
	}

	for (int i = 0; i < supported_implems_count; i++)
		if (*implem == supported_implems[i])
			return 0;

	/* No suitable implementation found */
	return -ENOSYS;
}


int mbuf_get_supported_implems(const enum mbuf_mem_implem_type **implems)
{
	ULOG_ERRNO_RETURN_ERR_IF(!implems, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!supported_implems_count, ENOSYS);

	*implems = supported_implems;

	return supported_implems_count;
}


enum mbuf_mem_implem_type mbuf_get_auto_implem(void)
{
	int ret;
	enum mbuf_mem_implem_type implem = MBUF_MEM_IMPLEM_TYPE_AUTO;

	ret = mbuf_get_implem(&implem);
	ULOG_ERRNO_RETURN_VAL_IF(ret < 0, -ret, MBUF_MEM_IMPLEM_TYPE_AUTO);

	return implem;
}


#define MAKE_MBUF_IMPLEM(type)                                                 \
	{                                                                      \
		MBUF_MEM_IMPLEM_TYPE_##type, #type                             \
	}

struct {
	enum mbuf_mem_implem_type type;
	const char *str;
} s_mbuf_mem_implem_typ_str[] = {
	MAKE_MBUF_IMPLEM(AUTO),
	MAKE_MBUF_IMPLEM(GENERIC),
	MAKE_MBUF_IMPLEM(CVPIXELBUFFER),
	MAKE_MBUF_IMPLEM(PBO),
	MAKE_MBUF_IMPLEM(SHM),
	MAKE_MBUF_IMPLEM(HISI),
	MAKE_MBUF_IMPLEM(ION),
	MAKE_MBUF_IMPLEM(VACQ),
};
#undef MAKE_MBUF_IMPLEM


enum mbuf_mem_implem_type mbuf_mem_implem_type_from_str(const char *str)
{
	if (!str)
		return MBUF_MEM_IMPLEM_TYPE_AUTO;

	for (size_t i = 0; i < SIZEOF_ARRAY(s_mbuf_mem_implem_typ_str); i++) {
		if (strcasecmp(str, s_mbuf_mem_implem_typ_str[i].str) == 0)
			return s_mbuf_mem_implem_typ_str[i].type;
	}
	return MBUF_MEM_IMPLEM_TYPE_AUTO;
}


const char *mbuf_mem_implem_type_to_str(enum mbuf_mem_implem_type implem)
{
	for (size_t i = 0; i < SIZEOF_ARRAY(s_mbuf_mem_implem_typ_str); i++) {
		if (implem == s_mbuf_mem_implem_typ_str[i].type)
			return s_mbuf_mem_implem_typ_str[i].str;
	}

	return "UNKNOWN";
}
