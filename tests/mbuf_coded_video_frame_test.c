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

#define MBUF_TEST_WIDTH 4
#define MBUF_TEST_HEIGHT 4
#define MBUF_TEST_SIZE 128


/* Create a mbuf pool suitable for all tests */
static struct mbuf_pool *create_pool()
{
	struct mbuf_pool *pool;
	int ret = mbuf_pool_new(mbuf_mem_generic_impl,
				MBUF_TEST_SIZE * 10,
				0,
				MBUF_POOL_LOW_MEM_GROW,
				0,
				"coded",
				&pool);
	if (ret != 0)
		return NULL;
	return pool;
}


/* Add a nalu to a buffer, and set its content to value */
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
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT(cap >= (MBUF_TEST_SIZE + offset));
	data = coded_data;

	if (ret != 0 || cap < (MBUF_TEST_SIZE + offset))
		return;

	memset(data + offset, value, MBUF_TEST_SIZE);
	struct vdef_nalu nalu = {
		.size = MBUF_TEST_SIZE,
		.importance = importance,
		.h264.type = type,
		.h264.slice_type = slice_type,
	};
	ret = mbuf_coded_video_frame_add_nalu(frame, mem, offset, &nalu);
	CU_ASSERT_EQUAL(ret, 0);
}


static void add_default_nalu(struct mbuf_coded_video_frame *frame)
{
	struct mbuf_mem *mem;
	int ret = mbuf_mem_generic_new(MBUF_TEST_SIZE, &mem);
	CU_ASSERT_EQUAL(ret, 0);
	add_nalu(frame,
		 mem,
		 0,
		 H264_NALU_TYPE_SPS,
		 H264_SLICE_TYPE_UNKNOWN,
		 1,
		 0);
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);
}


/* Insert an already filled nalu to a buffer at given index */
static void insert_nalu(struct mbuf_coded_video_frame *frame,
			struct mbuf_mem *mem,
			size_t offset,
			enum h264_nalu_type type,
			enum h264_slice_type slice_type,
			uint32_t index,
			int importance)
{
	int ret;
	struct vdef_nalu nalu = {
		.size = MBUF_TEST_SIZE,
		.importance = importance,
		.h264.type = type,
		.h264.slice_type = slice_type,
	};
	ret = mbuf_coded_video_frame_insert_nalu(
		frame, mem, offset, &nalu, index);
	CU_ASSERT_EQUAL(ret, 0);
}


