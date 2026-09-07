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
#include "media-buffers/mbuf_frame.hpp"

using mbuf::Frame;


constexpr unsigned int MBUF_TEST_WIDTH = 4;
constexpr unsigned int MBUF_TEST_HEIGHT = 4;
constexpr unsigned int MBUF_TEST_SIZE = 128;
constexpr unsigned int MBUF_TEST_SAMPLE_PER_FRAME = 1024;
constexpr unsigned int MBUF_TEST_AAC_BITRATE = 320000; /* 320Kbps */


static inline void testFrameInitialized(std::unique_ptr<Frame> &framePtr,
					Frame::Type t)
{
	CU_ASSERT_NOT_EQUAL_FATAL(framePtr, nullptr);
	CU_ASSERT_EQUAL(framePtr->getType(), t);
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


static void setupAudio(std::unique_ptr<Frame> &frame)
{
	int ret = 0;
	struct adef_frame frameInfo = {};
	size_t bufferSize = MBUF_TEST_SIZE;
	struct mbuf_mem *mem;
	Frame::Type t = frame->getType();

	if (t == Frame::Type::AUDIO) {
		ret = frame->getFrameInfo(&frameInfo);
		CU_ASSERT_EQUAL(ret, 0);

		bufferSize = getFrameSize(&frameInfo);

		ret = mbuf_mem_generic_new(bufferSize, &mem);
		if (ret != 0)
			return;

		ret = frame->setBuffer(mem, 0, bufferSize);
		CU_ASSERT_EQUAL(ret, 0);

		ret = frame->finalize();
		CU_ASSERT_EQUAL(ret, 0);
	} else {
		ret = mbuf_mem_generic_new(bufferSize, &mem);
		if (ret != 0)
			return;

		ret = frame->getFrameInfo(&frameInfo);
		CU_ASSERT_EQUAL(ret, -ENOSYS);

		ret = frame->setBuffer(mem, 0, bufferSize);
		CU_ASSERT_EQUAL(ret, -ENOSYS);
	}
	mbuf_mem_unref(mem);
}


static void setupRaw(std::unique_ptr<Frame> &frame)
{
	int ret = 0;
	struct vdef_raw_frame frameInfo = {};
	size_t planeSize[VDEF_RAW_MAX_PLANE_COUNT];
	size_t planeStride[VDEF_RAW_MAX_PLANE_COUNT] = {0};
	size_t planeHeight[VDEF_RAW_MAX_PLANE_COUNT];
	struct mbuf_mem *planeMem[VDEF_RAW_MAX_PLANE_COUNT] = {nullptr};
	size_t planeOffset[VDEF_RAW_MAX_PLANE_COUNT] = {0};
	struct mbuf_mem *mem = nullptr;
	struct mbuf_mem *p2mem;
	struct mbuf_mem *p3mem;
	Frame::Type t = frame->getType();

	ret = frame->getFrameInfo(&frameInfo);
	if (t == Frame::Type::RAW_VIDEO) {
		CU_ASSERT_EQUAL(ret, 0);

		ret = vdef_calc_raw_frame_size(&frameInfo.format,
					       &frameInfo.info.resolution,
					       planeStride,
					       NULL,
					       NULL,
					       NULL,
					       planeSize,
					       NULL);
		CU_ASSERT_EQUAL(ret, 0);
		if (ret != 0)
			return;

		for (unsigned int i = 0; i < 3; i++) {
			planeHeight[i] = planeSize[i] / planeStride[i];
			planeSize[i] =
				planeHeight[i] * frameInfo.plane_stride[i];
		}

		ret = mbuf_mem_generic_new(
			planeSize[0] + planeSize[1] + planeSize[2], &mem);
		if (ret != 0)
			return;
		p3mem = p2mem = mem;
		planeOffset[1] = planeSize[0];
		planeOffset[2] = planeSize[0] + planeSize[1];
		planeMem[0] = mem;
		planeMem[1] = p2mem;
		planeMem[2] = p3mem;

		for (unsigned int i = 0; i < 3; i++) {
			void *plane;
			size_t cap;
			ret = mbuf_mem_get_data(planeMem[i], &plane, &cap);
			CU_ASSERT_EQUAL(ret, 0);
			CU_ASSERT(planeOffset[i] + planeSize[i] <= cap);
			if (ret != 0 || planeOffset[i] + planeSize[i] > cap)
				return;
			for (size_t j = 0; j < planeHeight[i]; j++) {
				auto dst = static_cast<uint8_t *>(plane);
				dst += planeOffset[i];
				dst += j * frameInfo.plane_stride[i];
				memset(dst, i + 10, planeStride[i]);
				if (planeStride[i] < frameInfo.plane_stride[i])
					memset(dst + planeStride[i],
					       i + 20,
					       frameInfo.plane_stride[i] -
						       planeStride[i]);
			}
			ret = frame->setPlane(
				i, planeMem[i], planeOffset[i], planeSize[i]);
			CU_ASSERT_EQUAL(ret, 0);
		}

		ret = frame->finalize();
		CU_ASSERT_EQUAL(ret, 0);
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);
		ret = frame->setPlane(
			0, planeMem[0], planeOffset[0], planeSize[0]);
		CU_ASSERT_EQUAL(ret, -ENOSYS);
	}

	mbuf_mem_unref(mem);
}


