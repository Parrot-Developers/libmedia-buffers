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

#ifndef _MBUF_ANCILLARY_DATA_H_
#define _MBUF_ANCILLARY_DATA_H_

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* To be used for all public API */
#ifdef MBUF_API_EXPORTS
#	ifdef _WIN32
#		define MBUF_API __declspec(dllexport)
#	else /* !_WIN32 */
#		define MBUF_API __attribute__((visibility("default")))
#	endif /* !_WIN32 */
#else /* !MBUF_API_EXPORTS */
#	define MBUF_API
#endif /* !MBUF_API_EXPORTS */


/* Ancillary data keyspace documentation:
 *
 * To improve clarity and reduce the chances of key collisitons, ancillary data
 * keys should use reverse domain name-like notation, in the form of
 * `libname.[module.[submodule.]]key`
 *
 * All public keys should have a #define (or extern const char *) available in
 * the library header files. If a key is specific to a particular implementation
 * of a library, it must be reflected in its name (`library.implementation.key`)
 * Internal keys are up to the implementations, but still need to be prefixed by
 * the library name to avoid collisions (DO NOT USE `tmp`!).
 *
 * If a key needs to be dynamic, it is advised to use a static counter
 * (`example.key1`, `example.key2`...) rather than a random value, to avoid
 * random collisions that can not be catched by unit tests. If a dynamic key
 * needs to be public, the library should provide a function to properly create
 * the key based on the shared information (typically the counter value). */

/**
 * Key for Userdata SEI ancillary data.
 *
 * Userdata SEI are binary data included in a video stream, which are not part
 * of the stream itself. This key is where it should be stored on any type of
 * mbuf_xxx_frame.
 */
extern MBUF_API const char *MBUF_ANCILLARY_KEY_USERDATA_SEI;


struct mbuf_ancillary_data;


/**
 * Callback type for mbuf_xxx_frame_foreach_ancillary_data().
 *
 * The data reference count is not increased, and thus is only valid during the
 * callback duration.
 * The callback must not try to modify the ancillary data of the frame, as this
 * would cause a deadlock.
 *
 * @param data: The ancillary data.
 * @param userdata: Userdata passed to the
 *                  mbuf_xxx_frame_foreach_ancillary_data() function call.
 *
 * @return True to continue iterating, false to stop iterating.
 */
typedef bool (*mbuf_ancillary_data_cb_t)(struct mbuf_ancillary_data *data,
					 void *userdata);


/**
 * Increment the reference count of an ancillary data.
 *
 * @param data: The data to reference.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_ancillary_data_ref(struct mbuf_ancillary_data *data);


/**
 * Decrement the reference count of an ancillary data.
 *
 * @param data: The data to dereference.
 *
 * @return 0 on success, negative errno on error.
 */
MBUF_API int mbuf_ancillary_data_unref(struct mbuf_ancillary_data *data);


/**
 * Get the name of an ancillary data.
 *
 * @note The returned value is valid only while the caller holds a reference on
 * data.
 *
 * @param data: The ancillary data.
 *
 * @return A NULL-terminated string containing the data name, or NULL on error.
 */
MBUF_API const char *
mbuf_ancillary_data_get_name(struct mbuf_ancillary_data *data);


/**
 * Check if an ancillary data contains a string value.
 *
 * If this function returns true, then mbuf_ancillary_data_get_string(data) will
 * return a valid c-string pointer.
 *
 * @param data: The ancillary data.
 *
 * @return true if data contains a string value, false otherwise.
 */
MBUF_API bool mbuf_ancillary_data_is_string(struct mbuf_ancillary_data *data);


/**
 * Get the string value of an ancillary data.
 *
 * This function returns NULL if the data does not contains a string value.
 *
 * @note The returned value is valid only while the caller holds a reference on
 * data.
 *
 * @param data: The ancillary data.
 *
 * @return A NULL-terminated string containing the data value, or NULL on error.
 */
MBUF_API const char *
mbuf_ancillary_data_get_string(struct mbuf_ancillary_data *data);


/**
 * Get the buffer of an ancillary data.
 *
 * If the data contains a string value, the returned pointer will equal to the
 * return value of mbuf_ancillary_data_get_string(data), and len will be set to
 * strlen(string_value)+1.
 *
 * @note The returned value is valid only while the caller holds a reference on
 * data.
 *
 * @param data: The ancillary data.
 * @param len: [out] The buffer length.
 *
 * @return A valid pointer to the data buffer on success, NULL on error.
 */
MBUF_API const void *
mbuf_ancillary_data_get_buffer(struct mbuf_ancillary_data *data, size_t *len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MBUF_ANCILLARY_DATA_H_ */
