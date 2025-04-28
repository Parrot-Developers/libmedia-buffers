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

#include "mbuf_base_frame.h"
#include "mbuf_internal.h"

#include <errno.h>
#include <stdlib.h>

#include <libpomp.h>
#include <video-metadata/vmeta.h>

#define ULOG_TAG mbuf_base_frame
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);


struct mbuf_ancillary_data_holder {
	struct mbuf_ancillary_data *data;

	struct list_node node;
};


/* Frame API */


int mbuf_base_frame_init(struct mbuf_base_frame *frame,
			 void *parent,
			 frame_cleaner_t cleanup_cb)
{
	int ret;
	atomic_init(&frame->refcount, 1);
	atomic_init(&frame->finalized, false);
	mbuf_rwlock_init(&frame->rwlock);

	frame->parent = parent;
	frame->cleaner = cleanup_cb;

	list_init(&frame->ancillary_data);

	ret = pthread_mutex_init(&frame->ancillary_lock, NULL);
	if (ret != 0)
		return -ret;
	frame->ancillary_lock_created = true;

	ret = pthread_mutex_init(&frame->meta_lock, NULL);
	if (ret != 0)
		return -ret;
	frame->meta_lock_created = true;

	return 0;
}


int mbuf_base_frame_deinit(struct mbuf_base_frame *frame)
{
	struct mbuf_ancillary_data_holder *holder, *tmp;

	if (frame->meta_lock_created) {
		mbuf_base_frame_set_metadata(frame, NULL);
		pthread_mutex_destroy(&frame->meta_lock);
	}

	if (!frame->ancillary_lock_created)
		return 0;
	pthread_mutex_lock(&frame->ancillary_lock);

	list_walk_entry_forward_safe(&frame->ancillary_data, holder, tmp, node)
	{
		mbuf_ancillary_data_unref(holder->data);
		list_del(&holder->node);
		free(holder);
	}

	pthread_mutex_unlock(&frame->ancillary_lock);
	pthread_mutex_destroy(&frame->ancillary_lock);
	return 0;
}


int mbuf_base_frame_ref(struct mbuf_base_frame *frame)
{
	unsigned int prev = atomic_fetch_add(&frame->refcount, 1);
	if (prev == 0) {
		ULOGE("calling ref on an object with refcount zero, aborting");
		atomic_store(&frame->refcount, 0);
		return -EINVAL;
	}
	return 0;
}


int mbuf_base_frame_unref(struct mbuf_base_frame *frame)
{
	unsigned int prev = atomic_fetch_sub(&frame->refcount, 1);
	if (prev == 1)
		frame->cleaner(frame->parent);
	return 0;
}


int mbuf_base_frame_set_metadata(struct mbuf_base_frame *frame,
				 struct vmeta_frame *meta)
{
	int ret;

	pthread_mutex_lock(&frame->meta_lock);

	/* Remove old metadata if present */
	if (frame->meta) {
		ret = vmeta_frame_unref(frame->meta);
		if (ret != 0) {
			ULOG_ERRNO("vmeta_frame_unref", -ret);
			goto out;
		}
		frame->meta = NULL;
	}

	/* Add new metadata if given */
	if (meta) {
		ret = vmeta_frame_ref(meta);
		if (ret != 0) {
			ULOG_ERRNO("vmeta_frame_ref", -ret);
			goto out;
		}
		frame->meta = meta;
	}

	ret = 0;

out:
	pthread_mutex_unlock(&frame->meta_lock);
	return ret;
}


int mbuf_base_frame_get_metadata(struct mbuf_base_frame *frame,
				 struct vmeta_frame **meta)
{
	int ret;

	*meta = NULL;

	pthread_mutex_lock(&frame->meta_lock);

	if (!frame->meta) {
		ret = -ENOENT;
		goto out;
	}

	ret = vmeta_frame_ref(frame->meta);
	if (ret != 0)
		goto out;

	*meta = frame->meta;

	ret = 0;
out:
	pthread_mutex_unlock(&frame->meta_lock);
	return ret;
}


