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

#include <media-buffers/mbuf_mem_pbo.h>

#include "mbuf_mem_internal.h"

#include <errno.h>
#include <stdlib.h>

#define ULOG_TAG mbuf_mem_pbo
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);

/* OpenGL headers */
#if defined(__APPLE__)
#	include <TargetConditionals.h>
#	if TARGET_OS_IPHONE
#		include <OpenGLES/ES3/gl.h>
#		include <OpenGLES/ES3/glext.h>
#	else
#		define GL_GLEXT_PROTOTYPES
#		include <OpenGL/OpenGL.h>
#		include <OpenGL/gl3.h>
#		include <OpenGL/gl3ext.h>
#	endif
#elif defined(_WIN32)
#	include <epoxy/gl.h>
#else
#	include <GLES3/gl3.h>
#endif

/* Uncomment to enable extra GL error checking */
/* #define CHECK_GL_ERRORS */
#if defined(CHECK_GL_ERRORS)
#	warning CHECK_GL_ERRORS is enabled
#	include <assert.h>
#	define GLCHK(X)                                                       \
		do {                                                           \
			GLenum err = GL_NO_ERROR;                              \
			X;                                                     \
			while ((err = glGetError())) {                         \
				ULOGE("GL error 0x%x in " #X                   \
				      " file %s line %d",                      \
				      err,                                     \
				      __FILE__,                                \
				      __LINE__);                               \
				assert(err == GL_NO_ERROR);                    \
			}                                                      \
		} while (0)
#else
#	define GLCHK(X) X
#endif /* CHECK_GL_ERRORS */


/* PBO implementation memory specific */
struct mem_pbo_specific {
	/* Buffer object name */
	GLuint pbo;
	int target;
	int usage;
	int access;
};


/* PBO implementation implem specific */
struct impl_pbo_specific {
	struct mbuf_pbo_attrs attrs;
};


/* Cookie is 'pbo' in ascii coding */
const uint64_t mbuf_mem_pbo_cookie = UINT64_C(0x70626F);


static int pbo_alloc(struct mbuf_mem *mem, void *specific)
{
	struct impl_pbo_specific *impl_specific = specific;
	struct mem_pbo_specific *pbo_mem = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(mem->specific, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mem->data, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!impl_specific, EINVAL);

	pbo_mem = calloc(1, sizeof(*pbo_mem));
	if (!pbo_mem)
		return -ENOMEM;
	pbo_mem->target = impl_specific->attrs.target;
	pbo_mem->usage = impl_specific->attrs.usage;
	pbo_mem->access = impl_specific->attrs.access;

	GLCHK(glGenBuffers(1, &pbo_mem->pbo));
	if (pbo_mem->pbo == 0) {
		free(pbo_mem);
		return -ENOMEM;
	}
	GLCHK(glBindBuffer(pbo_mem->target, pbo_mem->pbo));
	GLCHK(glBufferData(pbo_mem->target,
			   impl_specific->attrs.mem_size,
			   0,
			   pbo_mem->usage));
	mem->size = impl_specific->attrs.mem_size;
	mem->cookie = mbuf_mem_pbo_cookie;
	mem->specific = pbo_mem;

	GLCHK(glBindBuffer(pbo_mem->target, 0));

	return 0;
}


static void *
pbo_map(GLenum target, GLuint buffer, GLsizeiptr size, GLenum access)
{
	void *data;
	GLCHK(glBindBuffer(target, buffer));
#if defined(__APPLE__) && !TARGET_OS_IPHONE
	GLenum _access;
	if (access & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT))
		_access = GL_READ_WRITE;
	else if (access & GL_MAP_READ_BIT)
		_access = GL_READ_ONLY;
	else if (access & GL_MAP_WRITE_BIT)
		_access = GL_WRITE_ONLY;
	else
		_access = 0;
	data = GLCHK(glMapBuffer(target, _access));
#else
	data = GLCHK(glMapBufferRange(target, 0, size, access));
#endif
	GLCHK(glBindBuffer(target, 0));
	return data;
}


static void pbo_unmap(GLenum target, GLuint buffer)
{
	GLCHK(glBindBuffer(target, buffer));
	GLCHK(glUnmapBuffer(target));
	GLCHK(glBindBuffer(target, 0));
}


static void pbo_free(struct mbuf_mem *mem, void *specific)
{
	struct mem_pbo_specific *pbo_mem = mem->specific;
	struct impl_pbo_specific *impl_specific = specific;

	ULOG_ERRNO_RETURN_IF(mem->cookie != mbuf_mem_pbo_cookie, EINVAL);
	ULOG_ERRNO_RETURN_IF(!pbo_mem, EINVAL);
	ULOG_ERRNO_RETURN_IF(!impl_specific, EINVAL);

	if (mem->data != NULL)
		pbo_unmap(impl_specific->attrs.target, pbo_mem->pbo);
	GLCHK(glDeleteBuffers(1, &pbo_mem->pbo));

	mem->specific = NULL;
	mem->data = NULL;

	free(pbo_mem);

	return;
}


struct mbuf_mem_implem *
mbuf_mem_pbo_get_implem(const struct mbuf_pbo_attrs *attrs)
{
	struct impl_pbo_specific *impl_specific = NULL;
	struct mbuf_mem_implem *impl = NULL;

	ULOG_ERRNO_RETURN_VAL_IF(!attrs, EINVAL, NULL);
	ULOG_ERRNO_RETURN_VAL_IF(attrs->mem_size == 0, EINVAL, NULL);

	impl_specific = calloc(1, sizeof(*impl_specific));
	if (!impl_specific) {
		ULOG_ERRNO("calloc", ENOMEM);
		goto error;
	}
	impl_specific->attrs = *attrs;

	impl = calloc(1, sizeof(*impl));
	if (!impl) {
		ULOG_ERRNO("calloc", ENOMEM);
		goto error;
	}
	impl->alloc = pbo_alloc;
	impl->free = pbo_free;
	impl->specific = impl_specific;

	return impl;

error:
	free(impl_specific);
	free(impl);
	return NULL;
}


void mbuf_mem_pbo_release_implem(struct mbuf_mem_implem *implem)
{
	if (!implem)
		return;

	free(implem->specific);
	free(implem);
}


int mbuf_mem_pbo_get_gl_name(struct mbuf_mem *mem)
{
	ULOG_ERRNO_RETURN_ERR_IF(!mem, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!mem->specific, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mem->cookie != mbuf_mem_pbo_cookie, EINVAL);

	return ((struct mem_pbo_specific *)mem->specific)->pbo;
}


int mbuf_mem_pbo_map(struct mbuf_mem *mem)
{
	struct mem_pbo_specific *pbo_mem = mem->specific;

	ULOG_ERRNO_RETURN_ERR_IF(mem->cookie != mbuf_mem_pbo_cookie, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!pbo_mem, EINVAL);

	if (mem->data != NULL)
		return 0;

	mem->data = pbo_map(
		pbo_mem->target, pbo_mem->pbo, mem->size, pbo_mem->access);
	if (mem->data == NULL)
		return -EADDRNOTAVAIL;

	return 0;
}


void mbuf_mem_pbo_unmap(struct mbuf_mem *mem)
{
	struct mem_pbo_specific *pbo_mem = mem->specific;

	ULOG_ERRNO_RETURN_IF(mem->cookie != mbuf_mem_pbo_cookie, EINVAL);
	ULOG_ERRNO_RETURN_IF(!pbo_mem, EINVAL);

	if (mem->data == NULL)
		return;

	pbo_unmap(pbo_mem->target, pbo_mem->pbo);
	mem->data = NULL;

	return;
}
