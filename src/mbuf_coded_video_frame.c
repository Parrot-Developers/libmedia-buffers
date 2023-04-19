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

#include <media-buffers/mbuf_coded_video_frame.h>

#include <futils/list.h>
#include <stdatomic.h>

#include "internal/mbuf_mem_internal.h"
#include "mbuf_base_frame.h"
#include "mbuf_utils.h"

#define ULOG_TAG mbuf_coded_video_frame
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);


struct mbuf_coded_video_frame_nalu {
	struct mbuf_mem *mem;
	struct vdef_nalu nalu;
	void *data;
};


struct mbuf_coded_video_frame {
	struct mbuf_base_frame base;
	struct vdef_coded_frame info;

	unsigned int nnalus;
	struct mbuf_coded_video_frame_nalu *nalus;

	struct mbuf_coded_video_frame_cbs cbs;
};


struct mbuf_coded_video_frame_queue {
	struct mbuf_base_frame_queue base;
	mbuf_coded_video_frame_queue_filter_t filter;
	void *filter_userdata;
};


static void mbuf_coded_video_frame_cleaner(void *cframe)
{
	struct mbuf_coded_video_frame *frame = cframe;
	ULOG_ERRNO_RETURN_IF(!frame, EINVAL);

	if (frame->cbs.pre_release)
		frame->cbs.pre_release(frame, frame->cbs.pre_release_userdata);

	int rc = mbuf_rwlock_get_value(&frame->base.rwlock);
	if (rc == RWLOCK_WRLOCKED)
		ULOGW("1 rw-nalu/packed-buffer not released"
		      " during frame deletion");
	else if (rc > 0)
		ULOGW("%d ro-nalu/packed-buffer not released"
		      " during frame deletion",
		      rc);
	for (unsigned int i = 0; i < frame->nnalus; i++) {
		int ret = mbuf_mem_unref(frame->nalus[i].mem);
		if (ret != 0)
			ULOG_ERRNO("mbuf_mem_unref(destroy)", -ret);
	}
	mbuf_base_frame_set_metadata(&frame->base, NULL);
	mbuf_base_frame_deinit(&frame->base);
	free(frame->nalus);
	free(frame);
}


int mbuf_coded_video_frame_new(struct vdef_coded_frame *frame_info,
			       struct mbuf_coded_video_frame **ret_obj)
{
	ULOG_ERRNO_RETURN_ERR_IF(!ret_obj, EINVAL);
	*ret_obj = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!frame_info, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(
		!vdef_is_coded_format_valid(&frame_info->format), EINVAL);

	struct mbuf_coded_video_frame *frame = calloc(1, sizeof(*frame));
	if (!frame)
		return -ENOMEM;
	frame->info = *frame_info;

	mbuf_base_frame_init(
		&frame->base, frame, mbuf_coded_video_frame_cleaner);

	*ret_obj = frame;

	return 0;
}


int mbuf_coded_video_frame_set_callbacks(struct mbuf_coded_video_frame *frame,
					 struct mbuf_coded_video_frame_cbs *cbs)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!cbs, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	frame->cbs = *cbs;
	return 0;
}


int mbuf_coded_video_frame_ref(struct mbuf_coded_video_frame *frame)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);

	return mbuf_base_frame_ref(&frame->base);
}


int mbuf_coded_video_frame_unref(struct mbuf_coded_video_frame *frame)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);

	return mbuf_base_frame_unref(&frame->base);
}


/* Writer API */


int mbuf_coded_video_frame_set_frame_info(struct mbuf_coded_video_frame *frame,
					  struct vdef_coded_frame *frame_info)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!frame_info, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);
	frame->info = *frame_info;
	return 0;
}


int mbuf_coded_video_frame_set_metadata(struct mbuf_coded_video_frame *frame,
					struct vmeta_frame *meta)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!meta, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	return mbuf_base_frame_set_metadata(&frame->base, meta);
}


