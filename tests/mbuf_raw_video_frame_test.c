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

/* For the stride test, we will set the test frames to have a stride of 2*width
 * on the Y plane, ensuring that their packed size will be width*height*2, while
 * the stride-less frame will properly have a size of width*height*3/2 */
static void init_frame_info(struct vdef_raw_frame *info, bool large_stride)
{
	memset(info, 0, sizeof(*info));
	info->format = vdef_i420;
	info->info.resolution.width = MBUF_TEST_WIDTH;
	info->info.resolution.height = MBUF_TEST_HEIGHT;
	info->plane_stride[0] = MBUF_TEST_WIDTH;
	info->plane_stride[1] = MBUF_TEST_WIDTH / 2;
	info->plane_stride[2] = MBUF_TEST_WIDTH / 2;
	if (large_stride) {
		for (int i = 0; i < 3; i++)
			info->plane_stride[i] *= 2;
	}
}

static size_t get_frame_size(struct vdef_raw_frame *info)
{
	size_t plane_size[VDEF_RAW_MAX_PLANE_COUNT] = {0};
	size_t plane_stride[VDEF_RAW_MAX_PLANE_COUNT] = {0};
	int ret = vdef_calc_raw_frame_size(&info->format,
					   &info->info.resolution,
					   plane_stride,
					   NULL,
					   NULL,
					   NULL,
					   plane_size,
					   NULL);
	if (ret < 0)
		return 0;
	size_t total = 0;
	for (int i = 0; i < VDEF_RAW_MAX_PLANE_COUNT; i++)
		total += plane_size[i];
	return total;
}


/* Create a mbuf pool suitable for all tests */
static struct mbuf_pool *create_pool()
{
	struct vdef_raw_frame tmp;
	init_frame_info(&tmp, true);
	size_t max_frame_size = get_frame_size(&tmp);
	struct mbuf_pool *pool;
	int ret = mbuf_pool_new(mbuf_mem_generic_impl,
				max_frame_size * 10,
				0,
				MBUF_POOL_LOW_MEM_GROW,
				0,
				"raw",
				&pool);
	if (ret != 0)
		return NULL;
	return pool;
}


/* Set all planes into an i420 frame
 * - If all three memory parameters are NULL, a new memory will be created and
 * used for all three planes.
 * - If either p2mem or p3mem is NULL, base_mem will be used for all planes.
 * - If all three memory parameters ar non-NULL, they will be used for each
 * plane.
 */
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
			uint8_t *dst = plane;
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


/* Check all planes into an i420 frame */
static void check_planes(struct mbuf_raw_video_frame *frame)
{
	struct vdef_raw_frame frame_info;
	size_t plane_size[VDEF_RAW_MAX_PLANE_COUNT];
	size_t plane_stride[VDEF_RAW_MAX_PLANE_COUNT] = {0};
	size_t plane_height[VDEF_RAW_MAX_PLANE_COUNT];

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

	for (unsigned int i = 0; i < 3; i++) {
		const void *plane;
		size_t len;
		ret = mbuf_raw_video_frame_get_plane(frame, i, &plane, &len);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(len, plane_size[i]);
		if (ret != 0 || len != plane_size[i])
			return;
		for (size_t j = 0; j < plane_height[i]; j++) {
			const uint8_t *src = plane;
			src += j * frame_info.plane_stride[i];
			for (unsigned int z = 0; z < frame_info.plane_stride[i];
			     z++) {
				int expected =
					(z < plane_stride[i]) ? i + 10 : i + 20;
				if (src[z] != expected) {
					CU_FAIL("bad plane content");
					return;
				}
			}
		}
		ret = mbuf_raw_video_frame_release_plane(frame, i, plane);
		CU_ASSERT_EQUAL(ret, 0);
	}
}


