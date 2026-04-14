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


static void test_mbuf_get_supported_implems(void)
{
	int ret;
	const enum mbuf_mem_implem_type *types = NULL;

	ret = mbuf_get_supported_implems(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	ret = mbuf_get_supported_implems(&types);
	CU_ASSERT(ret > 0);
	CU_ASSERT_PTR_NOT_NULL(types);

	bool found_generic = false;
	for (int i = 0; i < ret; i++) {
		CU_ASSERT(types[i] > MBUF_MEM_IMPLEM_TYPE_AUTO);
		CU_ASSERT(types[i] < MBUF_MEM_IMPLEM_TYPE_MAX);

		if (types[i] == MBUF_MEM_IMPLEM_TYPE_GENERIC)
			found_generic = true;
	}

#ifdef BUILD_LIBMEDIA_BUFFERS_MEMORY_GENERIC
	CU_ASSERT_TRUE(found_generic);
#endif
}


static void test_mbuf_get_auto_implem(void)
{
	enum mbuf_mem_implem_type auto_implem;
	const enum mbuf_mem_implem_type *types = NULL;
	int count;

	auto_implem = mbuf_get_auto_implem();
	count = mbuf_get_supported_implems(&types);

	CU_ASSERT(auto_implem > MBUF_MEM_IMPLEM_TYPE_AUTO);
	CU_ASSERT(auto_implem < MBUF_MEM_IMPLEM_TYPE_MAX);

	bool found = false;
	for (int i = 0; i < count; i++) {
		if (types[i] == auto_implem) {
			found = true;
			break;
		}
	}
	CU_ASSERT_TRUE(found);

	if (count > 0) {
		CU_ASSERT_EQUAL(auto_implem, types[0]);
	}
}


static void test_mbuf_mem_implem_type_from_str(void)
{
	enum mbuf_mem_implem_type implem;

	/* KO tests */
	implem = mbuf_mem_implem_type_from_str(NULL);
	CU_ASSERT_EQUAL(implem, MBUF_MEM_IMPLEM_TYPE_AUTO);

	implem = mbuf_mem_implem_type_from_str("unknown");
	CU_ASSERT_EQUAL(implem, MBUF_MEM_IMPLEM_TYPE_AUTO);

	implem = mbuf_mem_implem_type_from_str("");
	CU_ASSERT_EQUAL(implem, MBUF_MEM_IMPLEM_TYPE_AUTO);

	/* OK tests */
	implem = mbuf_mem_implem_type_from_str("auto");
	CU_ASSERT_EQUAL(implem, MBUF_MEM_IMPLEM_TYPE_AUTO);

	implem = mbuf_mem_implem_type_from_str("GENERIC");
	CU_ASSERT_EQUAL(implem, MBUF_MEM_IMPLEM_TYPE_GENERIC);

	implem = mbuf_mem_implem_type_from_str("gEnErIc");
	CU_ASSERT_EQUAL(implem, MBUF_MEM_IMPLEM_TYPE_GENERIC);

	implem = mbuf_mem_implem_type_from_str("shm");
	CU_ASSERT_EQUAL(implem, MBUF_MEM_IMPLEM_TYPE_SHM);
}


static void test_mbuf_mem_implem_type_to_str(void)
{
	const char *str;

	str = mbuf_mem_implem_type_to_str(MBUF_MEM_IMPLEM_TYPE_AUTO);
	CU_ASSERT_PTR_NOT_NULL(str);
	CU_ASSERT_STRING_EQUAL(str, "AUTO");

	str = mbuf_mem_implem_type_to_str(MBUF_MEM_IMPLEM_TYPE_MAX);
	CU_ASSERT_PTR_NOT_NULL(str);
	CU_ASSERT_STRING_EQUAL(str, "UNKNOWN");

	str = mbuf_mem_implem_type_to_str((enum mbuf_mem_implem_type)999);
	CU_ASSERT_PTR_NOT_NULL(str);
	CU_ASSERT_STRING_EQUAL(str, "UNKNOWN");

	str = mbuf_mem_implem_type_to_str(MBUF_MEM_IMPLEM_TYPE_GENERIC);
	CU_ASSERT_PTR_NOT_NULL(str);
	CU_ASSERT_STRING_EQUAL(str, "GENERIC");
}


static void test_mbuf_mem_implem_roundtrip(void)
{
	/* For each value, test Type => String => Type */
	for (size_t i = 0; i < MBUF_MEM_IMPLEM_TYPE_MAX; i++) {
		enum mbuf_mem_implem_type original =
			(enum mbuf_mem_implem_type)i;
		const char *str = mbuf_mem_implem_type_to_str(original);

		CU_ASSERT_PTR_NOT_NULL(str);
		CU_ASSERT_STRING_NOT_EQUAL(str, "UNKNOWN");
		CU_ASSERT_EQUAL(mbuf_mem_implem_type_from_str(str), original);
	}
}


CU_TestInfo g_mbuf_test_implem[] = {
	{(char *)"get_supported_implems", &test_mbuf_get_supported_implems},
	{(char *)"get_auto_implem", &test_mbuf_get_auto_implem},
	{(char *)"mem_implem_type_from_str",
	 &test_mbuf_mem_implem_type_from_str},
	{(char *)"mem_implem_type_to_str", &test_mbuf_mem_implem_type_to_str},
	{(char *)"mem_implem_roundtrip", &test_mbuf_mem_implem_roundtrip},
	CU_TEST_INFO_NULL,
};
