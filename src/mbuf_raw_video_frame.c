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

#include <media-buffers/mbuf_raw_video_frame.h>

#include <futils/list.h>
#include <stdatomic.h>
#include <string.h>

#include "internal/mbuf_mem_internal.h"
#include "mbuf_base_frame.h"
#include "mbuf_utils.h"

#define ULOG_TAG mbuf_raw_video_frame
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);


struct mbuf_raw_video_frame {
	struct mbuf_base_frame base;
	struct vdef_raw_frame info;

	struct {
		struct mbuf_mem *mem;
		void *data;
		size_t len;
	} planes[VDEF_RAW_MAX_PLANE_COUNT];
	unsigned int nplanes;

	struct mbuf_raw_video_frame_cbs cbs;
};


struct mbuf_raw_video_frame_queue {
	struct mbuf_base_frame_queue base;
	mbuf_raw_video_frame_queue_filter_t filter;
	void *filter_userdata;
};


static void mbuf_raw_video_frame_cleaner(void *rframe)
{
	struct mbuf_raw_video_frame *frame = rframe;
	ULOG_ERRNO_RETURN_IF(!frame, EINVAL);

	if (frame->cbs.pre_release)
		frame->cbs.pre_release(frame, frame->cbs.pre_release_userdata);

	/* The frame needs to be deleted */
	int rc = mbuf_rwlock_get_value(&frame->base.rwlock);
	if (rc == RWLOCK_WRLOCKED)
		ULOGW("1 rw-plane/packed-buffer not released"
		      " during frame deletion");
	else if (rc > 0)
		ULOGW("%d ro-plane/packed-buffer not released"
		      " during frame deletion",
		      rc);
	for (unsigned int i = 0; i < frame->nplanes; i++) {
		if (frame->planes[i].mem != NULL) {
			int ret = mbuf_mem_unref(frame->planes[i].mem);
			if (ret != 0)
				ULOG_ERRNO("mbuf_mem_unref(destroy)", -ret);
			frame->planes[i].mem = NULL;
		}
	}
	mbuf_base_frame_set_metadata(&frame->base, NULL);
	mbuf_base_frame_deinit(&frame->base);
	free(frame);
}


int mbuf_raw_video_frame_new(struct vdef_raw_frame *frame_info,
			     struct mbuf_raw_video_frame **ret_obj)
{
	ULOG_ERRNO_RETURN_ERR_IF(!ret_obj, EINVAL);
	*ret_obj = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!frame_info, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!vdef_is_raw_format_valid(&frame_info->format),
				 EINVAL);

	struct mbuf_raw_video_frame *frame = calloc(1, sizeof(*frame));
	if (!frame)
		return -ENOMEM;
	frame->info = *frame_info;
	frame->nplanes = vdef_get_raw_frame_plane_count(&frame_info->format);

	mbuf_base_frame_init(&frame->base, frame, mbuf_raw_video_frame_cleaner);

	*ret_obj = frame;

	return 0;
}


int mbuf_raw_video_frame_set_callbacks(struct mbuf_raw_video_frame *frame,
				       struct mbuf_raw_video_frame_cbs *cbs)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!cbs, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	frame->cbs = *cbs;
	return 0;
}


int mbuf_raw_video_frame_ref(struct mbuf_raw_video_frame *frame)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);

	return mbuf_base_frame_ref(&frame->base);
}


int mbuf_raw_video_frame_unref(struct mbuf_raw_video_frame *frame)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);

	return mbuf_base_frame_unref(&frame->base);
}


/* Writer API */


int mbuf_raw_video_frame_set_frame_info(struct mbuf_raw_video_frame *frame,
					struct vdef_raw_frame *frame_info)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!frame_info, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	/* Check that the change in frame_info will not lead to a bad number of
	 * planes */
	unsigned int new_nplanes =
		vdef_get_raw_frame_plane_count(&frame_info->format);
	unsigned int filled_planes = 0;
	for (int i = frame->nplanes; i >= 0; i--) {
		if (frame->planes[i].mem != NULL) {
			filled_planes = i + 1;
			break;
		}
	}
	if (filled_planes > new_nplanes) {
		ULOGE("new frame info has only %u planes, "
		      "while this frame already has %u set",
		      new_nplanes,
		      filled_planes);
		return -EINVAL;
	}

	frame->info = *frame_info;
	frame->nplanes = new_nplanes;
	return 0;
}