static void setupCoded(std::unique_ptr<Frame> &frame, bool insert)
{
	int ret = 0;
	size_t offset = 0;
	void *codedData;
	uint8_t *data;
	size_t cap;
	struct mbuf_mem *mem;
	struct mbuf_mem *mem2;
	struct vdef_nalu nalu = {};
	struct vdef_coded_frame frameInfo = {};
	nalu.size = MBUF_TEST_SIZE;
	nalu.importance = 0;
	nalu.h264.type = H264_NALU_TYPE_SPS;
	nalu.h264.slice_type = H264_SLICE_TYPE_UNKNOWN;

	Frame::Type t = frame->getType();

	(void)mbuf_mem_generic_new(MBUF_TEST_SIZE, &mem);
	(void)mbuf_mem_generic_new(MBUF_TEST_SIZE, &mem2);

	ret = mbuf_mem_get_data(mem, &codedData, &cap);
	data = static_cast<uint8_t *>(codedData);

	if (ret != 0 || cap < (MBUF_TEST_SIZE))
		return;

	memset(data, 1, MBUF_TEST_SIZE);

	ret = frame->getFrameInfo(&frameInfo);
	if (t == Frame::Type::CODED_VIDEO) {
		CU_ASSERT_EQUAL(ret, 0);

		ret = frame->addNalu(nullptr, offset, &nalu);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		ret = frame->addNalu(mem, offset, nullptr);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		ret = frame->addNalu(nullptr, offset, nullptr);
		CU_ASSERT_EQUAL(ret, -EINVAL);

		ret = frame->insertNalu(nullptr, offset, &nalu, 1);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		ret = frame->insertNalu(mem2, offset, nullptr, 1);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		ret = frame->insertNalu(nullptr, offset, nullptr, 1);
		CU_ASSERT_EQUAL(ret, -EINVAL);

		ret = frame->addNalu(mem, offset, &nalu);
		CU_ASSERT_EQUAL(ret, 0);

		if (insert) {
			ret = frame->insertNalu(mem2, offset, &nalu, 1);
			CU_ASSERT_EQUAL(ret, 0);
		}

		ret = frame->finalize();
		CU_ASSERT_EQUAL(ret, 0);
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);

		ret = frame->addNalu(mem, offset, &nalu);
		CU_ASSERT_EQUAL(ret, -ENOSYS);

		ret = frame->insertNalu(mem2, offset, &nalu, 0);
		CU_ASSERT_EQUAL(ret, -ENOSYS);
	}
	(void)mbuf_mem_unref(mem);
	(void)mbuf_mem_unref(mem2);
}


