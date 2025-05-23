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

#include <media-buffers/mbuf_ancillary_data.h>

#include "mbuf_internal.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define ULOG_TAG mbuf_ancillary_data
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);


const char *MBUF_ANCILLARY_KEY_USERDATA_SEI = "mbuf.userdata_sei";


int mbuf_ancillary_data_ref(struct mbuf_ancillary_data *data)
{
	ULOG_ERRNO_RETURN_ERR_IF(!data, EINVAL);

	atomic_fetch_add(&data->ref_count, 1);

	return 0;
}


int mbuf_ancillary_data_unref(struct mbuf_ancillary_data *data)
{
	ULOG_ERRNO_RETURN_ERR_IF(!data, EINVAL);

	int prev = atomic_fetch_sub(&data->ref_count, 1);

	/* If the refcount was >1, nothing to do */
	if (prev > 1)
		return 0;

	if (data->cbs.cleaner)
		data->cbs.cleaner(data, data->cbs.cleaner_userdata);

	free(data->name);
	free(data->buffer);
	free(data);

	return 0;
}


const char *mbuf_ancillary_data_get_name(struct mbuf_ancillary_data *data)
{
	ULOG_ERRNO_RETURN_VAL_IF(!data, EINVAL, NULL);

	return data->name;
}


bool mbuf_ancillary_data_is_string(struct mbuf_ancillary_data *data)
{
	ULOG_ERRNO_RETURN_VAL_IF(!data, EINVAL, false);

	return data->is_string;
}


const char *mbuf_ancillary_data_get_string(struct mbuf_ancillary_data *data)
{
	ULOG_ERRNO_RETURN_VAL_IF(!data, EINVAL, NULL);

	if (!data->is_string)
		return NULL;
	return data->buffer;
}


const void *mbuf_ancillary_data_get_buffer(struct mbuf_ancillary_data *data,
					   size_t *len)
{
	ULOG_ERRNO_RETURN_VAL_IF(!data, EINVAL, NULL);

	if (len)
		*len = data->len;
	return data->buffer;
}


int mbuf_ancillary_data_build_key(const char *name, uintptr_t ptr, char **key)
{
	int ret;

	ULOG_ERRNO_RETURN_ERR_IF(!name, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!key, EINVAL);

	*key = NULL;

	if (ptr != 0) {
		ret = asprintf(key, "%s:%" PRIxPTR, name, ptr);
		if (ret < 0)
			return -ENOMEM;
	} else {
		*key = strdup(name);
		if (*key == NULL)
			return -ENOMEM;
	}

	return 0;
}


int mbuf_ancillary_data_parse_key(const char *key, char **name, uintptr_t *ptr)
{
	int ret;
	char *_key = NULL, *_name = NULL, *_ptr_str = NULL;
	char *savedptr = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(!key, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!name, EINVAL);

	_key = strdup(key);
	if (_key == NULL)
		return -ENOMEM;

	*name = NULL;
	*ptr = 0;

	_name = strtok_r(_key, ":", &savedptr);
	_ptr_str = strtok_r(NULL, ":", &savedptr);

	if (_name == NULL) {
		ret = -EINVAL;
		goto out;
	}
	if (_ptr_str != NULL) {
		char *end_ptr = NULL;
		errno = 0;
		long unsigned int parsed_ulong = strtoul(_ptr_str, &end_ptr, 0);
		if (_ptr_str[0] == '\0' || end_ptr[0] != '\0' || errno != 0) {
			ret = -errno;
			goto out;
		}
		*ptr = (uintptr_t)parsed_ulong;
	}
	*name = strdup(_name);
	if (*name == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	ret = 0;

out:
	free(_key);
	return ret;
}
