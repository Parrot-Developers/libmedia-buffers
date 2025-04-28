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

#ifndef _MBUF_AUDIO_FRAME_H_
#define _MBUF_AUDIO_FRAME_H_

#include <audio-defs/adefs.h>
#include <libpomp.h>
#include <media-buffers/mbuf_ancillary_data.h>
#include <media-buffers/mbuf_mem.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct mbuf_audio_frame;
struct mbuf_audio_frame_queue;


/**
 * Pre-release callback function.
 *
 * Called before an audio frame is released.
 *
 * @param frame: The frame that will be released.
 * @param userdata: Callback function user data.
 */
typedef void (*mbuf_audio_frame_pre_release_t)(struct mbuf_audio_frame *frame,
					       void *userdata);


/**
 * Filter function for frame queue.
 *
 * @param frame: The frame to filter.
 * @param userdata: Filter userdata passed in mbuf_audio_frame_queue_args.
 *
 * @return true if the frame can be pushed in the queue, false otherwise.
 */
typedef bool (*mbuf_audio_frame_queue_filter_t)(struct mbuf_audio_frame *frame,
						void *userdata);


/**
 * Optional callback functions structure for mbuf_audio_frame.
 */
struct mbuf_audio_frame_cbs {
	/**
	 * Pre-release callback function
	 */
	mbuf_audio_frame_pre_release_t pre_release;
	/**
	 * Userdata for the pre-release callback function
	 */
	void *pre_release_userdata;
};


/**
 * Arguments structure for mbuf_audio_frame_queue_new_with_args.
 */
struct mbuf_audio_frame_queue_args {
	/**
	 * Frame filtering function
	 */
	mbuf_audio_frame_queue_filter_t filter;
	/**
	 * Userdata for the frame filtering function
	 */
	void *filter_userdata;
	/**
	 * Maximum number of frames in the queue.
	 * If max_frames+1 frames are pushed in the queue, the oldest frame will
	 * be dropped.
	 */
	uint32_t max_frames;
};


/**
 * Create a new audio frame based on the given infos.
 *
 * The frame_info structure is copied internally and doesn't need to stay valid
 * for the lifetime of the frame.
 * The frame should only be used by a single thread until it is finalized by
 * calling mbuf_audio_frame_finalize().
 *
 * @param frame_info: Parameters for the new frame.
 * @param ret_obj: [out] Pointer to the new frame object.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_new(struct adef_frame *frame_info,
				  struct mbuf_audio_frame **ret_obj);


/**
 * Set the optional callback functions for an audio frame.
 *
 * The cbs structure is copied internally and doesn't need to stay valid
 * for the lifetime of the frame.
 * The callback functions can only be set until the frame is finalized by
 * calling mbuf_audio_frame_finalize().
 *
 * @param frame: The frame.
 * @param cbs: Callback functions.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_set_callbacks(struct mbuf_audio_frame *frame,
					    struct mbuf_audio_frame_cbs *cbs);


/**
 * Increment the reference count of an audio frame.
 *
 * @note: A frame returned by mbuf_audio_frame_new() or
 * mbuf_audio_frame_copy() already has a refcount of 1. This call is only
 * needed if more than one object keep a reference on the frame at the same
 * time.
 *
 * @param frame: The frame to reference.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_ref(struct mbuf_audio_frame *frame);


/**
 * Decrement the reference count of an audio frame.
 *
 * When the reference count reaches zero, the frame will be freed. Any memory
 * attached to this frame will be unreferenced.
 *
 * @param frame: The frame to unreference.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_unref(struct mbuf_audio_frame *frame);


/* Writer API */


/**
 * Change the frame_info associated with a frame.
 *
 * The frame_info structure is copied internally and doesn't need to stay valid
 * for the lifetime of the frame.
 *
 * @param frame: The frame.
 * @param frame_info: New frame_info for the frame.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_set_frame_info(struct mbuf_audio_frame *frame,
					     struct adef_frame *frame_info);


/**
 * Set the memory associated with a buffer.
 *
 * If a memory was already set for the given buffer, it will be unreferenced and
 * replaced by the new one.
 *
 * @param frame: The frame.
 * @param mem: The memory to associate with the buffer.
 * @param offset: Start offset of the buffer in the memory buffer.
 * @param len: Length of the buffer in the memory buffer.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_set_buffer(struct mbuf_audio_frame *frame,
					 struct mbuf_mem *mem,
					 size_t offset,
					 size_t len);


/**
 * Finalize a frame.
 *
 * A finalized frame can no longer be modified with
 * mbuf_audio_frame_set_buffer(), and allow read-only access to the buffer
 * with mbuf_audio_frame_get_buffer().
 * A frame can only be finalized when its buffer is set.
 *
 * @note Any memory associated with a finalized frame should not have its
 * content modified. The library cannot check that, and it is the responsibility
 * of the caller to ensure this.
 *
 * @param frame: The frame.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_finalize(struct mbuf_audio_frame *frame);


/* Reader API */


