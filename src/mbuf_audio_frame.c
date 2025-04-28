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

#include <media-buffers/mbuf_audio_frame.h>

#include <futils/list.h>
#include <stdatomic.h>
#include <string.h>

#include "internal/mbuf_mem_internal.h"
#include "mbuf_base_frame.h"
#include "mbuf_utils.h"

#define ULOG_TAG mbuf_audio_frame
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);


struct mbuf_audio_frame {
	struct mbuf_base_frame base;
	struct adef_frame info;

	struct {
		struct mbuf_mem *mem;
		void *data;
		size_t len;
	} buffer;

	struct mbuf_audio_frame_cbs cbs;
};


struct mbuf_audio_frame_queue {
	struct mbuf_base_frame_queue base;
	mbuf_audio_frame_queue_filter_t filter;
	void *filter_userdata;
};


static void mbuf_audio_frame_cleaner(void *rframe)
{
	struct mbuf_audio_frame *frame = rframe;
	ULOG_ERRNO_RETURN_IF(!frame, EINVAL);

	if (frame->cbs.pre_release)
		frame->cbs.pre_release(frame, frame->cbs.pre_release_userdata);

	/* The frame needs to be deleted */
	int rc = mbuf_rwlock_get_value(&frame->base.rwlock);
	if (rc == RWLOCK_WRLOCKED)
		ULOGW("1 rw/buffer not released during frame deletion");
	else if (rc > 0)
		ULOGW("%d ro/buffer not released during frame deletion", rc);
	if (frame->buffer.mem != NULL) {
		int ret = mbuf_mem_unref(frame->buffer.mem);
		if (ret != 0)
			ULOG_ERRNO("mbuf_mem_unref(destroy)", -ret);
		frame->buffer.mem = NULL;
	}
	mbuf_base_frame_deinit(&frame->base);
	free(frame);
}


int mbuf_audio_frame_new(struct adef_frame *frame_info,
			 struct mbuf_audio_frame **ret_obj)
{
	ULOG_ERRNO_RETURN_ERR_IF(!ret_obj, EINVAL);
	*ret_obj = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!frame_info, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!adef_is_format_valid(&frame_info->format),
				 EINVAL);

	struct mbuf_audio_frame *frame = calloc(1, sizeof(*frame));
	if (!frame)
		return -ENOMEM;
	frame->info = *frame_info;

	mbuf_base_frame_init(&frame->base, frame, mbuf_audio_frame_cleaner);

	*ret_obj = frame;

	return 0;
}


int mbuf_audio_frame_set_callbacks(struct mbuf_audio_frame *frame,
				   struct mbuf_audio_frame_cbs *cbs)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!cbs, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	frame->cbs = *cbs;
	return 0;
}


int mbuf_audio_frame_ref(struct mbuf_audio_frame *frame)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);

	return mbuf_base_frame_ref(&frame->base);
}


int mbuf_audio_frame_unref(struct mbuf_audio_frame *frame)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);

	return mbuf_base_frame_unref(&frame->base);
}


/* Writer API */


int mbuf_audio_frame_set_frame_info(struct mbuf_audio_frame *frame,
				    struct adef_frame *frame_info)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!frame_info, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	frame->info = *frame_info;
	return 0;
}


int mbuf_audio_frame_set_buffer(struct mbuf_audio_frame *frame,
				struct mbuf_mem *mem,
				size_t offset,
				size_t len)
{
	int ret;

	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mem, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	/* Remove previous memory for buffer */
	if (frame->buffer.mem) {
		ret = mbuf_mem_unref(frame->buffer.mem);
		if (ret != 0) {
			ULOG_ERRNO("mbuf_mem_unref", -ret);
			return ret;
		}
		memset(&frame->buffer, 0, sizeof(frame->buffer));
	}

	ret = mbuf_mem_ref(mem);
	if (ret != 0) {
		ULOG_ERRNO("mbuf_mem_ref", -ret);
		return ret;
	}

	frame->buffer.mem = mem;
	frame->buffer.len = len;
	uint8_t *data = mem->data;
	frame->buffer.data = data + offset;

	return 0;
}


int mbuf_audio_frame_finalize(struct mbuf_audio_frame *frame)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!frame->buffer.mem, EPROTO);

	mbuf_base_frame_finalize(&frame->base);

	return 0;
}


/* Reader API */


int mbuf_audio_frame_uses_mem_from_pool(struct mbuf_audio_frame *frame,
					struct mbuf_pool *pool,
					bool *any_,
					bool *all_)
{
	bool any = false;
	bool all = true;

	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!pool, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	if (frame->buffer.mem->pool == pool)
		any = true;
	else
		all = false;

	if (any_)
		*any_ = any;
	if (all_)
		*all_ = all;
	return 0;
}