static int mbuf_coded_video_frame_insert_nalu_internal(
	struct mbuf_coded_video_frame *frame,
	struct mbuf_mem *mem,
	size_t offset,
	struct vdef_nalu *nalu,
	uint32_t index)
{
	/* If the index is greater than the actual number of NALUs,
	 * set it to the current number */
	if (index > frame->nnalus)
		index = frame->nnalus;

	struct mbuf_coded_video_frame_nalu *new =
		realloc(frame->nalus, (frame->nnalus + 1) * sizeof(*new));
	if (!new)
		return -ENOMEM;
	frame->nalus = new;

	int ret = mbuf_mem_ref(mem);
	if (ret != 0) {
		/* This should never happen, as (mem != NULL) has been
		 * checked by caller */
		ULOG_ERRNO("mbuf_mem_ref", -ret);
		return ret;
	}

	/* If we're inserting, copy the data after */
	if (index < frame->nnalus)
		memmove(&frame->nalus[index + 1],
			&frame->nalus[index],
			(frame->nnalus - index) * sizeof(*frame->nalus));

	frame->nalus[index].mem = mem;
	frame->nalus[index].nalu = *nalu;
	uint8_t *data = mem->data;
	frame->nalus[index].data = data + offset;
	frame->nnalus++;

	return 0;
}


int mbuf_coded_video_frame_add_nalu(struct mbuf_coded_video_frame *frame,
				    struct mbuf_mem *mem,
				    size_t offset,
				    struct vdef_nalu *nalu)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mem, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!nalu, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	return mbuf_coded_video_frame_insert_nalu_internal(
		frame, mem, offset, nalu, UINT32_MAX);
}


int mbuf_coded_video_frame_insert_nalu(struct mbuf_coded_video_frame *frame,
				       struct mbuf_mem *mem,
				       size_t offset,
				       struct vdef_nalu *nalu,
				       unsigned int index)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mem, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!nalu, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	return mbuf_coded_video_frame_insert_nalu_internal(
		frame, mem, offset, nalu, index);
}


int mbuf_coded_video_frame_finalize(struct mbuf_coded_video_frame *frame)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(frame->nnalus == 0, EPROTO);

	mbuf_base_frame_finalize(&frame->base);

	return 0;
}


/* Reader API */


int mbuf_coded_video_frame_uses_mem_from_pool(
	struct mbuf_coded_video_frame *frame,
	struct mbuf_pool *pool,
	bool *any_,
	bool *all_)
{
	bool any = false;
	bool all = true;

	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!frame->nnalus, ENOENT);
	ULOG_ERRNO_RETURN_ERR_IF(!pool, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	for (unsigned int i = 0; i < frame->nnalus; i++) {
		if (frame->nalus[i].mem->pool == pool)
			any = true;
		else
			all = false;
	}

	if (any_)
		*any_ = any;
	if (all_)
		*all_ = all;
	return 0;
}


int mbuf_coded_video_frame_get_metadata(struct mbuf_coded_video_frame *frame,
					struct vmeta_frame **meta)
{
	ULOG_ERRNO_RETURN_ERR_IF(!meta, EINVAL);
	*meta = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	return mbuf_base_frame_get_metadata(&frame->base, meta);
}


int mbuf_coded_video_frame_get_nalu_count(struct mbuf_coded_video_frame *frame)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);
	return frame->nnalus;
}


int mbuf_coded_video_frame_get_nalu_mem_info(
	struct mbuf_coded_video_frame *frame,
	unsigned int index,
	struct mbuf_mem_info *info)
{
	struct mbuf_mem *mem = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!info, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);
	ULOG_ERRNO_RETURN_ERR_IF(index > frame->nnalus, EINVAL);

	mem = frame->nalus[index].mem;
	info->cookie = mem->cookie;
	info->specific = mem->specific;

	return 0;
}


int mbuf_coded_video_frame_get_nalu(struct mbuf_coded_video_frame *frame,
				    unsigned int index,
				    const void **data,
				    struct vdef_nalu *nalu)
{
	ULOG_ERRNO_RETURN_ERR_IF(!data, EINVAL);
	*data = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!nalu, EINVAL);
	memset(nalu, 0, sizeof(*nalu));
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);
	ULOG_ERRNO_RETURN_ERR_IF(index >= frame->nnalus, ENOENT);

	int ret = mbuf_base_frame_rdlock(&frame->base);
	if (ret != 0)
		return ret;

	*data = frame->nalus[index].data;
	*nalu = frame->nalus[index].nalu;

	return 0;
}


int mbuf_coded_video_frame_release_nalu(struct mbuf_coded_video_frame *frame,
					unsigned int index,
					const void *data)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);
	ULOG_ERRNO_RETURN_ERR_IF(index >= frame->nnalus, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(data != frame->nalus[index].data, EINVAL);
	return mbuf_base_frame_rdunlock(&frame->base);
}


