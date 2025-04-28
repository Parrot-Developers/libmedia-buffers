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


#define MBUF_TEST_CHANNEL_COUNT 1
#define MBUF_TEST_BIT_DEPTH 8
#define MBUF_TEST_SAMPLE_RATE 44000
#define MBUF_TEST_SAMPLE_PER_FRAME 1024
#define MBUF_TEST_AAC_BITRATE 320000 /* 320Kbps */


static void init_frame_info(struct adef_frame *info,
			    enum adef_encoding encoding)
{
	memset(info, 0, sizeof(*info));
	if (encoding == ADEF_ENCODING_PCM)
		info->format = adef_pcm_16b_44100hz_stereo;
	else
		info->format = adef_aac_lc_16b_44100hz_stereo_raw;
	info->info.timestamp = info->info.capture_timestamp = rand() % 1000000;
	info->info.index = 33;
	info->info.timescale = 1000000;
}


static size_t get_frame_size(struct adef_frame *info)
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
		CU_ASSERT_FATAL(true);
		return 0;
	}
}


/* Create a mbuf pool suitable for all tests */
static struct mbuf_pool *create_pool()
{
	struct adef_frame tmp;
	init_frame_info(&tmp, ADEF_ENCODING_AAC_LC);
	size_t max_frame_size = get_frame_size(&tmp);
	struct mbuf_pool *pool;
	int ret = mbuf_pool_new(mbuf_mem_generic_impl,
				max_frame_size * 10,
				0,
				MBUF_POOL_LOW_MEM_GROW,
				0,
				"audio",
				&pool);
	if (ret != 0)
		return NULL;
	return pool;
}


/* Set buffer into an audio frame
 * - If the memory parameter is NULL, a new memory will be created.
 */
static void set_buffer(struct mbuf_audio_frame *frame,
		       struct mbuf_mem *base_mem)
{
	struct adef_frame frame_info;
	size_t buffer_size;
	struct mbuf_mem *buf_mem;
	bool internal_mem = false;

	int ret = mbuf_audio_frame_get_frame_info(frame, &frame_info);
	CU_ASSERT_EQUAL(ret, 0);
	if (ret != 0)
		return;

	buffer_size = get_frame_size(&frame_info);

	if (!base_mem) {
		ret = mbuf_mem_generic_new(buffer_size, &base_mem);
		if (ret != 0)
			return;
		internal_mem = true;
	}
	buf_mem = base_mem;

	ret = mbuf_audio_frame_set_buffer(frame, base_mem, 0, buffer_size);
	CU_ASSERT_EQUAL(ret, 0);
	if (internal_mem)
		mbuf_mem_unref(base_mem);
}