int mbuf_raw_video_frame_set_metadata(struct mbuf_raw_video_frame *frame,
				      struct vmeta_frame *meta)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!meta, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	return mbuf_base_frame_set_metadata(&frame->base, meta);
}


int mbuf_raw_video_frame_set_plane(struct mbuf_raw_video_frame *frame,
				   unsigned int plane,
				   struct mbuf_mem *mem,
				   size_t offset,
				   size_t len)
{
	int ret;

	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mem, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(plane >= frame->nplanes, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	/* Remove previous memory for plane */
	if (frame->planes[plane].mem) {
		ret = mbuf_mem_unref(frame->planes[plane].mem);
		if (ret != 0) {
			ULOG_ERRNO("mbuf_mem_unref", -ret);
			return ret;
		}
		memset(&frame->planes[plane], 0, sizeof(frame->planes[plane]));
	}

	ret = mbuf_mem_ref(mem);
	if (ret != 0) {
		ULOG_ERRNO("mbuf_mem_ref", -ret);
		return ret;
	}

	frame->planes[plane].mem = mem;
	frame->planes[plane].len = len;
	uint8_t *data = mem->data;
	frame->planes[plane].data = data + offset;

	return 0;
}


int mbuf_raw_video_frame_finalize(struct mbuf_raw_video_frame *frame)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);

	for (unsigned int i = 0; i < frame->nplanes; i++)
		ULOG_ERRNO_RETURN_ERR_IF(!frame->planes[i].mem, EPROTO);

	mbuf_base_frame_finalize(&frame->base);

	return 0;
}


/* Reader API */


int mbuf_raw_video_frame_uses_mem_from_pool(struct mbuf_raw_video_frame *frame,
					    struct mbuf_pool *pool,
					    bool *any_,
					    bool *all_)
{
	bool any = false;
	bool all = true;

	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!frame->nplanes, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!pool, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	for (unsigned int i = 0; i < frame->nplanes; i++) {
		if (frame->planes[i].mem->pool == pool)
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


int mbuf_raw_video_frame_get_metadata(struct mbuf_raw_video_frame *frame,
				      struct vmeta_frame **meta)
{
	ULOG_ERRNO_RETURN_ERR_IF(!meta, EINVAL);
	*meta = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	return mbuf_base_frame_get_metadata(&frame->base, meta);
}


int mbuf_raw_video_frame_get_plane_mem_info(struct mbuf_raw_video_frame *frame,
					    unsigned int plane,
					    struct mbuf_mem_info *info)
{
	struct mbuf_mem *mem = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!info, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(plane >= frame->nplanes, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	mem = frame->planes[plane].mem;
	info->cookie = mem->cookie;
	info->specific = mem->specific;

	return 0;
}


int mbuf_raw_video_frame_get_plane(struct mbuf_raw_video_frame *frame,
				   unsigned int plane,
				   const void **data,
				   size_t *len)
{
	ULOG_ERRNO_RETURN_ERR_IF(!data, EINVAL);
	*data = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!len, EINVAL);
	*len = 0;
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(plane >= frame->nplanes, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	int ret = mbuf_base_frame_rdlock(&frame->base);
	if (ret != 0)
		return ret;

	*data = frame->planes[plane].data;
	*len = frame->planes[plane].len;

	return 0;
}


int mbuf_raw_video_frame_release_plane(struct mbuf_raw_video_frame *frame,
				       unsigned int plane,
				       const void *data)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(plane >= frame->nplanes, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);
	ULOG_ERRNO_RETURN_ERR_IF(data != frame->planes[plane].data, EINVAL);
	return mbuf_base_frame_rdunlock(&frame->base);
}


int mbuf_raw_video_frame_get_rw_plane(struct mbuf_raw_video_frame *frame,
				      unsigned int plane,
				      void **data,
				      size_t *len)
{
	ULOG_ERRNO_RETURN_ERR_IF(!data, EINVAL);
	*data = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!len, EINVAL);
	*len = 0;
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(plane >= frame->nplanes, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	int ret = mbuf_base_frame_wrlock(&frame->base);
	if (ret != 0)
		return ret;

	*data = frame->planes[plane].data;
	*len = frame->planes[plane].len;

	return 0;
}


int mbuf_raw_video_frame_release_rw_plane(struct mbuf_raw_video_frame *frame,
					  unsigned int plane,
					  void *data)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(plane >= frame->nplanes, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);
	ULOG_ERRNO_RETURN_ERR_IF(data != frame->planes[plane].data, EINVAL);
	return mbuf_base_frame_wrunlock(&frame->base);
}


int mbuf_raw_video_frame_get_packed_buffer(struct mbuf_raw_video_frame *frame,
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
	ULOG_ERRNO_RETURN_ERR_IF(frame->nplanes == 0, EINVAL);

	int ret = mbuf_base_frame_rdlock(&frame->base);
	if (ret != 0)
		return ret;

	/* Prepare plane length & check if packed */
	*len = 0;
	const uint8_t *expected = NULL;
	bool packed = true;
	for (unsigned int i = 0; i < frame->nplanes; i++) {
		*len += frame->planes[i].len;
		if (expected && expected != frame->planes[i].data)
			packed = false;
		expected = frame->planes[i].data;
		expected += frame->planes[i].len;
	}

	if (packed) {
		*data = frame->planes[0].data;
		ret = 0;
	} else {
		*data = NULL;
		mbuf_base_frame_rdunlock(&frame->base);
		ret = -EPROTO;
	}

	return ret;
}


int mbuf_raw_video_frame_release_packed_buffer(
	struct mbuf_raw_video_frame *frame,
	const void *data)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);
	ULOG_ERRNO_RETURN_ERR_IF(frame->nplanes == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(data != frame->planes[0].data, EINVAL);
	return mbuf_base_frame_rdunlock(&frame->base);
}