/**
 * Check whether the frame uses memory chunks from the given pool.
 *
 * @param frame: The frame.
 * @param pool: The memory pool.
 * @param any: [out] True if the frame uses at least one chunk from the pool.
 *                   Optional, can be NULL.
 * @param all: [out] True if the frame uses only chunks from the pool.
 *                   Optional, can be NULL.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_uses_mem_from_pool(struct mbuf_audio_frame *frame,
						 struct mbuf_pool *pool,
						 bool *any,
						 bool *all);


/**
 * Get mbuf_mem related information about a given buffer.
 *
 * @param frame: The frame.
 * @param info: The structure filled by this call.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int
mbuf_audio_frame_get_buffer_mem_info(struct mbuf_audio_frame *frame,
				     struct mbuf_mem_info *info);


/**
 * Get a read-only buffer from the frame.
 *
 * @note When no longer used, the buffer must be released with
 * mbuf_audio_frame_release_buffer().
 *
 * @param frame: The frame.
 * @param data: [out] The buffer.
 * @param len: [out] The buffer size.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_get_buffer(struct mbuf_audio_frame *frame,
					 const void **data,
					 size_t *len);


/**
 * Release a read-only buffer to the frame.
 *
 * After calling this, the data pointer should no longer be used.
 *
 * @param frame: The frame.
 * @param data: The buffer.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_release_buffer(struct mbuf_audio_frame *frame,
					     const void *data);


/**
 * Get a read-write buffer from the frame.
 *
 * @note When no longer used, the buffer must be released with
 * mbuf_audio_frame_release_rw_buffer().
 * @note Only a single buffer can be held in read-write mode
 * at any time.
 *
 * @param frame: The frame.
 * @param data: [out] The buffer.
 * @param len: [out] The buffer size.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_get_rw_buffer(struct mbuf_audio_frame *frame,
					    void **data,
					    size_t *len);


/**
 * Release a read-write buffer to the frame.
 *
 * After calling this, the data pointer should no longer be used.
 *
 * @param frame: The frame.
 * @param data: The buffer buffer.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_release_rw_buffer(struct mbuf_audio_frame *frame,
						void *data);


/**
 * Get the size of an audio frame.
 *
 * This function retuns the size of the frame. A memory
 * of this capacity will be able to hold a copy of this frame.
 *
 * This function is usually useful before a call to mbuf_audio_frame_copy(),
 * in order to create a memory of compatible size.
 *
 * @param frame: The frame.
 *
 * @return The size on success, negative errno on error.
 */
MBUF_API ssize_t mbuf_audio_frame_get_size(struct mbuf_audio_frame *frame);


/**
 * Copy a frame into a new one, backed by the given memory.
 *
 * The memory must be big enough to hold the whole frame. The required size can
 * be retrived from a frame with the mbuf_audio_frame_get_size()
 * call. The returned frame is not finalized and can be modified by the caller.
 *
 * @param frame: The frame to copy.
 * @param dst: The memory for the new frame.

 * @param ret_obj: [out] The new frame.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_copy(struct mbuf_audio_frame *frame,
				   struct mbuf_mem *dst,
				   struct mbuf_audio_frame **ret_obj);


/**
 * Get the frame_info structure of the given frame.
 *
 * The content will be copied, and thus are valid even beyond the lifetime of
 * the frame.
 *
 * @param frame: The frame.
 * @param frame_info: Pointer filled with the frame_info of the frame.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_get_frame_info(struct mbuf_audio_frame *frame,
					     struct adef_frame *frame_info);


/* Ancillary data API */