static void
setupFrame(std::unique_ptr<Frame> &frame, Frame::Type t, bool insert)
{
	switch (t) {
	case Frame::Type::CODED_VIDEO:
		setupCoded(frame, insert);
		break;
	case Frame::Type::RAW_VIDEO:
		setupRaw(frame);
		break;
	case Frame::Type::AUDIO:
		setupAudio(frame);
		break;
	default:
		CU_FAIL("invalid type");
	}
}


static std::unique_ptr<Frame> testWrapCoded()
{
	struct mbuf_coded_video_frame *cFrame = nullptr;
	struct vdef_coded_frame frameInfo = {};
	frameInfo.format = vdef_h264_byte_stream;
	frameInfo.info.resolution.width = MBUF_TEST_WIDTH;
	frameInfo.info.resolution.height = MBUF_TEST_HEIGHT;

	int ret = mbuf_coded_video_frame_new(&frameInfo, &cFrame);
	CU_ASSERT_EQUAL(ret, 0);

	std::unique_ptr<Frame> frame = nullptr;
	try {
		frame = Frame::wrapExisting(cFrame, true);
	} catch (const std::bad_alloc &) {
		CU_FAIL("bad alloc");
		return nullptr;
	}

	testFrameInitialized(frame, Frame::Type::CODED_VIDEO);

	return frame;
}


static std::unique_ptr<Frame> testCreateCoded()
{
	struct vdef_coded_frame frameInfo = {};
	frameInfo.format = vdef_h264_byte_stream;
	frameInfo.info.resolution.width = MBUF_TEST_WIDTH;
	frameInfo.info.resolution.height = MBUF_TEST_HEIGHT;

	std::unique_ptr<Frame> frame = nullptr;
	try {
		frame = Frame::create(&frameInfo);
	} catch (const std::bad_alloc &) {
		CU_FAIL("bad alloc");
		return nullptr;
	}

	testFrameInitialized(frame, Frame::Type::CODED_VIDEO);

	return frame;
}


static std::unique_ptr<Frame> testWrapRaw()
{
	struct mbuf_raw_video_frame *cFrame = nullptr;
	struct vdef_raw_frame frameInfo = {};
	frameInfo.format = vdef_i420;
	frameInfo.info.resolution.width = MBUF_TEST_WIDTH;
	frameInfo.info.resolution.height = MBUF_TEST_HEIGHT;
	frameInfo.plane_stride[0] = MBUF_TEST_WIDTH;
	frameInfo.plane_stride[1] = MBUF_TEST_WIDTH / 2;
	frameInfo.plane_stride[2] = MBUF_TEST_WIDTH / 2;

	int ret = mbuf_raw_video_frame_new(&frameInfo, &cFrame);
	CU_ASSERT_EQUAL(ret, 0);

	std::unique_ptr<Frame> frame = nullptr;
	try {
		frame = Frame::wrapExisting(cFrame, true);
	} catch (const std::bad_alloc &) {
		CU_FAIL("bad alloc");
		return nullptr;
	}

	testFrameInitialized(frame, Frame::Type::RAW_VIDEO);

	return frame;
}


static std::unique_ptr<Frame> testCreateRaw()
{
	struct vdef_raw_frame frameInfo = {};
	frameInfo.format = vdef_i420;
	frameInfo.info.resolution.width = MBUF_TEST_WIDTH;
	frameInfo.info.resolution.height = MBUF_TEST_HEIGHT;
	frameInfo.plane_stride[0] = MBUF_TEST_WIDTH;
	frameInfo.plane_stride[1] = MBUF_TEST_WIDTH / 2;
	frameInfo.plane_stride[2] = MBUF_TEST_WIDTH / 2;

	std::unique_ptr<Frame> frame = nullptr;
	try {
		frame = Frame::create(&frameInfo);
	} catch (const std::bad_alloc &) {
		CU_FAIL("bad alloc");
		return nullptr;
	}

	testFrameInitialized(frame, Frame::Type::RAW_VIDEO);

	return frame;
}