int mbuf_raw_video_frame_get_rw_packed_buffer(
	struct mbuf_raw_video_frame *frame,
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
	ULOG_ERRNO_RETURN_ERR_IF(frame->nplanes == 0, EINVAL);

	ret = mbuf_base_frame_wrlock(&frame->base);
	if (ret != 0)
		return ret;

	/* Prepare plane length & check if packed */
	*len = 0;
	const uint8_t *expected = NULL;
	bool packed = true;
	for (unsigned int i = 0; i < frame->nplanes; i++) {
		*len += frame->planes[i].len;
		if (expected && expected != frame->planes[i].data)
			packed = false;
		expected = frame->planes[i].data;
		expected += frame->planes[i].len;
	}

	if (packed) {
		*data = frame->planes[0].data;
		ret = 0;
	} else {
		*data = NULL;
		mbuf_base_frame_wrunlock(&frame->base);
		ret = -EPROTO;
	}

	return ret;
}


int mbuf_raw_video_frame_release_rw_packed_buffer(
	struct mbuf_raw_video_frame *frame,
	void *data)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);
	ULOG_ERRNO_RETURN_ERR_IF(frame->nplanes == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(data != frame->planes[0].data, EINVAL);
	return mbuf_base_frame_wrunlock(&frame->base);
}


ssize_t mbuf_raw_video_frame_get_packed_size(struct mbuf_raw_video_frame *frame,
					     bool remove_stride)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);
	/* If we don't care about strides, just sum up the planes length */
	if (!remove_stride) {
		ssize_t ret = 0;
		for (unsigned int i = 0; i < frame->nplanes; i++)
			ret += frame->planes[i].len;
		return ret;
	}

	/* Otherwise, ask vdef about it */
	return vdef_calc_raw_contiguous_frame_size(&frame->info.format,
						   &frame->info.info.resolution,
						   NULL,
						   NULL,
						   NULL,
						   NULL,
						   NULL);
}