/**
 * Add a new ancillary data string to a given frame.
 *
 * The value will be copied internally.
 *
 * @note If a data with the same name already exists, the function returns
 * -EEXIST. It does not replaces the data.
 *
 * @param frame: The frame.
 * @param name: The ancillary data name.
 * @param value: The ancillary data value.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int
mbuf_audio_frame_add_ancillary_string(struct mbuf_audio_frame *frame,
				      const char *name,
				      const char *value);


/**
 * Add a new ancillary data buffer to a given frame.
 *
 * The data will be copied internally.
 *
 * @note If a data with the same name already exists, the function returns
 * -EEXIST. It does not replaces the data.
 *
 * @param frame: The frame.
 * @param name: The ancillary data name.
 * @param buffer: The ancillary data buffer.
 * @param len: The ancillary data length.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int
mbuf_audio_frame_add_ancillary_buffer(struct mbuf_audio_frame *frame,
				      const char *name,
				      const void *buffer,
				      size_t len);


/**
 * Add a new ancillary data buffer to a given frame (with callbacks).
 *
 * The data will be copied internally.
 *
 * @note If a data with the same name already exists, the function returns
 * -EEXIST. It does not replace the data.
 *
 * @param frame: The frame.
 * @param name: The ancillary data name.
 * @param buffer: The ancillary data buffer.
 * @param len: The ancillary data length.
 * @param cbs: Optional callback functions.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_add_ancillary_buffer_with_cbs(
	struct mbuf_audio_frame *frame,
	const char *name,
	const void *buffer,
	size_t len,
	const struct mbuf_ancillary_data_cbs *cbs);


/**
 * Add an existing ancillary data to a given frame.
 *
 * This function is mainly used when copying ancillary data from a frame to
 * another. The ancillary data will be referenced by the frame, no copy will be
 * done.
 *
 * @note If a data with the same name already exists, the function returns
 * -EEXIST. It does not replaces the data.
 *
 * @param frame: The frame.
 * @param data: The ancillary data to add.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int
mbuf_audio_frame_add_ancillary_data(struct mbuf_audio_frame *frame,
				    struct mbuf_ancillary_data *data);


/**
 * Get an ancillary data from a frame.
 *
 * The returned data has its reference count increased. The caller must call
 * mbuf_ancillary_data_unref() when the data is no longer needed.
 *
 * @param frame: The frame.
 * @param name: The ancillary data name.
 * @param data: [out] The ancillary data.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int
mbuf_audio_frame_get_ancillary_data(struct mbuf_audio_frame *frame,
				    const char *name,
				    struct mbuf_ancillary_data **data);


/**
 * Remove an ancillary data from a frame.
 *
 * @note This function returns -ENOENT if no ancillary data with the given name
 * exists.
 *
 * @param frame: The frame.
 * @param name: The ancillary data name.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int
mbuf_audio_frame_remove_ancillary_data(struct mbuf_audio_frame *frame,
				       const char *name);


/**
 * Iterate over ancillary data from a frame.
 *
 * The callback function will be called for each ancillary data. Iteration can
 * be stopped by the callback returning `false'. Ancillary data reference count
 * is not increased during the callback, so the callback should not call
 * mbuf_ancillary_data_unref() unless it previously called
 * mbuf_ancillary_data_ref() on the same data.
 *
 * @note The iteration order is the insertion order. Since insertion can occur
 * from multiple threads, applications should not rely on this order when
 * iterating the ancillary data list.
 * @warning While iterating, the ancillary data list is locked, and thus
 * concurrent modification of the list cannot occur. This means that calling any
 * mbuf_audio_frame_xxx_ancillary_data() function on this frame in the
 * callback will deadlock.
 *
 * @param frame: The frame.
 * @param cb: The iterator callback.
 * @param userdata: Userdata passed to the iterator callback.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int
mbuf_audio_frame_foreach_ancillary_data(struct mbuf_audio_frame *frame,
					mbuf_ancillary_data_cb_t cb,
					void *userdata);


/**
 * Callback for mbuf_xxx_frame_foreach_ancillary_data to copy all data to a
 * given mbuf_audio_frame.
 *
 * @note This function is not intended to be directly called by the application.
 * @warning The userdata pointer of the calling foreach function must be a
 * valid mbuf_audio_frame.
 *
 * @param data: The ancillary data.
 * @param userdata: The destination mbuf_audio_frame.
 *                  All the ancillary data will be copied to this frame.
 *
 * @return true.
 */
MBUF_API bool
mbuf_audio_frame_ancillary_data_copier(struct mbuf_ancillary_data *data,
				       void *userdata);


/* Queue API */