int mbuf_audio_frame_get_buffer_mem_info(struct mbuf_audio_frame *frame,
					 struct mbuf_mem_info *info)
{
	struct mbuf_mem *mem = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!info, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	mem = frame->buffer.mem;
	info->cookie = mem->cookie;
	info->specific = mem->specific;

	return 0;
}


int mbuf_audio_frame_get_buffer(struct mbuf_audio_frame *frame,
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

	int ret = mbuf_base_frame_rdlock(&frame->base);
	if (ret != 0)
		return ret;

	*data = frame->buffer.data;
	*len = frame->buffer.len;

	return 0;
}


int mbuf_audio_frame_release_buffer(struct mbuf_audio_frame *frame,
				    const void *data)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);
	ULOG_ERRNO_RETURN_ERR_IF(data != frame->buffer.data, EINVAL);
	return mbuf_base_frame_rdunlock(&frame->base);
}


int mbuf_audio_frame_get_rw_buffer(struct mbuf_audio_frame *frame,
				   void **data,
				   size_t *len)
{
	ULOG_ERRNO_RETURN_ERR_IF(!data, EINVAL);
	*data = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!len, EINVAL);
	*len = 0;
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	int ret = mbuf_base_frame_wrlock(&frame->base);
	if (ret != 0)
		return ret;

	*data = frame->buffer.data;
	*len = frame->buffer.len;

	return 0;
}


int mbuf_audio_frame_release_rw_buffer(struct mbuf_audio_frame *frame,
				       void *data)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);
	ULOG_ERRNO_RETURN_ERR_IF(data != frame->buffer.data, EINVAL);
	return mbuf_base_frame_wrunlock(&frame->base);
}


ssize_t mbuf_audio_frame_get_size(struct mbuf_audio_frame *frame)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	return frame->buffer.len;
}


int mbuf_audio_frame_copy(struct mbuf_audio_frame *frame,
			  struct mbuf_mem *dst,
			  struct mbuf_audio_frame **ret_obj)
{
	int ret;
	struct mbuf_audio_frame *new_frame = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(!ret_obj, EINVAL);
	*ret_obj = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!dst, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!dst->data, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	ssize_t tmp = mbuf_audio_frame_get_size(frame);
	if (tmp < 0)
		return tmp;
	size_t required_len = tmp;

	ret = mbuf_base_frame_rdlock(&frame->base);
	if (ret != 0)
		return ret;

	if (dst->size < required_len) {
		ret = -ENOSPC;
		goto out;
	}

	ret = mbuf_audio_frame_new(&frame->info, &new_frame);
	if (ret != 0)
		goto out;

	ret = mbuf_audio_frame_foreach_ancillary_data(
		frame, mbuf_audio_frame_ancillary_data_copier, new_frame);
	if (ret != 0)
		goto out;

	/* Simple case, just copy the buffer */
	memcpy(dst->data, frame->buffer.data, frame->buffer.len);
	ret = mbuf_audio_frame_set_buffer(new_frame, dst, 0, frame->buffer.len);
	if (ret != 0)
		goto out;

out:
	/* Release read-lock before returning */
	mbuf_base_frame_rdunlock(&frame->base);

	if (ret != 0 && new_frame)
		mbuf_audio_frame_unref(new_frame);
	else
		*ret_obj = new_frame;
	return ret;
}


int mbuf_audio_frame_get_frame_info(struct mbuf_audio_frame *frame,
				    struct adef_frame *frame_info)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!frame_info, EINVAL);

	*frame_info = frame->info;
	return 0;
}


/* Ancillary data API */


int mbuf_audio_frame_add_ancillary_string(struct mbuf_audio_frame *frame,
					  const char *name,
					  const char *value)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!name, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!value, EINVAL);

	return mbuf_base_frame_add_ancillary_string(&frame->base, name, value);
}


int mbuf_audio_frame_add_ancillary_buffer(struct mbuf_audio_frame *frame,
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


int mbuf_audio_frame_add_ancillary_buffer_with_cbs(
	struct mbuf_audio_frame *frame,
	const char *name,
	const void *buffer,
	size_t len,
	const struct mbuf_ancillary_data_cbs *cbs)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!name, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!buffer, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(len == 0, EINVAL);

	return mbuf_base_frame_add_ancillary_buffer_with_cbs(
		&frame->base, name, buffer, len, cbs);
}