static std::unique_ptr<Frame> testWrapAudio()
{
	struct mbuf_audio_frame *cFrame = nullptr;
	struct adef_frame frameInfo = {};
	frameInfo.format = adef_pcm_16b_44100hz_stereo;
	frameInfo.info.timestamp = frameInfo.info.capture_timestamp =
		rand() % 1000000;
	frameInfo.info.index = 33;
	frameInfo.info.timescale = 1000000;

	int ret = mbuf_audio_frame_new(&frameInfo, &cFrame);
	CU_ASSERT_EQUAL(ret, 0);

	std::unique_ptr<Frame> frame = nullptr;
	try {
		frame = Frame::wrapExisting(cFrame, true);
	} catch (const std::bad_alloc &) {
		CU_FAIL("bad alloc");
		return nullptr;
	}

	testFrameInitialized(frame, Frame::Type::AUDIO);

	return frame;
}


static std::unique_ptr<Frame> testCreateAudio()
{
	struct adef_frame frameInfo = {};
	frameInfo.format = adef_pcm_16b_44100hz_stereo;
	frameInfo.info.timestamp = frameInfo.info.capture_timestamp =
		rand() % 1000000;
	frameInfo.info.index = 33;
	frameInfo.info.timescale = 1000000;

	std::unique_ptr<Frame> frame = nullptr;
	try {
		frame = Frame::create(&frameInfo);
	} catch (const std::bad_alloc &) {
		CU_FAIL("bad alloc");
		return nullptr;
	}

	testFrameInitialized(frame, Frame::Type::AUDIO);

	return frame;
}


static void testMetadata(std::unique_ptr<Frame> &&frame)
{
	int ret = 0;
	struct vmeta_frame *meta;
	struct vmeta_frame *outMeta;

	ret = vmeta_frame_new(VMETA_FRAME_TYPE_PROTO, &meta);
	CU_ASSERT_EQUAL(ret, 0);

	ret = frame->setMetadata(meta);
	if (frame->getType() != Frame::Type::AUDIO) {
		CU_ASSERT_EQUAL(ret, 0);

		ret = frame->getMetadata(&outMeta);
		CU_ASSERT_EQUAL(ret, 0);

		ret = vmeta_frame_unref(outMeta);
		CU_ASSERT_EQUAL(ret, 0);
	} else {
		CU_ASSERT_EQUAL(ret, -ENOSYS);

		ret = frame->getMetadata(&outMeta);
		CU_ASSERT_EQUAL(ret, -ENOSYS);
	}

	ret = vmeta_frame_unref(meta);
	CU_ASSERT_EQUAL(ret, 0);
}


static void testMbufFrameMetadata()
{
	testMetadata(testCreateCoded());
	testMetadata(testCreateRaw());
	testMetadata(testCreateAudio());
}


void cleaner(struct mbuf_ancillary_data *, void *)
{
	/* nothing to do */
}