int mbuf_coded_video_frame_get_rw_nalu(struct mbuf_coded_video_frame *frame,
				       unsigned int index,
				       void **data,
				       struct vdef_nalu *nalu)
{
	ULOG_ERRNO_RETURN_ERR_IF(!data, EINVAL);
	*data = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!nalu, EINVAL);
	memset(nalu, 0, sizeof(*nalu));
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);
	ULOG_ERRNO_RETURN_ERR_IF(index >= frame->nnalus, ENOENT);

	int ret = mbuf_base_frame_wrlock(&frame->base);
	if (ret != 0)
		return ret;

	*data = frame->nalus[index].data;
	*nalu = frame->nalus[index].nalu;

	return 0;
}


int mbuf_coded_video_frame_release_rw_nalu(struct mbuf_coded_video_frame *frame,
					   unsigned int index,
					   void *data)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);
	ULOG_ERRNO_RETURN_ERR_IF(index >= frame->nnalus, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(data != frame->nalus[index].data, EINVAL);
	return mbuf_base_frame_wrunlock(&frame->base);
}


int mbuf_coded_video_frame_get_packed_buffer(
	struct mbuf_coded_video_frame *frame,
	const void **data,
	size_t *len)
{
	ULOG_ERRNO_RETURN_ERR_IF(!data, EINVAL);
	*data = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!len, EINVAL);
	*len = 0;
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);
	ULOG_ERRNO_RETURN_ERR_IF(frame->nnalus == 0, EINVAL);

	int ret = mbuf_base_frame_rdlock(&frame->base);
	if (ret != 0)
		return ret;

	/* Prepare total length & check if packed */
	*len = 0;
	const uint8_t *expected = NULL;
	bool packed = true;
	for (unsigned int i = 0; i < frame->nnalus; i++) {
		*len += frame->nalus[i].nalu.size;
		if (expected && expected != frame->nalus[i].data)
			packed = false;
		expected = frame->nalus[i].data;
		expected += frame->nalus[i].nalu.size;
	}

	if (packed) {
		*data = frame->nalus[0].data;
		ret = 0;
	} else {
		*data = NULL;
		mbuf_base_frame_rdunlock(&frame->base);
		ret = -EPROTO;
	}

	return ret;
}


int mbuf_coded_video_frame_release_packed_buffer(
	struct mbuf_coded_video_frame *frame,
	const void *data)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);
	ULOG_ERRNO_RETURN_ERR_IF(frame->nnalus == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(data != frame->nalus[0].data, EINVAL);
	return mbuf_base_frame_rdunlock(&frame->base);
}


int mbuf_coded_video_frame_get_rw_packed_buffer(
	struct mbuf_coded_video_frame *frame,
	void **data,
	size_t *len)
{
	int ret;

	ULOG_ERRNO_RETURN_ERR_IF(!data, EINVAL);
	*data = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!len, EINVAL);
	*len = 0;
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);
	ULOG_ERRNO_RETURN_ERR_IF(frame->nnalus == 0, EINVAL);

	ret = mbuf_base_frame_wrlock(&frame->base);
	if (ret != 0)
		return ret;

	/* Prepare total length & check if packed */
	*len = 0;
	const uint8_t *expected = NULL;
	bool packed = true;
	for (unsigned int i = 0; i < frame->nnalus; i++) {
		*len += frame->nalus[i].nalu.size;
		if (expected && expected != frame->nalus[i].data)
			packed = false;
		expected = frame->nalus[i].data;
		expected += frame->nalus[i].nalu.size;
	}

	if (packed) {
		*data = frame->nalus[0].data;
		ret = 0;
	} else {
		*data = NULL;
		mbuf_base_frame_wrunlock(&frame->base);
		ret = -EPROTO;
	}

	return ret;
}


int mbuf_coded_video_frame_release_rw_packed_buffer(
	struct mbuf_coded_video_frame *frame,
	void *data)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);
	ULOG_ERRNO_RETURN_ERR_IF(frame->nnalus == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(data != frame->nalus[0].data, EINVAL);
	return mbuf_base_frame_wrunlock(&frame->base);
}


ssize_t
mbuf_coded_video_frame_get_packed_size(struct mbuf_coded_video_frame *frame)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	ssize_t ret = 0;
	for (unsigned int i = 0; i < frame->nnalus; i++)
		ret += frame->nalus[i].nalu.size;
	return ret;
}