int mbuf_audio_frame_add_ancillary_data(struct mbuf_audio_frame *frame,
					struct mbuf_ancillary_data *data)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!data, EINVAL);

	return mbuf_base_frame_add_ancillary_data(&frame->base, data);
}


int mbuf_audio_frame_get_ancillary_data(struct mbuf_audio_frame *frame,
					const char *name,
					struct mbuf_ancillary_data **data)
{
	ULOG_ERRNO_RETURN_ERR_IF(!data, EINVAL);
	*data = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!name, EINVAL);

	return mbuf_base_frame_get_ancillary_data(&frame->base, name, data);
}


int mbuf_audio_frame_remove_ancillary_data(struct mbuf_audio_frame *frame,
					   const char *name)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!name, EINVAL);

	return mbuf_base_frame_remove_ancillary_data(&frame->base, name);
}


int mbuf_audio_frame_foreach_ancillary_data(struct mbuf_audio_frame *frame,
					    mbuf_ancillary_data_cb_t cb,
					    void *userdata)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!cb, EINVAL);

	return mbuf_base_frame_foreach_ancillary_data(
		&frame->base, cb, userdata);
}


bool mbuf_audio_frame_ancillary_data_copier(struct mbuf_ancillary_data *data,
					    void *userdata)
{
	int ret = 0;
	struct mbuf_audio_frame *copy = userdata;

	ret = mbuf_audio_frame_add_ancillary_data(copy, data);
	if (ret < 0)
		ULOG_ERRNO("mbuf_audio_frame_add_ancillary_data", -ret);

	return true;
}


/* Queue API */


int mbuf_audio_frame_queue_new(struct mbuf_audio_frame_queue **ret_obj)
{
	return mbuf_audio_frame_queue_new_with_args(NULL, ret_obj);
}


int mbuf_audio_frame_queue_new_with_args(
	struct mbuf_audio_frame_queue_args *args,
	struct mbuf_audio_frame_queue **ret_obj)
{
	ULOG_ERRNO_RETURN_ERR_IF(!ret_obj, EINVAL);
	*ret_obj = NULL;

	struct mbuf_audio_frame_queue *queue = calloc(1, sizeof(*queue));
	if (!queue)
		return -ENOMEM;
	queue->filter = args ? args->filter : NULL;
	queue->filter_userdata = args ? args->filter_userdata : NULL;
	int max_frames = args ? args->max_frames : 0;

	int ret = mbuf_base_frame_queue_init(&queue->base, max_frames);
	if (ret != 0) {
		mbuf_audio_frame_queue_destroy(queue);
		queue = NULL;
	}

	*ret_obj = queue;
	return ret;
}


int mbuf_audio_frame_queue_push(struct mbuf_audio_frame_queue *queue,
				struct mbuf_audio_frame *frame)
{
	ULOG_ERRNO_RETURN_ERR_IF(!queue, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	if (queue->filter && !queue->filter(frame, queue->filter_userdata))
		return -EPROTO;

	return mbuf_base_frame_queue_push(&queue->base, &frame->base);
}


int mbuf_audio_frame_queue_peek(struct mbuf_audio_frame_queue *queue,
				struct mbuf_audio_frame **out_frame)
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


int mbuf_audio_frame_queue_peek_at(struct mbuf_audio_frame_queue *queue,
				   unsigned int index,
				   struct mbuf_audio_frame **out_frame)
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


int mbuf_audio_frame_queue_pop(struct mbuf_audio_frame_queue *queue,
			       struct mbuf_audio_frame **out_frame)
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


int mbuf_audio_frame_queue_flush(struct mbuf_audio_frame_queue *queue)
{
	ULOG_ERRNO_RETURN_ERR_IF(!queue, EINVAL);

	return mbuf_base_frame_queue_flush(&queue->base);
}


int mbuf_audio_frame_queue_get_event(struct mbuf_audio_frame_queue *queue,
				     struct pomp_evt **out_evt)
{
	ULOG_ERRNO_RETURN_ERR_IF(!out_evt, EINVAL);
	*out_evt = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!queue, EINVAL);

	return mbuf_base_frame_queue_get_event(&queue->base, out_evt);
}


int mbuf_audio_frame_queue_get_count(struct mbuf_audio_frame_queue *queue)
{
	ULOG_ERRNO_RETURN_ERR_IF(!queue, EINVAL);

	return mbuf_base_frame_queue_get_count(&queue->base);
}


int mbuf_audio_frame_queue_destroy(struct mbuf_audio_frame_queue *queue)
{
	ULOG_ERRNO_RETURN_ERR_IF(!queue, EINVAL);

	mbuf_base_frame_queue_deinit(&queue->base);

	free(queue);
	return 0;
}