static void testAncillary(std::unique_ptr<Frame> &&frame)
{
	int ret = 0;
	char name[] = "name";
	char buffer[] = "test";
	int buffer2[25];
	struct mbuf_ancillary_data *data = nullptr;
	struct mbuf_ancillary_data *data2 = nullptr;
	struct mbuf_ancillary_data_cbs cbs = {
		.cleaner = cleaner,
		.cleaner_userdata = nullptr,
	};
	memset(buffer2, 1, sizeof(buffer2));

	ret = frame->getAncillaryData(name, &data);
	CU_ASSERT_EQUAL(ret, -ENOENT);

	ret = frame->removeAncillaryData(name);
	CU_ASSERT_EQUAL(ret, -ENOENT);

	ret = frame->addAncillaryString(name, buffer);
	CU_ASSERT_EQUAL(ret, 0);

	ret = frame->getAncillaryData(name, &data);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(name, mbuf_ancillary_data_get_name(data));
	CU_ASSERT_TRUE(mbuf_ancillary_data_is_string(data));
	CU_ASSERT_STRING_EQUAL(buffer, mbuf_ancillary_data_get_string(data));

	ret = frame->removeAncillaryData(name);
	CU_ASSERT_EQUAL(ret, 0);

	ret = frame->getAncillaryData(name, &data2);
	CU_ASSERT_EQUAL(ret, -ENOENT);

	ret = frame->addAncillaryData(data);
	CU_ASSERT_EQUAL(ret, 0);
	mbuf_ancillary_data_unref(data);

	ret = frame->getAncillaryData(name, &data2);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(name, mbuf_ancillary_data_get_name(data2));
	CU_ASSERT_TRUE(mbuf_ancillary_data_is_string(data2));
	CU_ASSERT_STRING_EQUAL(buffer, mbuf_ancillary_data_get_string(data2));
	mbuf_ancillary_data_unref(data2);

	ret = frame->removeAncillaryData(name);
	CU_ASSERT_EQUAL(ret, 0);

	ret = frame->addAncillaryBuffer(name, buffer2, sizeof(buffer2));
	CU_ASSERT_EQUAL(ret, 0);

	ret = frame->getAncillaryData(name, &data);
	CU_ASSERT_EQUAL(ret, 0);
	size_t len = 0;
	const void *buff = mbuf_ancillary_data_get_buffer(data, &len);
	CU_ASSERT_EQUAL(len, sizeof(buffer2));
	CU_ASSERT_EQUAL(memcmp(buff, buffer2, len), 0);
	mbuf_ancillary_data_unref(data);

	ret = frame->removeAncillaryData(name);
	CU_ASSERT_EQUAL(ret, 0);

	ret = frame->addAncillaryBufferWithCbs(
		name, buffer2, sizeof(buffer2), &cbs);
	CU_ASSERT_EQUAL(ret, 0);

	ret = frame->getAncillaryData(name, &data);
	CU_ASSERT_EQUAL(ret, 0);
	buff = mbuf_ancillary_data_get_buffer(data, &len);
	CU_ASSERT_EQUAL(len, sizeof(buffer2));
	CU_ASSERT_EQUAL(memcmp(buff, buffer2, len), 0);
	mbuf_ancillary_data_unref(data);

	ret = frame->removeAncillaryData(name);
	CU_ASSERT_EQUAL(ret, 0);
}


static void testMbufFrameAddAncillary()
{
	testAncillary(testCreateCoded());
	testAncillary(testCreateRaw());
	testAncillary(testCreateAudio());
}


static void testLocks(std::unique_ptr<Frame> &&frame)
{
	int ret = 0;

	setupFrame(frame, frame->getType(), true);

	ret = frame->rdUnlock();
	CU_ASSERT_EQUAL(ret, -EALREADY);

	ret = frame->rdLock();
	CU_ASSERT_EQUAL(ret, 0);

	ret = frame->rdUnlock();
	CU_ASSERT_EQUAL(ret, 0);

	ret = frame->wrUnlock();
	CU_ASSERT_EQUAL(ret, -EALREADY);

	ret = frame->wrLock();
	CU_ASSERT_EQUAL(ret, 0);

	ret = frame->wrUnlock();
	CU_ASSERT_EQUAL(ret, 0);
}


static void testMbufFrameLocks()
{
	testLocks(testCreateCoded());
	testLocks(testCreateRaw());
	testLocks(testCreateAudio());
}


