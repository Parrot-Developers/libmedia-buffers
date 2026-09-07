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


#pragma once

#include "mbuf_audio_frame.h"
#include "mbuf_coded_video_frame.h"
#include "mbuf_raw_video_frame.h"
#include <memory>


namespace mbuf {

class MBUF_API Frame {
	Frame(const Frame &) = delete;
	Frame &operator=(const Frame &) = delete;

	Frame(Frame &&) = delete;
	Frame &operator=(Frame &&) = delete;

protected:
	Frame() = default;

public:
	enum class Type { CODED_VIDEO, RAW_VIDEO, AUDIO };

	virtual ~Frame() = default;

	virtual void *getFramePtr() const = 0;

	virtual int ref() = 0;

	virtual int unref() = 0;

	virtual Type getType() const = 0;

	virtual int rdLock() = 0;

	virtual int rdUnlock() = 0;

	virtual int wrLock() = 0;

	virtual int wrUnlock() = 0;

	virtual int setMetadata(struct vmeta_frame *metadata) = 0;

	virtual int getMetadata(struct vmeta_frame **metadata) = 0;

	virtual int
	usesMemFromPool(const struct mbuf_pool *pool, bool *any, bool *all) = 0;

	virtual int finalize() = 0;

	virtual int getBuffer(const void **data, size_t *len) = 0;

	virtual int releaseBuffer(const void *data) = 0;

	virtual int getRWBuffer(void **data, size_t *len) = 0;

	virtual int releaseRWBuffer(const void *data) = 0;

	virtual int addAncillaryString(const char *name, const char *value) = 0;

	virtual int addAncillaryBuffer(const char *name,
				       const void *buffer,
				       size_t len) = 0;

	virtual int addAncillaryBufferWithCbs(
		const char *name,
		const void *buffer,
		size_t len,
		const struct mbuf_ancillary_data_cbs *cbs) = 0;

	virtual int addAncillaryData(struct mbuf_ancillary_data *data) = 0;

	virtual int getAncillaryData(const char *name,
				     struct mbuf_ancillary_data **data) = 0;

	virtual int removeAncillaryData(const char *name) = 0;

	/* Coded only */
	virtual int getFrameInfo(struct vdef_coded_frame *frame_info) const = 0;

	virtual int addNalu(struct mbuf_mem *mem,
			    size_t offset,
			    const struct vdef_nalu *nalu) = 0;

	virtual int insertNalu(struct mbuf_mem *mem,
			       size_t offset,
			       const struct vdef_nalu *nalu,
			       unsigned int index) = 0;

	/* Raw only */
	virtual int getFrameInfo(struct vdef_raw_frame *frame_info) const = 0;

	virtual int setPlane(unsigned int plane,
			     struct mbuf_mem *mem,
			     size_t offset,
			     size_t len) = 0;

	/* Audio only */
	virtual int getFrameInfo(struct adef_frame *frame_info) const = 0;

	virtual int
	setBuffer(struct mbuf_mem *mem, size_t offset, size_t len) = 0;

	static std::unique_ptr<Frame>
	create(const struct vdef_coded_frame *frame_info);

	static std::unique_ptr<Frame>
	create(const struct vdef_raw_frame *frame_info);

	static std::unique_ptr<Frame>
	create(const struct adef_frame *frame_info);

	static std::unique_ptr<Frame>
	wrapExisting(struct mbuf_coded_video_frame *frame, bool owner = false);

	static std::unique_ptr<Frame>
	wrapExisting(struct mbuf_raw_video_frame *frame, bool owner = false);

	static std::unique_ptr<Frame>
	wrapExisting(struct mbuf_audio_frame *frame, bool owner = false);
};


} /* namespace mbuf */
