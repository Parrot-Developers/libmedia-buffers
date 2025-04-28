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

static void test_mbuf_ancillary_data_build_key(void)
{
	int ret;
	char *str;
	char *expected;
	const char *NAME1 = "com.parrot.key1";
	const uintptr_t PTR1 = 0x123;
	const char *NAME2 = "com.parrot.some.long.key2";
	const uintptr_t PTR2 = 0xABCDEF;

	ret = mbuf_ancillary_data_build_key(NULL, 0, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	ret = mbuf_ancillary_data_build_key(NAME1, 0, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	ret = mbuf_ancillary_data_build_key(NULL, 0, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	expected = strdup(NAME1);
	CU_ASSERT_PTR_NOT_NULL(expected);
	ret = mbuf_ancillary_data_build_key(NAME1, 0, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(str, expected);
	free(str);
	free(expected);

	ret = asprintf(&expected, "%s:%" PRIxPTR, NAME1, PTR1);
	CU_ASSERT_PTR_NOT_NULL(expected);
	ret = mbuf_ancillary_data_build_key(NAME1, PTR1, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(str, expected);
	free(str);
	free(expected);

	ret = asprintf(&expected, "%s:%" PRIxPTR, NAME2, PTR1);
	CU_ASSERT_PTR_NOT_NULL(expected);
	ret = mbuf_ancillary_data_build_key(NAME2, PTR1, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(str, expected);
	free(str);
	free(expected);

	ret = asprintf(&expected, "%s:%" PRIxPTR, NAME2, PTR2);
	CU_ASSERT_PTR_NOT_NULL(expected);
	ret = mbuf_ancillary_data_build_key(NAME2, PTR2, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(str, expected);
	free(str);
	free(expected);
}


static void test_mbuf_ancillary_data_parse_key(void)
{
	int ret;
	const char *KEY1 = "com.parrot.key1:0x123";
	const char *EXPECTED_NAME1 = "com.parrot.key1";
	const uintptr_t EXPECTED_PTR1 = 0x123;
	const char *KEY2 = "com.parrot.some.long.key2:0xABCDEF";
	const char *EXPECTED_NAME2 = "com.parrot.some.long.key2";
	const uintptr_t EXPECTED_PTR2 = 0xABCDEF;

	char *str = NULL;
	uintptr_t ptr = 0;

	ret = mbuf_ancillary_data_parse_key(NULL, NULL, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	ret = mbuf_ancillary_data_parse_key(KEY1, NULL, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	ret = mbuf_ancillary_data_parse_key(NULL, &str, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	ret = mbuf_ancillary_data_parse_key(NULL, NULL, &ptr);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	ret = mbuf_ancillary_data_parse_key(NULL, &str, &ptr);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	ret = mbuf_ancillary_data_parse_key(KEY1, NULL, &ptr);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	ret = mbuf_ancillary_data_parse_key(KEY1, &str, &ptr);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL(str);
	CU_ASSERT_STRING_EQUAL(str, EXPECTED_NAME1);
	CU_ASSERT_EQUAL(ptr, EXPECTED_PTR1);
	free(str);

	ret = mbuf_ancillary_data_parse_key(KEY2, &str, &ptr);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL(str);
	CU_ASSERT_STRING_EQUAL(str, EXPECTED_NAME2);
	CU_ASSERT_EQUAL(ptr, EXPECTED_PTR2);
	free(str);

	ret = mbuf_ancillary_data_parse_key(EXPECTED_NAME1, &str, &ptr);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL(str);
	CU_ASSERT_STRING_EQUAL(str, EXPECTED_NAME1);
	CU_ASSERT_EQUAL(ptr, 0);
	free(str);
}


CU_TestInfo g_mbuf_test_ancillary[] = {
	{(char *)"build-key", &test_mbuf_ancillary_data_build_key},
	{(char *)"parse-key", &test_mbuf_ancillary_data_parse_key},
	CU_TEST_INFO_NULL,
};