void mbuf_base_frame_finalize(struct mbuf_base_frame *frame)
{
	atomic_store(&frame->finalized, true);
}


bool mbuf_base_frame_is_finalized(struct mbuf_base_frame *frame)
{
	return atomic_load(&frame->finalized);
}


int mbuf_base_frame_rdlock(struct mbuf_base_frame *frame)
{
	return mbuf_rwlock_rdlock(&frame->rwlock);
}


int mbuf_base_frame_rdunlock(struct mbuf_base_frame *frame)
{
	return mbuf_rwlock_rdunlock(&frame->rwlock);
}


int mbuf_base_frame_wrlock(struct mbuf_base_frame *frame)
{
	return mbuf_rwlock_wrlock(&frame->rwlock);
}


int mbuf_base_frame_wrunlock(struct mbuf_base_frame *frame)
{
	return mbuf_rwlock_wrunlock(&frame->rwlock);
}


static int
mbuf_base_frame_add_ancillary_internal(struct mbuf_base_frame *frame,
				       struct mbuf_ancillary_data *data)
{
	int ret = 0;
	struct mbuf_ancillary_data_holder *holder;

	pthread_mutex_lock(&frame->ancillary_lock);

	list_walk_entry_forward(&frame->ancillary_data, holder, node)
	{
		if (strcmp(data->name, holder->data->name) == 0) {
			ret = -EEXIST;
			goto out;
		}
	}

	holder = calloc(1, sizeof(*holder));
	if (!holder) {
		ret = -ENOMEM;
		goto out;
	}

	ret = mbuf_ancillary_data_ref(data);
	if (ret != 0) {
		free(holder);
		goto out;
	}

	holder->data = data;

	list_add_before(&frame->ancillary_data, &holder->node);

out:
	pthread_mutex_unlock(&frame->ancillary_lock);
	return ret;
}


static int mbuf_base_frame_create_ancillary_internal(
	struct mbuf_base_frame *frame,
	const char *name,
	const void *buffer,
	size_t len,
	bool is_string,
	const struct mbuf_ancillary_data_cbs *cbs)
{
	int ret = 0;
	struct mbuf_ancillary_data *ad;

	ad = calloc(1, sizeof(*ad));
	if (!ad)
		return -ENOMEM;

	/* Initialize ref_count to 1, we will unref it later */
	atomic_store(&ad->ref_count, 1);

	ad->is_string = is_string;
	ad->len = len;
	ad->name = strdup(name);
	if (!ad->name) {
		ret = -ENOMEM;
		goto out;
	}
	ad->buffer = malloc(len);
	if (!ad->buffer) {
		ret = -ENOMEM;
		goto out;
	}
	memcpy(ad->buffer, buffer, len);

	if (cbs != NULL)
		memcpy(&ad->cbs, cbs, sizeof(*cbs));

	ret = mbuf_base_frame_add_ancillary_internal(frame, ad);

out:
	mbuf_ancillary_data_unref(ad);
	return ret;
}


int mbuf_base_frame_add_ancillary_string(struct mbuf_base_frame *frame,
					 const char *name,
					 const char *value)
{
	size_t len = strlen(value) + 1;
	return mbuf_base_frame_create_ancillary_internal(
		frame, name, value, len, true, NULL);
}


int mbuf_base_frame_add_ancillary_buffer(struct mbuf_base_frame *frame,
					 const char *name,
					 const void *buffer,
					 size_t len)
{
	return mbuf_base_frame_create_ancillary_internal(
		frame, name, buffer, len, false, NULL);
}


int mbuf_base_frame_add_ancillary_buffer_with_cbs(
	struct mbuf_base_frame *frame,
	const char *name,
	const void *buffer,
	size_t len,
	const struct mbuf_ancillary_data_cbs *cbs)
{
	return mbuf_base_frame_create_ancillary_internal(
		frame, name, buffer, len, false, cbs);
}


int mbuf_base_frame_add_ancillary_data(struct mbuf_base_frame *frame,
				       struct mbuf_ancillary_data *data)
{
	return mbuf_base_frame_add_ancillary_internal(frame, data);
}


