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


#include "mbuf_test.h"
#include "media-buffers/mbuf_queue.hpp"

using mbuf::Frame;
using mbuf::Queue;


constexpr uint32_t MBUF_TEST_WIDTH = 4;
constexpr uint32_t MBUF_TEST_HEIGHT = 4;
constexpr uint32_t MBUF_TEST_SIZE = 128;
constexpr uint32_t MBUF_TEST_DEFAULT_FRAME_TEST_COUNT = 3;
constexpr uint32_t MBUF_TEST_SAMPLE_PER_FRAME = 1024;
constexpr uint32_t MBUF_TEST_AAC_BITRATE = 320000; /* 320Kbps */


constexpr Queue::Type allTypes[] = {
	Queue::Type::CODED_VIDEO,
	Queue::Type::RAW_VIDEO,
	Queue::Type::AUDIO,
};


static void testPopAudio(std::unique_ptr<Queue> &queuePtr)
{
	size_t count = queuePtr->getCount();
	struct mbuf_audio_frame *frame = nullptr;
	int ret = 0;

	ret = queuePtr->popFrame(&frame);
	if (queuePtr->getType() == Queue::Type::AUDIO) {
		CU_ASSERT_EQUAL(ret, (count > 0) ? 0 : -EAGAIN);
		(void)mbuf_audio_frame_unref(frame);
		CU_ASSERT_EQUAL(queuePtr->getCount(), count - 1);
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
		CU_ASSERT_EQUAL(queuePtr->getCount(), count);
	}
}


static void testPeekAtAudio(std::unique_ptr<Queue> &queuePtr)
{
	size_t count = queuePtr->getCount();
	struct mbuf_audio_frame *frame = nullptr;
	int ret = 0;

	ret = queuePtr->peekAtFrame(0, &frame);
	if (queuePtr->getType() == Queue::Type::AUDIO) {
		CU_ASSERT_EQUAL(ret, (count > 0) ? 0 : -EAGAIN);
		(void)mbuf_audio_frame_unref(frame);

		for (size_t i = 0; i < count; i++) {
			ret = queuePtr->peekAtFrame(i, &frame);
			CU_ASSERT_EQUAL(ret, 0);
			(void)mbuf_audio_frame_unref(frame);
		}
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
	}
}


static void testPeekAudio(std::unique_ptr<Queue> &queuePtr)
{
	size_t count = queuePtr->getCount();
	struct mbuf_audio_frame *frame = nullptr;
	int ret = 0;

	ret = queuePtr->peekFrame(&frame);
	if (queuePtr->getType() == Queue::Type::AUDIO) {
		CU_ASSERT_EQUAL(ret, (count > 0) ? 0 : -EAGAIN);
		(void)mbuf_audio_frame_unref(frame);
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
	}
}


static size_t getFrameSize(const struct adef_frame *info)
{
	switch (info->format.encoding) {
	case ADEF_ENCODING_PCM:
		/* In PCM, N samples are bundled into a frame (usually 1024). */
		return MBUF_TEST_SAMPLE_PER_FRAME * info->format.channel_count *
		       (info->format.bit_depth / 8);
	case ADEF_ENCODING_AAC_LC:
		/* In AAC, packet size is dynamic and depends on the expected
		 * bitrate. */
		return MBUF_TEST_AAC_BITRATE * MBUF_TEST_SAMPLE_PER_FRAME /
		       info->format.sample_rate;
	default:
		return 0;
	}
}


static void setBuffer(struct mbuf_audio_frame *frame, struct mbuf_mem *baseMem)
{
	struct adef_frame frameInfo;
	size_t bufferSize;
	bool internalMem = false;

	int ret = mbuf_audio_frame_get_frame_info(frame, &frameInfo);
	if (ret != 0)
		return;

	bufferSize = getFrameSize(&frameInfo);

	if (!baseMem) {
		ret = mbuf_mem_generic_new(bufferSize, &baseMem);
		CU_ASSERT_EQUAL(ret, 0);
		if (ret != 0)
			return;
		internalMem = true;
	}
	CU_ASSERT_PTR_NOT_NULL_FATAL(baseMem);


	(void)mbuf_audio_frame_set_buffer(frame, baseMem, 0, bufferSize);
	if (internalMem)
		mbuf_mem_unref(baseMem);
}


static void setBuffer(std::unique_ptr<Frame> &frame, struct mbuf_mem *baseMem)
{
	struct adef_frame frameInfo;
	size_t bufferSize;
	bool internalMem = false;

	int ret = frame->getFrameInfo(&frameInfo);
	if (ret != 0)
		return;

	bufferSize = getFrameSize(&frameInfo);

	if (!baseMem) {
		ret = mbuf_mem_generic_new(bufferSize, &baseMem);
		CU_ASSERT_EQUAL(ret, 0);
		if (ret != 0)
			return;
		internalMem = true;
	}
	CU_ASSERT_PTR_NOT_NULL_FATAL(baseMem);


	(void)frame->setBuffer(baseMem, 0, bufferSize);
	if (internalMem)
		mbuf_mem_unref(baseMem);
}


