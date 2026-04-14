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


#pragma once


#include "mbuf_audio_frame.h"
#include "mbuf_coded_video_frame.h"
#include "mbuf_raw_video_frame.h"
#include <libpomp.hpp>
#include <memory>

/**
 * Main namespace for media buffers C++ API.
 */
namespace mbuf {

/**
 * @brief Abstract base class for media buffer queues.
 *
 * This class provides a C++ interface for managing queues of media buffers,
 * abstracting the underlying C API for different frame types (coded video,
 * raw video, audio). It supports creating, pushing, popping, peeking, and
 * managing events for these queues.
 */
class MBUF_API Queue {
	Queue(const Queue &) = delete;
	Queue &operator=(const Queue &) = delete;

	Queue(Queue &&) = default;
	Queue &operator=(Queue &&) = default;

protected:
	Queue() = default;

public:
	/**
	 * Enumeration for the type of media buffer queue.
	 */
	enum class Type { CODED_VIDEO, RAW_VIDEO, AUDIO };

	/**
	 * @brief Destructor for the Queue class.
	 */
	virtual ~Queue() = default;

	/**
	 * Get the pomp event associated with the queue.
	 *
	 * This event is signaled when a new frame is pushed to the queue.
	 *
	 * @param evt: [out] Pointer to receive the pomp event.
	 *
	 * @return 0 on success, negative errno on error.
	 */
	virtual int getEvent(struct pomp_evt **evt) const = 0;

	/**
	 * Get the current number of frames in the queue.
	 *
	 * @return The number of frames in the queue, or a negative errno on
	 * error.
	 */
	virtual int getCount() const = 0;

	/**
	 * Flush all frames from the queue.
	 *
	 * All frames currently in the queue will be unreferenced.
	 *
	 * @return 0 on success, negative errno on error.
	 */
	virtual int flush() = 0;

	/**
	 * @brief Get the raw pointer to the underlying C queue object.
	 *
	 * This can be used for direct interaction with the C API if necessary.
	 *
	 * @return A void pointer to the native queue object.
	 */
	virtual void *getQueuePtr() const = 0;

	/**
	 * Destroy the queue.
	 *
	 * If the queue was created with `owner = true`, this will destroy
	 * the underlying native queue. If `owner = false`, it will only
	 * release the C++ wrapper.
	 *
	 * @return 0 on success, negative errno on error.
	 */
	virtual int destroy() = 0;

	/**
	 * Get the type of the queue.
	 *
	 * @return The type of the queue (e.g., CODED_VIDEO, RAW_VIDEO, AUDIO).
	 */
	virtual Type getType() const = 0;

	/**
	 * Comparison operator to check if the queue wraps a specific native
	 * queue pointer.
	 *
	 * @param queueRawPtr: A raw pointer to a native queue object.
	 *
	 * @return true if this Queue object wraps the given native queue
	 * pointer, false otherwise.
	 */
	virtual bool operator==(const void *) const = 0;

	/**
	 * Push a frame into the queue.
	 *
	 * This call does not transfer ownership of the frame; the queue will
	 * reference the frame internally. The caller still retains a reference
	 * on it and must call `mbuf_xxx_frame_unref()` when it no
	 * longer needs the reference.
	 *
	 * @param frame: The frame to push.
	 *
	 * @return 0 on success, negative errno on error.
	 */
	virtual int pushFrame(struct mbuf_coded_video_frame *frame) = 0;
	virtual int pushFrame(struct mbuf_raw_video_frame *frame) = 0;
	virtual int pushFrame(struct mbuf_audio_frame *frame) = 0;

	/**
	 * Pop a frame from the queue.
	 *
	 * This function returns the first frame in the queue (or
	 * -EAGAIN if the queue is empty), and removes it from the queue. The
	 * returned frame is properly referenced, so the caller will need to
	 * call `mbuf_xxx_frame_unref()` when the frame is no longer
	 * needed.
	 *
	 * @param frame: [out] The first frame in the queue.
	 *
	 * @return 0 on success, negative errno on error.
	 */
	virtual int popFrame(struct mbuf_coded_video_frame **frame) = 0;
	virtual int popFrame(struct mbuf_raw_video_frame **frame) = 0;
	virtual int popFrame(struct mbuf_audio_frame **frame) = 0;

	/**
	 * Peek at the first frame in the queue.
	 *
	 * This function returns the first frame in the queue (or
	 * -EAGAIN if the queue is empty), but does not remove it. The returned
	 * frame is properly referenced, so the caller will need to call
	 * `mbuf_xxx_frame_unref()` when the frame is no longer needed.
	 *
	 * @param frame: [out] The first frame in the queue.
	 *
	 * @return 0 on success, negative errno on error.
	 */
	virtual int peekFrame(struct mbuf_coded_video_frame **frame) = 0;
	virtual int peekFrame(struct mbuf_raw_video_frame **frame) = 0;
	virtual int peekFrame(struct mbuf_audio_frame **frame) = 0;

