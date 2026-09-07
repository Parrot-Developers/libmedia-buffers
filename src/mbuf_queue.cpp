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

#include "mbuf_queue_impl.hpp"

namespace mbuf {

/*
 * Factory methods implementation.
 */

std::unique_ptr<Queue> Queue::create(Type type, bool owner)
{
	switch (type) {
	case Type::CODED_VIDEO:
		return createWithArgs(
			(struct mbuf_coded_video_frame_queue_args *)nullptr,
			owner);
	case Type::RAW_VIDEO:
		return createWithArgs(
			(struct mbuf_raw_video_frame_queue_args *)nullptr,
			owner);
	case Type::AUDIO:
		return createWithArgs(
			(struct mbuf_audio_frame_queue_args *)nullptr, owner);
	default:
		return nullptr;
	}
}


std::unique_ptr<Queue>
Queue::createWithArgs(struct mbuf_coded_video_frame_queue_args *args,
		      bool owner)
{
	using T = struct mbuf_coded_video_frame;
	typename QueueTraits<T>::QueueType *q = nullptr;
	int ret = QueueTraits<T>::create(&q, args);
	if (ret < 0)
		return nullptr;
	return std::make_unique<QueueImpl<T>>(q, owner);
}


std::unique_ptr<Queue>
Queue::createWithArgs(struct mbuf_raw_video_frame_queue_args *args, bool owner)
{
	using T = struct mbuf_raw_video_frame;
	typename QueueTraits<T>::QueueType *q = nullptr;
	int ret = QueueTraits<T>::create(&q, args);
	if (ret < 0)
		return nullptr;
	return std::make_unique<QueueImpl<T>>(q, owner);
}


std::unique_ptr<Queue>
Queue::createWithArgs(struct mbuf_audio_frame_queue_args *args, bool owner)
{
	using T = struct mbuf_audio_frame;
	typename QueueTraits<T>::QueueType *q = nullptr;
	int ret = QueueTraits<T>::create(&q, args);
	if (ret < 0)
		return nullptr;
	return std::make_unique<QueueImpl<T>>(q, owner);
}


std::unique_ptr<Queue>
Queue::wrapExisting(struct mbuf_coded_video_frame_queue *existingNativeQueue,
		    bool owner)
{
	if (!existingNativeQueue)
		return nullptr;
	return std::make_unique<QueueImpl<struct mbuf_coded_video_frame>>(
		existingNativeQueue, owner);
}


std::unique_ptr<Queue>
Queue::wrapExisting(struct mbuf_raw_video_frame_queue *existingNativeQueue,
		    bool owner)
{
	if (!existingNativeQueue)
		return nullptr;
	return std::make_unique<QueueImpl<struct mbuf_raw_video_frame>>(
		existingNativeQueue, owner);
}


std::unique_ptr<Queue>
Queue::wrapExisting(struct mbuf_audio_frame_queue *existingNativeQueue,
		    bool owner)
{
	if (!existingNativeQueue)
		return nullptr;
	return std::make_unique<QueueImpl<struct mbuf_audio_frame>>(
		existingNativeQueue, owner);
}


int Queue::attachToLoop(struct pomp_loop *loop,
			pomp_evt_cb_t cb,
			void *userdata) const
{
	struct pomp_evt *evt = nullptr;
	int ret = getEvent(&evt);
	if (ret < 0)
		return ret;
	return pomp_evt_attach_to_loop(evt, loop, cb, userdata);
}


int Queue::attachToLoop(const pomp::Loop &loop,
			pomp_evt_cb_t cb,
			void *userdata) const
{
	return attachToLoop(loop.get(), cb, userdata);
}


int Queue::detachFromLoop(struct pomp_loop *loop) const
{
	struct pomp_evt *evt = nullptr;
	if (getEvent(&evt) < 0 || !pomp_evt_is_attached(evt, loop))
		return 0;
	return pomp_evt_detach_from_loop(evt, loop);
}


int Queue::detachFromLoop(const pomp::Loop &loop) const
{
	return detachFromLoop(loop.get());
}

} /* namespace mbuf */