static void testPushAudio(std::unique_ptr<Queue> &queuePtr)
{
	int count = queuePtr->getCount();
	struct mbuf_audio_frame *frame = nullptr;
	struct mbuf_audio_frame *nullFrame = nullptr;
	struct adef_frame frameInfo = {};
	int ret = 0;
	frameInfo.format = adef_pcm_16b_44100hz_stereo;
	frameInfo.info.timestamp = frameInfo.info.capture_timestamp =
		rand() % 1000000;
	frameInfo.info.index = 33;
	frameInfo.info.timescale = 1000000;

	ret = mbuf_audio_frame_new(&frameInfo, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(frame);

	ret = queuePtr->pushFrame(frame);
	if (queuePtr->getType() == Queue::Type::AUDIO) {
		CU_ASSERT_EQUAL(ret, -EBUSY);

		setBuffer(frame, NULL);

		ret = mbuf_audio_frame_finalize(frame);
		CU_ASSERT_EQUAL(ret, 0);

		ret = queuePtr->pushFrame(frame);
		CU_ASSERT_EQUAL(ret, 0);

		ret = queuePtr->pushFrame(nullFrame);
		CU_ASSERT_EQUAL(ret, -EINVAL);

		CU_ASSERT_EQUAL(queuePtr->getCount(), count + 1);
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
		CU_ASSERT_EQUAL(queuePtr->getCount(), count);
	}

	(void)mbuf_audio_frame_unref(frame);
}


static void testPopRaw(std::unique_ptr<Queue> &queuePtr)
{
	size_t count = queuePtr->getCount();
	struct mbuf_raw_video_frame *frame = nullptr;
	int ret = 0;

	ret = queuePtr->popFrame(&frame);
	if (queuePtr->getType() == Queue::Type::RAW_VIDEO) {
		CU_ASSERT_EQUAL(ret, (count > 0) ? 0 : -EAGAIN);
		(void)mbuf_raw_video_frame_unref(frame);
		CU_ASSERT_EQUAL(queuePtr->getCount(), count - 1);
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
		CU_ASSERT_EQUAL(queuePtr->getCount(), count);
	}
}


static void testPeekAtRaw(std::unique_ptr<Queue> &queuePtr)
{
	size_t count = queuePtr->getCount();
	struct mbuf_raw_video_frame *frame = nullptr;
	int ret = 0;

	ret = queuePtr->peekAtFrame(0, &frame);
	if (queuePtr->getType() == Queue::Type::RAW_VIDEO) {
		CU_ASSERT_EQUAL(ret, (count > 0) ? 0 : -EAGAIN);
		(void)mbuf_raw_video_frame_unref(frame);

		for (size_t i = 0; i < count; i++) {
			ret = queuePtr->peekAtFrame(i, &frame);
			CU_ASSERT_EQUAL(ret, 0);
			(void)mbuf_raw_video_frame_unref(frame);
		}
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
	}
}


static void testPeekRaw(std::unique_ptr<Queue> &queuePtr)
{
	size_t count = queuePtr->getCount();
	struct mbuf_raw_video_frame *frame = nullptr;
	int ret = 0;

	ret = queuePtr->peekFrame(&frame);
	if (queuePtr->getType() == Queue::Type::RAW_VIDEO) {
		CU_ASSERT_EQUAL(ret, (count > 0) ? 0 : -EAGAIN);
		(void)mbuf_raw_video_frame_unref(frame);
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
	}
}


static int create_planes(struct vdef_raw_frame &frameInfo,
			 size_t plane_size[],
			 size_t plane_stride[],
			 size_t plane_height[],
			 size_t plane_offset[],
			 struct mbuf_mem *&baseMem,
			 struct mbuf_mem *plane_mem[],
			 struct mbuf_mem *p2mem,
			 struct mbuf_mem *p3mem,
			 bool &internalMem)
{
	int ret = 0;

	if (!vdef_raw_format_cmp(&frameInfo.format, &vdef_i420)) {
		CU_FAIL("This test only operates on i420 frames");
		return -EPROTO;
	}

	ret = vdef_calc_raw_frame_size(&frameInfo.format,
				       &frameInfo.info.resolution,
				       plane_stride,
				       NULL,
				       NULL,
				       NULL,
				       plane_size,
				       NULL);
	CU_ASSERT_EQUAL(ret, 0);
	if (ret != 0)
		return ret;
	for (unsigned int i = 0; i < 3; i++) {
		plane_height[i] = plane_size[i] / plane_stride[i];
		plane_size[i] = plane_height[i] * frameInfo.plane_stride[i];
	}
	if (!baseMem) {
		ret = mbuf_mem_generic_new(plane_size[0] + plane_size[1] +
						   plane_size[2],
					   &baseMem);
		if (ret != 0)
			return ret;
		internalMem = true;
	}
	if (!p2mem && !p3mem) {
		p3mem = p2mem = baseMem;
		plane_offset[1] = plane_size[0];
		plane_offset[2] = plane_size[0] + plane_size[1];
	}
	plane_mem[0] = baseMem;
	plane_mem[1] = p2mem;
	plane_mem[2] = p3mem;
	return 0;
}


static int setupMem(unsigned int i,
		    const struct vdef_raw_frame &frameInfo,
		    const size_t plane_size[],
		    const size_t plane_stride[],
		    const size_t plane_height[],
		    const size_t plane_offset[],
		    struct mbuf_mem *plane_mem[])
{
	int ret = 0;
	void *plane;
	size_t cap;

	ret = mbuf_mem_get_data(plane_mem[i], &plane, &cap);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT(plane_offset[i] + plane_size[i] <= cap);
	if (ret != 0 || plane_offset[i] + plane_size[i] > cap)
		return -EPROTO;
	for (size_t j = 0; j < plane_height[i]; j++) {
		auto dst = static_cast<uint8_t *>(plane);
		dst += plane_offset[i];
		dst += j * frameInfo.plane_stride[i];
		memset(dst, i + 10, plane_stride[i]);
		if (plane_stride[i] < frameInfo.plane_stride[i])
			memset(dst + plane_stride[i],
			       i + 20,
			       frameInfo.plane_stride[i] - plane_stride[i]);
	}
	return 0;
}


static void setPlanes(struct mbuf_raw_video_frame *frame,
		      struct mbuf_mem *baseMem,
		      struct mbuf_mem *p2mem,
		      struct mbuf_mem *p3mem)
{
	struct vdef_raw_frame frameInfo;
	size_t plane_size[VDEF_RAW_MAX_PLANE_COUNT];
	size_t plane_stride[VDEF_RAW_MAX_PLANE_COUNT] = {0};
	size_t plane_height[VDEF_RAW_MAX_PLANE_COUNT];
	struct mbuf_mem *plane_mem[VDEF_RAW_MAX_PLANE_COUNT];
	size_t plane_offset[VDEF_RAW_MAX_PLANE_COUNT] = {0};
	bool internalMem = false;

	int ret = mbuf_raw_video_frame_get_frame_info(frame, &frameInfo);
	CU_ASSERT_EQUAL(ret, 0);
	if (ret != 0)
		return;

	ret = create_planes(frameInfo,
			    plane_size,
			    plane_stride,
			    plane_height,
			    plane_offset,
			    baseMem,
			    plane_mem,
			    p2mem,
			    p3mem,
			    internalMem);
	if (ret < 0)
		return;

	for (unsigned int i = 0; i < 3; i++) {
		ret = setupMem(i,
			       frameInfo,
			       plane_size,
			       plane_stride,
			       plane_height,
			       plane_offset,
			       plane_mem);
		if (ret != 0)
			return;
		ret = mbuf_raw_video_frame_set_plane(
			frame, i, plane_mem[i], plane_offset[i], plane_size[i]);
		CU_ASSERT_EQUAL(ret, 0);
	}
	if (internalMem)
		mbuf_mem_unref(baseMem);
}


static void setPlanes(std::unique_ptr<Frame> &frame,
		      struct mbuf_mem *baseMem,
		      struct mbuf_mem *p2mem,
		      struct mbuf_mem *p3mem)
{
	struct vdef_raw_frame frameInfo;
	size_t plane_size[VDEF_RAW_MAX_PLANE_COUNT];
	size_t plane_stride[VDEF_RAW_MAX_PLANE_COUNT] = {0};
	size_t plane_height[VDEF_RAW_MAX_PLANE_COUNT];
	struct mbuf_mem *plane_mem[VDEF_RAW_MAX_PLANE_COUNT];
	size_t plane_offset[VDEF_RAW_MAX_PLANE_COUNT] = {0};
	bool internalMem = false;

	int ret = frame->getFrameInfo(&frameInfo);
	CU_ASSERT_EQUAL(ret, 0);
	if (ret != 0)
		return;

	ret = create_planes(frameInfo,
			    plane_size,
			    plane_stride,
			    plane_height,
			    plane_offset,
			    baseMem,
			    plane_mem,
			    p2mem,
			    p3mem,
			    internalMem);
	if (ret < 0)
		return;

	for (unsigned int i = 0; i < 3; i++) {
		ret = setupMem(i,
			       frameInfo,
			       plane_size,
			       plane_stride,
			       plane_height,
			       plane_offset,
			       plane_mem);
		if (ret != 0)
			return;
		ret = frame->setPlane(
			i, plane_mem[i], plane_offset[i], plane_size[i]);
		CU_ASSERT_EQUAL(ret, 0);
	}
	if (internalMem)
		mbuf_mem_unref(baseMem);
}


static void testPushRaw(std::unique_ptr<Queue> &queuePtr)
{
	struct vdef_raw_frame frameInfo = {};
	frameInfo.format = vdef_i420;
	frameInfo.info.resolution.width = MBUF_TEST_WIDTH;
	frameInfo.info.resolution.height = MBUF_TEST_HEIGHT;
	frameInfo.plane_stride[0] = MBUF_TEST_WIDTH;
	frameInfo.plane_stride[1] = MBUF_TEST_WIDTH / 2;
	frameInfo.plane_stride[2] = MBUF_TEST_WIDTH / 2;
	int count = queuePtr->getCount();
	struct mbuf_raw_video_frame *frame = nullptr;
	struct mbuf_raw_video_frame *nullFrame = nullptr;
	int ret = 0;

	ret = mbuf_raw_video_frame_new(&frameInfo, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(frame);

	ret = queuePtr->pushFrame(frame);
	if (queuePtr->getType() == Queue::Type::RAW_VIDEO) {
		CU_ASSERT_EQUAL(ret, -EBUSY);

		setPlanes(frame, NULL, NULL, NULL);

		ret = mbuf_raw_video_frame_finalize(frame);
		CU_ASSERT_EQUAL(ret, 0);

		ret = queuePtr->pushFrame(frame);
		CU_ASSERT_EQUAL(ret, 0);

		ret = queuePtr->pushFrame(nullFrame);
		CU_ASSERT_EQUAL(ret, -EINVAL);

		CU_ASSERT_EQUAL(queuePtr->getCount(), count + 1);
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
		CU_ASSERT_EQUAL(queuePtr->getCount(), count);
	}

	(void)mbuf_raw_video_frame_unref(frame);
}


static int createNalu(struct mbuf_mem *mem,
		      size_t offset,
		      enum h264_nalu_type type,
		      enum h264_slice_type slice_type,
		      int value,
		      int importance,
		      struct vdef_nalu &nalu)
{
	void *coded_data;
	uint8_t *data;
	size_t cap;
	int ret = mbuf_mem_get_data(mem, &coded_data, &cap);
	data = static_cast<uint8_t *>(coded_data);

	if (ret != 0 || cap < (MBUF_TEST_SIZE + offset))
		return -EPROTO;

	memset(data + offset, value, MBUF_TEST_SIZE);
	nalu.size = MBUF_TEST_SIZE;
	nalu.importance = importance;
	nalu.h264.type = type;
	nalu.h264.slice_type = slice_type;

	return 0;
}


static void addNalu(std::unique_ptr<Frame> &frame,
		    struct mbuf_mem *mem,
		    size_t offset,
		    enum h264_nalu_type type,
		    enum h264_slice_type slice_type,
		    int value,
		    int importance)
{
	struct vdef_nalu nalu = {};
	int ret = createNalu(
		mem, offset, type, slice_type, value, importance, nalu);
	if (ret != 0)
		return;

	frame->addNalu(mem, offset, &nalu);
}


static void addNalu(struct mbuf_coded_video_frame *frame,
		    struct mbuf_mem *mem,
		    size_t offset,
		    enum h264_nalu_type type,
		    enum h264_slice_type slice_type,
		    int value,
		    int importance)
{
	struct vdef_nalu nalu = {};
	int ret = createNalu(
		mem, offset, type, slice_type, value, importance, nalu);
	if (ret != 0)
		return;

	(void)mbuf_coded_video_frame_add_nalu(frame, mem, offset, &nalu);
}


template <typename T> static void addDefaultNalu(T &frame)
{
	struct mbuf_mem *mem;
	int ret = mbuf_mem_generic_new(MBUF_TEST_SIZE, &mem);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(mem);
	addNalu(frame,
		mem,
		0,
		H264_NALU_TYPE_SPS,
		H264_SLICE_TYPE_UNKNOWN,
		1,
		0);
	(void)mbuf_mem_unref(mem);
}


static void testPopAbstract(std::unique_ptr<Queue> &queuePtr)
{
	std::unique_ptr<Frame> frame;
	size_t count = queuePtr->getCount();
	int ret = 0;

	ret = queuePtr->popFrame(frame);
	CU_ASSERT_EQUAL(ret, (count > 0) ? 0 : -EAGAIN);
}


static void testPeekAtAbstract(std::unique_ptr<Queue> &queuePtr)
{
	std::unique_ptr<Frame> frame;
	size_t count = queuePtr->getCount();
	int ret = 0;

	ret = queuePtr->peekAtFrame(0, frame);
	CU_ASSERT_EQUAL(ret, (count > 0) ? 0 : -EAGAIN);

	for (size_t i = 0; i < count; i++) {
		ret = queuePtr->peekAtFrame(i, frame);
		CU_ASSERT_EQUAL(ret, 0);
	}
}


static void testPeekAbstract(std::unique_ptr<Queue> &queuePtr)
{
	std::unique_ptr<Frame> frame;
	size_t count = queuePtr->getCount();
	int ret = 0;

	ret = queuePtr->peekFrame(frame);
	CU_ASSERT_EQUAL(ret, (count > 0) ? 0 : -EAGAIN);
}


static void testPushAbstractCoded(std::unique_ptr<Queue> &queuePtr)
{
	struct vdef_coded_frame frameInfo = {};
	frameInfo.format = vdef_h264_byte_stream;
	frameInfo.info.resolution.width = MBUF_TEST_WIDTH;
	frameInfo.info.resolution.height = MBUF_TEST_HEIGHT;
	int count = queuePtr->getCount();
	int ret = 0;

	std::unique_ptr<Frame> frame = Frame::create(&frameInfo);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_NOT_EQUAL(frame, nullptr);

	ret = queuePtr->pushFrame(frame.get());
	if (queuePtr->getType() == Queue::Type::CODED_VIDEO) {
		CU_ASSERT_EQUAL(ret, -EBUSY);

		addDefaultNalu(frame);
		ret = frame->finalize();
		CU_ASSERT_EQUAL(ret, 0);

		ret = queuePtr->pushFrame(frame.get());
		CU_ASSERT_EQUAL(ret, 0);

		CU_ASSERT_EQUAL(queuePtr->getCount(), count + 1);
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
		CU_ASSERT_EQUAL(queuePtr->getCount(), count);
	}
}


static void testPushAbstractRaw(std::unique_ptr<Queue> &queuePtr)
{
	struct vdef_raw_frame frameInfo = {};
	frameInfo.format = vdef_i420;
	frameInfo.info.resolution.width = MBUF_TEST_WIDTH;
	frameInfo.info.resolution.height = MBUF_TEST_HEIGHT;
	frameInfo.plane_stride[0] = MBUF_TEST_WIDTH;
	frameInfo.plane_stride[1] = MBUF_TEST_WIDTH / 2;
	frameInfo.plane_stride[2] = MBUF_TEST_WIDTH / 2;
	int count = queuePtr->getCount();
	int ret = 0;

	std::unique_ptr<Frame> frame = Frame::create(&frameInfo);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_NOT_EQUAL(frame, nullptr);

	ret = queuePtr->pushFrame(frame.get());
	if (queuePtr->getType() == Queue::Type::RAW_VIDEO) {
		CU_ASSERT_EQUAL(ret, -EBUSY);

		setPlanes(frame, NULL, NULL, NULL);

		ret = frame->finalize();
		CU_ASSERT_EQUAL(ret, 0);

		ret = queuePtr->pushFrame(frame.get());
		CU_ASSERT_EQUAL(ret, 0);

		CU_ASSERT_EQUAL(queuePtr->getCount(), count + 1);
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
		CU_ASSERT_EQUAL(queuePtr->getCount(), count);
	}
}


static void testPushAbstractAudio(std::unique_ptr<Queue> &queuePtr)
{
	int count = queuePtr->getCount();
	struct adef_frame frameInfo = {};
	int ret = 0;
	frameInfo.format = adef_pcm_16b_44100hz_stereo;
	frameInfo.info.timestamp = frameInfo.info.capture_timestamp =
		rand() % 1000000;
	frameInfo.info.index = 33;
	frameInfo.info.timescale = 1000000;

	std::unique_ptr<Frame> frame = Frame::create(&frameInfo);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_NOT_EQUAL(frame, nullptr);

	ret = queuePtr->pushFrame(frame.get());
	if (queuePtr->getType() == Queue::Type::AUDIO) {
		CU_ASSERT_EQUAL(ret, -EBUSY);

		setBuffer(frame, NULL);

		ret = frame->finalize();
		CU_ASSERT_EQUAL(ret, 0);

		ret = queuePtr->pushFrame(frame.get());
		CU_ASSERT_EQUAL(ret, 0);

		CU_ASSERT_EQUAL(queuePtr->getCount(), count + 1);
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
		CU_ASSERT_EQUAL(queuePtr->getCount(), count);
	}
}


static void testPushAbstract(std::unique_ptr<Queue> &queuePtr)
{
	testPushAbstractCoded(queuePtr);
	testPushAbstractRaw(queuePtr);
	testPushAbstractAudio(queuePtr);
}


static void testPopCoded(std::unique_ptr<Queue> &queuePtr)
{
	size_t count = queuePtr->getCount();
	struct mbuf_coded_video_frame *frame = nullptr;
	int ret = 0;

	ret = queuePtr->popFrame(&frame);
	if (queuePtr->getType() == Queue::Type::CODED_VIDEO) {
		CU_ASSERT_EQUAL(ret, (count > 0) ? 0 : -EAGAIN);
		(void)mbuf_coded_video_frame_unref(frame);
		CU_ASSERT_EQUAL(queuePtr->getCount(), count - 1);
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
		CU_ASSERT_EQUAL(queuePtr->getCount(), count);
	}
}


static void testPeekAtCoded(std::unique_ptr<Queue> &queuePtr)
{
	size_t count = queuePtr->getCount();
	struct mbuf_coded_video_frame *frame = nullptr;
	int ret = 0;

	ret = queuePtr->peekAtFrame(0, &frame);
	if (queuePtr->getType() == Queue::Type::CODED_VIDEO) {
		CU_ASSERT_EQUAL(ret, (count > 0) ? 0 : -EAGAIN);
		(void)mbuf_coded_video_frame_unref(frame);

		for (size_t i = 0; i < count; i++) {
			ret = queuePtr->peekAtFrame(i, &frame);
			CU_ASSERT_EQUAL(ret, 0);
			(void)mbuf_coded_video_frame_unref(frame);
		}
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
	}
}


static void testPeekCoded(std::unique_ptr<Queue> &queuePtr)
{
	size_t count = queuePtr->getCount();
	struct mbuf_coded_video_frame *frame = nullptr;
	int ret = 0;

	ret = queuePtr->peekFrame(&frame);
	if (queuePtr->getType() == Queue::Type::CODED_VIDEO) {
		CU_ASSERT_EQUAL(ret, (count > 0) ? 0 : -EAGAIN);
		(void)mbuf_coded_video_frame_unref(frame);
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
	}
}


static void testPushCoded(std::unique_ptr<Queue> &queuePtr)
{
	struct vdef_coded_frame frameInfo = {};
	frameInfo.format = vdef_h264_byte_stream;
	frameInfo.info.resolution.width = MBUF_TEST_WIDTH;
	frameInfo.info.resolution.height = MBUF_TEST_HEIGHT;
	int count = queuePtr->getCount();
	struct mbuf_coded_video_frame *frame = nullptr;
	struct mbuf_coded_video_frame *nullFrame = nullptr;
	int ret = 0;

	ret = mbuf_coded_video_frame_new(&frameInfo, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(frame);

	ret = queuePtr->pushFrame(frame);
	if (queuePtr->getType() == Queue::Type::CODED_VIDEO) {
		CU_ASSERT_EQUAL(ret, -EBUSY);

		addDefaultNalu(frame);

		ret = mbuf_coded_video_frame_finalize(frame);
		CU_ASSERT_EQUAL(ret, 0);

		ret = queuePtr->pushFrame(frame);
		CU_ASSERT_EQUAL(ret, 0);

		ret = queuePtr->pushFrame(nullFrame);
		CU_ASSERT_EQUAL(ret, -EINVAL);

		CU_ASSERT_EQUAL(queuePtr->getCount(), count + 1);
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
		CU_ASSERT_EQUAL(queuePtr->getCount(), count);
	}

	(void)mbuf_coded_video_frame_unref(frame);
}


static void testAudioMethods(std::unique_ptr<Queue> &queuePtr)
{
	testPeekAudio(queuePtr);
	testPeekAtAudio(queuePtr);

	for (size_t i = 0; i < MBUF_TEST_DEFAULT_FRAME_TEST_COUNT; i++)
		testPushAudio(queuePtr);

	testPeekAudio(queuePtr);
	testPeekAtAudio(queuePtr);

	for (size_t i = 0; i < MBUF_TEST_DEFAULT_FRAME_TEST_COUNT; i++)
		testPopAudio(queuePtr);

	testPeekAudio(queuePtr);
	testPeekAtAudio(queuePtr);

	testPushAudio(queuePtr);
	CU_ASSERT_EQUAL(queuePtr->flush(), 0);
	CU_ASSERT_EQUAL(queuePtr->getCount(), 0);
}


static void testRawMethods(std::unique_ptr<Queue> &queuePtr)
{
	testPeekRaw(queuePtr);
	testPeekAtRaw(queuePtr);

	for (size_t i = 0; i < MBUF_TEST_DEFAULT_FRAME_TEST_COUNT; i++)
		testPushRaw(queuePtr);

	testPeekRaw(queuePtr);
	testPeekAtRaw(queuePtr);

	for (size_t i = 0; i < MBUF_TEST_DEFAULT_FRAME_TEST_COUNT; i++)
		testPopRaw(queuePtr);

	testPeekRaw(queuePtr);
	testPeekAtRaw(queuePtr);

	testPushRaw(queuePtr);
	CU_ASSERT_EQUAL(queuePtr->flush(), 0);
	CU_ASSERT_EQUAL(queuePtr->getCount(), 0);
}


static void testAbstractMethods(std::unique_ptr<Queue> &queuePtr)
{
	testPeekAbstract(queuePtr);
	testPeekAtAbstract(queuePtr);

	for (size_t i = 0; i < MBUF_TEST_DEFAULT_FRAME_TEST_COUNT; i++)
		testPushAbstract(queuePtr);

	testPeekAbstract(queuePtr);
	testPeekAtAbstract(queuePtr);

	for (size_t i = 0; i < MBUF_TEST_DEFAULT_FRAME_TEST_COUNT; i++)
		testPopAbstract(queuePtr);

	testPeekAbstract(queuePtr);
	testPeekAtAbstract(queuePtr);

	testPushAbstract(queuePtr);
	CU_ASSERT_EQUAL(queuePtr->flush(), 0);
	CU_ASSERT_EQUAL(queuePtr->getCount(), 0);
}


static void testCodedMethods(std::unique_ptr<Queue> &queuePtr)
{
	testPeekCoded(queuePtr);
	testPeekAtCoded(queuePtr);

	for (size_t i = 0; i < MBUF_TEST_DEFAULT_FRAME_TEST_COUNT; i++)
		testPushCoded(queuePtr);

	testPeekCoded(queuePtr);
	testPeekAtCoded(queuePtr);

	for (size_t i = 0; i < MBUF_TEST_DEFAULT_FRAME_TEST_COUNT; i++)
		testPopCoded(queuePtr);

	testPeekCoded(queuePtr);
	testPeekAtCoded(queuePtr);

	testPushCoded(queuePtr);
	CU_ASSERT_EQUAL(queuePtr->flush(), 0);
	CU_ASSERT_EQUAL(queuePtr->getCount(), 0);
}


static inline void testGetEvent(std::unique_ptr<Queue> &queuePtr)
{
	struct pomp_evt *evt = nullptr;

	CU_ASSERT_EQUAL(queuePtr->getEvent(nullptr), -EINVAL);
	CU_ASSERT_EQUAL(queuePtr->getEvent(&evt), 0);
	CU_ASSERT_PTR_NOT_NULL(evt);
}


static inline void testQueueInitialized(std::unique_ptr<Queue> &queuePtr,
					Queue::Type t)
{
	int ret = 0;
	struct mbuf_coded_video_frame_queue *coded = nullptr;
	struct mbuf_raw_video_frame_queue *raw = nullptr;
	struct mbuf_audio_frame_queue *audio = nullptr;
	struct mbuf_coded_video_frame_queue **nullCoded = nullptr;
	struct mbuf_raw_video_frame_queue **nullRaw = nullptr;
	struct mbuf_audio_frame_queue **nullAudio = nullptr;

	CU_ASSERT_NOT_EQUAL_FATAL(queuePtr, nullptr);
	CU_ASSERT_EQUAL(queuePtr->getType(), t);
	CU_ASSERT_PTR_NOT_NULL(queuePtr->getQueuePtr());
	testGetEvent(queuePtr);
	CU_ASSERT(queuePtr->getCount() >= 0);

	ret = queuePtr->getCQueue(&coded);
	if (t == Queue::Type::CODED_VIDEO) {
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_PTR_NOT_NULL(coded);
		ret = queuePtr->getCQueue(nullCoded);
		CU_ASSERT_EQUAL(ret, -EINVAL);
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
		CU_ASSERT_PTR_NULL(coded);
	}
	ret = queuePtr->getCQueue(&raw);
	if (t == Queue::Type::RAW_VIDEO) {
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_PTR_NOT_NULL(raw);
		ret = queuePtr->getCQueue(nullRaw);
		CU_ASSERT_EQUAL(ret, -EINVAL);
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
		CU_ASSERT_PTR_NULL(raw);
	}
	ret = queuePtr->getCQueue(&audio);
	if (t == Queue::Type::AUDIO) {
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_PTR_NOT_NULL(audio);
		ret = queuePtr->getCQueue(nullAudio);
		CU_ASSERT_EQUAL(ret, -EINVAL);
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
		CU_ASSERT_PTR_NULL(audio);
	}
}


static inline void testQueueWrapped(std::unique_ptr<Queue> &queuePtr,
				    Queue::Type t,
				    void *existing)
{
	testQueueInitialized(queuePtr, t);
	CU_ASSERT_EQUAL(*queuePtr, existing);
}


static void destroyRawQueue(Queue::Type type, void *ptr)
{
	if (!ptr)
		return;
	switch (type) {
	case Queue::Type::CODED_VIDEO:
		mbuf_coded_video_frame_queue_destroy(
			(struct mbuf_coded_video_frame_queue *)ptr);
		break;
	case Queue::Type::RAW_VIDEO:
		mbuf_raw_video_frame_queue_destroy(
			(struct mbuf_raw_video_frame_queue *)ptr);
		break;
	case Queue::Type::AUDIO:
		mbuf_audio_frame_queue_destroy(
			(struct mbuf_audio_frame_queue *)ptr);
		break;
	}
}


template <typename ArgsType>
static void createWithArgs(Queue::Type type, ArgsType &args)
{
	std::unique_ptr<Queue> queuePtr = Queue::createWithArgs(&args, true);
	testQueueInitialized(queuePtr, type);

	std::unique_ptr<Queue> queuePtr2 = Queue::createWithArgs(&args, false);
	testQueueInitialized(queuePtr2, type);
	void *raw = queuePtr2->getQueuePtr();
	destroyRawQueue(type, raw);
}


struct testEventCbArg {
	std::unique_ptr<Queue> queuePtr;
	size_t count;
	bool called;
};


static void queueEvtCb(struct pomp_evt *evt, void *userdata)
{
	auto args = reinterpret_cast<struct testEventCbArg *>(userdata);

	args->called = true;

	CU_ASSERT_EQUAL(args->queuePtr->getCount(), args->count);
	for (size_t i = 0; i < MBUF_TEST_DEFAULT_FRAME_TEST_COUNT; i++) {
		testPopCoded(args->queuePtr);
		testPopRaw(args->queuePtr);
		testPopAudio(args->queuePtr);

		args->count--;
	}
}


static void testMbufQueueCreate()
{
	try {
		std::unique_ptr<Queue> queuePtr =
			Queue::create(static_cast<Queue::Type>(25), true);
		CU_ASSERT_EQUAL(queuePtr, nullptr);
	} catch (const std::bad_alloc &) {
		CU_FAIL("bad alloc");
	}

	for (Queue::Type t : allTypes) {
		try {
			std::unique_ptr<Queue> queuePtr =
				Queue::create(t, true);
			testQueueInitialized(queuePtr, t);

		} catch (const std::bad_alloc &) {
			CU_FAIL("bad alloc");
		}

		try {
			std::unique_ptr<Queue> queuePtr =
				Queue::create(t, false);
			testQueueInitialized(queuePtr, t);
			void *raw = queuePtr->getQueuePtr();
			destroyRawQueue(t, raw);

		} catch (const std::bad_alloc &) {
			CU_FAIL("bad alloc");
		}
	}
}


static void testMbufQueueCreateWithArgs()
{
	try {
		struct mbuf_coded_video_frame_queue_args args = {};
		createWithArgs(Queue::Type::CODED_VIDEO, args);
	} catch (const std::bad_alloc &) {
		CU_FAIL("coded queue creation failed");
	}

	try {
		struct mbuf_raw_video_frame_queue_args args = {};
		createWithArgs(Queue::Type::RAW_VIDEO, args);
	} catch (const std::bad_alloc &) {
		CU_FAIL("raw queue creation failed");
	}

	try {
		struct mbuf_audio_frame_queue_args args = {};
		createWithArgs(Queue::Type::AUDIO, args);
	} catch (const std::bad_alloc &) {
		CU_FAIL("audio queue creation failed");
	}
}


static void testMbufQueueArgsFeatures()
{
	/* Test max_frames functionality (frame dropping) */
	{
		struct mbuf_coded_video_frame_queue_args args = {};
		args.max_frames = 1;
		std::unique_ptr<Queue> queue;
		try {
			queue = mbuf::Queue::createWithArgs(&args);
			CU_ASSERT_NOT_EQUAL_FATAL(queue, nullptr);
		} catch (const std::bad_alloc &) {
			CU_FAIL("coded queue creation failed");
		}

		/* Create two frames */
		struct vdef_coded_frame frameInfo = {};
		frameInfo.format = vdef_h264_byte_stream;
		struct mbuf_coded_video_frame *frame1 = nullptr;
		struct mbuf_coded_video_frame *frame2 = nullptr;
		mbuf_coded_video_frame_new(&frameInfo, &frame1);
		addDefaultNalu(frame1);
		mbuf_coded_video_frame_finalize(frame1);
		mbuf_coded_video_frame_new(&frameInfo, &frame2);
		addDefaultNalu(frame2);
		mbuf_coded_video_frame_finalize(frame2);

		/* Push frame1 */
		int ret = queue->pushFrame(frame1);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(queue->getCount(), 1);

		/* Push frame2, frame1 should be dropped */
		ret = queue->pushFrame(frame2);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(queue->getCount(), 1);

		/* Pop frame, it should be frame2 */
		struct mbuf_coded_video_frame *out_frame = nullptr;
		ret = queue->popFrame(&out_frame);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_PTR_EQUAL(out_frame, frame2);

		mbuf_coded_video_frame_unref(frame1);
		mbuf_coded_video_frame_unref(frame2);
		mbuf_coded_video_frame_unref(out_frame);
	}

	/* Test filter functionality */
	{
		struct mbuf_coded_video_frame_queue_args args = {};
		args.filter = [](struct mbuf_coded_video_frame *frame,
				 void *userdata) {
			return false; /* Always reject */
		};
		std::unique_ptr<Queue> queue;
		try {
			queue = mbuf::Queue::createWithArgs(&args);
			CU_ASSERT_NOT_EQUAL_FATAL(queue, nullptr);
		} catch (const std::bad_alloc &) {
			CU_FAIL("coded queue creation failed");
		}

		struct vdef_coded_frame frameInfo = {};
		frameInfo.format = vdef_h264_byte_stream;
		struct mbuf_coded_video_frame *frame = nullptr;
		mbuf_coded_video_frame_new(&frameInfo, &frame);
		addDefaultNalu(frame);
		mbuf_coded_video_frame_finalize(frame);

		/* Push should fail with -EPROTO due to the filter */
		int ret = queue->pushFrame(frame);
		CU_ASSERT_EQUAL(ret, -EPROTO);
		CU_ASSERT_EQUAL(queue->getCount(), 0);

		mbuf_coded_video_frame_unref(frame);
	}
}


static void testMbufQueueWrapExisting()
{
	try {
		struct mbuf_coded_video_frame_queue *queue = nullptr;
		struct mbuf_coded_video_frame_queue *nullQueue = nullptr;
		int ret = mbuf_coded_video_frame_queue_new(&queue);
		CU_ASSERT_EQUAL(ret, 0);

		std::unique_ptr<Queue> queuePtr =
			Queue::wrapExisting(queue, true);
		testQueueWrapped(queuePtr, Queue::Type::CODED_VIDEO, queue);

		CU_ASSERT_EQUAL(Queue::wrapExisting(nullQueue, true), nullptr);

	} catch (const std::bad_alloc &) {
		CU_FAIL("coded queue wrapping failed");
	}

	try {
		struct mbuf_raw_video_frame_queue *queue = nullptr;
		struct mbuf_raw_video_frame_queue *nullQueue = nullptr;
		int ret = mbuf_raw_video_frame_queue_new(&queue);
		CU_ASSERT_EQUAL(ret, 0);

		std::unique_ptr<Queue> queuePtr =
			Queue::wrapExisting(queue, true);
		testQueueWrapped(queuePtr, Queue::Type::RAW_VIDEO, queue);

		CU_ASSERT_EQUAL(Queue::wrapExisting(nullQueue, true), nullptr);

	} catch (const std::bad_alloc &) {
		CU_FAIL("raw queue wrapping failed");
	}

	try {
		struct mbuf_audio_frame_queue *queue = nullptr;
		struct mbuf_audio_frame_queue *nullQueue = nullptr;
		int ret = mbuf_audio_frame_queue_new(&queue);
		CU_ASSERT_EQUAL(ret, 0);

		std::unique_ptr<Queue> queuePtr =
			Queue::wrapExisting(queue, true);
		testQueueWrapped(queuePtr, Queue::Type::AUDIO, queue);

		CU_ASSERT_EQUAL(Queue::wrapExisting(nullQueue, true), nullptr);

	} catch (const std::bad_alloc &) {
		CU_FAIL("audio queue wrapping failed");
	}
}


static void testMbufQueueMethods()
{
	for (Queue::Type t : allTypes) {
		std::unique_ptr<Queue> queuePtr;
		try {
			queuePtr = Queue::create(t);
			CU_ASSERT_NOT_EQUAL_FATAL(queuePtr, nullptr);
		} catch (const std::bad_alloc &) {
			CU_FAIL_FATAL("bad alloc");
		}

		testCodedMethods(queuePtr);
		testRawMethods(queuePtr);
		testAudioMethods(queuePtr);
		testAbstractMethods(queuePtr);
	}
}


static void testMbufQueueLoopMethods()
{
	int ret = 0;
	struct pomp_loop *loop = pomp_loop_new();
	struct testEventCbArg args;

	for (Queue::Type t : allTypes) {
		try {
			args.queuePtr = Queue::create(t);
			CU_ASSERT_NOT_EQUAL_FATAL(args.queuePtr, nullptr);
		} catch (const std::bad_alloc &) {
			CU_FAIL_FATAL("bad alloc");
		}

		args.count = MBUF_TEST_DEFAULT_FRAME_TEST_COUNT;
		args.called = false;

		ret = args.queuePtr->detachFromLoop(loop);
		CU_ASSERT_EQUAL(ret, 0);

		ret = args.queuePtr->attachToLoop(loop, queueEvtCb, &args);
		CU_ASSERT_EQUAL(ret, 0);

		for (size_t i = 0; i < MBUF_TEST_DEFAULT_FRAME_TEST_COUNT;
		     i++) {
			testPushCoded(args.queuePtr);
			testPushRaw(args.queuePtr);
			testPushAudio(args.queuePtr);
		}

		ret = pomp_loop_wait_and_process(loop, 100);
		CU_ASSERT_EQUAL(ret, 0);

		ret = args.queuePtr->detachFromLoop(loop);
		CU_ASSERT_EQUAL(ret, 0);

		CU_ASSERT_TRUE(args.called);
		CU_ASSERT_EQUAL(args.count, 0);
	}

	pomp_loop_destroy(loop);
}


CU_TestInfo g_mbuf_test_queue_cpp[] = {
	{(char *)"queue_create", &testMbufQueueCreate},
	{(char *)"queue_create_with_args", &testMbufQueueCreateWithArgs},
	{(char *)"queue_args_features", &testMbufQueueArgsFeatures},
	{(char *)"queue_wrap_existing", &testMbufQueueWrapExisting},
	{(char *)"queue_methods", &testMbufQueueMethods},
	{(char *)"queue_loops_methods", &testMbufQueueLoopMethods},
	CU_TEST_INFO_NULL,
};