static void test_mbuf_audio_frame_single(void)
{
	struct mbuf_mem *mem;
	struct adef_frame frame_info;
	struct mbuf_audio_frame *frame;

	init_frame_info(&frame_info, ADEF_ENCODING_AAC_LC);

	/* Create the pool, frame, and memories used by the test */
	struct mbuf_pool *pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);
	int ret = mbuf_audio_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem);
	CU_ASSERT_EQUAL(ret, 0);

	/* Add the buffer to the frame */
	set_buffer(frame, mem);

	/* Unref the memories which are no longer used */
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);

	/* Finalize the frame */
	ret = mbuf_audio_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Cleanup */
	ret = mbuf_audio_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_audio_frame_infos(void)
{
	int ret;
	struct mbuf_audio_frame *frame;
	struct adef_frame frame_info;
	struct adef_frame out_frame_info;
	struct mbuf_mem_info mem_info;

	init_frame_info(&frame_info, ADEF_ENCODING_AAC_LC);

	/* Create & finalize the frame used by the test */
	ret = mbuf_audio_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	set_buffer(frame, NULL);
	ret = mbuf_audio_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Get back and compare the frame_info */
	ret = mbuf_audio_frame_get_frame_info(frame, &out_frame_info);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(
		memcmp(&frame_info, &out_frame_info, sizeof(frame_info)), 0);

	/* Get the memory info for the buffer */
	ret = mbuf_audio_frame_get_buffer_mem_info(frame, &mem_info);
	CU_ASSERT_EQUAL(ret, 0);
	/* The memory must be of type "generic-wrap" */
	CU_ASSERT_EQUAL(mem_info.cookie, mbuf_mem_generic_wrap_cookie);

	/* Cleanup */
	ret = mbuf_audio_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_audio_frame_pool_origin(void)
{
	int ret;
	struct mbuf_pool *pool;
	struct mbuf_mem *mem_pool, *mem_non_pool;
	struct mbuf_audio_frame *frame1, *frame2, *frame3;
	struct adef_frame frame_info;
	bool any, all;

	init_frame_info(&frame_info, ADEF_ENCODING_AAC_LC);
	size_t frame_size = get_frame_size(&frame_info);

	/* Create the pools/mem/frames */
	pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);
	ret = mbuf_audio_frame_new(&frame_info, &frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_new(&frame_info, &frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_new(&frame_info, &frame3);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem_pool);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_generic_new(frame_size, &mem_non_pool);
	CU_ASSERT_EQUAL(ret, 0);

	/* Fill & finalize the frames:
	 * frame1 will not use any memory
	 * frame2 will use a memory not from the pool
	 * frame3 will only use memory from the pool
	 */
	set_buffer(frame1, NULL);
	ret = mbuf_audio_frame_finalize(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	set_buffer(frame2, mem_non_pool);
	ret = mbuf_audio_frame_finalize(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	set_buffer(frame3, mem_pool);
	ret = mbuf_audio_frame_finalize(frame3);
	CU_ASSERT_EQUAL(ret, 0);

	/* Check pool origin for each frame */
	ret = mbuf_audio_frame_uses_mem_from_pool(frame1, pool, &any, &all);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(any);
	CU_ASSERT_FALSE(all);
	ret = mbuf_audio_frame_uses_mem_from_pool(frame2, pool, &any, &all);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(any);
	CU_ASSERT_FALSE(all);
	ret = mbuf_audio_frame_uses_mem_from_pool(frame3, pool, &any, &all);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_TRUE(any);
	CU_ASSERT_TRUE(all);

	/* Cleanup */
	ret = mbuf_audio_frame_unref(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_unref(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_unref(frame3);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem_pool);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem_non_pool);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_audio_frame_bad_args(void)
{
	int ret;
	struct mbuf_mem *mem, *mem_cp;
	struct mbuf_audio_frame *frame, *frame_cp;
	struct mbuf_audio_frame_queue *queue;
	struct adef_frame frame_info;
	struct pomp_evt *evt;
	size_t buf_size;
	const void *data;
	void *rwdata;
	size_t len;
	const void *tmp;
	size_t tmp_size;
	struct vmeta_frame *meta;
	bool any, all;
	struct mbuf_mem_info mem_info;

	init_frame_info(&frame_info, ADEF_ENCODING_AAC_LC);

	buf_size = get_frame_size(&frame_info);

	/* Create a pool for the test */
	struct mbuf_pool *pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);

	/* Bad arguments for constructors */
	ret = mbuf_audio_frame_new(NULL, &frame);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_new(&frame_info, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_queue_new(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_queue_new_with_args(NULL, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* Get real objects for following tests */
	ret = mbuf_audio_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem_cp);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_new(&queue);
	CU_ASSERT_EQUAL(ret, 0);
	ret = vmeta_frame_new(VMETA_FRAME_TYPE_PROTO, &meta);
	CU_ASSERT_EQUAL(ret, 0);

	/* Add buffer, finalize frame & get the buffer for reader API tests */
	set_buffer(frame, mem);
	ret = mbuf_audio_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_get_buffer(frame, &tmp, &tmp_size);
	CU_ASSERT_EQUAL(ret, 0);

	/* Bad arguments for other functions */
	ret = mbuf_audio_frame_ref(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_unref(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_set_frame_info(NULL, &frame_info);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_set_frame_info(frame, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_set_buffer(NULL, mem, 0, buf_size);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_set_buffer(frame, NULL, 0, buf_size);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_finalize(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_uses_mem_from_pool(NULL, pool, &any, &all);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_uses_mem_from_pool(frame, NULL, &any, &all);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_get_buffer_mem_info(NULL, &mem_info);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_get_buffer_mem_info(frame, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_get_buffer(NULL, &data, &len);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_get_buffer(frame, NULL, &len);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_get_buffer(frame, &data, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	data = (void *)(intptr_t)1; /* Make sure data is non-null */
	ret = mbuf_audio_frame_release_buffer(NULL, data);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_release_buffer(frame, data);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_get_rw_buffer(NULL, &rwdata, &len);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_get_rw_buffer(frame, NULL, &len);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_get_rw_buffer(frame, &rwdata, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	rwdata = (void *)(intptr_t)1; /* Make sure rwdata is non-null */
	ret = mbuf_audio_frame_release_rw_buffer(NULL, rwdata);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_release_rw_buffer(frame, rwdata);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_copy(NULL, mem_cp, &frame_cp);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_copy(frame, NULL, &frame_cp);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_copy(frame, mem_cp, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_get_frame_info(NULL, &frame_info);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_get_frame_info(frame, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_queue_push(NULL, frame);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_queue_push(queue, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_queue_peek(NULL, &frame_cp);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_queue_peek(queue, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_queue_peek_at(NULL, 0, &frame_cp);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_queue_peek_at(queue, 0, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_queue_pop(NULL, &frame_cp);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_queue_pop(queue, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_queue_flush(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_queue_get_event(NULL, &evt);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_queue_get_event(queue, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_queue_get_count(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = mbuf_audio_frame_queue_destroy(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* Cleanup */
	ret = vmeta_frame_unref(meta);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_destroy(queue);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem_cp);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_release_buffer(frame, tmp);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_audio_frame_bad_state(void)
{
	int ret;
	struct mbuf_mem *mem;
	struct mbuf_audio_frame *frame;
	struct adef_frame frame_info_aac;
	struct adef_frame frame_info_pcm;
	struct adef_frame frame_info_out;
	size_t buf_size;
	const void *data;
	void *rwdata;
	size_t len;

	init_frame_info(&frame_info_aac, ADEF_ENCODING_AAC_LC);
	init_frame_info(&frame_info_pcm, ADEF_ENCODING_PCM);

	buf_size = get_frame_size(&frame_info_aac);

	/* Create the pool, frame, and memories used by the test */
	struct mbuf_pool *pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);
	ret = mbuf_audio_frame_new(&frame_info_aac, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem);
	CU_ASSERT_EQUAL(ret, 0);

	/* Check that we can properly get/set the frame_info */
	ret = mbuf_audio_frame_get_frame_info(frame, &frame_info_out);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(
		adef_format_cmp(&frame_info_out.format, &frame_info_aac.format),
		1);
	ret = mbuf_audio_frame_set_frame_info(frame, &frame_info_pcm);
	ret = mbuf_audio_frame_get_frame_info(frame, &frame_info_out);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(
		adef_format_cmp(&frame_info_out.format, &frame_info_pcm.format),
		1);

	/* We should not be able to finalize a frame without buffer */
	ret = mbuf_audio_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, -EPROTO);

	/* Add the buffer to the frame */
	set_buffer(frame, mem);

	/* Buffer is not finalized yet, getters should fail */
	ret = mbuf_audio_frame_get_buffer(frame, &data, &len);
	CU_ASSERT_EQUAL(ret, -EBUSY);

	/* Finalize now */
	ret = mbuf_audio_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Buffer is now read-only, setting the buffer should fail */
	ret = mbuf_audio_frame_set_buffer(frame, mem, 0, buf_size);
	CU_ASSERT_EQUAL(ret, -EBUSY);
	/* Setting the frame_info should fail too */
	ret = mbuf_audio_frame_set_frame_info(frame, &frame_info_aac);
	CU_ASSERT_EQUAL(ret, -EBUSY);

	/* Take a read-only reference on the buffer */
	ret = mbuf_audio_frame_get_buffer(frame, &data, &len);
	CU_ASSERT_EQUAL(ret, 0);
	/* Try to take a read-write reference on the buffer.
	 * Frame should be read-locked */
	ret = mbuf_audio_frame_get_rw_buffer(frame, &rwdata, &len);
	CU_ASSERT_EQUAL(ret, -EBUSY);

	/* Release the buffer */
	ret = mbuf_audio_frame_release_buffer(frame, data);
	CU_ASSERT_EQUAL(ret, 0);
	/* Double release of the buffer should fail */
	ret = mbuf_audio_frame_release_buffer(frame, data);
	CU_ASSERT_EQUAL(ret, -EALREADY);

	/* Take a read-write reference on the buffer */
	ret = mbuf_audio_frame_get_rw_buffer(frame, &rwdata, &len);
	CU_ASSERT_EQUAL(ret, 0);
	/* Try to take a read-only reference on the buffer.
	 * Frame should be write-locked */
	ret = mbuf_audio_frame_get_buffer(frame, &data, &len);
	CU_ASSERT_EQUAL(ret, -EBUSY);

	/* Release the buffer */
	ret = mbuf_audio_frame_release_rw_buffer(frame, rwdata);
	CU_ASSERT_EQUAL(ret, 0);
	/* Double release of the buffer should fail */
	ret = mbuf_audio_frame_release_rw_buffer(frame, rwdata);
	CU_ASSERT_EQUAL(ret, -EALREADY);

	/* Cleanup */
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_audio_frame_queue(void)
{
	int ret;
	struct adef_frame frame_info;
	struct mbuf_audio_frame *frame1, *frame2, *frame3, *out_frame;
	struct mbuf_audio_frame_queue *queue;

	init_frame_info(&frame_info, ADEF_ENCODING_AAC_LC);

	/* Create the frames and the queue used by the test */
	ret = mbuf_audio_frame_new(&frame_info, &frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_new(&frame_info, &frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_new(&frame_info, &frame3);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_new(&queue);
	CU_ASSERT_EQUAL(ret, 0);

	/* Peek / Pop from empty queue should fail */
	ret = mbuf_audio_frame_queue_peek(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);
	ret = mbuf_audio_frame_queue_peek_at(queue, 0, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);
	ret = mbuf_audio_frame_queue_pop(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);
	ret = mbuf_audio_frame_queue_get_count(queue);
	CU_ASSERT_EQUAL(ret, 0);

	/* Pushing a non-finalized frame should fail */
	ret = mbuf_audio_frame_queue_push(queue, frame1);
	CU_ASSERT_EQUAL(ret, -EBUSY);

	/* Finalize frames for the test */
	set_buffer(frame1, NULL);
	ret = mbuf_audio_frame_finalize(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	set_buffer(frame2, NULL);
	ret = mbuf_audio_frame_finalize(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	set_buffer(frame3, NULL);
	ret = mbuf_audio_frame_finalize(frame3);
	CU_ASSERT_EQUAL(ret, 0);

	/* Push three frames in order */
	ret = mbuf_audio_frame_queue_push(queue, frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_push(queue, frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_push(queue, frame3);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_get_count(queue);
	CU_ASSERT_EQUAL(ret, 3);

	/* Peek and compare to frame 1 */
	ret = mbuf_audio_frame_queue_peek(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(out_frame, frame1);
	ret = mbuf_audio_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Peek at index 0 and compare to frame 1 */
	ret = mbuf_audio_frame_queue_peek_at(queue, 0, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(out_frame, frame1);
	ret = mbuf_audio_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Peek at index 1 and compare to frame 2 */
	ret = mbuf_audio_frame_queue_peek_at(queue, 1, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(out_frame, frame2);
	ret = mbuf_audio_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Pop and compare to frame 1 */
	ret = mbuf_audio_frame_queue_pop(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(out_frame, frame1);
	ret = mbuf_audio_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_get_count(queue);
	CU_ASSERT_EQUAL(ret, 2);

	/* Peek and compare to frame 2 */
	ret = mbuf_audio_frame_queue_peek(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(out_frame, frame2);
	ret = mbuf_audio_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_get_count(queue);
	CU_ASSERT_EQUAL(ret, 2);

	/* Peek at index 0 and compare to frame 2 */
	ret = mbuf_audio_frame_queue_peek_at(queue, 0, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(out_frame, frame2);
	ret = mbuf_audio_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Peek at index 1 and compare to frame 3 */
	ret = mbuf_audio_frame_queue_peek_at(queue, 1, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(out_frame, frame3);
	ret = mbuf_audio_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Flush */
	ret = mbuf_audio_frame_queue_flush(queue);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_get_count(queue);
	CU_ASSERT_EQUAL(ret, 0);

	/* Peek / Pop from flushed queue should fail */
	ret = mbuf_audio_frame_queue_peek(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);
	ret = mbuf_audio_frame_queue_peek_at(queue, 0, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);
	ret = mbuf_audio_frame_queue_pop(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);

	/* Cleanup */
	ret = mbuf_audio_frame_unref(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_unref(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_unref(frame3);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_destroy(queue);
	CU_ASSERT_EQUAL(ret, 0);
}


struct test_mbuf_audio_frame_queue_flush_userdata {
	bool freed;
};


static void
test_mbuf_audio_frame_queue_flush_free(void *data, size_t len, void *userdata)
{
	struct test_mbuf_audio_frame_queue_flush_userdata *ud = userdata;
	free(data);
	ud->freed = true;
}


static void test_mbuf_audio_frame_queue_flush(void)
{
	int ret;
	struct adef_frame frame_info;
	struct mbuf_mem *mem;
	struct mbuf_audio_frame *frame;
	struct mbuf_audio_frame_queue *queue;
	struct test_mbuf_audio_frame_queue_flush_userdata ud = {
		.freed = false,
	};

	init_frame_info(&frame_info, ADEF_ENCODING_AAC_LC);
	size_t datalen = get_frame_size(&frame_info);
	void *data = malloc(datalen);
	CU_ASSERT_PTR_NOT_NULL(data);

	/* Create the frame and the queue used by the test */
	ret = mbuf_audio_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_generic_wrap(data,
				    datalen,
				    test_mbuf_audio_frame_queue_flush_free,
				    &ud,
				    &mem);
	CU_ASSERT_EQUAL(ret, 0);
	set_buffer(frame, mem);
	ret = mbuf_audio_frame_finalize(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_new(&queue);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);

	/* Push the frame */
	ret = mbuf_audio_frame_queue_push(queue, frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Unref the frames */
	ret = mbuf_audio_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(ud.freed);

	/* Flush the queue */
	ret = mbuf_audio_frame_queue_flush(queue);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_TRUE(ud.freed);

	/* Cleanup */
	ret = mbuf_audio_frame_queue_destroy(queue);
	CU_ASSERT_EQUAL(ret, 0);
}


/* Userdata for the pomp_evt callback */
struct audio_queue_evt_userdata {
	struct mbuf_audio_frame_queue *queue;
	int expected_frames;
};


/* pomp_evt callback for the queue, this function pops all frames in the queue
 * and decrement userdata->expected_frames for each frame. */
static void audio_queue_evt(struct pomp_evt *evt, void *userdata)
{
	int ret = 0;
	struct audio_queue_evt_userdata *data = userdata;

	while (true) {
		struct mbuf_audio_frame *frame;

		ret = mbuf_audio_frame_queue_pop(data->queue, &frame);
		if (ret == -EAGAIN)
			break;
		CU_ASSERT_EQUAL(ret, 0);
		ret = mbuf_audio_frame_unref(frame);
		CU_ASSERT_EQUAL(ret, 0);
		data->expected_frames--;
	}
}


static void test_mbuf_audio_frame_queue_evt(void)
{
	int ret;
	struct adef_frame frame_info;
	struct mbuf_audio_frame *frame1, *frame2;
	struct mbuf_audio_frame_queue *queue;
	struct pomp_evt *evt;
	struct pomp_loop *loop;
	struct audio_queue_evt_userdata userdata;

	init_frame_info(&frame_info, ADEF_ENCODING_AAC_LC);

	/* Create the frames and the queue used by the test */
	ret = mbuf_audio_frame_new(&frame_info, &frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_new(&frame_info, &frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_new(&queue);
	CU_ASSERT_EQUAL(ret, 0);
	userdata.queue = queue;

	/* Finalize the frames */
	set_buffer(frame1, NULL);
	ret = mbuf_audio_frame_finalize(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	set_buffer(frame2, NULL);
	ret = mbuf_audio_frame_finalize(frame2);
	CU_ASSERT_EQUAL(ret, 0);

	/* Create a loop, get the queue event, and attach it to the loop */
	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL(loop);
	ret = mbuf_audio_frame_queue_get_event(queue, &evt);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_evt_attach_to_loop(evt, loop, audio_queue_evt, &userdata);

	/* Push two frames in order */
	ret = mbuf_audio_frame_queue_push(queue, frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_push(queue, frame2);
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
	ret = mbuf_audio_frame_queue_push(queue, frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_flush(queue);
	CU_ASSERT_EQUAL(ret, 0);

	/* Rerun the loop and expect a timeout */
	ret = pomp_loop_wait_and_process(loop, 100);
	CU_ASSERT_EQUAL(ret, -ETIMEDOUT);

	/* Cleanup */
	ret = pomp_evt_detach_from_loop(evt, loop);
	CU_ASSERT_EQUAL(ret, 0);
	ret = pomp_loop_destroy(loop);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_unref(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_unref(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_destroy(queue);
	CU_ASSERT_EQUAL(ret, 0);
}


/* queue filter function which refuses all frames */
static bool queue_filter_none(struct mbuf_audio_frame *frame, void *userdata)
{
	return false;
}


/* queue filter function which accepts all frames, equivalent to no filter
 * function */
static bool queue_filter_all(struct mbuf_audio_frame *frame, void *userdata)
{
	return true;
}


/* queue filter function which only accepts AAC frames */
static bool _queue_filter_is_encoding(struct mbuf_audio_frame *frame,
				      void *userdata,
				      enum adef_encoding encoding)
{
	int ret;

	CU_ASSERT_PTR_NOT_NULL(frame);

	struct adef_frame frame_info;

	ret = mbuf_audio_frame_get_frame_info(frame, &frame_info);
	CU_ASSERT_EQUAL(ret, 0);

	return (frame_info.format.encoding == encoding);
}


/* queue filter function which only accepts PCM frames */
static bool queue_filter_is_pcm(struct mbuf_audio_frame *frame, void *userdata)
{
	return _queue_filter_is_encoding(frame, userdata, ADEF_ENCODING_PCM);
}


/* queue filter function which only accepts AAC_LC frames */
static bool queue_filter_is_aac_lc(struct mbuf_audio_frame *frame,
				   void *userdata)
{
	return _queue_filter_is_encoding(frame, userdata, ADEF_ENCODING_AAC_LC);
}


static void test_mbuf_audio_frame_queue_filter(void)
{
	int ret;
	struct adef_frame frame_info_aac, frame_info_pcm;
	struct mbuf_mem *mem;
	struct mbuf_audio_frame *frame_aac, *frame_pcm;
	struct mbuf_audio_frame_queue *queue_none, *queue_all, *queue_pcm,
		*queue_aac;

	init_frame_info(&frame_info_aac, ADEF_ENCODING_AAC_LC);
	init_frame_info(&frame_info_pcm, ADEF_ENCODING_PCM);

	/* Create the pool, frames and memories used by the test */
	struct mbuf_pool *pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);
	ret = mbuf_audio_frame_new(&frame_info_aac, &frame_aac);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_new(&frame_info_pcm, &frame_pcm);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem);
	CU_ASSERT_EQUAL(ret, 0);

	set_buffer(frame_aac, mem);
	set_buffer(frame_pcm, mem);

	/* Finalize both frames */
	ret = mbuf_audio_frame_finalize(frame_aac);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_finalize(frame_pcm);
	CU_ASSERT_EQUAL(ret, 0);

	/* Create the three test queues, one with each filter function */
	struct mbuf_audio_frame_queue_args args = {
		.filter = queue_filter_none,
	};
	ret = mbuf_audio_frame_queue_new_with_args(&args, &queue_none);
	CU_ASSERT_EQUAL(ret, 0);
	args.filter = queue_filter_all;
	ret = mbuf_audio_frame_queue_new_with_args(&args, &queue_all);
	CU_ASSERT_EQUAL(ret, 0);
	args.filter = queue_filter_is_pcm;
	ret = mbuf_audio_frame_queue_new_with_args(&args, &queue_pcm);
	CU_ASSERT_EQUAL(ret, 0);
	args.filter = queue_filter_is_aac_lc;
	ret = mbuf_audio_frame_queue_new_with_args(&args, &queue_aac);
	CU_ASSERT_EQUAL(ret, 0);

	/* Push to queue_none should fail */
	ret = mbuf_audio_frame_queue_push(queue_none, frame_aac);
	CU_ASSERT_EQUAL(ret, -EPROTO);
	ret = mbuf_audio_frame_queue_push(queue_none, frame_pcm);
	CU_ASSERT_EQUAL(ret, -EPROTO);
	ret = mbuf_audio_frame_queue_get_count(queue_none);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_flush(queue_none);
	CU_ASSERT_EQUAL(ret, 0);

	/* Pushing to queue_all should work */
	ret = mbuf_audio_frame_queue_push(queue_all, frame_aac);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_push(queue_all, frame_pcm);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_get_count(queue_all);
	CU_ASSERT_EQUAL(ret, 2);
	ret = mbuf_audio_frame_queue_flush(queue_all);
	CU_ASSERT_EQUAL(ret, 0);

	/* Pushing to queue_aac should only work for frame_pcm */
	ret = mbuf_audio_frame_queue_push(queue_pcm, frame_aac);
	CU_ASSERT_EQUAL(ret, -EPROTO);
	ret = mbuf_audio_frame_queue_push(queue_pcm, frame_pcm);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_get_count(queue_pcm);
	CU_ASSERT_EQUAL(ret, 1);
	ret = mbuf_audio_frame_queue_flush(queue_pcm);
	CU_ASSERT_EQUAL(ret, 0);

	/* Pushing to queue_aac should only work for frame_aac */
	ret = mbuf_audio_frame_queue_push(queue_aac, frame_pcm);
	CU_ASSERT_EQUAL(ret, -EPROTO);
	ret = mbuf_audio_frame_queue_push(queue_aac, frame_aac);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_get_count(queue_aac);
	CU_ASSERT_EQUAL(ret, 1);
	ret = mbuf_audio_frame_queue_flush(queue_aac);
	CU_ASSERT_EQUAL(ret, 0);

	/* Cleanup */
	ret = mbuf_audio_frame_unref(frame_aac);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_unref(frame_pcm);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_destroy(queue_none);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_destroy(queue_all);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_destroy(queue_pcm);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_destroy(queue_aac);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_mbuf_audio_frame_queue_drop(void)
{
	int ret;
	struct adef_frame frame_info;
	struct mbuf_audio_frame *frame1, *frame2, *out_frame;
	struct mbuf_audio_frame_queue *queue;

	init_frame_info(&frame_info, ADEF_ENCODING_AAC_LC);

	/* Create the frames used by the test */
	ret = mbuf_audio_frame_new(&frame_info, &frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_new(&frame_info, &frame2);
	CU_ASSERT_EQUAL(ret, 0);

	/* Finalize both frames */
	set_buffer(frame1, NULL);
	ret = mbuf_audio_frame_finalize(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	set_buffer(frame2, NULL);
	ret = mbuf_audio_frame_finalize(frame2);
	CU_ASSERT_EQUAL(ret, 0);

	/* Create the test queues, with max_frames = 1 */
	struct mbuf_audio_frame_queue_args args = {
		.max_frames = 1,
	};
	ret = mbuf_audio_frame_queue_new_with_args(&args, &queue);
	CU_ASSERT_EQUAL(ret, 0);

	/* Push the two frames, it should succeed every time */
	ret = mbuf_audio_frame_queue_push(queue, frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_push(queue, frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_get_count(queue);
	CU_ASSERT_EQUAL(ret, 1);

	/* Pop a frame, it should be frame 2 */
	ret = mbuf_audio_frame_queue_pop(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_EQUAL(frame2, out_frame);
	ret = mbuf_audio_frame_unref(out_frame);
	CU_ASSERT_EQUAL(ret, 0);

	/* Popping a second frame should fail */
	ret = mbuf_audio_frame_queue_pop(queue, &out_frame);
	CU_ASSERT_EQUAL(ret, -EAGAIN);
	ret = mbuf_audio_frame_queue_get_count(queue);
	CU_ASSERT_EQUAL(ret, 0);

	/* Cleanup */
	ret = mbuf_audio_frame_unref(frame1);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_unref(frame2);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_queue_destroy(queue);
	CU_ASSERT_EQUAL(ret, 0);
}


struct mbuf_ancillary_data_dyn_test {
	char *dyn_str;
};


struct mbuf_ancillary_data_test {
	const char *str_name;
	const char *str_value;
	bool has_str;
	const char *buf_name;
	uint8_t buf_value[5];
	const char *buf_dyn_name;
	struct mbuf_ancillary_data_dyn_test buf_dyn_value;
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


static void
mbuf_audio_frame_ancillary_data_cleaner_cb(struct mbuf_ancillary_data *data,
					   void *userdata)
{
	struct mbuf_ancillary_data_dyn_test *buf_dyn_value =
		(struct mbuf_ancillary_data_dyn_test *)userdata;
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf_dyn_value->dyn_str);

	free(buf_dyn_value->dyn_str);
	buf_dyn_value->dyn_str = NULL;
}


static void test_mbuf_audio_frame_ancillary_data(void)
{
	int ret;
	struct adef_frame frame_info;
	struct mbuf_mem *mem;
	struct mbuf_audio_frame *frame, *copy;

	struct mbuf_ancillary_data_cbs ancillary_cbs = {};
	struct mbuf_ancillary_data_test adt = {
		.str_name = "str",
		.str_value = "test",
		.buf_name = "buf",
		.buf_value = {1, 2, 3, 4, 5},
		.buf_dyn_name = "buf_dyn",
		.buf_dyn_value = {},
	};

	init_frame_info(&frame_info, ADEF_ENCODING_AAC_LC);

	/* Create the pool, frame and memory used by the test */
	struct mbuf_pool *pool = create_pool();
	CU_ASSERT_PTR_NOT_NULL(pool);
	ret = mbuf_audio_frame_new(&frame_info, &frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_get(pool, &mem);
	CU_ASSERT_EQUAL(ret, 0);
	set_buffer(frame, NULL);

	/* Iterate over an empty ancillary data list */
	ret = mbuf_audio_frame_foreach_ancillary_data(
		frame, ancillary_iterator, &adt);
	CU_ASSERT_EQUAL(ret, 0);

	/* Add the string and relaunch iteration */
	ret = mbuf_audio_frame_add_ancillary_string(
		frame, adt.str_name, adt.str_value);
	CU_ASSERT_EQUAL(ret, 0);
	adt.has_str = true;
	adt.has_buf = false;
	ret = mbuf_audio_frame_foreach_ancillary_data(
		frame, ancillary_iterator, &adt);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(adt.has_str);
	CU_ASSERT_FALSE(adt.has_buf);

	/* Finalize the frame, we should still be able to manipulate ancillary
	 * data */
	ret = mbuf_audio_frame_finalize(frame);

	/* Add the buffer and relaunch iteration */
	ret = mbuf_audio_frame_add_ancillary_buffer(
		frame, adt.buf_name, adt.buf_value, sizeof(adt.buf_value));
	CU_ASSERT_EQUAL(ret, 0);
	adt.has_str = true;
	adt.has_buf = true;
	ret = mbuf_audio_frame_foreach_ancillary_data(
		frame, ancillary_iterator, &adt);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(adt.has_str);
	CU_ASSERT_FALSE(adt.has_buf);

	/* Allocate dynamic data that will be freed by the
	 * ancillary_data_cleaner_cb */
	adt.buf_dyn_value.dyn_str = strdup("dyn_str");
	CU_ASSERT_PTR_NOT_NULL_FATAL(adt.buf_dyn_value.dyn_str);

	ancillary_cbs.cleaner = mbuf_audio_frame_ancillary_data_cleaner_cb;
	ancillary_cbs.cleaner_userdata = &adt.buf_dyn_value;

	/* Add the dynamic buffer */
	ret = mbuf_audio_frame_add_ancillary_buffer_with_cbs(
		frame,
		adt.buf_dyn_name,
		&adt.buf_dyn_value,
		sizeof(adt.buf_dyn_value),
		&ancillary_cbs);
	CU_ASSERT_EQUAL(ret, 0);

	/* Delete the ancillary buffer, ancillary_data_cleaner_cb must be
	 * called to free dyn_str */
	ret = mbuf_audio_frame_remove_ancillary_data(frame, adt.buf_dyn_name);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NULL(adt.buf_dyn_value.dyn_str);

	/* Test the single element getter */
	struct mbuf_ancillary_data *tmp;
	const void *tmpdata;
	size_t tmplen;
	ret = mbuf_audio_frame_get_ancillary_data(frame, adt.str_name, &tmp);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_TRUE(mbuf_ancillary_data_is_string(tmp));
	CU_ASSERT_STRING_EQUAL(mbuf_ancillary_data_get_string(tmp),
			       adt.str_value);
	ret = mbuf_ancillary_data_unref(tmp);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_get_ancillary_data(frame, adt.buf_name, &tmp);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(mbuf_ancillary_data_is_string(tmp));
	tmpdata = mbuf_ancillary_data_get_buffer(tmp, &tmplen);
	CU_ASSERT_EQUAL(tmplen, sizeof(adt.buf_value));
	if (tmplen == sizeof(adt.buf_value))
		CU_ASSERT_EQUAL(memcmp(tmpdata, adt.buf_value, tmplen), 0);
	ret = mbuf_ancillary_data_unref(tmp);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_get_ancillary_data(frame, "don't exist", &tmp);
	CU_ASSERT_EQUAL(ret, -ENOENT);

	/* Copy the frame, all the ancillary data should be copied too */
	ret = mbuf_audio_frame_copy(frame, mem, &copy);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_finalize(copy);
	CU_ASSERT_EQUAL(ret, 0);

	/* Remove the string and relaunch iteration */
	ret = mbuf_audio_frame_remove_ancillary_data(frame, adt.str_name);
	CU_ASSERT_EQUAL(ret, 0);
	adt.has_str = false;
	adt.has_buf = true;
	ret = mbuf_audio_frame_foreach_ancillary_data(
		frame, ancillary_iterator, &adt);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(adt.has_str);
	CU_ASSERT_FALSE(adt.has_buf);

	/* Test invalid cases */
	/* Add an already present data */
	ret = mbuf_audio_frame_add_ancillary_buffer(
		frame, adt.buf_name, adt.buf_value, sizeof(adt.buf_value));
	CU_ASSERT_EQUAL(ret, -EEXIST);
	/* Remove an absent data */
	ret = mbuf_audio_frame_remove_ancillary_data(frame, adt.str_name);
	CU_ASSERT_EQUAL(ret, -ENOENT);

	/* Iterate on the copy, both data should be present */
	adt.has_str = true;
	adt.has_buf = true;
	ret = mbuf_audio_frame_foreach_ancillary_data(
		copy, ancillary_iterator, &adt);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_FALSE(adt.has_str);
	CU_ASSERT_FALSE(adt.has_buf);

	/* Cleanup */
	ret = mbuf_audio_frame_unref(frame);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_audio_frame_unref(copy);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}


CU_TestInfo g_mbuf_test_audio_frame[] = {
	{(char *)"single", &test_mbuf_audio_frame_single},
	{(char *)"get_infos", &test_mbuf_audio_frame_infos},
	{(char *)"pool_origin", &test_mbuf_audio_frame_pool_origin},
	{(char *)"bad_args", &test_mbuf_audio_frame_bad_args},
	{(char *)"bad_state", &test_mbuf_audio_frame_bad_state},
	{(char *)"queue", &test_mbuf_audio_frame_queue},
	{(char *)"queue_flush", &test_mbuf_audio_frame_queue_flush},
	{(char *)"queue_event", &test_mbuf_audio_frame_queue_evt},
	{(char *)"queue_filter", &test_mbuf_audio_frame_queue_filter},
	{(char *)"queue_drop", &test_mbuf_audio_frame_queue_drop},
	{(char *)"ancillary_data", &test_mbuf_audio_frame_ancillary_data},
	CU_TEST_INFO_NULL,
};
