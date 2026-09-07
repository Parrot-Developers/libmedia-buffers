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


#include "media-buffers/mbuf_frame.hpp"
#include "mbuf_frame_impl.hpp"


namespace mbuf {

using CodedFrame = FrameImpl<mbuf_coded_video_frame,
			     vdef_coded_frame,
			     mbuf_coded_video_frame_cbs>;
using RawFrame = FrameImpl<mbuf_raw_video_frame,
			   vdef_raw_frame,
			   mbuf_raw_video_frame_cbs>;
using AudioFrame =
	FrameImpl<mbuf_audio_frame, adef_frame, mbuf_audio_frame_cbs>;


std::unique_ptr<Frame> Frame::create(const struct vdef_coded_frame *frame_info)
{
	if (frame_info == nullptr)
		return nullptr;
	auto frame = std::make_unique<CodedFrame>(frame_info);
	if (frame->getFramePtr() == nullptr)
		return nullptr;
	return frame;
}


std::unique_ptr<Frame> Frame::create(const struct vdef_raw_frame *frame_info)
{
	if (frame_info == nullptr)
		return nullptr;
	auto frame = std::make_unique<RawFrame>(frame_info);
	if (frame->getFramePtr() == nullptr)
		return nullptr;
	return frame;
}


std::unique_ptr<Frame> Frame::create(const struct adef_frame *frame_info)
{
	if (frame_info == nullptr)
		return nullptr;
	auto frame = std::make_unique<AudioFrame>(frame_info);
	if (frame->getFramePtr() == nullptr)
		return nullptr;
	return frame;
}


std::unique_ptr<Frame>
Frame::wrapExisting(struct mbuf_coded_video_frame *existingNativeFrame,
		    bool owner)
{
	if (!existingNativeFrame)
		return nullptr;
	return std::make_unique<CodedFrame>(existingNativeFrame, owner);
}


std::unique_ptr<Frame>
Frame::wrapExisting(struct mbuf_raw_video_frame *existingNativeFrame,
		    bool owner)
{
	if (!existingNativeFrame)
		return nullptr;
	return std::make_unique<RawFrame>(existingNativeFrame, owner);
}


std::unique_ptr<Frame>
Frame::wrapExisting(struct mbuf_audio_frame *existingNativeFrame, bool owner)
{
	if (!existingNativeFrame)
		return nullptr;
	return std::make_unique<AudioFrame>(existingNativeFrame, owner);
}

} /* namespace mbuf */