int mbuf_base_frame_get_ancillary_data(struct mbuf_base_frame *frame,
				       const char *name,
				       struct mbuf_ancillary_data **data)
{
	int ret = 0;
	struct mbuf_ancillary_data_holder *holder;

	pthread_mutex_lock(&frame->ancillary_lock);

	list_walk_entry_forward(&frame->ancillary_data, holder, node)
	{
		if (strcmp(name, holder->data->name) != 0)
			continue;
		mbuf_ancillary_data_ref(holder->data);
		*data = holder->data;
		goto out;
	}

	ret = -ENOENT;

out:
	pthread_mutex_unlock(&frame->ancillary_lock);
	return ret;
}


int mbuf_base_frame_remove_ancillary_data(struct mbuf_base_frame *frame,
					  const char *name)
{
	int ret = 0;
	struct mbuf_ancillary_data_holder *holder, *tmp;

	pthread_mutex_lock(&frame->ancillary_lock);

	list_walk_entry_forward_safe(&frame->ancillary_data, holder, tmp, node)
	{
		if (strcmp(name, holder->data->name) != 0)
			continue;
		mbuf_ancillary_data_unref(holder->data);
		list_del(&holder->node);
		free(holder);
		goto out;
	}

	ret = -ENOENT;

out:
	pthread_mutex_unlock(&frame->ancillary_lock);
	return ret;
}


int mbuf_base_frame_foreach_ancillary_data(struct mbuf_base_frame *frame,
					   mbuf_ancillary_data_cb_t cb,
					   void *userdata)
{
	struct mbuf_ancillary_data_holder *holder;

	pthread_mutex_lock(&frame->ancillary_lock);

	list_walk_entry_forward(&frame->ancillary_data, holder, node)
	{
		bool cont = cb(holder->data, userdata);
		if (!cont)
			break;
	}

	pthread_mutex_unlock(&frame->ancillary_lock);
	return 0;
}


/* Queue API */


static int
mbuf_base_frame_queue_flush_internal(struct mbuf_base_frame_queue *queue)
{
	struct mbuf_frame_holder *holder, *tmp;

	list_walk_entry_forward_safe(&queue->frames, holder, tmp, node)
	{
		int res = mbuf_base_frame_unref(holder->base);
		if (res != 0 && res != -ENOENT)
			ULOG_ERRNO("mbuf_base_frame_unref", -res);
		list_del(&holder->node);
		free(holder);
	}

	queue->nframes = 0;
	pomp_evt_clear(queue->event);

	return 0;
}


int mbuf_base_frame_queue_init(struct mbuf_base_frame_queue *queue,
			       int maxframes)
{
	queue->maxframes = maxframes;
	list_init(&queue->frames);
	int ret = pthread_mutex_init(&queue->lock, NULL);
	if (ret != 0)
		return ret;
	queue->lock_created = true;

	queue->event = pomp_evt_new();
	if (!queue->event) {
		ret = -ENOMEM;
		return ret;
	}

	return 0;
}


int mbuf_base_frame_queue_deinit(struct mbuf_base_frame_queue *queue)
{
	int ret;

	if (queue->lock_created)
		pthread_mutex_lock(&queue->lock);

	if (queue->nframes > 0) {
		ULOGW("destroying a non-empty queue");
		ret = mbuf_base_frame_queue_flush_internal(queue);
		if (ret != 0)
			ULOG_ERRNO("mbuf_base_frame_queue_flush_internal",
				   -ret);
	}

	if (queue->event) {
		ret = pomp_evt_destroy(queue->event);
		if (ret != 0)
			ULOG_ERRNO("pomp_evt_destroy", -ret);
		queue->event = NULL;
	}

	if (queue->lock_created) {
		pthread_mutex_unlock(&queue->lock);
		pthread_mutex_destroy(&queue->lock);
	}
	return 0;
}


int mbuf_base_frame_queue_push(struct mbuf_base_frame_queue *queue,
			       struct mbuf_base_frame *base)
{
	int ret;
	struct mbuf_frame_holder *holder, *tmp;