/**
 * Create a new audio frame queue with the default parameters.
 *
 * This call is equivalent to calling mbuf_audio_frame_queue_new_with_args()
 * with a NULL arguments pointer.
 *
 * @param ret_obj: [out] The new queue.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int
mbuf_audio_frame_queue_new(struct mbuf_audio_frame_queue **ret_obj);


/**
 * Create a new audio frame queue with specific arguments.
 *
 * The filter callback will be called each time
 * mbuf_audio_frame_queue_push() is called on the returned queue. If the
 * filter returns false, then the push call will be aborted and will return
 * -EPROTO. The filter function can be set to NULL to allow all frames to be
 * accepted by this queue.
 *
 * The max_frames parameter will limit the number of frames in the queue,
 * dropping the oldest frame in the queue when a new one is pushed. If
 * max_frames is set to zero, then the queue has no limit.
 *
 * The default values if no argument structure is provided is no filter, and no
 * size limit.
 *
 * @param args: The argument structure pointer,
 *              or NULL to use the default arguments.
 * @param ret_obj: [out] The new queue.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int
mbuf_audio_frame_queue_new_with_args(struct mbuf_audio_frame_queue_args *args,
				     struct mbuf_audio_frame_queue **ret_obj);


/**
 * Push a frame into a queue.
 *
 * If a filter is present on the queue, it will be applied to the frame and can
 * refuse the push. In this case -EPROTO will be returned.
 *
 * This call does not transfer ownership of the frame, the queue will reference
 * the frame internally, the caller still retains a reference on it and must
 * call mbuf_audio_frame_unref() when it no longer needs the reference.
 *
 * @param queue: The queue.
 * @param frame: The frame to push.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_queue_push(struct mbuf_audio_frame_queue *queue,
					 struct mbuf_audio_frame *frame);


/**
 * Peek a frame from a queue.
 *
 * This function returns the first frame in the queue (or -EAGAIN if the queue
 * is empty), but does not remove it from the queue. The returned frame is
 * properly referenced, so the caller will need to call
 * mbuf_audio_frame_unref() when the frame is no longer needed.
 *
 * @param queue: The queue.
 * @param frame: [out] The first frame in the queue.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_queue_peek(struct mbuf_audio_frame_queue *queue,
					 struct mbuf_audio_frame **frame);


/**
 * Peek a frame from a queue at a given index.
 *
 * This function returns the frame in the queue (or -ENOENT if the queue
 * is empty) at a given index, but does not remove it from the queue.
 * A 0 index corresponds to the first frame in the queue. The returned
 * frame is properly referenced, so the caller will need to call
 * mbuf_audio_frame_unref() when the frame is no longer needed.
 *
 * @param queue: The queue.
 * @param index: Index in the queue.
 * @param frame: [out] The first frame in the queue.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int
mbuf_audio_frame_queue_peek_at(struct mbuf_audio_frame_queue *queue,
			       unsigned int index,
			       struct mbuf_audio_frame **frame);


/**
 * Pop a frame from a queue.
 *
 * This function returns the first frame in the queue (or -EAGAIN if the queue
 * is empty), and removes it from the queue. The returned frame is properly
 * referenced, so the caller will need to call mbuf_audio_frame_unref() when
 * the frame is no longer needed.
 *
 * @param queue: The queue.
 * @param frame: [out] The first frame in the queue.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_queue_pop(struct mbuf_audio_frame_queue *queue,
					struct mbuf_audio_frame **frame);


/**
 * Flush a queue.
 *
 * This function will unreference all frames inside the queue, and clear the
 * queue event.
 *
 * @param queue: The queue.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_audio_frame_queue_flush(struct mbuf_audio_frame_queue *queue);


/**
 * Get the pomp_evt associated with the queue.
 *
 * The event can be associated with a pomp_loop to wait for incoming frames.
 * The event is signaled when a frame is pushed, not when the queue is
 * non-empty. To check if the queue is empty, check the return value of
 * mbuf_audio_frame_queue_peek(). A callback which needs to handle all
 * available frames in the queue should call mbuf_audio_frame_queue_pop() in
 * a loop until -ENOENT is returned.
 *
 * @param queue: The queue.
 * @param evt: [out] The pomp_evt of the queue.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int
mbuf_audio_frame_queue_get_event(struct mbuf_audio_frame_queue *queue,
				 struct pomp_evt **evt);


/**
 * Get the queue frame count.
 *
 * This function returns the current number of frames held by the queue.
 *
 * @param queue: The queue.
 *
 * @return the queue frame count on success, negative errno on error.
 */
MBUF_API int
mbuf_audio_frame_queue_get_count(struct mbuf_audio_frame_queue *queue);


/**
 * Destroy an audio frame queue.
 *
 * The queue event must not be attached to any loop when this function is
 * called.
 *
 * @note If the queue is not empty, this function will warn and issue an
 * internal call to mbuf_audio_frame_queue_flush()
 *
 * @param queue: The queue.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int
mbuf_audio_frame_queue_destroy(struct mbuf_audio_frame_queue *queue);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MBUF_AUDIO_FRAME_H_ */
