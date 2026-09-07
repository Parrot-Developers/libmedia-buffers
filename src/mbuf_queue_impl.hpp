/*
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

#include <media-buffers/mbuf_queue.hpp>

#include <media-buffers/mbuf_audio_frame.h>
#include <media-buffers/mbuf_coded_video_frame.h>
#include <media-buffers/mbuf_raw_video_frame.h>

#include <errno.h>
#include <type_traits>

namespace mbuf {

/*
 * Traits definition to map C++ types to C functions and types.
 */
template <typename T> struct QueueTraits;

#define MBUF_QUEUE_TRAITS_DEF(_T, _prefix, _EnumVal)                           \
	template <> struct QueueTraits<struct _T> {                            \
		using FrameType = struct _T;                                   \
		using QueueType = struct _T##_queue;                           \
		using ArgsType = struct _T##_queue_args;                       \
		static constexpr Queue::Type Type = Queue::Type::_EnumVal;     \
		static int create(QueueType **q, ArgsType *args)               \
		{                                                              \
			return _prefix##_queue_new_with_args(args, q);         \
		}                                                              \
		static int destroy(QueueType *q)                               \
		{                                                              \
			return _prefix##_queue_destroy(q);                     \
		}                                                              \
		static int push(QueueType *q, FrameType *f)                    \
		{                                                              \
			return _prefix##_queue_push(q, f);                     \
		}                                                              \
		static int pop(QueueType *q, FrameType **f)                    \
		{                                                              \
			return _prefix##_queue_pop(q, f);                      \
		}                                                              \
		static int peek(QueueType *q, FrameType **f)                   \
		{                                                              \
			return _prefix##_queue_peek(q, f);                     \
		}                                                              \
		static int                                                     \
		peek_at(QueueType *q, unsigned int idx, FrameType **f)         \
		{                                                              \
			return _prefix##_queue_peek_at(q, idx, f);             \
		}                                                              \
		static int flush(QueueType *q)                                 \
		{                                                              \
			return _prefix##_queue_flush(q);                       \
		}                                                              \
		static int get_count(QueueType *q)                             \
		{                                                              \
			return _prefix##_queue_get_count(q);                   \
		}                                                              \
		static int get_event(QueueType *q, struct pomp_evt **evt)      \
		{                                                              \
			return _prefix##_queue_get_event(q, evt);              \
		}                                                              \
	};

MBUF_QUEUE_TRAITS_DEF(mbuf_coded_video_frame,
		      mbuf_coded_video_frame,
		      CODED_VIDEO)
MBUF_QUEUE_TRAITS_DEF(mbuf_raw_video_frame, mbuf_raw_video_frame, RAW_VIDEO)
MBUF_QUEUE_TRAITS_DEF(mbuf_audio_frame, mbuf_audio_frame, AUDIO)


static inline bool frameMatchQueue(mbuf::Frame::Type frameType,
				   mbuf::Queue::Type queueType)
{
	if (frameType == mbuf::Frame::Type::CODED_VIDEO &&
	    queueType == mbuf::Queue::Type::CODED_VIDEO)
		return true;

	if (frameType == mbuf::Frame::Type::RAW_VIDEO &&
	    queueType == mbuf::Queue::Type::RAW_VIDEO)
		return true;

	if (frameType == mbuf::Frame::Type::AUDIO &&
	    queueType == mbuf::Queue::Type::AUDIO)
		return true;

	return false;
}

/*
 * Template implementation of the Queue interface.
 * This class is internal to this file (or could be in an internal header).
 */
