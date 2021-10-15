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

#ifndef _MBUF_BASE_FRAME_H_
#define _MBUF_BASE_FRAME_H_

#include <pthread.h>
#include <stdatomic.h>

#include <media-buffers/mbuf_ancillary_data.h>

#include <futils/list.h>
#include <video-metadata/vmeta.h>

#include "mbuf_utils.h"

typedef void (*frame_cleaner_t)(void *frame);

struct mbuf_base_frame {
	struct vmeta_frame *meta;
	void *parent;
	frame_cleaner_t cleaner;

	pthread_mutex_t ancillary_lock;
	bool ancillary_lock_created;
	struct list_node ancillary_data;

	atomic_bool finalized;
	mbuf_rwlock_t rwlock;
	atomic_uint refcount;
};

struct mbuf_frame_holder {
	struct mbuf_base_frame *base;
	struct list_node node;
};

struct mbuf_base_frame_queue {
	pthread_mutex_t lock;
	bool lock_created;
	struct list_node frames;
	int nframes;
	int maxframes;
	struct pomp_evt *event;
};

/* Frame API */

int mbuf_base_frame_init(struct mbuf_base_frame *frame,
			 void *parent,
			 frame_cleaner_t cleanup_cb);

int mbuf_base_frame_deinit(struct mbuf_base_frame *frame);

int mbuf_base_frame_ref(struct mbuf_base_frame *frame);

int mbuf_base_frame_unref(struct mbuf_base_frame *frame);

int mbuf_base_frame_set_metadata(struct mbuf_base_frame *frame,
				 struct vmeta_frame *meta);

int mbuf_base_frame_get_metadata(struct mbuf_base_frame *frame,
				 struct vmeta_frame **meta);

void mbuf_base_frame_finalize(struct mbuf_base_frame *frame);

bool mbuf_base_frame_is_finalized(struct mbuf_base_frame *frame);

int mbuf_base_frame_rdlock(struct mbuf_base_frame *frame);

int mbuf_base_frame_rdunlock(struct mbuf_base_frame *frame);

int mbuf_base_frame_wrlock(struct mbuf_base_frame *frame);

int mbuf_base_frame_wrunlock(struct mbuf_base_frame *frame);

int mbuf_base_frame_add_ancillary_string(struct mbuf_base_frame *frame,
					 const char *name,
					 const char *value);

int mbuf_base_frame_add_ancillary_buffer(struct mbuf_base_frame *frame,
					 const char *name,
					 const void *buffer,
					 size_t len);

int mbuf_base_frame_add_ancillary_data(struct mbuf_base_frame *frame,
				       struct mbuf_ancillary_data *data);

int mbuf_base_frame_get_ancillary_data(struct mbuf_base_frame *frame,
				       const char *name,
				       struct mbuf_ancillary_data **data);

int mbuf_base_frame_remove_ancillary_data(struct mbuf_base_frame *frame,
					  const char *name);

int mbuf_base_frame_foreach_ancillary_data(struct mbuf_base_frame *frame,
					   mbuf_ancillary_data_cb_t cb,
					   void *userdata);

/* Queue API */

int mbuf_base_frame_queue_init(struct mbuf_base_frame_queue *queue,
			       int maxframes);

int mbuf_base_frame_queue_deinit(struct mbuf_base_frame_queue *queue);

int mbuf_base_frame_queue_push(struct mbuf_base_frame_queue *queue,
			       struct mbuf_base_frame *base);

int mbuf_base_frame_queue_peek(struct mbuf_base_frame_queue *queue,
			       void **out_frame);

int mbuf_base_frame_queue_pop(struct mbuf_base_frame_queue *queue,
			      void **out_frame);

int mbuf_base_frame_queue_flush(struct mbuf_base_frame_queue *queue);

int mbuf_base_frame_queue_get_event(struct mbuf_base_frame_queue *queue,
				    struct pomp_evt **out_evt);

#endif /* _MBUF_BASE_FRAME_H_ */