	holder = calloc(1, sizeof(*holder));
	if (!holder)
		return -ENOMEM;
	list_node_unref(&holder->node);

	pthread_mutex_lock(&queue->lock);

	/* Drop a frame if needed */
	if (queue->maxframes != 0 && queue->nframes >= queue->maxframes) {
		tmp = list_pop(&queue->frames, struct mbuf_frame_holder, node);
		if (!tmp) {
			ret = -EPROTO;
			goto out;
		}
		queue->nframes--;
		mbuf_base_frame_unref(tmp->base);
		free(tmp);
	}

	ret = mbuf_base_frame_ref(base);
	if (ret != 0)
		goto out;
	holder->base = base;

	ret = pomp_evt_signal(queue->event);
	if (ret != 0)
		goto out;

	list_push(&queue->frames, &holder->node);
	queue->nframes++;

out:
	pthread_mutex_unlock(&queue->lock);

	if (ret != 0)
		free(holder);
	return ret;
}


int mbuf_base_frame_queue_peek(struct mbuf_base_frame_queue *queue,
			       void **out_frame)
{
	int ret;
	struct mbuf_frame_holder *holder;

	pthread_mutex_lock(&queue->lock);

	if (queue->nframes == 0) {
		ret = -EAGAIN;
		goto out;
	}

	holder = list_entry(
		list_first(&queue->frames), struct mbuf_frame_holder, node);
	if (!holder) {
		ret = -EPROTO;
		goto out;
	}

	ret = mbuf_base_frame_ref(holder->base);
	if (ret != 0)
		goto out;
	*out_frame = holder->base->parent;

out:
	pthread_mutex_unlock(&queue->lock);
	return ret;
}


int mbuf_base_frame_queue_peek_at(struct mbuf_base_frame_queue *queue,
				  unsigned int index,
				  void **out_frame)
{
	int ret;
	struct mbuf_frame_holder *holder = NULL;
	unsigned int i = 0;
	bool found = false;

	pthread_mutex_lock(&queue->lock);

	if (queue->nframes == 0) {
		ret = -EAGAIN;
		goto out;
	}

	list_walk_entry_forward(&queue->frames, holder, node)
	{
		if (i == index) {
			found = true;
			break;
		}
		i++;
	}

	if (!found) {
		ret = -ENOENT;
		goto out;
	}

	ret = mbuf_base_frame_ref(holder->base);
	if (ret != 0)
		goto out;
	*out_frame = holder->base->parent;

out:
	pthread_mutex_unlock(&queue->lock);
	return ret;
}


int mbuf_base_frame_queue_pop(struct mbuf_base_frame_queue *queue,
			      void **out_frame)
{
	int ret = 0;
	struct mbuf_frame_holder *holder;

	pthread_mutex_lock(&queue->lock);

	if (queue->nframes == 0) {
		ret = -EAGAIN;
		goto out;
	}

	holder = list_pop(&queue->frames, struct mbuf_frame_holder, node);
	if (!holder) {
		ret = -EPROTO;
		goto out;
	}
	queue->nframes--;

	if (queue->nframes == 0)
		pomp_evt_clear(queue->event);

	*out_frame = holder->base->parent;
	free(holder);

out:
	pthread_mutex_unlock(&queue->lock);
	return ret;
}


int mbuf_base_frame_queue_flush(struct mbuf_base_frame_queue *queue)
{
	pthread_mutex_lock(&queue->lock);

	mbuf_base_frame_queue_flush_internal(queue);

	pthread_mutex_unlock(&queue->lock);
	return 0;
}


int mbuf_base_frame_queue_get_event(struct mbuf_base_frame_queue *queue,
				    struct pomp_evt **out_evt)
{
	*out_evt = queue->event;

	return 0;
}


int mbuf_base_frame_queue_get_count(struct mbuf_base_frame_queue *queue)
{
	int ret;

	pthread_mutex_lock(&queue->lock);
	ret = queue->nframes;
	pthread_mutex_unlock(&queue->lock);

	return ret;
}