static void testGetBuffer(std::unique_ptr<Frame> &&frame, bool wr)
{
	int ret = 0;
	const void *buffer;
	void *rwBuffer;
	size_t len = 0;
	size_t bufferSize = 0;
	struct adef_frame audioFrameInfo = {};
	struct vdef_raw_frame rawFrameInfo = {};
	size_t planeSize[VDEF_RAW_MAX_PLANE_COUNT];

	setupFrame(frame, frame->getType(), false);

	switch (frame->getType()) {
	case Frame::Type::CODED_VIDEO:
		bufferSize = MBUF_TEST_SIZE;
		break;
	case Frame::Type::RAW_VIDEO:
		ret = frame->getFrameInfo(&rawFrameInfo);
		CU_ASSERT_EQUAL(ret, 0);
		ret = vdef_calc_raw_frame_size(&rawFrameInfo.format,
					       &rawFrameInfo.info.resolution,
					       NULL,
					       NULL,
					       NULL,
					       NULL,
					       planeSize,
					       NULL);
		CU_ASSERT_EQUAL(ret, 0);
		bufferSize = planeSize[0] + planeSize[1] + planeSize[2];
		break;
	case Frame::Type::AUDIO:
		ret = frame->getFrameInfo(&audioFrameInfo);
		CU_ASSERT_EQUAL(ret, 0);

		bufferSize = getFrameSize(&audioFrameInfo);
		break;
	}

	if (wr) {
		ret = frame->getRWBuffer(&rwBuffer, &len);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(len, bufferSize);

		ret = frame->getBuffer(&buffer, &len);
		CU_ASSERT_EQUAL(ret, -EBUSY);

		ret = frame->releaseRWBuffer(rwBuffer);
		CU_ASSERT_EQUAL(ret, 0);

		ret = frame->releaseRWBuffer(rwBuffer);
		CU_ASSERT_EQUAL(ret, -EALREADY);
	} else {
		ret = frame->getBuffer(&buffer, &len);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(len, bufferSize);

		ret = frame->getRWBuffer(&rwBuffer, &len);
		CU_ASSERT_EQUAL(ret, -EBUSY);

		ret = frame->releaseBuffer(buffer);
		CU_ASSERT_EQUAL(ret, 0);

		ret = frame->releaseBuffer(buffer);
		CU_ASSERT_EQUAL(ret, -EALREADY);
	}
}


static void testMbufFrameGetBuffer()
{
	testGetBuffer(testCreateCoded(), false);
	testGetBuffer(testCreateCoded(), true);
	testGetBuffer(testCreateRaw(), false);
	testGetBuffer(testCreateRaw(), true);
	testGetBuffer(testCreateAudio(), false);
	testGetBuffer(testCreateAudio(), true);
}


static void testMbufFrameSetup()
{
	std::unique_ptr<Frame> coded = testCreateCoded();
	setupFrame(coded, Frame::Type::CODED_VIDEO, true);
	setupFrame(coded, Frame::Type::RAW_VIDEO, true);
	setupFrame(coded, Frame::Type::AUDIO, true);

	std::unique_ptr<Frame> raw = testCreateRaw();
	setupFrame(raw, Frame::Type::CODED_VIDEO, true);
	setupFrame(raw, Frame::Type::RAW_VIDEO, true);
	setupFrame(raw, Frame::Type::AUDIO, true);

	std::unique_ptr<Frame> audio = testCreateAudio();
	setupFrame(audio, Frame::Type::CODED_VIDEO, true);
	setupFrame(audio, Frame::Type::RAW_VIDEO, true);
	setupFrame(audio, Frame::Type::AUDIO, true);
}