template <typename T> class QueueImpl : public Queue {
public:
	using Traits = QueueTraits<T>;
	using NativeQueue = typename Traits::QueueType;

	QueueImpl(NativeQueue *q, bool owner) : mQueue(q), mOwner(owner) {}

	~QueueImpl() override
	{
		if (mOwner && mQueue)
			Traits::destroy(mQueue);
	}

	int getEvent(struct pomp_evt **evt) const override
	{
		return Traits::get_event(mQueue, evt);
	}

	int getCount() const override
	{
		return Traits::get_count(mQueue);
	}

	int flush() override
	{
		return Traits::flush(mQueue);
	}

	void *getQueuePtr() const override
	{
		return mQueue;
	}

	int destroy() override
	{
		int ret = 0;
		if (mQueue) {
			if (mOwner)
				ret = Traits::destroy(mQueue);
			mQueue = nullptr;
		}
		return ret;
	}

	Type getType() const override
	{
		return Traits::Type;
	}

	bool operator==(const void *ptr) const override
	{
		return (const void *)mQueue == ptr;
	}

	/* Push overrides */
	int pushFrame(const Frame *frame) override
	{
		if (frameMatchQueue(frame->getType(), getType())) {
			return pushT(static_cast<T *>(frame->getFramePtr()));
		}

		return -ENOSYS;
	}
	int pushFrame(struct mbuf_coded_video_frame *frame) override
	{
		return pushT(frame);
	}
	int pushFrame(struct mbuf_raw_video_frame *frame) override
	{
		return pushT(frame);
	}
	int pushFrame(struct mbuf_audio_frame *frame) override
	{
		return pushT(frame);
	}

	/* Pop overrides */
	int popFrame(std::unique_ptr<Frame> &frame) override
	{
		T *f;
		int ret = popT(&f);
		if (ret < 0)
			return ret;

		frame = Frame::wrapExisting(f, true);

		return ret;
	}
	int popFrame(struct mbuf_coded_video_frame **frame) override
	{
		return popT(frame);
	}
	int popFrame(struct mbuf_raw_video_frame **frame) override
	{
		return popT(frame);
	}
	int popFrame(struct mbuf_audio_frame **frame) override
	{
		return popT(frame);
	}

	/* Peek overrides */
	int peekFrame(std::unique_ptr<Frame> &frame) override
	{
		T *f;
		int ret = peekT(&f);
		if (ret < 0)
			return ret;

		frame = Frame::wrapExisting(f, true);

		return ret;
	}
	int peekFrame(struct mbuf_coded_video_frame **frame) override
	{
		return peekT(frame);
	}
	int peekFrame(struct mbuf_raw_video_frame **frame) override
	{
		return peekT(frame);
	}
	int peekFrame(struct mbuf_audio_frame **frame) override
	{
		return peekT(frame);
	}

	/* PeekAt overrides */
	int peekAtFrame(unsigned int index,
			std::unique_ptr<Frame> &frame) override
	{
		T *f;
		int ret = peekAtT(index, &f);
		if (ret < 0)
			return ret;

		frame = Frame::wrapExisting(f, true);

		return ret;
	}
	int peekAtFrame(unsigned int index,
			struct mbuf_coded_video_frame **frame) override
	{
		return peekAtT(index, frame);
	}
	int peekAtFrame(unsigned int index,
			struct mbuf_raw_video_frame **frame) override
	{
		return peekAtT(index, frame);
	}
	int peekAtFrame(unsigned int index,
			struct mbuf_audio_frame **frame) override
	{
		return peekAtT(index, frame);
	}

	/* GetCQueue overrides */
	int
	getCQueue(struct mbuf_coded_video_frame_queue **queue) const override
	{
		return getCQueueT(queue);
	}
	int getCQueue(struct mbuf_raw_video_frame_queue **queue) const override
	{
		return getCQueueT(queue);
	}
	int getCQueue(struct mbuf_audio_frame_queue **queue) const override
	{
		return getCQueueT(queue);
	}

private:
	NativeQueue *mQueue = nullptr;
	bool mOwner;

	/*
	 * Helpers using overload resolution to dispatch to correct
	 * implementation. If the type matches T, the first overload is chosen.
	 * Otherwise, the template fallback is chosen.
	 */

	int pushT(T *frame)
	{
		return Traits::push(mQueue, frame);
	}
	template <typename U> int pushT([[maybe_unused]] U *frame) const
	{
		return -ENOSYS;
	}

	int popT(T **frame)
	{
		return Traits::pop(mQueue, frame);
	}
	template <typename U> int popT([[maybe_unused]] U **frame) const
	{
		return -ENOSYS;
	}

	int peekT(T **frame)
	{
		return Traits::peek(mQueue, frame);
	}
	template <typename U> int peekT([[maybe_unused]] U **frame) const
	{
		return -ENOSYS;
	}

	int peekAtT(unsigned int index, T **frame)
	{
		return Traits::peek_at(mQueue, index, frame);
	}
	template <typename U>
	int peekAtT([[maybe_unused]] unsigned int index,
		    [[maybe_unused]] U **frame) const
	{
		return -ENOSYS;
	}

	int getCQueueT(NativeQueue **queue) const
	{
		if (queue == nullptr)
			return -EINVAL;
		*queue = mQueue;
		return 0;
	}
	template <typename Q> int getCQueueT([[maybe_unused]] Q **queue) const
	{
		return -ENOSYS;
	}
};

} /* namespace mbuf */