/* Check if a nalu exists with the right parameters, and the proper content */
static void check_nalu(struct mbuf_coded_video_frame *frame,
		       unsigned int index,
		       enum h264_nalu_type type,
		       enum h264_slice_type slice_type,
		       int value,
		       int importance)
{
	int ret;
	const void *nalu_data;
	struct vdef_nalu nalu;
	const uint8_t *data;

	ret = mbuf_coded_video_frame_get_nalu(frame, index, &nalu_data, &nalu);
	CU_ASSERT_EQUAL(ret, 0);
	if (ret != 0)
		return;

	CU_ASSERT_EQUAL(nalu.size, MBUF_TEST_SIZE);
	CU_ASSERT_EQUAL(nalu.importance, importance);
	data = nalu_data;
	for (size_t i = 0; i < nalu.size; i++) {
		if (data[i] != value) {
			CU_FAIL("Bad content for nalu");
			break;
		}
	}
	CU_ASSERT_EQUAL(nalu.h264.type, type);
	CU_ASSERT_EQUAL(nalu.h264.slice_type, slice_type);

	ret = mbuf_coded_video_frame_release_nalu(frame, index, nalu_data);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_coded_video_frame_scattered(void)
{
	struct mbuf_mem *mem1, *mem2, *mem3, *mempack;
	struct vdef_coded_frame frame_info = {
		.format = vdef_h264_byte_stream,
		.info.resolution.width = MBUF_TEST_WIDTH,
		.info.resolution.height = MBUF_TEST_HEIGHT,
	};
	size_t required_len;
	const void *data;
	struct mbuf_coded_video_frame *frame, *packed;

	/* Create the pool, frame, and memories used by the test */
	struct mbuf_pool *pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);
	int ret = mbuf_coded_video_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem3);
	CU_ASSERT_EQUAL(ret, 0);

	/* Add the nalus to the frame */
	add_nalu(frame,
		 mem1,
		 0,
		 H264_NALU_TYPE_SPS,
		 H264_SLICE_TYPE_UNKNOWN,
		 42,
		 6);
	add_nalu(frame,
		 mem2,
		 0,
		 H264_NALU_TYPE_PPS,
		 H264_SLICE_TYPE_UNKNOWN,
		 43,
		 5);
	add_nalu(frame,
		 mem3,
		 0,
		 H264_NALU_TYPE_SLICE_IDR,
		 H264_SLICE_TYPE_I,
		 44,
		 4);

	/* Finalize the frame */
	ret = mbuf_coded_video_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Check frame content via the nalu getters */
	check_nalu(
		frame, 0, H264_NALU_TYPE_SPS, H264_SLICE_TYPE_UNKNOWN, 42, 6);
	check_nalu(
		frame, 1, H264_NALU_TYPE_PPS, H264_SLICE_TYPE_UNKNOWN, 43, 5);
	check_nalu(
		frame, 2, H264_NALU_TYPE_SLICE_IDR, H264_SLICE_TYPE_I, 44, 4);

	/* Check that the frame is not packed, as it uses scattered buffers */
	ret = mbuf_coded_video_frame_get_packed_buffer(
		frame, &data, &required_len);
	CU_ASSERT_EQUAL(ret, -EPROTO);
	CU_ASSERT_EQUAL(required_len, MBUF_TEST_SIZE * 3);

	/* Check that the required_len returned by get_packed_buffer is the same
	 * as the one returned by mbuf_coded_video_frame_get_packed_size() */
	ssize_t packed_len = mbuf_coded_video_frame_get_packed_size(frame);
	CU_ASSERT(packed_len > 0);
	CU_ASSERT_EQUAL(packed_len, required_len);

	/* Get a memory for the copied frame */
	ret = mbuf_pool_get(pool, &mempack);
	CU_ASSERT_EQUAL(ret, 0);

	/* Copy the frame into the destination memory, finalize the frame and
	 * unref the memory */
	ret = mbuf_coded_video_frame_copy(frame, mempack, &packed);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_finalize(packed);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mempack);
	CU_ASSERT_EQUAL(ret, 0);

	/* Copied frame should still have the good nalu contents */
	check_nalu(
		packed, 0, H264_NALU_TYPE_SPS, H264_SLICE_TYPE_UNKNOWN, 42, 6);
	check_nalu(
		packed, 1, H264_NALU_TYPE_PPS, H264_SLICE_TYPE_UNKNOWN, 43, 5);
	check_nalu(
		packed, 2, H264_NALU_TYPE_SLICE_IDR, H264_SLICE_TYPE_I, 44, 4);

	/* Copied frame is packed, and thus we can get its packed_buffer */
	ret = mbuf_coded_video_frame_get_packed_buffer(
		packed, &data, &required_len);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_release_packed_buffer(packed, data);
	CU_ASSERT_EQUAL(ret, 0);

	/* Unref the initial frame & recreate it with arbitrary order */
	ret = mbuf_coded_video_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	insert_nalu(frame,
		    mem3,
		    0,
		    H264_NALU_TYPE_SLICE_IDR,
		    H264_SLICE_TYPE_I,
		    UINT32_MAX,
		    7);
	insert_nalu(frame,
		    mem1,
		    0,
		    H264_NALU_TYPE_SPS,
		    H264_SLICE_TYPE_UNKNOWN,
		    0,
		    8);
	insert_nalu(frame,
		    mem2,
		    0,
		    H264_NALU_TYPE_PPS,
		    H264_SLICE_TYPE_UNKNOWN,
		    1,
		    9);

	/* Finalize the frame */
	ret = mbuf_coded_video_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Check frame content via the nalu getters */
	check_nalu(
		frame, 0, H264_NALU_TYPE_SPS, H264_SLICE_TYPE_UNKNOWN, 42, 8);
	check_nalu(
		frame, 1, H264_NALU_TYPE_PPS, H264_SLICE_TYPE_UNKNOWN, 43, 9);
	check_nalu(
		frame, 2, H264_NALU_TYPE_SLICE_IDR, H264_SLICE_TYPE_I, 44, 7);

	/* Cleanup */
	ret = mbuf_mem_unref(mem1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem3);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_unref(packed);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_coded_video_frame_single(void)
{
	struct mbuf_mem *mem;
	struct vdef_coded_frame frame_info = {
		.format = vdef_h264_byte_stream,
		.info.resolution.width = MBUF_TEST_WIDTH,
		.info.resolution.height = MBUF_TEST_HEIGHT,
	};
	struct mbuf_coded_video_frame *frame;
	const void *data;
	size_t len;

	/* Create the pool, frame, and memories used by the test */
	struct mbuf_pool *pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);
	int ret = mbuf_coded_video_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem);
	CU_ASSERT_EQUAL(ret, 0);

	/* Add the nalus to the frame */
	add_nalu(frame,
		 mem,
		 0,
		 H264_NALU_TYPE_SPS,
		 H264_SLICE_TYPE_UNKNOWN,
		 42,
		 1);
	add_nalu(frame,
		 mem,
		 MBUF_TEST_SIZE,
		 H264_NALU_TYPE_PPS,
		 H264_SLICE_TYPE_UNKNOWN,
		 43,
		 2);
	add_nalu(frame,
		 mem,
		 2 * MBUF_TEST_SIZE,
		 H264_NALU_TYPE_SLICE_IDR,
		 H264_SLICE_TYPE_I,
		 44,
		 3);

	/* Unref the memories which are no longer used */
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);

	/* Finalize the frame */
	ret = mbuf_coded_video_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Check frame content via the nalu getters */
	check_nalu(
		frame, 0, H264_NALU_TYPE_SPS, H264_SLICE_TYPE_UNKNOWN, 42, 1);
	check_nalu(
		frame, 1, H264_NALU_TYPE_PPS, H264_SLICE_TYPE_UNKNOWN, 43, 2);
	check_nalu(
		frame, 2, H264_NALU_TYPE_SLICE_IDR, H264_SLICE_TYPE_I, 44, 3);

	/* Check that the frame is packed, as it uses a single buffer with no
	 * gaps */
	ret = mbuf_coded_video_frame_get_packed_buffer(frame, &data, &len);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_release_packed_buffer(frame, data);
	CU_ASSERT_EQUAL(ret, 0);

	/* Cleanup */
	ret = mbuf_coded_video_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_coded_video_frame_infos(void)
{
	int ret;
	struct mbuf_coded_video_frame *frame;
	struct vdef_coded_frame frame_info = {
		.format = vdef_h264_byte_stream,
		.info.resolution.width = MBUF_TEST_WIDTH,
		.info.resolution.height = MBUF_TEST_HEIGHT,
	};
	struct vdef_coded_frame out_frame_info;
	struct mbuf_mem_info mem_info;

	/* Create & finalize the frame used by the test */
	ret = mbuf_coded_video_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	add_default_nalu(frame);
	ret = mbuf_coded_video_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Get back and compare the frame_info */
	ret = mbuf_coded_video_frame_get_frame_info(frame, &out_frame_info);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(
		memcmp(&frame_info, &out_frame_info, sizeof(frame_info)), 0);

	/* Get the memory info for the first nalu */
	ret = mbuf_coded_video_frame_get_nalu_mem_info(frame, 0, &mem_info);
	CU_ASSERT_EQUAL(ret, 0);
	/* The memory must be of type "generic-wrap" */
	CU_ASSERT_EQUAL(mem_info.cookie, mbuf_mem_generic_wrap_cookie);

	/* Cleanup */
	ret = mbuf_coded_video_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_coded_video_frame_pool_origin(void)
{
	int ret;
	struct mbuf_pool *pool;
	struct mbuf_mem *mem;
	struct mbuf_coded_video_frame *frame1, *frame2, *frame3;
	struct vdef_coded_frame frame_info = {
		.format = vdef_h264_byte_stream,
		.info.resolution.width = MBUF_TEST_WIDTH,
		.info.resolution.height = MBUF_TEST_HEIGHT,
	};
	bool any, all;

	/* Create the pools/mem/frames */
	pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);
	ret = mbuf_coded_video_frame_new(&frame_info, &frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_new(&frame_info, &frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_new(&frame_info, &frame3);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem);
	CU_ASSERT_EQUAL(ret, 0);

	/* Fill & finalize the frames:
	 * frame1 will not use any memory from the pool
	 * frame2 will use one from the pool, and one other
	 * frame3 will only use memory from the pool
	 */
	add_default_nalu(frame1);
	ret = mbuf_coded_video_frame_finalize(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	add_nalu(frame2,
		 mem,
		 0,
		 H264_NALU_TYPE_PPS,
		 H264_SLICE_TYPE_UNKNOWN,
		 1,
		 0);
	add_default_nalu(frame2);
	ret = mbuf_coded_video_frame_finalize(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	add_nalu(frame3,
		 mem,
		 0,
		 H264_NALU_TYPE_PPS,
		 H264_SLICE_TYPE_UNKNOWN,
		 1,
		 0);
	ret = mbuf_coded_video_frame_finalize(frame3);
	CU_ASSERT_EQUAL(ret, 0);

	/* Check pool origin for each frame */
	ret = mbuf_coded_video_frame_uses_mem_from_pool(
		frame1, pool, &any, &all);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(any);
	CU_ASSERT_FALSE(all);
	ret = mbuf_coded_video_frame_uses_mem_from_pool(
		frame2, pool, &any, &all);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_TRUE(any);
	CU_ASSERT_FALSE(all);
	ret = mbuf_coded_video_frame_uses_mem_from_pool(
		frame3, pool, &any, &all);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_TRUE(any);
	CU_ASSERT_TRUE(all);


	/* Cleanup */
	ret = mbuf_coded_video_frame_unref(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_unref(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_unref(frame3);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_coded_video_frame_bad_args(void)
{
	int ret;
	struct mbuf_mem *mem, *mem_cp;
	struct mbuf_coded_video_frame *frame, *frame_cp;
	struct mbuf_coded_video_frame_queue *queue;
	struct vdef_coded_frame frame_info = {
		.format = vdef_h264_byte_stream,
		.info.resolution.width = MBUF_TEST_WIDTH,
		.info.resolution.height = MBUF_TEST_HEIGHT,
	};
	struct pomp_evt *evt;
	size_t len;
	const void *data, *tmp;
	void *rwdata;
	struct vdef_nalu nalu = {
		.size = MBUF_TEST_SIZE,
		.importance = 0,
		.h264.type = H264_NALU_TYPE_SLICE_IDR,
		.h264.slice_type = H264_SLICE_TYPE_I,
	};
	struct vmeta_frame *meta, *out_meta;
	bool any, all;
	struct mbuf_mem_info mem_info;

	/* Create a pool for the test */
	struct mbuf_pool *pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);

	/* Bad arguments for constructors */
	ret = mbuf_coded_video_frame_new(NULL, &frame);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_new(&frame_info, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_queue_new(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_queue_new_with_args(NULL, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* Get real objects for following tests */
	ret = mbuf_coded_video_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem_cp);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_new(&queue);
	CU_ASSERT_EQUAL(ret, 0);
	ret = vmeta_frame_new(VMETA_FRAME_TYPE_PROTO, &meta);
	CU_ASSERT_EQUAL(ret, 0);

	/* Add 1 nalu, finalize frame & get a nalu for reader API tests */
	add_nalu(frame,
		 mem,
		 0,
		 H264_NALU_TYPE_SLICE_IDR,
		 H264_SLICE_TYPE_I,
		 42,
		 0);
	ret = mbuf_coded_video_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_get_nalu(frame, 0, &tmp, &nalu);
	CU_ASSERT_EQUAL(ret, 0);

	/* Bad arguments for other functions */
	ret = mbuf_coded_video_frame_ref(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_unref(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_set_frame_info(NULL, &frame_info);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_set_frame_info(frame, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_set_metadata(NULL, meta);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_set_metadata(frame, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_add_nalu(NULL, mem, 0, &nalu);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_add_nalu(frame, NULL, 0, &nalu);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_add_nalu(frame, mem, 0, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_finalize(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_uses_mem_from_pool(NULL, pool, &any, &all);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_uses_mem_from_pool(
		frame, NULL, &any, &all);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_metadata(NULL, &out_meta);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_metadata(frame, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_nalu_mem_info(NULL, 0, &mem_info);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_nalu_mem_info(frame, 200, &mem_info);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_nalu_mem_info(frame, 0, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_nalu(NULL, 0, &data, &nalu);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_nalu(frame, 200, &data, &nalu);
	CU_ASSERT_EQUAL(ret, -ENOENT);
	ret = mbuf_coded_video_frame_get_nalu(frame, 0, NULL, &nalu);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_nalu(frame, 0, &data, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_release_nalu(NULL, 0, data);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_release_nalu(frame, 200, data);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_rw_nalu(NULL, 0, &rwdata, &nalu);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_rw_nalu(frame, 0, NULL, &nalu);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_rw_nalu(frame, 200, &rwdata, &nalu);
	CU_ASSERT_EQUAL(ret, -ENOENT);
	ret = mbuf_coded_video_frame_get_rw_nalu(frame, 0, &rwdata, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	rwdata = (void *)(intptr_t)1; /* Make sure rwdata is non-null */
	ret = mbuf_coded_video_frame_release_rw_nalu(NULL, 0, rwdata);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_release_rw_nalu(frame, 200, rwdata);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_release_rw_nalu(frame, 0, rwdata);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_packed_buffer(NULL, &data, &len);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_packed_buffer(frame, NULL, &len);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_packed_buffer(frame, &data, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_release_packed_buffer(NULL, data);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_release_packed_buffer(frame, data);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_rw_packed_buffer(NULL, &rwdata, &len);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_rw_packed_buffer(frame, NULL, &len);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_rw_packed_buffer(frame, &rwdata, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_release_rw_packed_buffer(NULL, rwdata);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_release_rw_packed_buffer(frame, rwdata);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ssize_t sret = mbuf_coded_video_frame_get_packed_size(NULL);
	CU_ASSERT_EQUAL(sret, -EINVAL);
	ret = mbuf_coded_video_frame_copy(NULL, mem_cp, &frame_cp);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_copy(frame, NULL, &frame_cp);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_copy(frame, mem_cp, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_frame_info(NULL, &frame_info);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_get_frame_info(frame, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_queue_push(NULL, frame);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_queue_push(queue, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_queue_peek(NULL, &frame_cp);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_queue_peek(queue, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_queue_peek_at(NULL, 0, &frame_cp);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_queue_peek_at(queue, 0, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_queue_pop(NULL, &frame_cp);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_queue_pop(queue, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_queue_flush(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_queue_get_event(NULL, &evt);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_queue_get_event(queue, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_queue_get_count(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_coded_video_frame_queue_destroy(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* Cleanup */
	ret = vmeta_frame_unref(meta);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_destroy(queue);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem_cp);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_release_nalu(frame, 0, tmp);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_coded_video_frame_bad_state(void)
{
	int ret;
	struct mbuf_mem *mem;
	struct mbuf_coded_video_frame *frame;
	struct vdef_coded_frame frame_info_byte_stream = {
		.format = vdef_h264_byte_stream,
		.info.resolution.width = MBUF_TEST_WIDTH,
		.info.resolution.height = MBUF_TEST_HEIGHT,
	};
	struct vdef_coded_frame frame_info_avcc = {
		.format = vdef_h264_avcc,
		.info.resolution.width = MBUF_TEST_WIDTH,
		.info.resolution.height = MBUF_TEST_HEIGHT,
	};
	struct vdef_coded_frame frame_info_out;
	const void *data;
	void *rwdata;
	struct vdef_nalu nalu = {
		.size = MBUF_TEST_SIZE,
		.importance = 0,
		.h264.type = H264_NALU_TYPE_SLICE_IDR,
		.h264.slice_type = H264_SLICE_TYPE_I,
	};

	/* Create the pool, frame, and memories used by the test */
	struct mbuf_pool *pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);
	ret = mbuf_coded_video_frame_new(&frame_info_avcc, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem);
	CU_ASSERT_EQUAL(ret, 0);

	/* Check that we can properly get/set the frame_info */
	ret = mbuf_coded_video_frame_get_frame_info(frame, &frame_info_out);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(vdef_coded_format_cmp(&frame_info_out.format,
					      &frame_info_avcc.format),
			1);
	ret = mbuf_coded_video_frame_set_frame_info(frame,
						    &frame_info_byte_stream);
	ret = mbuf_coded_video_frame_get_frame_info(frame, &frame_info_out);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(vdef_coded_format_cmp(&frame_info_out.format,
					      &frame_info_byte_stream.format),
			1);

	/* We should not be able to finalize a frame without nalus */
	ret = mbuf_coded_video_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, -EPROTO);

	/* Add the nalus to the frame */
	add_nalu(frame,
		 mem,
		 0,
		 H264_NALU_TYPE_SPS,
		 H264_SLICE_TYPE_UNKNOWN,
		 42,
		 0);
	add_nalu(frame,
		 mem,
		 MBUF_TEST_SIZE,
		 H264_NALU_TYPE_PPS,
		 H264_SLICE_TYPE_UNKNOWN,
		 43,
		 0);
	add_nalu(frame,
		 mem,
		 2 * MBUF_TEST_SIZE,
		 H264_NALU_TYPE_SLICE_IDR,
		 H264_SLICE_TYPE_I,
		 44,
		 0);

	/* Buffer is not finalized yet, getters should fail */
	ret = mbuf_coded_video_frame_get_nalu(frame, 0, &data, &nalu);
	CU_ASSERT_EQUAL(ret, -EBUSY);
	ret = mbuf_coded_video_frame_get_nalu(frame, 1, &data, &nalu);
	CU_ASSERT_EQUAL(ret, -EBUSY);
	ret = mbuf_coded_video_frame_get_nalu(frame, 2, &data, &nalu);
	CU_ASSERT_EQUAL(ret, -EBUSY);

	/* Finalize now */
	ret = mbuf_coded_video_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Buffer is now read-only, adding a nalu should fail */
	ret = mbuf_coded_video_frame_add_nalu(frame, mem, 0, &nalu);
	CU_ASSERT_EQUAL(ret, -EBUSY);
	/* Setting the frame_info should fail too */
	ret = mbuf_coded_video_frame_set_frame_info(frame, &frame_info_avcc);
	CU_ASSERT_EQUAL(ret, -EBUSY);

	/* Take a read-only reference on a nalu */
	ret = mbuf_coded_video_frame_get_nalu(frame, 0, &data, &nalu);
	CU_ASSERT_EQUAL(ret, 0);
	/* Try to take a read-write reference on a nalu.
	 * Frame should be read-locked */
	ret = mbuf_coded_video_frame_get_rw_nalu(frame, 0, &rwdata, &nalu);
	CU_ASSERT_EQUAL(ret, -EBUSY);

	/* Release the nalu */
	ret = mbuf_coded_video_frame_release_nalu(frame, 0, data);
	CU_ASSERT_EQUAL(ret, 0);
	/* Double release of a nalu should fail */
	ret = mbuf_coded_video_frame_release_nalu(frame, 0, data);
	CU_ASSERT_EQUAL(ret, -EALREADY);

	/* Take a read-write reference on a nalu */
	ret = mbuf_coded_video_frame_get_rw_nalu(frame, 0, &rwdata, &nalu);
	CU_ASSERT_EQUAL(ret, 0);
	/* Try to take a read-only reference on a nalu.
	 * Frame should be write-locked */
	ret = mbuf_coded_video_frame_get_nalu(frame, 0, &data, &nalu);
	CU_ASSERT_EQUAL(ret, -EBUSY);

	/* Release the nalu */
	ret = mbuf_coded_video_frame_release_rw_nalu(frame, 0, rwdata);
	CU_ASSERT_EQUAL(ret, 0);
	/* Double release of a nalu should fail */
	ret = mbuf_coded_video_frame_release_rw_nalu(frame, 0, rwdata);
	CU_ASSERT_EQUAL(ret, -EALREADY);

	/* Cleanup */
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_coded_video_frame_queue(void)
{
	int ret;
	struct vdef_coded_frame frame_info = {
		.format = vdef_h264_byte_stream,
		.info.resolution.width = MBUF_TEST_WIDTH,
		.info.resolution.height = MBUF_TEST_HEIGHT,
	};
	struct mbuf_coded_video_frame *frame1, *frame2, *frame3, *out_frame;
	struct mbuf_coded_video_frame_queue *queue;

	/* Create the frames and the queue used by the test */
	ret = mbuf_coded_video_frame_new(&frame_info, &frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_new(&frame_info, &frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_new(&frame_info, &frame3);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_new(&queue);
	CU_ASSERT_EQUAL(ret, 0);

	/* Peek / Pop from empty queue should fail */
	ret = mbuf_coded_video_frame_queue_peek(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);
	ret = mbuf_coded_video_frame_queue_peek_at(queue, 0, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);
	ret = mbuf_coded_video_frame_queue_pop(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);
	ret = mbuf_coded_video_frame_queue_get_count(queue);
	CU_ASSERT_EQUAL(ret, 0);

	/* Pushing a non-finalized frame should fail */
	ret = mbuf_coded_video_frame_queue_push(queue, frame1);
	CU_ASSERT_EQUAL(ret, -EBUSY);

	/* Finalize frames for the test */
	add_default_nalu(frame1);
	ret = mbuf_coded_video_frame_finalize(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	add_default_nalu(frame2);
	ret = mbuf_coded_video_frame_finalize(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	add_default_nalu(frame3);
	ret = mbuf_coded_video_frame_finalize(frame3);
	CU_ASSERT_EQUAL(ret, 0);

	/* Push three frames in order */
	ret = mbuf_coded_video_frame_queue_push(queue, frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_push(queue, frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_push(queue, frame3);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_get_count(queue);
	CU_ASSERT_EQUAL(ret, 3);

	/* Peek and compare to frame 1 */
	ret = mbuf_coded_video_frame_queue_peek(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(out_frame, frame1);
	ret = mbuf_coded_video_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Peek at index 0 and compare to frame 1 */
	ret = mbuf_coded_video_frame_queue_peek_at(queue, 0, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(out_frame, frame1);
	ret = mbuf_coded_video_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Peek at index 1 and compare to frame 2 */
	ret = mbuf_coded_video_frame_queue_peek_at(queue, 1, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(out_frame, frame2);
	ret = mbuf_coded_video_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Pop and compare to frame 1 */
	ret = mbuf_coded_video_frame_queue_pop(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(out_frame, frame1);
	ret = mbuf_coded_video_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_get_count(queue);
	CU_ASSERT_EQUAL(ret, 2);

	/* Peek and compare to frame 2 */
	ret = mbuf_coded_video_frame_queue_peek(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(out_frame, frame2);
	ret = mbuf_coded_video_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_get_count(queue);
	CU_ASSERT_EQUAL(ret, 2);

	/* Peek at index 0 and compare to frame 2 */
	ret = mbuf_coded_video_frame_queue_peek_at(queue, 0, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(out_frame, frame2);
	ret = mbuf_coded_video_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Peek at index 1 and compare to frame 3 */
	ret = mbuf_coded_video_frame_queue_peek_at(queue, 1, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(out_frame, frame3);
	ret = mbuf_coded_video_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Flush */
	ret = mbuf_coded_video_frame_queue_flush(queue);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_get_count(queue);
	CU_ASSERT_EQUAL(ret, 0);

	/* Peek / Pop from flushed queue should fail */
	ret = mbuf_coded_video_frame_queue_peek(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);
	ret = mbuf_coded_video_frame_queue_peek_at(queue, 0, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);
	ret = mbuf_coded_video_frame_queue_pop(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);

	/* Cleanup */
	ret = mbuf_coded_video_frame_unref(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_unref(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_unref(frame3);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_destroy(queue);
	CU_ASSERT_EQUAL(ret, 0);
}


struct test_mbuf_coded_video_frame_queue_flush_userdata {
	bool freed;
};


static void test_mbuf_coded_video_frame_queue_flush_free(void *data,
							 size_t len,
							 void *userdata)
{
	struct test_mbuf_coded_video_frame_queue_flush_userdata *ud = userdata;
	free(data);
	ud->freed = true;
}


static void test_mbuf_coded_video_frame_queue_flush(void)
{
	int ret;
	struct vdef_coded_frame frame_info = {
		.format = vdef_h264_byte_stream,
		.info.resolution.width = MBUF_TEST_WIDTH,
		.info.resolution.height = MBUF_TEST_HEIGHT,
	};
	struct mbuf_mem *mem;
	struct mbuf_coded_video_frame *frame;
	struct mbuf_coded_video_frame_queue *queue;
	struct test_mbuf_coded_video_frame_queue_flush_userdata ud = {
		.freed = false,
	};
	struct vdef_nalu nalu = {
		.size = MBUF_TEST_SIZE,
		.importance = 0,
		.h264.type = H264_NALU_TYPE_SPS,
		.h264.slice_type = H264_SLICE_TYPE_UNKNOWN,
	};
	void *data = malloc(nalu.size);
	CU_ASSERT_PTR_NOT_NULL(data);

	/* Create the frame and the queue used by the test */
	ret = mbuf_coded_video_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_generic_wrap(
		data,
		nalu.size,
		test_mbuf_coded_video_frame_queue_flush_free,
		&ud,
		&mem);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_add_nalu(frame, mem, 0, &nalu);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_new(&queue);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);

	/* Push the frame */
	ret = mbuf_coded_video_frame_queue_push(queue, frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Unref the frames */
	ret = mbuf_coded_video_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(ud.freed);

	/* Flush the queue */
	ret = mbuf_coded_video_frame_queue_flush(queue);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_TRUE(ud.freed);

	/* Cleanup */
	ret = mbuf_coded_video_frame_queue_destroy(queue);
	CU_ASSERT_EQUAL(ret, 0);
}


/* Userdata for the pomp_evt callback */
struct coded_queue_evt_userdata {
	struct mbuf_coded_video_frame_queue *queue;
	int expected_frames;
};


/* pomp_evt callback for the queue, this function pops all frames in the queue
 * and decrement userdata->expected_frames for each frame. */
static void coded_queue_evt(struct pomp_evt *evt, void *userdata)
{
	int ret = 0;
	struct coded_queue_evt_userdata *data = userdata;

	while (true) {
		struct mbuf_coded_video_frame *frame;

		ret = mbuf_coded_video_frame_queue_pop(data->queue, &frame);
		if (ret == -EAGAIN)
			break;
		CU_ASSERT_EQUAL(ret, 0);
		ret = mbuf_coded_video_frame_unref(frame);
		CU_ASSERT_EQUAL(ret, 0);
		data->expected_frames--;
	}
}


static void test_mbuf_coded_video_frame_queue_evt(void)
{
	int ret;
	struct vdef_coded_frame frame_info = {
		.format = vdef_h264_byte_stream,
		.info.resolution.width = MBUF_TEST_WIDTH,
		.info.resolution.height = MBUF_TEST_HEIGHT,
	};
	struct mbuf_coded_video_frame *frame1, *frame2;
	struct mbuf_coded_video_frame_queue *queue;
	struct pomp_evt *evt;
	struct pomp_loop *loop;
	struct coded_queue_evt_userdata userdata;

	/* Create the frames and the queue used by the test */
	ret = mbuf_coded_video_frame_new(&frame_info, &frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_new(&frame_info, &frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_new(&queue);
	CU_ASSERT_EQUAL(ret, 0);
	userdata.queue = queue;

	/* Finalize the frames */
	add_default_nalu(frame1);
	ret = mbuf_coded_video_frame_finalize(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	add_default_nalu(frame2);
	ret = mbuf_coded_video_frame_finalize(frame2);
	CU_ASSERT_EQUAL(ret, 0);

	/* Create a loop, get the queue event, and attach it to the loop */
	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL(loop);
	ret = mbuf_coded_video_frame_queue_get_event(queue, &evt);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_evt_attach_to_loop(evt, loop, coded_queue_evt, &userdata);

	/* Push two frames in order */
	ret = mbuf_coded_video_frame_queue_push(queue, frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_push(queue, frame2);
	CU_ASSERT_EQUAL(ret, 0);

	/* Run the loop and check that we got two frames */
	userdata.expected_frames = 2;
	ret = pomp_loop_wait_and_process(loop, 100);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(userdata.expected_frames, 0);

	/* Rerun the loop but expect a timeout */
	ret = pomp_loop_wait_and_process(loop, 100);
	CU_ASSERT_EQUAL(ret, -ETIMEDOUT);

	/* Push one frame, and flush */
	ret = mbuf_coded_video_frame_queue_push(queue, frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_flush(queue);
	CU_ASSERT_EQUAL(ret, 0);

	/* Rerun the loop and expect a timeout */
	ret = pomp_loop_wait_and_process(loop, 100);
	CU_ASSERT_EQUAL(ret, -ETIMEDOUT);

	/* Cleanup */
	ret = pomp_evt_detach_from_loop(evt, loop);
	CU_ASSERT_EQUAL(ret, 0);
	ret = pomp_loop_destroy(loop);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_unref(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_unref(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_destroy(queue);
	CU_ASSERT_EQUAL(ret, 0);
}


/* queue filter function which refuses all frames */
static bool queue_filter_none(struct mbuf_coded_video_frame *frame,
			      void *userdata)
{
	return false;
}


/* queue filter function which accepts all frames, equivalent to no filter
 * function */
static bool queue_filter_all(struct mbuf_coded_video_frame *frame,
			     void *userdata)
{
	return true;
}


/* queue filter function which only accepts frames whose nalu count is 1 */
static bool queue_filter_has_single_nalu(struct mbuf_coded_video_frame *frame,
					 void *userdata)
{
	int ret = mbuf_coded_video_frame_get_nalu_count(frame);
	if (ret < 0)
		CU_FAIL("get_nalu_count failed");
	return (ret == 1);
}


static void test_mbuf_coded_video_frame_queue_filter(void)
{
	int ret;
	struct vdef_coded_frame frame_info = {
		.format = vdef_h264_byte_stream,
		.info.resolution.width = MBUF_TEST_WIDTH,
		.info.resolution.height = MBUF_TEST_HEIGHT,
	};
	struct mbuf_mem *mem;
	struct mbuf_coded_video_frame *frame1, *frame2;
	struct mbuf_coded_video_frame_queue *queue_none, *queue_all,
		*queue_single_nalu;

	/* Create the pool, frames and memory used by the test */
	struct mbuf_pool *pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);
	ret = mbuf_coded_video_frame_new(&frame_info, &frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_new(&frame_info, &frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem);
	CU_ASSERT_EQUAL(ret, 0);

	/* Add one nalu to the first frame, and two to the second */
	add_default_nalu(frame1);
	add_default_nalu(frame2);
	add_default_nalu(frame2);

	/* Finalize both frames */
	ret = mbuf_coded_video_frame_finalize(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_finalize(frame2);
	CU_ASSERT_EQUAL(ret, 0);

	/* Create the three test queues, one with each filter function */
	struct mbuf_coded_video_frame_queue_args args = {
		.filter = queue_filter_none,
	};
	ret = mbuf_coded_video_frame_queue_new_with_args(&args, &queue_none);
	CU_ASSERT_EQUAL(ret, 0);
	args.filter = queue_filter_all;
	ret = mbuf_coded_video_frame_queue_new_with_args(&args, &queue_all);
	CU_ASSERT_EQUAL(ret, 0);
	args.filter = queue_filter_has_single_nalu;
	ret = mbuf_coded_video_frame_queue_new_with_args(&args,
							 &queue_single_nalu);
	CU_ASSERT_EQUAL(ret, 0);

	/* Push to queue_none should fail */
	ret = mbuf_coded_video_frame_queue_push(queue_none, frame1);
	CU_ASSERT_EQUAL(ret, -EPROTO);
	ret = mbuf_coded_video_frame_queue_push(queue_none, frame2);
	CU_ASSERT_EQUAL(ret, -EPROTO);
	ret = mbuf_coded_video_frame_queue_get_count(queue_none);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_flush(queue_none);
	CU_ASSERT_EQUAL(ret, 0);

	/* Pushing to queue_all should be OK */
	ret = mbuf_coded_video_frame_queue_push(queue_all, frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_push(queue_all, frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_get_count(queue_all);
	CU_ASSERT_EQUAL(ret, 2);
	ret = mbuf_coded_video_frame_queue_flush(queue_all);
	CU_ASSERT_EQUAL(ret, 0);

	/* Pushing to queue_nalu should only work for frame1 */
	ret = mbuf_coded_video_frame_queue_push(queue_single_nalu, frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_push(queue_single_nalu, frame2);
	CU_ASSERT_EQUAL(ret, -EPROTO);
	ret = mbuf_coded_video_frame_queue_get_count(queue_single_nalu);
	CU_ASSERT_EQUAL(ret, 1);
	ret = mbuf_coded_video_frame_queue_flush(queue_single_nalu);
	CU_ASSERT_EQUAL(ret, 0);

	/* Cleanup */
	ret = mbuf_coded_video_frame_unref(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_unref(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_destroy(queue_all);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_destroy(queue_none);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_destroy(queue_single_nalu);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_coded_video_frame_queue_drop(void)
{
	int ret;
	struct vdef_coded_frame frame_info = {
		.format = vdef_h264_byte_stream,
		.info.resolution.width = MBUF_TEST_WIDTH,
		.info.resolution.height = MBUF_TEST_HEIGHT,
	};
	struct mbuf_coded_video_frame *frame1, *frame2, *out_frame;
	struct mbuf_coded_video_frame_queue *queue;

	/* Create the frames used by the test */
	ret = mbuf_coded_video_frame_new(&frame_info, &frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_new(&frame_info, &frame2);
	CU_ASSERT_EQUAL(ret, 0);

	/* Finalize both frames */
	add_default_nalu(frame1);
	ret = mbuf_coded_video_frame_finalize(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	add_default_nalu(frame2);
	ret = mbuf_coded_video_frame_finalize(frame2);
	CU_ASSERT_EQUAL(ret, 0);

	/* Create the test queues, with max_frames = 1 */
	struct mbuf_coded_video_frame_queue_args args = {
		.max_frames = 1,
	};
	ret = mbuf_coded_video_frame_queue_new_with_args(&args, &queue);
	CU_ASSERT_EQUAL(ret, 0);

	/* Push the two frames, it should succeed every time */
	ret = mbuf_coded_video_frame_queue_push(queue, frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_push(queue, frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_get_count(queue);
	CU_ASSERT_EQUAL(ret, 1);

	/* Pop a frame, it should be frame 2 */
	ret = mbuf_coded_video_frame_queue_pop(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(frame2, out_frame);
	ret = mbuf_coded_video_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Popping a second frame should fail */
	ret = mbuf_coded_video_frame_queue_pop(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);
	ret = mbuf_coded_video_frame_queue_get_count(queue);
	CU_ASSERT_EQUAL(ret, 0);

	/* Cleanup */
	ret = mbuf_coded_video_frame_unref(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_unref(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_queue_destroy(queue);
	CU_ASSERT_EQUAL(ret, 0);
}

struct mbuf_ancillary_data_test {
	const char *str_name;
	const char *str_value;
	bool has_str;
	const char *buf_name;
	uint8_t buf_value[5];
	bool has_buf;
};


static bool ancillary_iterator(struct mbuf_ancillary_data *data, void *userdata)
{
	struct mbuf_ancillary_data_test *adt = userdata;

	const char *name = mbuf_ancillary_data_get_name(data);
	bool is_string = mbuf_ancillary_data_is_string(data);
	const char *strval = mbuf_ancillary_data_get_string(data);
	size_t len;
	const uint8_t *buffer = mbuf_ancillary_data_get_buffer(data, &len);

	if (strcmp(name, adt->str_name) == 0) {
		CU_ASSERT_TRUE(adt->has_str);
		adt->has_str = false;
		CU_ASSERT_TRUE(is_string);
		CU_ASSERT_STRING_EQUAL(strval, adt->str_value);
	} else if (strcmp(name, adt->buf_name) == 0) {
		CU_ASSERT_TRUE(adt->has_buf);
		adt->has_buf = false;
		CU_ASSERT_FALSE(is_string);
		CU_ASSERT_EQUAL(len, sizeof(adt->buf_value));
		if (len == sizeof(adt->buf_value))
			CU_ASSERT_EQUAL(memcmp(buffer, adt->buf_value, len), 0);
	} else {
		CU_FAIL("bad key in ancillary data");
	}

	return true;
}


static void test_mbuf_coded_video_frame_ancillary_data(void)
{
	int ret;
	struct vdef_coded_frame frame_info = {
		.format = vdef_h264_byte_stream,
		.info.resolution.width = MBUF_TEST_WIDTH,
		.info.resolution.height = MBUF_TEST_HEIGHT,
	};
	struct mbuf_mem *mem;
	struct mbuf_coded_video_frame *frame, *copy;

	struct mbuf_ancillary_data_test adt = {
		.str_name = "str",
		.str_value = "test",
		.buf_name = "buf",
		.buf_value = {1, 2, 3, 4, 5},
	};

	/* Create the pool, frame and memory used by the test */
	struct mbuf_pool *pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);
	ret = mbuf_coded_video_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem);
	CU_ASSERT_EQUAL(ret, 0);

	/* Iterate over an empty ancillary data list */
	ret = mbuf_coded_video_frame_foreach_ancillary_data(
		frame, ancillary_iterator, &adt);
	CU_ASSERT_EQUAL(ret, 0);

	/* Add the string and relaunch iteration */
	ret = mbuf_coded_video_frame_add_ancillary_string(
		frame, adt.str_name, adt.str_value);
	CU_ASSERT_EQUAL(ret, 0);
	adt.has_str = true;
	adt.has_buf = false;
	ret = mbuf_coded_video_frame_foreach_ancillary_data(
		frame, ancillary_iterator, &adt);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(adt.has_str);
	CU_ASSERT_FALSE(adt.has_buf);

	/* Finalize the frame, we should still be able to manipulate ancillary
	 * data */
	add_default_nalu(frame);
	ret = mbuf_coded_video_frame_finalize(frame);

	/* Add the buffer and relaunch iteration */
	ret = mbuf_coded_video_frame_add_ancillary_buffer(
		frame, adt.buf_name, adt.buf_value, sizeof(adt.buf_value));
	CU_ASSERT_EQUAL(ret, 0);
	adt.has_str = true;
	adt.has_buf = true;
	ret = mbuf_coded_video_frame_foreach_ancillary_data(
		frame, ancillary_iterator, &adt);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(adt.has_str);
	CU_ASSERT_FALSE(adt.has_buf);

	/* Test the single element getter */
	struct mbuf_ancillary_data *tmp;
	const void *tmpdata;
	size_t tmplen;
	ret = mbuf_coded_video_frame_get_ancillary_data(
		frame, adt.str_name, &tmp);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_TRUE(mbuf_ancillary_data_is_string(tmp));
	CU_ASSERT_STRING_EQUAL(mbuf_ancillary_data_get_string(tmp),
			       adt.str_value);
	ret = mbuf_ancillary_data_unref(tmp);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_get_ancillary_data(
		frame, adt.buf_name, &tmp);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(mbuf_ancillary_data_is_string(tmp));
	tmpdata = mbuf_ancillary_data_get_buffer(tmp, &tmplen);
	CU_ASSERT_EQUAL(tmplen, sizeof(adt.buf_value));
	if (tmplen == sizeof(adt.buf_value))
		CU_ASSERT_EQUAL(memcmp(tmpdata, adt.buf_value, tmplen), 0);
	ret = mbuf_ancillary_data_unref(tmp);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_get_ancillary_data(
		frame, "don't exist", &tmp);
	CU_ASSERT_EQUAL(ret, -ENOENT);

	/* Copy the frame, all the ancillary data should be copied too */
	ret = mbuf_coded_video_frame_copy(frame, mem, &copy);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_finalize(copy);
	CU_ASSERT_EQUAL(ret, 0);

	/* Remove the string and relaunch iteration */
	ret = mbuf_coded_video_frame_remove_ancillary_data(frame, adt.str_name);
	CU_ASSERT_EQUAL(ret, 0);
	adt.has_str = false;
	adt.has_buf = true;
	ret = mbuf_coded_video_frame_foreach_ancillary_data(
		frame, ancillary_iterator, &adt);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(adt.has_str);
	CU_ASSERT_FALSE(adt.has_buf);

	/* Test invalid cases */
	/* Add an already present data */
	ret = mbuf_coded_video_frame_add_ancillary_buffer(
		frame, adt.buf_name, adt.buf_value, sizeof(adt.buf_value));
	CU_ASSERT_EQUAL(ret, -EEXIST);
	/* Remove an absent data */
	ret = mbuf_coded_video_frame_remove_ancillary_data(frame, adt.str_name);
	CU_ASSERT_EQUAL(ret, -ENOENT);

	/* Iterate on the copy, both data should be present */
	adt.has_str = true;
	adt.has_buf = true;
	ret = mbuf_coded_video_frame_foreach_ancillary_data(
		copy, ancillary_iterator, &adt);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(adt.has_str);
	CU_ASSERT_FALSE(adt.has_buf);

	/* Cleanup */
	ret = mbuf_coded_video_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_coded_video_frame_unref(copy);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


CU_TestInfo g_mbuf_test_coded_video_frame[] = {
	{(char *)"scattered", &test_mbuf_coded_video_frame_scattered},
	{(char *)"single", &test_mbuf_coded_video_frame_single},
	{(char *)"get_infos", &test_mbuf_coded_video_frame_infos},
	{(char *)"pool_origin", &test_mbuf_coded_video_frame_pool_origin},
	{(char *)"bad_args", &test_mbuf_coded_video_frame_bad_args},
	{(char *)"bad_state", &test_mbuf_coded_video_frame_bad_state},
	{(char *)"queue", &test_mbuf_coded_video_frame_queue},
	{(char *)"queue_flush", &test_mbuf_coded_video_frame_queue_flush},
	{(char *)"queue_event", &test_mbuf_coded_video_frame_queue_evt},
	{(char *)"queue_filter", &test_mbuf_coded_video_frame_queue_filter},
	{(char *)"queue_drop", &test_mbuf_coded_video_frame_queue_drop},
	{(char *)"ancillary_data", &test_mbuf_coded_video_frame_ancillary_data},
	CU_TEST_INFO_NULL,
};