static void testMbufFrameWrap()
{
	struct mbuf_coded_video_frame *nullCoded = nullptr;
	struct mbuf_raw_video_frame *nullRaw = nullptr;
	struct mbuf_audio_frame *nullAudio = nullptr;

	std::unique_ptr<Frame> coded = testWrapCoded();
	std::unique_ptr<Frame> raw = testWrapRaw();
	std::unique_ptr<Frame> audio = testWrapAudio();

	try {
		std::unique_ptr<Frame> framePtr2 =
			Frame::wrapExisting(nullCoded, true);
		CU_ASSERT_EQUAL_FATAL(framePtr2, nullptr);
	} catch (const std::bad_alloc &) {
		CU_FAIL("bad alloc");
	}

	try {
		std::unique_ptr<Frame> framePtr2 =
			Frame::wrapExisting(nullAudio, true);
		CU_ASSERT_EQUAL_FATAL(framePtr2, nullptr);
	} catch (const std::bad_alloc &) {
		CU_FAIL("bad alloc");
	}

	try {
		std::unique_ptr<Frame> framePtr2 =
			Frame::wrapExisting(nullRaw, true);
		CU_ASSERT_EQUAL_FATAL(framePtr2, nullptr);
	} catch (const std::bad_alloc &) {
		CU_FAIL("bad alloc");
	}
}


static void testMbufFrameCreate()
{
	struct vdef_coded_frame *nullCoded = nullptr;
	struct vdef_raw_frame *nullRaw = nullptr;
	struct adef_frame *nullAudio = nullptr;

	std::unique_ptr<Frame> coded = testCreateCoded();
	std::unique_ptr<Frame> raw = testCreateRaw();
	std::unique_ptr<Frame> audio = testCreateAudio();

	try {
		std::unique_ptr<Frame> framePtr2 = Frame::create(nullCoded);
		CU_ASSERT_EQUAL_FATAL(framePtr2, nullptr);
	} catch (const std::bad_alloc &) {
		CU_FAIL("bad alloc");
	}

	try {
		std::unique_ptr<Frame> framePtr2 = Frame::create(nullAudio);
		CU_ASSERT_EQUAL_FATAL(framePtr2, nullptr);
	} catch (const std::bad_alloc &) {
		CU_FAIL("bad alloc");
	}

	try {
		std::unique_ptr<Frame> framePtr2 = Frame::create(nullRaw);
		CU_ASSERT_EQUAL_FATAL(framePtr2, nullptr);
	} catch (const std::bad_alloc &) {
		CU_FAIL("bad alloc");
	}
}


static void testMbufFrameCreateInvalidFormat()
{
	/* Zero-initialized formats are rejected by
	 * vdef_is_coded_format_valid(), vdef_is_raw_format_valid() and
	 * adef_is_format_valid(): the native frame creation must fail, and
	 * Frame::create() must report it by returning nullptr rather than a
	 * non-functional wrapper. */
	struct vdef_coded_frame invalidCoded = {};
	struct vdef_raw_frame invalidRaw = {};
	struct adef_frame invalidAudio = {};

	try {
		std::unique_ptr<Frame> frame = Frame::create(&invalidCoded);
		CU_ASSERT_EQUAL_FATAL(frame, nullptr);
	} catch (const std::bad_alloc &) {
		CU_FAIL("bad alloc");
	}

	try {
		std::unique_ptr<Frame> frame = Frame::create(&invalidRaw);
		CU_ASSERT_EQUAL_FATAL(frame, nullptr);
	} catch (const std::bad_alloc &) {
		CU_FAIL("bad alloc");
	}

	try {
		std::unique_ptr<Frame> frame = Frame::create(&invalidAudio);
		CU_ASSERT_EQUAL_FATAL(frame, nullptr);
	} catch (const std::bad_alloc &) {
		CU_FAIL("bad alloc");
	}
}


CU_TestInfo g_mbuf_test_frame_cpp[] = {
	{(char *)"frame_create", &testMbufFrameCreate},
	{(char *)"frame_create_invalid_format",
	 &testMbufFrameCreateInvalidFormat},
	{(char *)"frame_wrap", &testMbufFrameWrap},
	{(char *)"frame_setup", &testMbufFrameSetup},
	{(char *)"frame_get_buffer", &testMbufFrameGetBuffer},
	{(char *)"frame_locks", &testMbufFrameLocks},
	{(char *)"frame_add_ancillary", &testMbufFrameAddAncillary},
	{(char *)"frame_metadata", &testMbufFrameMetadata},
	CU_TEST_INFO_NULL,
};
