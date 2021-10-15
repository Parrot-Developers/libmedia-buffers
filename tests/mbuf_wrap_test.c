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


#define TEST_LEN 1024
#define TEST_USERDATA (void *)(uintptr_t)0x12345
#define TEST_CONTENT 0x42
static void *tmp;


static void test_malloc_release(void *data, size_t len, void *userdata)
{
	CU_ASSERT_EQUAL(data, tmp);
	CU_ASSERT_EQUAL(len, TEST_LEN);
	CU_ASSERT_EQUAL(userdata, TEST_USERDATA);

	uint8_t *d = data;
	bool ok = true;
	for (size_t i = 0; i < len && ok; i++)
		ok = (d[i] == TEST_CONTENT);
	if (!ok)
		CU_FAIL("malloc_mem content corrupted");

	tmp = NULL;
	free(data);
}


static void test_mbuf_wrap_malloc(void)
{
	int ret;
	struct mbuf_mem *mem;

	size_t len = TEST_LEN;
	uint8_t *data = malloc(len);
	CU_ASSERT_PTR_NOT_NULL(data);

	tmp = data;

	ret = mbuf_mem_generic_wrap(
		data, len, test_malloc_release, TEST_USERDATA, &mem);
	CU_ASSERT_EQUAL(ret, 0);

	for (size_t i = 0; i < len; i++)
		data[i] = TEST_CONTENT;

	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);
}


static uint8_t static_data[TEST_LEN];


static void test_static_release(void *data, size_t len, void *userdata)
{
	CU_ASSERT_EQUAL(data, static_data);
	CU_ASSERT_EQUAL(len, TEST_LEN);
	CU_ASSERT_EQUAL(userdata, TEST_USERDATA);

	uint8_t *d = data;
	bool ok = true;
	for (size_t i = 0; i < len && ok; i++)
		ok = (d[i] == TEST_CONTENT);
	if (!ok)
		CU_FAIL("static_mem content corrupted");
}


static void test_mbuf_wrap_static(void)
{
	int ret;
	struct mbuf_mem *mem;

	size_t len = TEST_LEN;
	uint8_t *data = static_data;

	tmp = data;

	ret = mbuf_mem_generic_wrap(
		data, len, test_static_release, TEST_USERDATA, &mem);
	CU_ASSERT_EQUAL(ret, 0);

	for (size_t i = 0; i < len; i++)
		data[i] = TEST_CONTENT;

	ret = mbuf_mem_unref(mem);
	CU_ASSERT_EQUAL(ret, 0);
}


CU_TestInfo g_mbuf_test_wrap[] = {
	{(char *)"malloc/free", &test_mbuf_wrap_malloc},
	{(char *)"static", &test_mbuf_wrap_static},
	CU_TEST_INFO_NULL,
};
