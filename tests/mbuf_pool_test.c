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


#define MBUF_POOL_TEST_NAME "test-name"


static void test_mbuf_pool_name(void)
{
	struct mbuf_pool *pool;
	const char *name;

	/* Test default name */
	int ret = mbuf_pool_new(mbuf_mem_generic_impl,
				1024,
				MBUF_TEST_POOL_SIZE,
				MBUF_POOL_NO_GROW,
				0,
				NULL,
				&pool);
	CU_ASSERT_EQUAL(ret, 0);

	name = mbuf_pool_get_name(pool);
	CU_ASSERT_PTR_NOT_NULL(name);
	CU_ASSERT_STRING_EQUAL(name, "default");

	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);

	/* Test custom name */
	ret = mbuf_pool_new(mbuf_mem_generic_impl,
			    1024,
			    MBUF_TEST_POOL_SIZE,
			    MBUF_POOL_NO_GROW,
			    0,
			    MBUF_POOL_TEST_NAME,
			    &pool);
	CU_ASSERT_EQUAL(ret, 0);

	name = mbuf_pool_get_name(pool);
	CU_ASSERT_PTR_NOT_NULL(name);
	CU_ASSERT_STRING_EQUAL(name, MBUF_POOL_TEST_NAME);

	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_mbuf_pool(void)
{
	struct mbuf_pool *pool;
	struct mbuf_mem *mem;
	struct mbuf_mem *save[MBUF_TEST_POOL_SIZE];

	/* Create a non-growing pool */
	int ret = mbuf_pool_new(mbuf_mem_generic_impl,
				1024,
				MBUF_TEST_POOL_SIZE,
				MBUF_POOL_NO_GROW,
				0,
				"test",
				&pool);
	CU_ASSERT_EQUAL(ret, 0);

	/* Get all available buffers */
	for (int i = 0; i < MBUF_TEST_POOL_SIZE; i++) {
		ret = mbuf_pool_get(pool, &mem);
		CU_ASSERT_EQUAL(ret, 0);
		save[i] = mem;
	}

	/* Try to get one more buffer, this should fail */
	ret = mbuf_pool_get(pool, &mem);
	CU_ASSERT_EQUAL(ret, -EAGAIN);

	/* Release all acquired buffers */
	for (int i = 0; i < MBUF_TEST_POOL_SIZE; i++) {
		ret = mbuf_mem_unref(save[i]);
		CU_ASSERT_EQUAL(ret, 0);
	}

	/* The pool should be full, with no change in its size */
	size_t cur = 0, max = 0;
	ret = mbuf_pool_get_count(pool, &max, &cur);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(cur, MBUF_TEST_POOL_SIZE);
	CU_ASSERT_EQUAL(max, MBUF_TEST_POOL_SIZE);

	/* Cleanup */
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_mbuf_pool_grow(void)
{
	struct mbuf_pool *pool;
	struct mbuf_mem *mem;
	struct mbuf_mem *save[MBUF_TEST_POOL_SIZE];

	/* Create a growing pool */
	int ret = mbuf_pool_new(mbuf_mem_generic_impl,
				1024,
				MBUF_TEST_POOL_SIZE,
				MBUF_POOL_GROW,
				0,
				"test_grow",
				&pool);
	CU_ASSERT_EQUAL(ret, 0);

	/* Get all available buffers */
	for (int i = 0; i < MBUF_TEST_POOL_SIZE; i++) {
		ret = mbuf_pool_get(pool, &mem);
		CU_ASSERT_EQUAL(ret, 0);
		save[i] = mem;
	}

	/* Get one more buffer and release it. The pool will grow */
	ret = mbuf_pool_get(pool, &mem);
	CU_ASSERT_EQUAL(ret, 0);
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);

	/* Release all acquired buffers */
	for (int i = 0; i < MBUF_TEST_POOL_SIZE; i++) {
		ret = mbuf_mem_unref(save[i]);
		CU_ASSERT_EQUAL(ret, 0);
	}

	/* The pool should be full, with size increase */
	size_t cur = 0, max = 0;
	ret = mbuf_pool_get_count(pool, &max, &cur);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(cur, MBUF_TEST_POOL_SIZE + 1);
	CU_ASSERT_EQUAL(max, MBUF_TEST_POOL_SIZE + 1);

	/* Cleanup */
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_mbuf_pool_grow_max(void)
{
	struct mbuf_pool *pool;
	struct mbuf_mem *mem;
	struct mbuf_mem *save[MBUF_TEST_POOL_SIZE];
	struct mbuf_mem *extra[MBUF_TEST_POOL_SIZE];

	/* Create a growing pool, with max capacity */
	int ret = mbuf_pool_new(mbuf_mem_generic_impl,
				1024,
				MBUF_TEST_POOL_SIZE,
				MBUF_POOL_GROW,
				2 * MBUF_TEST_POOL_SIZE,
				"test_grow_max",
				&pool);
	CU_ASSERT_EQUAL(ret, 0);

	/* Get all available buffers */
	for (int i = 0; i < MBUF_TEST_POOL_SIZE; i++) {
		ret = mbuf_pool_get(pool, &mem);
		CU_ASSERT_EQUAL(ret, 0);
		save[i] = mem;
	}

	/* Get new buffers up to the maximum pool capacity */
	for (int i = 0; i < MBUF_TEST_POOL_SIZE; i++) {
		ret = mbuf_pool_get(pool, &mem);
		CU_ASSERT_EQUAL(ret, 0);
		extra[i] = mem;
	}

	/* Getting a new buffer should fail, the pool is at its maximum capacity
	 */
	ret = mbuf_pool_get(pool, &mem);
	CU_ASSERT_EQUAL(ret, -EAGAIN);

	/* Release the original buffers */
	for (int i = 0; i < MBUF_TEST_POOL_SIZE; i++) {
		ret = mbuf_mem_unref(save[i]);
		CU_ASSERT_EQUAL(ret, 0);
	}

	/* Release the new buffers */
	for (int i = 0; i < MBUF_TEST_POOL_SIZE; i++) {
		ret = mbuf_mem_unref(extra[i]);
		CU_ASSERT_EQUAL(ret, 0);
	}

	/* The pool should be full, with its maximum size */
	size_t cur = 0, max = 0;
	ret = mbuf_pool_get_count(pool, &max, &cur);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(cur, 2 * MBUF_TEST_POOL_SIZE);
	CU_ASSERT_EQUAL(max, 2 * MBUF_TEST_POOL_SIZE);

	/* Cleanup */
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_mbuf_pool_smart_grow(void)
{
	struct mbuf_pool *pool;
	struct mbuf_mem *mem;
	struct mbuf_mem *save[MBUF_TEST_POOL_SIZE];
	struct mbuf_mem *extra[MBUF_TEST_POOL_SIZE];
	size_t cur = 0, max = 0;

	/* Create a smart-growing pool */
	int ret = mbuf_pool_new(mbuf_mem_generic_impl,
				1024,
				MBUF_TEST_POOL_SIZE,
				MBUF_POOL_SMART_GROW,
				0,
				"test_smart_grow",
				&pool);
	CU_ASSERT_EQUAL(ret, 0);

	/* Get all available buffers */
	for (int i = 0; i < MBUF_TEST_POOL_SIZE; i++) {
		ret = mbuf_pool_get(pool, &mem);
		CU_ASSERT_EQUAL(ret, 0);
		save[i] = mem;
	}

	/* The pool should be empty, with its initial size */
	ret = mbuf_pool_get_count(pool, &max, &cur);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(cur, 0);
	CU_ASSERT_EQUAL(max, MBUF_TEST_POOL_SIZE);

	/* Get some new buffers from it */
	for (int i = 0; i < MBUF_TEST_POOL_SIZE; i++) {
		ret = mbuf_pool_get(pool, &mem);
		CU_ASSERT_EQUAL(ret, 0);
		extra[i] = mem;
	}

	/* The pool should still be empty, but with increased size */
	ret = mbuf_pool_get_count(pool, &max, &cur);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(cur, 0);
	CU_ASSERT_EQUAL(max, 2 * MBUF_TEST_POOL_SIZE);

	/* Release half of the original buffers */
	for (int i = 0; i < MBUF_TEST_POOL_SIZE / 2; i++) {
		ret = mbuf_mem_unref(save[i]);
		CU_ASSERT_EQUAL(ret, 0);
	}

	/* The pool should have some buffers available, and since we did not
	 * release enough buffers, the capacity is not changed */
	ret = mbuf_pool_get_count(pool, &max, &cur);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(cur, MBUF_TEST_POOL_SIZE / 2);
	CU_ASSERT_EQUAL(max, 2 * MBUF_TEST_POOL_SIZE);

	/* Release all extra buffers */
	for (int i = 0; i < MBUF_TEST_POOL_SIZE; i++) {
		ret = mbuf_mem_unref(extra[i]);
		CU_ASSERT_EQUAL(ret, 0);
	}

	/* The pool should have its original capacity in available buffers, and
	 * the total size should still be greater */
	ret = mbuf_pool_get_count(pool, &max, &cur);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(cur, MBUF_TEST_POOL_SIZE);
	CU_ASSERT_EQUAL(max, MBUF_TEST_POOL_SIZE * 3 / 2);

	/* Release the rest of the original buffers */
	for (int i = MBUF_TEST_POOL_SIZE / 2; i < MBUF_TEST_POOL_SIZE; i++) {
		ret = mbuf_mem_unref(save[i]);
		CU_ASSERT_EQUAL(ret, 0);
	}

	/* The pool should be back to full, with initial capacity */
	ret = mbuf_pool_get_count(pool, &max, &cur);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(cur, MBUF_TEST_POOL_SIZE);
	CU_ASSERT_EQUAL(max, MBUF_TEST_POOL_SIZE);

	/* Cleanup */
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_mbuf_pool_lowmem_grow(void)
{
	struct mbuf_pool *pool;
	struct mbuf_mem *mem;
	struct mbuf_mem *save[MBUF_TEST_POOL_SIZE];
	size_t cur = 0, max = 0;

	/* Create a lowmem-growing pool */
	int ret = mbuf_pool_new(mbuf_mem_generic_impl,
				1024,
				MBUF_TEST_POOL_SIZE,
				MBUF_POOL_LOW_MEM_GROW,
				0,
				"test_lowmem_grow",
				&pool);
	CU_ASSERT_EQUAL(ret, 0);

	/* Get all available buffers */
	for (int i = 0; i < MBUF_TEST_POOL_SIZE; i++) {
		ret = mbuf_pool_get(pool, &mem);
		CU_ASSERT_EQUAL(ret, 0);
		save[i] = mem;
	}

	/* The pool should be empty, with its initial size */
	ret = mbuf_pool_get_count(pool, &max, &cur);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(cur, 0);
	CU_ASSERT_EQUAL(max, MBUF_TEST_POOL_SIZE);

	/* Get one new buffer from it */
	ret = mbuf_pool_get(pool, &mem);
	CU_ASSERT_EQUAL(ret, 0);

	/* The pool should still be empty, but with increased size */
	ret = mbuf_pool_get_count(pool, &max, &cur);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(cur, 0);
	CU_ASSERT_EQUAL(max, MBUF_TEST_POOL_SIZE + 1);

	/* Release the new buffer */
	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);

	/* The pool should still be empty, but back to initial capacity */
	ret = mbuf_pool_get_count(pool, &max, &cur);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(cur, 0);
	CU_ASSERT_EQUAL(max, MBUF_TEST_POOL_SIZE);

	/* Release all buffers */
	for (int i = 0; i < MBUF_TEST_POOL_SIZE; i++) {
		ret = mbuf_mem_unref(save[i]);
		CU_ASSERT_EQUAL(ret, 0);
	}

	/* The pool should be back to full, with initial capacity */
	ret = mbuf_pool_get_count(pool, &max, &cur);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(cur, MBUF_TEST_POOL_SIZE);
	CU_ASSERT_EQUAL(max, MBUF_TEST_POOL_SIZE);

	/* Cleanup */
	ret = mbuf_pool_destroy(pool);
	CU_ASSERT_EQUAL(ret, 0);
}

CU_TestInfo g_mbuf_test_pool[] = {
	{(char *)"name", &test_mbuf_pool_name},
	{(char *)"nogrow", &test_mbuf_pool},
	{(char *)"grow", &test_mbuf_pool_grow},
	{(char *)"grow-with-max", &test_mbuf_pool_grow_max},
	{(char *)"smart-grow", &test_mbuf_pool_smart_grow},
	{(char *)"lowmem-grow", &test_mbuf_pool_lowmem_grow},
	CU_TEST_INFO_NULL,
};