	/**
	 * Peek at a frame at a specific index in the queue.
	 *
	 * This function returns the frame at the given index (or
	 * -ENOENT if the index is out of bounds or the queue is empty), but
	 * does not remove it. A 0 index corresponds to the first frame. The
	 * returned frame is properly referenced, so the caller will need to
	 * call `mbuf_xxx_frame_unref()` when the frame is no longer
	 * needed.
	 *
	 * @param index: The index of the frame to peek at.
	 * @param frame: [out] The frame at the specified index.
	 *
	 * @return 0 on success, negative errno on error.
	 */
	virtual int peekAtFrame(unsigned int index,
				struct mbuf_coded_video_frame **frame) = 0;
	virtual int peekAtFrame(unsigned int index,
				struct mbuf_raw_video_frame **frame) = 0;
	virtual int peekAtFrame(unsigned int index,
				struct mbuf_audio_frame **frame) = 0;

	/**
	 * Get the underlying native frame queue.
	 *
	 * This method is only valid if the requested type matches the queue
	 * type.
	 *
	 * @param queue: [out] Pointer to receive the native queue object.
	 *
	 * @return 0 on success, -ENOSYS if the queue type does not match,
	 * negative errno on other errors.
	 */
	virtual int
	getCQueue(struct mbuf_coded_video_frame_queue **queue) const = 0;
	virtual int
	getCQueue(struct mbuf_raw_video_frame_queue **queue) const = 0;
	virtual int getCQueue(struct mbuf_audio_frame_queue **queue) const = 0;

	/**
	 * Attach the queue's event to a pomp loop.
	 *
	 * The provided callback will be called when a new frame is pushed to
	 * the queue.
	 *
	 * @param loop: The pomp loop to attach to.
	 * @param cb: The callback function to be called.
	 * @param userdata: User data for the callback.
	 *
	 * @return 0 on success, negative errno on error.
	 */
	int
	attachToLoop(struct pomp_loop *loop, pomp_evt_cb_t cb, void *userdata);

	int attachToLoop(pomp::Loop &loop, pomp_evt_cb_t cb, void *userdata);

	/**
	 * Detach the queue's event from a pomp loop.
	 *
	 * @param loop: The pomp loop to detach from.
	 *
	 * @return 0 on success, negative errno on error.
	 */
	int detachFromLoop(struct pomp_loop *loop);

	int detachFromLoop(pomp::Loop &loop);

	/**
	 * Create a new media buffer queue of a specified type.
	 *
	 * @param type: The type of queue to create (CODED_VIDEO, RAW_VIDEO,
	 * AUDIO).
	 * @param owner: If true, the returned unique_ptr owns the underlying
	 * native queue and will destroy it when it goes out of scope. Defaults
	 * to true.
	 *
	 * @return A unique_ptr to the newly created Queue object, or nullptr on
	 * error.
	 */
	static std::unique_ptr<Queue> create(Type type, bool owner = true);

	/**
	 * Create a new frame queue with specific arguments.
	 *
	 * @param args: Pointer to a structure containing creation arguments for
	 * the queue.
	 * @param owner: If true, the returned unique_ptr owns the underlying
	 * native queue. Defaults to true.
	 *
	 * @return A unique_ptr to the newly created Queue object, or nullptr on
	 * error.
	 */
	static std::unique_ptr<Queue>
	createWithArgs(struct mbuf_coded_video_frame_queue_args *args,
		       bool owner = true);

	static std::unique_ptr<Queue>
	createWithArgs(struct mbuf_raw_video_frame_queue_args *args,
		       bool owner = true);

	static std::unique_ptr<Queue>
	createWithArgs(struct mbuf_audio_frame_queue_args *args,
		       bool owner = true);

	/**
	 * Wrap an existing native frame queue with a C++ Queue
	 * object.
	 *
	 * @param existingNativeQueue: Pointer to the existing native queue.
	 * @param owner: If true, the returned unique_ptr takes ownership of the
	 * native queue and will destroy it when it goes out of scope. Defaults
	 * to false.
	 *
	 * @return A unique_ptr to the wrapped Queue object, or nullptr if
	 * `existingNativeQueue` is null.
	 */
	static std::unique_ptr<Queue>
	wrapExisting(struct mbuf_coded_video_frame_queue *existingNativeQueue,
		     bool owner = false);

	static std::unique_ptr<Queue>
	wrapExisting(struct mbuf_raw_video_frame_queue *existingNativeQueue,
		     bool owner = false);

	static std::unique_ptr<Queue>
	wrapExisting(struct mbuf_audio_frame_queue *existingNativeQueue,
		     bool owner = false);
};


} /* namespace mbuf */