int mbuf_coded_video_frame_copy(struct mbuf_coded_video_frame *frame,
				struct mbuf_mem *dst,
				struct mbuf_coded_video_frame **ret_obj)
{
	int ret;
	size_t offset;
	struct mbuf_coded_video_frame *new_frame = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(!ret_obj, EINVAL);
	*ret_obj = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!dst, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!dst->data, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	ret = mbuf_base_frame_rdlock(&frame->base);
	if (ret != 0)
		return ret;

	size_t required_len = 0;
	for (unsigned int i = 0; i < frame->nnalus; i++)
		required_len += frame->nalus[i].nalu.size;

	if (dst->size < required_len) {
		ret = -ENOSPC;
		goto out;
	}

	ret = mbuf_coded_video_frame_new(&frame->info, &new_frame);
	if (ret != 0)
		goto out;

	ret = mbuf_coded_video_frame_foreach_ancillary_data(
		frame, mbuf_coded_video_frame_ancillary_data_copier, new_frame);
	if (ret != 0)
		goto out;

	offset = 0;
	for (unsigned int i = 0; i < frame->nnalus; i++) {
		uint8_t *cpdst = dst->data;
		cpdst += offset;
		memcpy(cpdst, frame->nalus[i].data, frame->nalus[i].nalu.size);
		ret = mbuf_coded_video_frame_add_nalu(
			new_frame, dst, offset, &frame->nalus[i].nalu);
		if (ret != 0)
			goto out;
		offset += frame->nalus[i].nalu.size;
	}

	mbuf_base_frame_set_metadata(&new_frame->base, frame->base.meta);

out:
	/* Release read-lock before returning */
	mbuf_base_frame_rdunlock(&frame->base);

	if (ret != 0 && new_frame)
		mbuf_coded_video_frame_unref(new_frame);
	else
		*ret_obj = new_frame;
	return ret;
}


int mbuf_coded_video_frame_get_frame_info(struct mbuf_coded_video_frame *frame,
					  struct vdef_coded_frame *frame_info)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!frame_info, EINVAL);

	*frame_info = frame->info;
	return 0;
}


/* Ancillary data API */


int mbuf_coded_video_frame_add_ancillary_string(
	struct mbuf_coded_video_frame *frame,
	const char *name,
	const char *value)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!name, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!value, EINVAL);

	return mbuf_base_frame_add_ancillary_string(&frame->base, name, value);
}


int mbuf_coded_video_frame_add_ancillary_buffer(
	struct mbuf_coded_video_frame *frame,
	const char *name,
	const void *buffer,
	size_t len)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!name, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!buffer, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(len == 0, EINVAL);

	return mbuf_base_frame_add_ancillary_buffer(
		&frame->base, name, buffer, len);
}


int mbuf_coded_video_frame_add_ancillary_data(
	struct mbuf_coded_video_frame *frame,
	struct mbuf_ancillary_data *data)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!data, EINVAL);

	return mbuf_base_frame_add_ancillary_data(&frame->base, data);
}


int mbuf_coded_video_frame_get_ancillary_data(
	struct mbuf_coded_video_frame *frame,
	const char *name,
	struct mbuf_ancillary_data **data)
{
	ULOG_ERRNO_RETURN_ERR_IF(!data, EINVAL);
	*data = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!name, EINVAL);

	return mbuf_base_frame_get_ancillary_data(&frame->base, name, data);
}


int mbuf_coded_video_frame_remove_ancillary_data(
	struct mbuf_coded_video_frame *frame,
	const char *name)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!name, EINVAL);

	return mbuf_base_frame_remove_ancillary_data(&frame->base, name);
}


int mbuf_coded_video_frame_foreach_ancillary_data(
	struct mbuf_coded_video_frame *frame,
	mbuf_ancillary_data_cb_t cb,
	void *userdata)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!cb, EINVAL);

	return mbuf_base_frame_foreach_ancillary_data(
		&frame->base, cb, userdata);
}


bool mbuf_coded_video_frame_ancillary_data_copier(
	struct mbuf_ancillary_data *data,
	void *userdata)
{
	int ret = 0;
	struct mbuf_coded_video_frame *copy = userdata;

	ret = mbuf_coded_video_frame_add_ancillary_data(copy, data);
	if (ret < 0)
		ULOG_ERRNO("mbuf_coded_video_frame_add_ancillary_data", -ret);

	return true;
}


/* Queue API */