static void test_mbuf_raw_video_frame_scattered(void)
{
	struct mbuf_mem *memy, *memu, *memv, *mempack, *memnostride;
	struct vdef_raw_frame frame_info;
	size_t required_len;
	const void *data;
	struct mbuf_raw_video_frame *frame, *packed, *nostride;

	init_frame_info(&frame_info, true);

	/* Create the pool, frame, and memories used by the test */
	struct mbuf_pool *pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);
	int ret = mbuf_raw_video_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &memy);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &memu);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &memv);
	CU_ASSERT_EQUAL(ret, 0);

	/* Add the planes to the frame */
	set_planes(frame, memy, memu, memv);

	/* Unref the memories which are no longer used */
	ret = mbuf_mem_unref(memy);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(memu);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(memv);
	CU_ASSERT_EQUAL(ret, 0);

	/* Finalize the frame */
	ret = mbuf_raw_video_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Check frame content via the plane getters */
	check_planes(frame);

	/* Check that the frame is not packed, as it uses scattered buffers */
	ret = mbuf_raw_video_frame_get_packed_buffer(
		frame, &data, &required_len);
	CU_ASSERT_EQUAL(ret, -EPROTO);

	/* Get a memory for the copied frame */
	ret = mbuf_pool_get(pool, &mempack);
	CU_ASSERT_EQUAL(ret, 0);

	/* Copy the frame into the destination memory, finalize the frame and
	 * unref the memory */
	ret = mbuf_raw_video_frame_copy(frame, mempack, false, &packed);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_finalize(packed);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mempack);
	CU_ASSERT_EQUAL(ret, 0);

	/* Copied frame should still have the good plane contents */
	check_planes(packed);

	/* Copied frame is packed, and thus we can get its packed_buffer */
	ret = mbuf_raw_video_frame_get_packed_buffer(
		packed, &data, &required_len);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_release_packed_buffer(packed, data);
	CU_ASSERT_EQUAL(ret, 0);

	/* Get a memory for the copied frame */
	ret = mbuf_pool_get(pool, &memnostride);
	CU_ASSERT_EQUAL(ret, 0);

	/* Copy the frame into the destination memory removing stride, finalize
	 * the frame and unref the memory */
	ret = mbuf_raw_video_frame_copy(frame, memnostride, true, &nostride);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_finalize(nostride);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(memnostride);
	CU_ASSERT_EQUAL(ret, 0);

	/* Copied frame should still have the good plane contents */
	check_planes(nostride);

	/* Nostride version should be smaller than the other versions */
	ssize_t base = mbuf_raw_video_frame_get_packed_size(frame, false);
	CU_ASSERT(base > 0);
	ssize_t base_nostride =
		mbuf_raw_video_frame_get_packed_size(frame, true);
	CU_ASSERT(base_nostride > 0);
	ssize_t nostride_false =
		mbuf_raw_video_frame_get_packed_size(nostride, false);
	CU_ASSERT(nostride_false > 0);
	ssize_t nostride_true =
		mbuf_raw_video_frame_get_packed_size(nostride, true);
	CU_ASSERT(nostride_true > 0);
	CU_ASSERT(base > nostride_false);
	CU_ASSERT_EQUAL(base_nostride, nostride_false);
	CU_ASSERT_EQUAL(nostride_false, nostride_true);

	/* Copied frame is packed, and thus we can get its packed_buffer */
	ret = mbuf_raw_video_frame_get_packed_buffer(
		packed, &data, &required_len);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_release_packed_buffer(packed, data);
	CU_ASSERT_EQUAL(ret, 0);

	/* Cleanup */
	ret = mbuf_raw_video_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_unref(packed);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_unref(nostride);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_raw_video_frame_single(void)
{
	struct mbuf_mem *memyuv;
	struct vdef_raw_frame frame_info;
	struct mbuf_raw_video_frame *frame;
	const void *data;
	size_t len;

	init_frame_info(&frame_info, false);

	/* Create the pool, frame, and memories used by the test */
	struct mbuf_pool *pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);
	int ret = mbuf_raw_video_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &memyuv);
	CU_ASSERT_EQUAL(ret, 0);

	/* Add the planes to the frame */
	set_planes(frame, memyuv, NULL, NULL);

	/* Unref the memories which are no longer used */
	ret = mbuf_mem_unref(memyuv);
	CU_ASSERT_EQUAL(ret, 0);

	/* Finalize the frame */
	ret = mbuf_raw_video_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Check frame content via the plane getters */
	check_planes(frame);

	/* Check that the frame is packed, as it uses a single buffer with no
	 * gaps */
	ret = mbuf_raw_video_frame_get_packed_buffer(frame, &data, &len);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_release_packed_buffer(frame, data);
	CU_ASSERT_EQUAL(ret, 0);

	/* Cleanup */
	ret = mbuf_raw_video_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_raw_video_frame_infos(void)
{
	int ret;
	struct mbuf_raw_video_frame *frame;
	struct vdef_raw_frame frame_info;
	struct vdef_raw_frame out_frame_info;
	struct mbuf_mem_info mem_info;

	init_frame_info(&frame_info, false);

	/* Create & finalize the frame used by the test */
	ret = mbuf_raw_video_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	set_planes(frame, NULL, NULL, NULL);
	ret = mbuf_raw_video_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Get back and compare the frame_info */
	ret = mbuf_raw_video_frame_get_frame_info(frame, &out_frame_info);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(
		memcmp(&frame_info, &out_frame_info, sizeof(frame_info)), 0);

	/* Get the memory info for the first plane */
	ret = mbuf_raw_video_frame_get_plane_mem_info(frame, 0, &mem_info);
	CU_ASSERT_EQUAL(ret, 0);
	/* The memory must be of type "generic-wrap" */
	CU_ASSERT_EQUAL(mem_info.cookie, mbuf_mem_generic_wrap_cookie);

	/* Cleanup */
	ret = mbuf_raw_video_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_raw_video_frame_pool_origin(void)
{
	int ret;
	struct mbuf_pool *pool;
	struct mbuf_mem *mem_pool, *mem_non_pool1, *mem_non_pool2;
	struct mbuf_raw_video_frame *frame1, *frame2, *frame3;
	struct vdef_raw_frame frame_info;
	bool any, all;

	init_frame_info(&frame_info, false);
	size_t frame_size = get_frame_size(&frame_info);

	/* Create the pools/mem/frames */
	pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);
	ret = mbuf_raw_video_frame_new(&frame_info, &frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_new(&frame_info, &frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_new(&frame_info, &frame3);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem_pool);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_generic_new(frame_size, &mem_non_pool1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_generic_new(frame_size, &mem_non_pool2);
	CU_ASSERT_EQUAL(ret, 0);

	/* Fill & finalize the frames:
	 * frame1 will not use any memory from the pool
	 * frame2 will use one from the pool, and two others
	 * frame3 will only use memory from the pool
	 */
	set_planes(frame1, NULL, NULL, NULL);
	ret = mbuf_raw_video_frame_finalize(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	set_planes(frame2, mem_pool, mem_non_pool1, mem_non_pool2);
	ret = mbuf_raw_video_frame_finalize(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	set_planes(frame3, mem_pool, NULL, NULL);
	ret = mbuf_raw_video_frame_finalize(frame3);
	CU_ASSERT_EQUAL(ret, 0);

	/* Check pool origin for each frame */
	ret = mbuf_raw_video_frame_uses_mem_from_pool(frame1, pool, &any, &all);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(any);
	CU_ASSERT_FALSE(all);
	ret = mbuf_raw_video_frame_uses_mem_from_pool(frame2, pool, &any, &all);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_TRUE(any);
	CU_ASSERT_FALSE(all);
	ret = mbuf_raw_video_frame_uses_mem_from_pool(frame3, pool, &any, &all);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_TRUE(any);
	CU_ASSERT_TRUE(all);


	/* Cleanup */
	ret = mbuf_raw_video_frame_unref(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_unref(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_unref(frame3);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem_pool);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem_non_pool1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem_non_pool2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_raw_video_frame_bad_args(void)
{
	int ret;
	struct mbuf_mem *mem, *mem_cp;
	struct mbuf_raw_video_frame *frame, *frame_cp;
	struct mbuf_raw_video_frame_queue *queue;
	struct vdef_raw_frame frame_info;
	struct pomp_evt *evt;
	size_t plane_size = MBUF_TEST_WIDTH * MBUF_TEST_HEIGHT;
	const void *data;
	void *rwdata;
	size_t len;
	const void *tmp;
	size_t tmp_size;
	struct vmeta_frame *meta, *out_meta;
	bool any, all;
	struct mbuf_mem_info mem_info;
	unsigned int plane_count;

	init_frame_info(&frame_info, false);

	/* Create a pool for the test */
	struct mbuf_pool *pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);

	/* Bad arguments for constructors */
	ret = mbuf_raw_video_frame_new(NULL, &frame);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_new(&frame_info, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_queue_new(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_queue_new_with_args(NULL, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* Get real objects for following tests */
	ret = mbuf_raw_video_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem_cp);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_new(&queue);
	CU_ASSERT_EQUAL(ret, 0);
	ret = vmeta_frame_new(VMETA_FRAME_TYPE_PROTO, &meta);
	CU_ASSERT_EQUAL(ret, 0);
	plane_count = vdef_get_raw_frame_plane_count(&frame_info.format);

	/* Add planes, finalize frame & get a plane for reader API tests */
	set_planes(frame, mem, NULL, NULL);
	ret = mbuf_raw_video_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_get_plane(frame, 0, &tmp, &tmp_size);
	CU_ASSERT_EQUAL(ret, 0);

	/* Bad arguments for other functions */
	ret = mbuf_raw_video_frame_ref(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_unref(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_set_frame_info(NULL, &frame_info);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_set_frame_info(frame, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_set_metadata(NULL, meta);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_set_metadata(frame, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_set_plane(NULL, 0, mem, 0, plane_size);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_set_plane(
		frame, plane_count, mem, 0, plane_size);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_set_plane(frame, 0, NULL, 0, plane_size);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_finalize(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_uses_mem_from_pool(NULL, pool, &any, &all);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_uses_mem_from_pool(frame, NULL, &any, &all);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_metadata(NULL, &out_meta);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_metadata(frame, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_plane_mem_info(NULL, 0, &mem_info);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_plane_mem_info(
		frame, plane_count, &mem_info);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_plane_mem_info(frame, 0, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_plane(NULL, 0, &data, &len);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_plane(frame, plane_count, &data, &len);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_plane(frame, 0, NULL, &len);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_plane(frame, 0, &data, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	data = (void *)(intptr_t)1; /* Make sure data is non-null */
	ret = mbuf_raw_video_frame_release_plane(NULL, 0, data);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_release_plane(frame, plane_count, data);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_release_plane(frame, 0, data);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_rw_plane(NULL, 0, &rwdata, &len);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_rw_plane(frame, 0, NULL, &len);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_rw_plane(
		frame, plane_count, &rwdata, &len);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_rw_plane(frame, 0, &rwdata, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	rwdata = (void *)(intptr_t)1; /* Make sure rwdata is non-null */
	ret = mbuf_raw_video_frame_release_rw_plane(NULL, 0, rwdata);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_release_rw_plane(frame, plane_count, rwdata);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_release_rw_plane(frame, 0, rwdata);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_packed_buffer(NULL, &data, &len);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_packed_buffer(frame, NULL, &len);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_packed_buffer(frame, &data, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_release_packed_buffer(NULL, data);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_release_packed_buffer(frame, data);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_rw_packed_buffer(NULL, &rwdata, &len);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_rw_packed_buffer(frame, NULL, &len);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_rw_packed_buffer(frame, &rwdata, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_release_rw_packed_buffer(NULL, rwdata);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_release_rw_packed_buffer(frame, rwdata);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_copy(NULL, mem_cp, false, &frame_cp);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_copy(frame, NULL, false, &frame_cp);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_copy(frame, mem_cp, false, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_frame_info(NULL, &frame_info);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_get_frame_info(frame, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_queue_push(NULL, frame);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_queue_push(queue, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_queue_peek(NULL, &frame_cp);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_queue_peek(queue, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_queue_pop(NULL, &frame_cp);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_queue_pop(queue, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_queue_flush(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_queue_get_event(NULL, &evt);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_queue_get_event(queue, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_raw_video_frame_queue_destroy(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* Cleanup */
	ret = vmeta_frame_unref(meta);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_destroy(queue);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem_cp);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_release_plane(frame, 0, tmp);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_raw_video_frame_bad_state(void)
{
	int ret;
	struct mbuf_mem *mem;
	struct mbuf_raw_video_frame *frame;
	struct vdef_raw_frame frame_info_i420;
	struct vdef_raw_frame frame_info_nv12 = {
		.format = vdef_nv12,
		.info.resolution.width = MBUF_TEST_WIDTH,
		.info.resolution.height = MBUF_TEST_HEIGHT,
	};
	struct vdef_raw_frame frame_info_out;
	size_t plane_size = MBUF_TEST_WIDTH * MBUF_TEST_HEIGHT;
	const void *data;
	void *rwdata;
	size_t len;

	init_frame_info(&frame_info_i420, false);

	/* Create the pool, frame, and memories used by the test */
	struct mbuf_pool *pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);
	ret = mbuf_raw_video_frame_new(&frame_info_nv12, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem);
	CU_ASSERT_EQUAL(ret, 0);

	/* Check that we can properly get/set the frame_info */
	ret = mbuf_raw_video_frame_get_frame_info(frame, &frame_info_out);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(vdef_raw_format_cmp(&frame_info_out.format,
					    &frame_info_nv12.format),
			1);
	ret = mbuf_raw_video_frame_set_frame_info(frame, &frame_info_i420);
	ret = mbuf_raw_video_frame_get_frame_info(frame, &frame_info_out);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(vdef_raw_format_cmp(&frame_info_out.format,
					    &frame_info_i420.format),
			1);

	/* We should not be able to finalize a frame without planes */
	ret = mbuf_raw_video_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, -EPROTO);

	/* Add the planes to the frame */
	set_planes(frame, mem, NULL, NULL);

	/* Setting the frame back to nv12 should fail as there are already three
	 * planes set */
	ret = mbuf_raw_video_frame_set_frame_info(frame, &frame_info_nv12);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* Buffer is not finalized yet, getters should fail */
	ret = mbuf_raw_video_frame_get_plane(frame, 0, &data, &len);
	CU_ASSERT_EQUAL(ret, -EBUSY);
	ret = mbuf_raw_video_frame_get_plane(frame, 1, &data, &len);
	CU_ASSERT_EQUAL(ret, -EBUSY);
	ret = mbuf_raw_video_frame_get_plane(frame, 2, &data, &len);
	CU_ASSERT_EQUAL(ret, -EBUSY);

	/* Finalize now */
	ret = mbuf_raw_video_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Buffer is now read-only, setting a plane should fail */
	ret = mbuf_raw_video_frame_set_plane(frame, 0, mem, 0, plane_size);
	CU_ASSERT_EQUAL(ret, -EBUSY);
	/* Setting the frame_info should fail too */
	ret = mbuf_raw_video_frame_set_frame_info(frame, &frame_info_nv12);
	CU_ASSERT_EQUAL(ret, -EBUSY);

	/* Take a read-only reference on a plane */
	ret = mbuf_raw_video_frame_get_plane(frame, 0, &data, &len);
	CU_ASSERT_EQUAL(ret, 0);
	/* Try to take a read-write reference on a plane.
	 * Frame should be read-locked */
	ret = mbuf_raw_video_frame_get_rw_plane(frame, 0, &rwdata, &len);
	CU_ASSERT_EQUAL(ret, -EBUSY);

	/* Release the plane */
	ret = mbuf_raw_video_frame_release_plane(frame, 0, data);
	CU_ASSERT_EQUAL(ret, 0);
	/* Double release of a plane should fail */
	ret = mbuf_raw_video_frame_release_plane(frame, 0, data);
	CU_ASSERT_EQUAL(ret, -EALREADY);

	/* Take a read-write reference on a plane */
	ret = mbuf_raw_video_frame_get_rw_plane(frame, 0, &rwdata, &len);
	CU_ASSERT_EQUAL(ret, 0);
	/* Try to take a read-only reference on a plane.
	 * Frame should be write-locked */
	ret = mbuf_raw_video_frame_get_plane(frame, 0, &data, &len);
	CU_ASSERT_EQUAL(ret, -EBUSY);

	/* Release the plane */
	ret = mbuf_raw_video_frame_release_rw_plane(frame, 0, rwdata);
	CU_ASSERT_EQUAL(ret, 0);
	/* Double release of a plane should fail */
	ret = mbuf_raw_video_frame_release_rw_plane(frame, 0, rwdata);
	CU_ASSERT_EQUAL(ret, -EALREADY);

	/* Cleanup */
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_raw_video_frame_queue(void)
{
	int ret;
	struct vdef_raw_frame frame_info;
	struct mbuf_raw_video_frame *frame1, *frame2, *frame3, *out_frame;
	struct mbuf_raw_video_frame_queue *queue;

	init_frame_info(&frame_info, false);

	/* Create the frames and the queue used by the test */
	ret = mbuf_raw_video_frame_new(&frame_info, &frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_new(&frame_info, &frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_new(&frame_info, &frame3);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_new(&queue);
	CU_ASSERT_EQUAL(ret, 0);

	/* Peek / Pop from empty queue should fail */
	ret = mbuf_raw_video_frame_queue_peek(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);
	ret = mbuf_raw_video_frame_queue_pop(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);

	/* Pushing a non-finalized frame should fail */
	ret = mbuf_raw_video_frame_queue_push(queue, frame1);
	CU_ASSERT_EQUAL(ret, -EBUSY);

	/* Finalize frames for the test */
	set_planes(frame1, NULL, NULL, NULL);
	ret = mbuf_raw_video_frame_finalize(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	set_planes(frame2, NULL, NULL, NULL);
	ret = mbuf_raw_video_frame_finalize(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	set_planes(frame3, NULL, NULL, NULL);
	ret = mbuf_raw_video_frame_finalize(frame3);
	CU_ASSERT_EQUAL(ret, 0);

	/* Push three frames in order */
	ret = mbuf_raw_video_frame_queue_push(queue, frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_push(queue, frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_push(queue, frame3);
	CU_ASSERT_EQUAL(ret, 0);

	/* Peek and compare to frame 1 */
	ret = mbuf_raw_video_frame_queue_peek(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(out_frame, frame1);
	ret = mbuf_raw_video_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Pop and compare to frame 1 */
	ret = mbuf_raw_video_frame_queue_pop(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(out_frame, frame1);
	ret = mbuf_raw_video_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Peek and compare to frame 2 */
	ret = mbuf_raw_video_frame_queue_peek(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(out_frame, frame2);
	ret = mbuf_raw_video_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Flush */
	ret = mbuf_raw_video_frame_queue_flush(queue);
	CU_ASSERT_EQUAL(ret, 0);

	/* Peek / Pop from flushed queue should fail */
	ret = mbuf_raw_video_frame_queue_peek(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);
	ret = mbuf_raw_video_frame_queue_pop(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);

	/* Cleanup */
	ret = mbuf_raw_video_frame_unref(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_unref(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_unref(frame3);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_destroy(queue);
	CU_ASSERT_EQUAL(ret, 0);
}


struct test_mbuf_raw_video_frame_queue_flush_userdata {
	bool freed;
};


static void test_mbuf_raw_video_frame_queue_flush_free(void *data,
						       size_t len,
						       void *userdata)
{
	struct test_mbuf_raw_video_frame_queue_flush_userdata *ud = userdata;
	free(data);
	ud->freed = true;
}


static void test_mbuf_raw_video_frame_queue_flush(void)
{
	int ret;
	struct vdef_raw_frame frame_info;
	struct mbuf_mem *mem;
	struct mbuf_raw_video_frame *frame;
	struct mbuf_raw_video_frame_queue *queue;
	struct test_mbuf_raw_video_frame_queue_flush_userdata ud = {
		.freed = false,
	};

	init_frame_info(&frame_info, false);
	size_t datalen = get_frame_size(&frame_info);
	void *data = malloc(datalen);
	CU_ASSERT_PTR_NOT_NULL(data);

	/* Create the frame and the queue used by the test */
	ret = mbuf_raw_video_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_generic_wrap(data,
				    datalen,
				    test_mbuf_raw_video_frame_queue_flush_free,
				    &ud,
				    &mem);
	CU_ASSERT_EQUAL(ret, 0);
	set_planes(frame, mem, NULL, NULL);
	ret = mbuf_raw_video_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_new(&queue);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);

	/* Push the frame */
	ret = mbuf_raw_video_frame_queue_push(queue, frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Unref the frames */
	ret = mbuf_raw_video_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(ud.freed);

	/* Flush the queue */
	ret = mbuf_raw_video_frame_queue_flush(queue);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_TRUE(ud.freed);

	/* Cleanup */
	ret = mbuf_raw_video_frame_queue_destroy(queue);
	CU_ASSERT_EQUAL(ret, 0);
}


/* Userdata for the pomp_evt callback */
struct raw_queue_evt_userdata {
	struct mbuf_raw_video_frame_queue *queue;
	int expected_frames;
};


/* pomp_evt callback for the queue, this function pops all frames in the queue
 * and decrement userdata->expected_frames for each frame. */
static void raw_queue_evt(struct pomp_evt *evt, void *userdata)
{
	int ret = 0;
	struct raw_queue_evt_userdata *data = userdata;

	while (true) {
		struct mbuf_raw_video_frame *frame;

		ret = mbuf_raw_video_frame_queue_pop(data->queue, &frame);
		if (ret == -EAGAIN)
			break;
		CU_ASSERT_EQUAL(ret, 0);
		ret = mbuf_raw_video_frame_unref(frame);
		CU_ASSERT_EQUAL(ret, 0);
		data->expected_frames--;
	}
}


static void test_mbuf_raw_video_frame_queue_evt(void)
{
	int ret;
	struct vdef_raw_frame frame_info;
	struct mbuf_raw_video_frame *frame1, *frame2;
	struct mbuf_raw_video_frame_queue *queue;
	struct pomp_evt *evt;
	struct pomp_loop *loop;
	struct raw_queue_evt_userdata userdata;

	init_frame_info(&frame_info, false);

	/* Create the frames and the queue used by the test */
	ret = mbuf_raw_video_frame_new(&frame_info, &frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_new(&frame_info, &frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_new(&queue);
	CU_ASSERT_EQUAL(ret, 0);
	userdata.queue = queue;

	/* Finalize the frames */
	set_planes(frame1, NULL, NULL, NULL);
	ret = mbuf_raw_video_frame_finalize(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	set_planes(frame2, NULL, NULL, NULL);
	ret = mbuf_raw_video_frame_finalize(frame2);
	CU_ASSERT_EQUAL(ret, 0);

	/* Create a loop, get the queue event, and attach it to the loop */
	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL(loop);
	ret = mbuf_raw_video_frame_queue_get_event(queue, &evt);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_evt_attach_to_loop(evt, loop, raw_queue_evt, &userdata);

	/* Push two frames in order */
	ret = mbuf_raw_video_frame_queue_push(queue, frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_push(queue, frame2);
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
	ret = mbuf_raw_video_frame_queue_push(queue, frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_flush(queue);
	CU_ASSERT_EQUAL(ret, 0);

	/* Rerun the loop and expect a timeout */
	ret = pomp_loop_wait_and_process(loop, 100);
	CU_ASSERT_EQUAL(ret, -ETIMEDOUT);

	/* Cleanup */
	ret = pomp_evt_detach_from_loop(evt, loop);
	CU_ASSERT_EQUAL(ret, 0);
	ret = pomp_loop_destroy(loop);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_unref(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_unref(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_destroy(queue);
	CU_ASSERT_EQUAL(ret, 0);
}


/* queue filter function which refuses all frames */
static bool queue_filter_none(struct mbuf_raw_video_frame *frame,
			      void *userdata)
{
	return false;
}


/* queue filter function which accepts all frames, equivalent to no filter
 * function */
static bool queue_filter_all(struct mbuf_raw_video_frame *frame, void *userdata)
{
	return true;
}


/* queue filter function which only accepts packed frames */
static bool queue_filter_is_packed(struct mbuf_raw_video_frame *frame,
				   void *userdata)
{
	const void *tmp;
	size_t tmp_size;
	int ret =
		mbuf_raw_video_frame_get_packed_buffer(frame, &tmp, &tmp_size);
	CU_ASSERT((ret == 0 || ret == -EPROTO));
	if (ret == 0) {
		ret = mbuf_raw_video_frame_release_packed_buffer(frame, tmp);
		CU_ASSERT_EQUAL(ret, 0);
		return true;
	}
	return false;
}


static void test_mbuf_raw_video_frame_queue_filter(void)
{
	int ret;
	struct vdef_raw_frame frame_info;
	struct mbuf_mem *memy, *memu, *memv;
	struct mbuf_raw_video_frame *frame1, *frame2;
	struct mbuf_raw_video_frame_queue *queue_none, *queue_all,
		*queue_packed;

	init_frame_info(&frame_info, false);

	/* Create the pool, frames and memories used by the test */
	struct mbuf_pool *pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);
	ret = mbuf_raw_video_frame_new(&frame_info, &frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_new(&frame_info, &frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &memy);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &memu);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &memv);
	CU_ASSERT_EQUAL(ret, 0);

	/* Add scattered planes to the first frame, and packed on the second */
	set_planes(frame1, memy, memu, memv);
	set_planes(frame2, NULL, NULL, NULL);

	/* Finalize both frames */
	ret = mbuf_raw_video_frame_finalize(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_finalize(frame2);
	CU_ASSERT_EQUAL(ret, 0);

	/* Create the three test queues, one with each filter function */
	struct mbuf_raw_video_frame_queue_args args = {
		.filter = queue_filter_none,
	};
	ret = mbuf_raw_video_frame_queue_new_with_args(&args, &queue_none);
	CU_ASSERT_EQUAL(ret, 0);
	args.filter = queue_filter_all;
	ret = mbuf_raw_video_frame_queue_new_with_args(&args, &queue_all);
	CU_ASSERT_EQUAL(ret, 0);
	args.filter = queue_filter_is_packed;
	ret = mbuf_raw_video_frame_queue_new_with_args(&args, &queue_packed);
	CU_ASSERT_EQUAL(ret, 0);

	/* Push to queue_none should fail */
	ret = mbuf_raw_video_frame_queue_push(queue_none, frame1);
	CU_ASSERT_EQUAL(ret, -EPROTO);
	ret = mbuf_raw_video_frame_queue_push(queue_none, frame2);
	CU_ASSERT_EQUAL(ret, -EPROTO);
	ret = mbuf_raw_video_frame_queue_flush(queue_none);
	CU_ASSERT_EQUAL(ret, 0);

	/* Pushing to queue_all should be OK */
	ret = mbuf_raw_video_frame_queue_push(queue_all, frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_push(queue_all, frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_flush(queue_all);
	CU_ASSERT_EQUAL(ret, 0);

	/* Pushing to queue_packed should only work for frame2 */
	ret = mbuf_raw_video_frame_queue_push(queue_packed, frame1);
	CU_ASSERT_EQUAL(ret, -EPROTO);
	ret = mbuf_raw_video_frame_queue_push(queue_packed, frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_flush(queue_packed);
	CU_ASSERT_EQUAL(ret, 0);

	/* Cleanup */
	ret = mbuf_raw_video_frame_unref(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_unref(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_destroy(queue_all);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_destroy(queue_none);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_destroy(queue_packed);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(memy);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(memu);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(memv);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_raw_video_frame_queue_drop(void)
{
	int ret;
	struct vdef_raw_frame frame_info;
	struct mbuf_raw_video_frame *frame1, *frame2, *out_frame;
	struct mbuf_raw_video_frame_queue *queue;

	init_frame_info(&frame_info, false);

	/* Create the frames used by the test */
	ret = mbuf_raw_video_frame_new(&frame_info, &frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_new(&frame_info, &frame2);
	CU_ASSERT_EQUAL(ret, 0);

	/* Finalize both frames */
	set_planes(frame1, NULL, NULL, NULL);
	ret = mbuf_raw_video_frame_finalize(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	set_planes(frame2, NULL, NULL, NULL);
	ret = mbuf_raw_video_frame_finalize(frame2);
	CU_ASSERT_EQUAL(ret, 0);

	/* Create the test queues, with max_frames = 1 */
	struct mbuf_raw_video_frame_queue_args args = {
		.max_frames = 1,
	};
	ret = mbuf_raw_video_frame_queue_new_with_args(&args, &queue);
	CU_ASSERT_EQUAL(ret, 0);

	/* Push the two frames, it should succeed every time */
	ret = mbuf_raw_video_frame_queue_push(queue, frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_push(queue, frame2);
	CU_ASSERT_EQUAL(ret, 0);

	/* Pop a frame, it should be frame 2 */
	ret = mbuf_raw_video_frame_queue_pop(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(frame2, out_frame);
	ret = mbuf_raw_video_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Popping a second frame should fail */
	ret = mbuf_raw_video_frame_queue_pop(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);

	/* Cleanup */
	ret = mbuf_raw_video_frame_unref(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_unref(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_queue_destroy(queue);
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


static void test_mbuf_raw_video_frame_ancillary_data(void)
{
	int ret;
	struct vdef_raw_frame frame_info;
	struct mbuf_mem *mem;
	struct mbuf_raw_video_frame *frame, *copy;

	struct mbuf_ancillary_data_test adt = {
		.str_name = "str",
		.str_value = "test",
		.buf_name = "buf",
		.buf_value = {1, 2, 3, 4, 5},
	};

	init_frame_info(&frame_info, false);

	/* Create the pool, frame and memory used by the test */
	struct mbuf_pool *pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);
	ret = mbuf_raw_video_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem);
	CU_ASSERT_EQUAL(ret, 0);
	set_planes(frame, NULL, NULL, NULL);

	/* Iterate over an empty ancillary data list */
	ret = mbuf_raw_video_frame_foreach_ancillary_data(
		frame, ancillary_iterator, &adt);
	CU_ASSERT_EQUAL(ret, 0);

	/* Add the string and relaunch iteration */
	ret = mbuf_raw_video_frame_add_ancillary_string(
		frame, adt.str_name, adt.str_value);
	CU_ASSERT_EQUAL(ret, 0);
	adt.has_str = true;
	adt.has_buf = false;
	ret = mbuf_raw_video_frame_foreach_ancillary_data(
		frame, ancillary_iterator, &adt);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(adt.has_str);
	CU_ASSERT_FALSE(adt.has_buf);

	/* Finalize the frame, we should still be able to manipulate ancillary
	 * data */
	ret = mbuf_raw_video_frame_finalize(frame);

	/* Add the buffer and relaunch iteration */
	ret = mbuf_raw_video_frame_add_ancillary_buffer(
		frame, adt.buf_name, adt.buf_value, sizeof(adt.buf_value));
	CU_ASSERT_EQUAL(ret, 0);
	adt.has_str = true;
	adt.has_buf = true;
	ret = mbuf_raw_video_frame_foreach_ancillary_data(
		frame, ancillary_iterator, &adt);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(adt.has_str);
	CU_ASSERT_FALSE(adt.has_buf);

	/* Test the single element getter */
	struct mbuf_ancillary_data *tmp;
	const void *tmpdata;
	size_t tmplen;
	ret = mbuf_raw_video_frame_get_ancillary_data(
		frame, adt.str_name, &tmp);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_TRUE(mbuf_ancillary_data_is_string(tmp));
	CU_ASSERT_STRING_EQUAL(mbuf_ancillary_data_get_string(tmp),
			       adt.str_value);
	ret = mbuf_ancillary_data_unref(tmp);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_get_ancillary_data(
		frame, adt.buf_name, &tmp);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(mbuf_ancillary_data_is_string(tmp));
	tmpdata = mbuf_ancillary_data_get_buffer(tmp, &tmplen);
	CU_ASSERT_EQUAL(tmplen, sizeof(adt.buf_value));
	if (tmplen == sizeof(adt.buf_value))
		CU_ASSERT_EQUAL(memcmp(tmpdata, adt.buf_value, tmplen), 0);
	ret = mbuf_ancillary_data_unref(tmp);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_get_ancillary_data(
		frame, "don't exist", &tmp);
	CU_ASSERT_EQUAL(ret, -ENOENT);

	/* Copy the frame, all the ancillary data should be copied too */
	ret = mbuf_raw_video_frame_copy(frame, mem, false, &copy);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_finalize(copy);
	CU_ASSERT_EQUAL(ret, 0);

	/* Remove the string and relaunch iteration */
	ret = mbuf_raw_video_frame_remove_ancillary_data(frame, adt.str_name);
	CU_ASSERT_EQUAL(ret, 0);
	adt.has_str = false;
	adt.has_buf = true;
	ret = mbuf_raw_video_frame_foreach_ancillary_data(
		frame, ancillary_iterator, &adt);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(adt.has_str);
	CU_ASSERT_FALSE(adt.has_buf);

	/* Test invalid cases */
	/* Add an already present data */
	ret = mbuf_raw_video_frame_add_ancillary_buffer(
		frame, adt.buf_name, adt.buf_value, sizeof(adt.buf_value));
	CU_ASSERT_EQUAL(ret, -EEXIST);
	/* Remove an absent data */
	ret = mbuf_raw_video_frame_remove_ancillary_data(frame, adt.str_name);
	CU_ASSERT_EQUAL(ret, -ENOENT);

	/* Iterate on the copy, both data should be present */
	adt.has_str = true;
	adt.has_buf = true;
	ret = mbuf_raw_video_frame_foreach_ancillary_data(
		copy, ancillary_iterator, &adt);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(adt.has_str);
	CU_ASSERT_FALSE(adt.has_buf);

	/* Cleanup */
	ret = mbuf_raw_video_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_raw_video_frame_unref(copy);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


CU_TestInfo g_mbuf_test_raw_video_frame[] = {
	{(char *)"scattered", &test_mbuf_raw_video_frame_scattered},
	{(char *)"single", &test_mbuf_raw_video_frame_single},
	{(char *)"get_infos", &test_mbuf_raw_video_frame_infos},
	{(char *)"pool_origin", &test_mbuf_raw_video_frame_pool_origin},
	{(char *)"bad_args", &test_mbuf_raw_video_frame_bad_args},
	{(char *)"bad_state", &test_mbuf_raw_video_frame_bad_state},
	{(char *)"queue", &test_mbuf_raw_video_frame_queue},
	{(char *)"queue_flush", &test_mbuf_raw_video_frame_queue_flush},
	{(char *)"queue_event", &test_mbuf_raw_video_frame_queue_evt},
	{(char *)"queue_filter", &test_mbuf_raw_video_frame_queue_filter},
	{(char *)"queue_drop", &test_mbuf_raw_video_frame_queue_drop},
	{(char *)"ancillary_data", &test_mbuf_raw_video_frame_ancillary_data},
	CU_TEST_INFO_NULL,
};