int mbuf_raw_video_frame_copy(struct mbuf_raw_video_frame *frame,
			      struct mbuf_mem *dst,
			      bool remove_stride,
			      struct mbuf_raw_video_frame **ret_obj)
{
	int ret;
	size_t offset;
	struct mbuf_raw_video_frame *new_frame = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(!ret_obj, EINVAL);
	*ret_obj = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!dst, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!dst->data, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	ssize_t tmp =
		mbuf_raw_video_frame_get_packed_size(frame, remove_stride);
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

	ret = mbuf_raw_video_frame_new(&frame->info, &new_frame);
	if (ret != 0)
		goto out;

	ret = mbuf_raw_video_frame_foreach_ancillary_data(
		frame, mbuf_raw_video_frame_ancillary_data_copier, new_frame);
	if (ret != 0)
		goto out;

	offset = 0;
	if (!remove_stride) {
		/* Simple case, just copy the planes */
		for (unsigned int i = 0; i < frame->nplanes; i++) {
			uint8_t *cpdst = dst->data;
			cpdst += offset;
			memcpy(cpdst,
			       frame->planes[i].data,
			       frame->planes[i].len);
			ret = mbuf_raw_video_frame_set_plane(
				new_frame,
				i,
				dst,
				offset,
				frame->planes[i].len);
			if (ret != 0)
				goto out;
			offset += frame->planes[i].len;
		}
	} else {
		/* Remove stride */
		size_t plane_size[VDEF_RAW_MAX_PLANE_COUNT];
		size_t plane_stride[VDEF_RAW_MAX_PLANE_COUNT] = {0};
		ret = vdef_calc_raw_frame_size(&frame->info.format,
					       &frame->info.info.resolution,
					       plane_stride,
					       NULL,
					       NULL,
					       NULL,
					       plane_size,
					       NULL);
		if (ret != 0)
			goto out;
		for (unsigned int i = 0; i < frame->nplanes; i++) {
			uint8_t *cpsrc = frame->planes[i].data;
			uint8_t *cpdst = dst->data;
			cpdst += offset;
			size_t nlines = plane_size[i] / plane_stride[i];
			for (size_t j = 0; j < nlines; j++) {
				size_t dst_offset = j * plane_stride[i];
				size_t src_offset =
					j * frame->info.plane_stride[i];
				memcpy(cpdst + dst_offset,
				       cpsrc + src_offset,
				       plane_stride[i]);
			}
			ret = mbuf_raw_video_frame_set_plane(
				new_frame, i, dst, offset, plane_size[i]);
			if (ret != 0)
				goto out;
			offset += plane_size[i];
			new_frame->info.plane_stride[i] = plane_stride[i];
		}
	}

	mbuf_base_frame_set_metadata(&new_frame->base, frame->base.meta);

out:
	/* Release read-lock before returning */
	mbuf_base_frame_rdunlock(&frame->base);

	if (ret != 0 && new_frame)
		mbuf_raw_video_frame_unref(new_frame);
	else
		*ret_obj = new_frame;
	return ret;
}


int mbuf_raw_video_frame_get_frame_info(struct mbuf_raw_video_frame *frame,
					struct vdef_raw_frame *frame_info)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!frame_info, EINVAL);

	*frame_info = frame->info;
	return 0;
}


/* Ancillary data API */


int mbuf_raw_video_frame_add_ancillary_string(
	struct mbuf_raw_video_frame *frame,
	const char *name,
	const char *value)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!name, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!value, EINVAL);

	return mbuf_base_frame_add_ancillary_string(&frame->base, name, value);
}


int mbuf_raw_video_frame_add_ancillary_buffer(
	struct mbuf_raw_video_frame *frame,
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


int mbuf_raw_video_frame_add_ancillary_data(struct mbuf_raw_video_frame *frame,
					    struct mbuf_ancillary_data *data)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!data, EINVAL);

	return mbuf_base_frame_add_ancillary_data(&frame->base, data);
}


int mbuf_raw_video_frame_get_ancillary_data(struct mbuf_raw_video_frame *frame,
					    const char *name,
					    struct mbuf_ancillary_data **data)
{
	ULOG_ERRNO_RETURN_ERR_IF(!data, EINVAL);
	*data = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!name, EINVAL);

	return mbuf_base_frame_get_ancillary_data(&frame->base, name, data);
}


