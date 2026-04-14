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


static void test_pop_audio(std::unique_ptr<Queue> &queuePtr)
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


static void test_peek_at_audio(std::unique_ptr<Queue> &queuePtr)
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


static void test_peek_audio(std::unique_ptr<Queue> &queuePtr)
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


static size_t get_frame_size(const struct adef_frame *info)
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


static void set_buffer(struct mbuf_audio_frame *frame,
		       struct mbuf_mem *base_mem)
{
	struct adef_frame frame_info;
	size_t buffer_size;
	bool internal_mem = false;

	int ret = mbuf_audio_frame_get_frame_info(frame, &frame_info);
	if (ret != 0)
		return;

	buffer_size = get_frame_size(&frame_info);

	if (!base_mem) {
		ret = mbuf_mem_generic_new(buffer_size, &base_mem);
		CU_ASSERT_EQUAL(ret, 0);
		if (ret != 0)
			return;
		internal_mem = true;
	}
	CU_ASSERT_PTR_NOT_NULL_FATAL(base_mem);


	(void)mbuf_audio_frame_set_buffer(frame, base_mem, 0, buffer_size);
	if (internal_mem)
		mbuf_mem_unref(base_mem);
}


static void test_push_audio(std::unique_ptr<Queue> &queuePtr)
{
	int count = queuePtr->getCount();
	struct mbuf_audio_frame *frame = nullptr;
	struct mbuf_audio_frame *nullFrame = nullptr;
	struct adef_frame frame_info = {};
	int ret = 0;
	frame_info.format = adef_pcm_16b_44100hz_stereo;
	frame_info.info.timestamp = frame_info.info.capture_timestamp =
		rand() % 1000000;
	frame_info.info.index = 33;
	frame_info.info.timescale = 1000000;

	ret = mbuf_audio_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(frame);

	ret = queuePtr->pushFrame(frame);
	if (queuePtr->getType() == Queue::Type::AUDIO) {
		CU_ASSERT_EQUAL(ret, -EBUSY);

		set_buffer(frame, NULL);

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


static void test_pop_raw(std::unique_ptr<Queue> &queuePtr)
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


static void test_peek_at_raw(std::unique_ptr<Queue> &queuePtr)
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


static void test_peek_raw(std::unique_ptr<Queue> &queuePtr)
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


static void set_planes(struct mbuf_raw_video_frame *frame,
		       struct mbuf_mem *base_mem,
		       struct mbuf_mem *p2mem,
		       struct mbuf_mem *p3mem)
{
	struct vdef_raw_frame frame_info;
	size_t plane_size[VDEF_RAW_MAX_PLANE_COUNT];
	size_t plane_stride[VDEF_RAW_MAX_PLANE_COUNT] = {0};
	size_t plane_height[VDEF_RAW_MAX_PLANE_COUNT];
	struct mbuf_mem *plane_mem[VDEF_RAW_MAX_PLANE_COUNT];
	size_t plane_offset[VDEF_RAW_MAX_PLANE_COUNT] = {0};
	bool internal_mem = false;

	int ret = mbuf_raw_video_frame_get_frame_info(frame, &frame_info);
	CU_ASSERT_EQUAL(ret, 0);
	if (ret != 0)
		return;

	if (!vdef_raw_format_cmp(&frame_info.format, &vdef_i420)) {
		CU_FAIL("This test only operates on i420 frames");
		return;
	}

	ret = vdef_calc_raw_frame_size(&frame_info.format,
				       &frame_info.info.resolution,
				       plane_stride,
				       NULL,
				       NULL,
				       NULL,
				       plane_size,
				       NULL);
	CU_ASSERT_EQUAL(ret, 0);
	if (ret != 0)
		return;
	for (unsigned int i = 0; i < 3; i++) {
		plane_height[i] = plane_size[i] / plane_stride[i];
		plane_size[i] = plane_height[i] * frame_info.plane_stride[i];
	}
	if (!base_mem) {
		ret = mbuf_mem_generic_new(plane_size[0] + plane_size[1] +
						   plane_size[2],
					   &base_mem);
		if (ret != 0)
			return;
		internal_mem = true;
	}
	if (!p2mem && !p3mem) {
		p3mem = p2mem = base_mem;
		plane_offset[1] = plane_size[0];
		plane_offset[2] = plane_size[0] + plane_size[1];
	}
	plane_mem[0] = base_mem;
	plane_mem[1] = p2mem;
	plane_mem[2] = p3mem;

	for (unsigned int i = 0; i < 3; i++) {
		void *plane;
		size_t cap;
		ret = mbuf_mem_get_data(plane_mem[i], &plane, &cap);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT(plane_offset[i] + plane_size[i] <= cap);
		if (ret != 0 || plane_offset[i] + plane_size[i] > cap)
			return;
		for (size_t j = 0; j < plane_height[i]; j++) {
			auto dst = static_cast<uint8_t *>(plane);
			dst += plane_offset[i];
			dst += j * frame_info.plane_stride[i];
			memset(dst, i + 10, plane_stride[i]);
			if (plane_stride[i] < frame_info.plane_stride[i])
				memset(dst + plane_stride[i],
				       i + 20,
				       frame_info.plane_stride[i] -
					       plane_stride[i]);
		}
		ret = mbuf_raw_video_frame_set_plane(
			frame, i, plane_mem[i], plane_offset[i], plane_size[i]);
		CU_ASSERT_EQUAL(ret, 0);
	}
	if (internal_mem)
		mbuf_mem_unref(base_mem);
}


static void test_push_raw(std::unique_ptr<Queue> &queuePtr)
{
	struct vdef_raw_frame frame_info = {};
	frame_info.format = vdef_i420;
	frame_info.info.resolution.width = MBUF_TEST_WIDTH;
	frame_info.info.resolution.height = MBUF_TEST_HEIGHT;
	frame_info.plane_stride[0] = MBUF_TEST_WIDTH;
	frame_info.plane_stride[1] = MBUF_TEST_WIDTH / 2;
	frame_info.plane_stride[2] = MBUF_TEST_WIDTH / 2;
	int count = queuePtr->getCount();
	struct mbuf_raw_video_frame *frame = nullptr;
	struct mbuf_raw_video_frame *nullFrame = nullptr;
	int ret = 0;

	ret = mbuf_raw_video_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(frame);

	ret = queuePtr->pushFrame(frame);
	if (queuePtr->getType() == Queue::Type::RAW_VIDEO) {
		CU_ASSERT_EQUAL(ret, -EBUSY);

		set_planes(frame, NULL, NULL, NULL);

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


static void add_nalu(struct mbuf_coded_video_frame *frame,
		     struct mbuf_mem *mem,
		     size_t offset,
		     enum h264_nalu_type type,
		     enum h264_slice_type slice_type,
		     int value,
		     int importance)
{
	void *coded_data;
	uint8_t *data;
	size_t cap;
	int ret = mbuf_mem_get_data(mem, &coded_data, &cap);
	data = static_cast<uint8_t *>(coded_data);

	if (ret != 0 || cap < (MBUF_TEST_SIZE + offset))
		return;

	memset(data + offset, value, MBUF_TEST_SIZE);
	struct vdef_nalu nalu = {};
	nalu.size = MBUF_TEST_SIZE;
	nalu.importance = importance;
	nalu.h264.type = type;
	nalu.h264.slice_type = slice_type;

	(void)mbuf_coded_video_frame_add_nalu(frame, mem, offset, &nalu);
}


static void add_default_nalu(struct mbuf_coded_video_frame *frame)
{
	struct mbuf_mem *mem;
	int ret = mbuf_mem_generic_new(MBUF_TEST_SIZE, &mem);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(mem);
	add_nalu(frame,
		 mem,
		 0,
		 H264_NALU_TYPE_SPS,
		 H264_SLICE_TYPE_UNKNOWN,
		 1,
		 0);
	(void)mbuf_mem_unref(mem);
}


static void test_pop_coded(std::unique_ptr<Queue> &queuePtr)
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


static void test_peek_at_coded(std::unique_ptr<Queue> &queuePtr)
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


static void test_peek_coded(std::unique_ptr<Queue> &queuePtr)
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


static void test_push_coded(std::unique_ptr<Queue> &queuePtr)
{
	struct vdef_coded_frame frame_info = {};
	frame_info.format = vdef_h264_byte_stream;
	frame_info.info.resolution.width = MBUF_TEST_WIDTH;
	frame_info.info.resolution.height = MBUF_TEST_HEIGHT;
	int count = queuePtr->getCount();
	struct mbuf_coded_video_frame *frame = nullptr;
	struct mbuf_coded_video_frame *nullFrame = nullptr;
	int ret = 0;

	ret = mbuf_coded_video_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(frame);

	ret = queuePtr->pushFrame(frame);
	if (queuePtr->getType() == Queue::Type::CODED_VIDEO) {
		CU_ASSERT_EQUAL(ret, -EBUSY);

		add_default_nalu(frame);

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


static void test_audio_methods(std::unique_ptr<Queue> &queuePtr)
{
	test_peek_audio(queuePtr);
	test_peek_at_audio(queuePtr);

	for (size_t i = 0; i < MBUF_TEST_DEFAULT_FRAME_TEST_COUNT; i++)
		test_push_audio(queuePtr);

	test_peek_audio(queuePtr);
	test_peek_at_audio(queuePtr);

	for (size_t i = 0; i < MBUF_TEST_DEFAULT_FRAME_TEST_COUNT; i++)
		test_pop_audio(queuePtr);

	test_peek_audio(queuePtr);
	test_peek_at_audio(queuePtr);

	test_push_audio(queuePtr);
	CU_ASSERT_EQUAL(queuePtr->flush(), 0);
	CU_ASSERT_EQUAL(queuePtr->getCount(), 0);
}


static void test_raw_methods(std::unique_ptr<Queue> &queuePtr)
{
	test_peek_raw(queuePtr);
	test_peek_at_raw(queuePtr);

	for (size_t i = 0; i < MBUF_TEST_DEFAULT_FRAME_TEST_COUNT; i++)
		test_push_raw(queuePtr);

	test_peek_raw(queuePtr);
	test_peek_at_raw(queuePtr);

	for (size_t i = 0; i < MBUF_TEST_DEFAULT_FRAME_TEST_COUNT; i++)
		test_pop_raw(queuePtr);

	test_peek_raw(queuePtr);
	test_peek_at_raw(queuePtr);

	test_push_raw(queuePtr);
	CU_ASSERT_EQUAL(queuePtr->flush(), 0);
	CU_ASSERT_EQUAL(queuePtr->getCount(), 0);
}


static void test_coded_methods(std::unique_ptr<Queue> &queuePtr)
{
	test_peek_coded(queuePtr);
	test_peek_at_coded(queuePtr);

	for (size_t i = 0; i < MBUF_TEST_DEFAULT_FRAME_TEST_COUNT; i++)
		test_push_coded(queuePtr);

	test_peek_coded(queuePtr);
	test_peek_at_coded(queuePtr);

	for (size_t i = 0; i < MBUF_TEST_DEFAULT_FRAME_TEST_COUNT; i++)
		test_pop_coded(queuePtr);

	test_peek_coded(queuePtr);
	test_peek_at_coded(queuePtr);

	test_push_coded(queuePtr);
	CU_ASSERT_EQUAL(queuePtr->flush(), 0);
	CU_ASSERT_EQUAL(queuePtr->getCount(), 0);
}


static inline void test_get_event(std::unique_ptr<Queue> &queuePtr)
{
	struct pomp_evt *evt = nullptr;

	CU_ASSERT_EQUAL(queuePtr->getEvent(nullptr), -EINVAL);
	CU_ASSERT_EQUAL(queuePtr->getEvent(&evt), 0);
	CU_ASSERT_PTR_NOT_NULL(evt);
}


static inline void test_queue_initialized(std::unique_ptr<Queue> &queuePtr,
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
	test_get_event(queuePtr);
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


static inline void test_queue_wrapped(std::unique_ptr<Queue> &queuePtr,
				      Queue::Type t,
				      void *existing)
{
	test_queue_initialized(queuePtr, t);
	CU_ASSERT_EQUAL(*queuePtr, existing);
}


static void destroy_raw_queue(Queue::Type type, void *ptr)
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
static void create_with_args(Queue::Type type, ArgsType &args)
{
	std::unique_ptr<Queue> queuePtr = Queue::createWithArgs(&args, true);
	test_queue_initialized(queuePtr, type);

	std::unique_ptr<Queue> queuePtr2 = Queue::createWithArgs(&args, false);
	test_queue_initialized(queuePtr2, type);
	void *raw = queuePtr2->getQueuePtr();
	destroy_raw_queue(type, raw);
}


struct test_event_cb_arg {
	std::unique_ptr<Queue> queuePtr;
	size_t count;
	bool called;
};


static void queue_evt_cb(struct pomp_evt *evt, void *userdata)
{
	auto args = reinterpret_cast<struct test_event_cb_arg *>(userdata);

	args->called = true;

	CU_ASSERT_EQUAL(args->queuePtr->getCount(), args->count);
	for (size_t i = 0; i < MBUF_TEST_DEFAULT_FRAME_TEST_COUNT; i++) {
		test_pop_coded(args->queuePtr);
		test_pop_raw(args->queuePtr);
		test_pop_audio(args->queuePtr);

		args->count--;
	}
}


static void test_mbuf_queue_create()
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
			test_queue_initialized(queuePtr, t);

		} catch (const std::bad_alloc &) {
			CU_FAIL("bad alloc");
		}

		try {
			std::unique_ptr<Queue> queuePtr =
				Queue::create(t, false);
			test_queue_initialized(queuePtr, t);
			void *raw = queuePtr->getQueuePtr();
			destroy_raw_queue(t, raw);

		} catch (const std::bad_alloc &) {
			CU_FAIL("bad alloc");
		}
	}
}


static void test_mbuf_queue_create_with_args()
{
	try {
		struct mbuf_coded_video_frame_queue_args args = {};
		create_with_args(Queue::Type::CODED_VIDEO, args);
	} catch (const std::bad_alloc &) {
		CU_FAIL("coded queue creation failed");
	}

	try {
		struct mbuf_raw_video_frame_queue_args args = {};
		create_with_args(Queue::Type::RAW_VIDEO, args);
	} catch (const std::bad_alloc &) {
		CU_FAIL("raw queue creation failed");
	}

	try {
		struct mbuf_audio_frame_queue_args args = {};
		create_with_args(Queue::Type::AUDIO, args);
	} catch (const std::bad_alloc &) {
		CU_FAIL("audio queue creation failed");
	}
}


static void test_mbuf_queue_args_features()
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
		struct vdef_coded_frame frame_info = {};
		frame_info.format = vdef_h264_byte_stream;
		struct mbuf_coded_video_frame *frame1 = nullptr;
		struct mbuf_coded_video_frame *frame2 = nullptr;
		mbuf_coded_video_frame_new(&frame_info, &frame1);
		add_default_nalu(frame1);
		mbuf_coded_video_frame_finalize(frame1);
		mbuf_coded_video_frame_new(&frame_info, &frame2);
		add_default_nalu(frame2);
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

		struct vdef_coded_frame frame_info = {};
		frame_info.format = vdef_h264_byte_stream;
		struct mbuf_coded_video_frame *frame = nullptr;
		mbuf_coded_video_frame_new(&frame_info, &frame);
		add_default_nalu(frame);
		mbuf_coded_video_frame_finalize(frame);

		/* Push should fail with -EPROTO due to the filter */
		int ret = queue->pushFrame(frame);
		CU_ASSERT_EQUAL(ret, -EPROTO);
		CU_ASSERT_EQUAL(queue->getCount(), 0);

		mbuf_coded_video_frame_unref(frame);
	}
}


static void test_mbuf_queue_wrap_existing()
{
	try {
		struct mbuf_coded_video_frame_queue *queue = nullptr;
		struct mbuf_coded_video_frame_queue *nullQueue = nullptr;
		int ret = mbuf_coded_video_frame_queue_new(&queue);
		CU_ASSERT_EQUAL(ret, 0);

		std::unique_ptr<Queue> queuePtr =
			Queue::wrapExisting(queue, true);
		test_queue_wrapped(queuePtr, Queue::Type::CODED_VIDEO, queue);

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
		test_queue_wrapped(queuePtr, Queue::Type::RAW_VIDEO, queue);

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
		test_queue_wrapped(queuePtr, Queue::Type::AUDIO, queue);

		CU_ASSERT_EQUAL(Queue::wrapExisting(nullQueue, true), nullptr);

	} catch (const std::bad_alloc &) {
		CU_FAIL("audio queue wrapping failed");
	}
}


static void test_mbuf_queue_methods()
{
	for (Queue::Type t : allTypes) {
		std::unique_ptr<Queue> queuePtr;
		try {
			queuePtr = Queue::create(t);
			CU_ASSERT_NOT_EQUAL_FATAL(queuePtr, nullptr);
		} catch (const std::bad_alloc &) {
			CU_FAIL_FATAL("bad alloc");
		}

		test_coded_methods(queuePtr);
		test_raw_methods(queuePtr);
		test_audio_methods(queuePtr);
	}
}


static void test_mbuf_queue_loop_methods()
{
	int ret = 0;
	struct pomp_loop *loop = pomp_loop_new();
	struct test_event_cb_arg args;

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

		ret = args.queuePtr->attachToLoop(loop, queue_evt_cb, &args);
		CU_ASSERT_EQUAL(ret, 0);

		for (size_t i = 0; i < MBUF_TEST_DEFAULT_FRAME_TEST_COUNT;
		     i++) {
			test_push_coded(args.queuePtr);
			test_push_raw(args.queuePtr);
			test_push_audio(args.queuePtr);
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
	{(char *)"queue_create", &test_mbuf_queue_create},
	{(char *)"queue_create_with_args", &test_mbuf_queue_create_with_args},
	{(char *)"queue_args_features", &test_mbuf_queue_args_features},
	{(char *)"queue_wrap_existing", &test_mbuf_queue_wrap_existing},
	{(char *)"queue_methods", &test_mbuf_queue_methods},
	{(char *)"queue_loops_methods", &test_mbuf_queue_loop_methods},
	CU_TEST_INFO_NULL,
};