int mbuf_coded_video_frame_queue_new(
	struct mbuf_coded_video_frame_queue **ret_obj)
{
	return mbuf_coded_video_frame_queue_new_with_args(NULL, ret_obj);
}


int mbuf_coded_video_frame_queue_new_with_args(
	struct mbuf_coded_video_frame_queue_args *args,
	struct mbuf_coded_video_frame_queue **ret_obj)
{
	ULOG_ERRNO_RETURN_ERR_IF(!ret_obj, EINVAL);
	*ret_obj = NULL;

	struct mbuf_coded_video_frame_queue *queue = calloc(1, sizeof(*queue));
	if (!queue)
		return -ENOMEM;
	queue->filter = args ? args->filter : NULL;
	queue->filter_userdata = args ? args->filter_userdata : NULL;
	int max_frames = args ? args->max_frames : 0;

	int ret = mbuf_base_frame_queue_init(&queue->base, max_frames);
	if (ret != 0) {
		mbuf_coded_video_frame_queue_destroy(queue);
		queue = NULL;
	}
	*ret_obj = queue;
	return ret;
}


int mbuf_coded_video_frame_queue_push(
	struct mbuf_coded_video_frame_queue *queue,
	struct mbuf_coded_video_frame *frame)
{
	ULOG_ERRNO_RETURN_ERR_IF(!queue, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	if (queue->filter && !queue->filter(frame, queue->filter_userdata))
		return -EPROTO;

	return mbuf_base_frame_queue_push(&queue->base, &frame->base);
}


int mbuf_coded_video_frame_queue_peek(
	struct mbuf_coded_video_frame_queue *queue,
	struct mbuf_coded_video_frame **out_frame)
{
	ULOG_ERRNO_RETURN_ERR_IF(!out_frame, EINVAL);
	*out_frame = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!queue, EINVAL);

	void *tmp_frame;
	int ret = mbuf_base_frame_queue_peek(&queue->base, &tmp_frame);

	if (ret == 0)
		*out_frame = tmp_frame;
	else
		*out_frame = NULL;
	return ret;
}


int mbuf_coded_video_frame_queue_peek_at(
	struct mbuf_coded_video_frame_queue *queue,
	unsigned int index,
	struct mbuf_coded_video_frame **out_frame)
{
	ULOG_ERRNO_RETURN_ERR_IF(!out_frame, EINVAL);
	*out_frame = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!queue, EINVAL);

	void *tmp_frame;
	int ret =
		mbuf_base_frame_queue_peek_at(&queue->base, index, &tmp_frame);

	if (ret == 0)
		*out_frame = tmp_frame;
	else
		*out_frame = NULL;
	return ret;
}


int mbuf_coded_video_frame_queue_pop(struct mbuf_coded_video_frame_queue *queue,
				     struct mbuf_coded_video_frame **out_frame)
{
	ULOG_ERRNO_RETURN_ERR_IF(!out_frame, EINVAL);
	*out_frame = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!queue, EINVAL);

	void *tmp_frame;
	int ret = mbuf_base_frame_queue_pop(&queue->base, &tmp_frame);

	if (ret == 0)
		*out_frame = tmp_frame;
	else
		*out_frame = NULL;
	return ret;
}


int mbuf_coded_video_frame_queue_flush(
	struct mbuf_coded_video_frame_queue *queue)
{
	ULOG_ERRNO_RETURN_ERR_IF(!queue, EINVAL);

	return mbuf_base_frame_queue_flush(&queue->base);
}


int mbuf_coded_video_frame_queue_get_event(
	struct mbuf_coded_video_frame_queue *queue,
	struct pomp_evt **out_evt)
{
	ULOG_ERRNO_RETURN_ERR_IF(!out_evt, EINVAL);
	*out_evt = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!queue, EINVAL);

	return mbuf_base_frame_queue_get_event(&queue->base, out_evt);
}


int mbuf_coded_video_frame_queue_get_count(
	struct mbuf_coded_video_frame_queue *queue)
{
	ULOG_ERRNO_RETURN_ERR_IF(!queue, EINVAL);

	return mbuf_base_frame_queue_get_count(&queue->base);
}


int mbuf_coded_video_frame_queue_destroy(
	struct mbuf_coded_video_frame_queue *queue)
{
	ULOG_ERRNO_RETURN_ERR_IF(!queue, EINVAL);

	mbuf_base_frame_queue_deinit(&queue->base);

	free(queue);
	return 0;
}