int mbuf_raw_video_frame_remove_ancillary_data(
	struct mbuf_raw_video_frame *frame,
	const char *name)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!name, EINVAL);

	return mbuf_base_frame_remove_ancillary_data(&frame->base, name);
}


int mbuf_raw_video_frame_foreach_ancillary_data(
	struct mbuf_raw_video_frame *frame,
	mbuf_ancillary_data_cb_t cb,
	void *userdata)
{
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!cb, EINVAL);

	return mbuf_base_frame_foreach_ancillary_data(
		&frame->base, cb, userdata);
}


bool mbuf_raw_video_frame_ancillary_data_copier(
	struct mbuf_ancillary_data *data,
	void *userdata)
{
	int ret = 0;
	struct mbuf_raw_video_frame *copy = userdata;

	ret = mbuf_raw_video_frame_add_ancillary_data(copy, data);
	if (ret < 0)
		ULOG_ERRNO("mbuf_raw_video_frame_add_ancillary_data", -ret);

	return true;
}


/* Queue API */


int mbuf_raw_video_frame_queue_new(struct mbuf_raw_video_frame_queue **ret_obj)
{
	return mbuf_raw_video_frame_queue_new_with_args(NULL, ret_obj);
}


int mbuf_raw_video_frame_queue_new_with_args(
	struct mbuf_raw_video_frame_queue_args *args,
	struct mbuf_raw_video_frame_queue **ret_obj)
{
	ULOG_ERRNO_RETURN_ERR_IF(!ret_obj, EINVAL);
	*ret_obj = NULL;

	struct mbuf_raw_video_frame_queue *queue = calloc(1, sizeof(*queue));
	if (!queue)
		return -ENOMEM;
	queue->filter = args ? args->filter : NULL;
	queue->filter_userdata = args ? args->filter_userdata : NULL;
	int max_frames = args ? args->max_frames : 0;

	int ret = mbuf_base_frame_queue_init(&queue->base, max_frames);
	if (ret != 0) {
		mbuf_raw_video_frame_queue_destroy(queue);
		queue = NULL;
	}

	*ret_obj = queue;
	return ret;
}


int mbuf_raw_video_frame_queue_push(struct mbuf_raw_video_frame_queue *queue,
				    struct mbuf_raw_video_frame *frame)
{
	ULOG_ERRNO_RETURN_ERR_IF(!queue, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!frame, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mbuf_base_frame_is_finalized(&frame->base),
				 EBUSY);

	if (queue->filter && !queue->filter(frame, queue->filter_userdata))
		return -EPROTO;

	return mbuf_base_frame_queue_push(&queue->base, &frame->base);
}


int mbuf_raw_video_frame_queue_peek(struct mbuf_raw_video_frame_queue *queue,
				    struct mbuf_raw_video_frame **out_frame)
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


int mbuf_raw_video_frame_queue_pop(struct mbuf_raw_video_frame_queue *queue,
				   struct mbuf_raw_video_frame **out_frame)
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


int mbuf_raw_video_frame_queue_flush(struct mbuf_raw_video_frame_queue *queue)
{
	ULOG_ERRNO_RETURN_ERR_IF(!queue, EINVAL);

	return mbuf_base_frame_queue_flush(&queue->base);
}


int mbuf_raw_video_frame_queue_get_event(
	struct mbuf_raw_video_frame_queue *queue,
	struct pomp_evt **out_evt)
{
	ULOG_ERRNO_RETURN_ERR_IF(!out_evt, EINVAL);
	*out_evt = NULL;
	ULOG_ERRNO_RETURN_ERR_IF(!queue, EINVAL);

	return mbuf_base_frame_queue_get_event(&queue->base, out_evt);
}


int mbuf_raw_video_frame_queue_destroy(struct mbuf_raw_video_frame_queue *queue)
{
	ULOG_ERRNO_RETURN_ERR_IF(!queue, EINVAL);

	mbuf_base_frame_queue_deinit(&queue->base);

	free(queue);
	return 0;
}
