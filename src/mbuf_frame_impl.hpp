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

#include "media-buffers/mbuf_frame.hpp"


namespace mbuf {

/*
 * Traits definition to map C++ types to C functions and types.
 */
template <typename T> struct FrameTraits;


#define MBUF_FRAME_TRAITS_DEF(_T, _info, _cbs)                                 \
	static int create(FrameType **q, const InfoType *info)                 \
	{                                                                      \
		return _T##_new(info, q);                                      \
	}                                                                      \
	static int ref(FrameType *q)                                           \
	{                                                                      \
		return _T##_ref(q);                                            \
	}                                                                      \
	static int unref(FrameType *q)                                         \
	{                                                                      \
		return _T##_unref(q);                                          \
	}                                                                      \
	static int setCbs(FrameType *q, const CbsType *cbs)                    \
	{                                                                      \
		return _T##_set_callbacks(q, cbs);                             \
	}                                                                      \
	static int rdLock(FrameType *q)                                        \
	{                                                                      \
		return _T##_rdlock(q);                                         \
	}                                                                      \
	static int rdUnlock(FrameType *q)                                      \
	{                                                                      \
		return _T##_rdunlock(q);                                       \
	}                                                                      \
	static int wrLock(FrameType *q)                                        \
	{                                                                      \
		return _T##_wrlock(q);                                         \
	}                                                                      \
	static int wrUnlock(FrameType *q)                                      \
	{                                                                      \
		return _T##_wrunlock(q);                                       \
	}                                                                      \
	static int finalize(FrameType *q)                                      \
	{                                                                      \
		return _T##_finalize(q);                                       \
	}                                                                      \
	static int addAncillaryString(                                         \
		FrameType *q, const char *name, const char *value)             \
	{                                                                      \
		return _T##_add_ancillary_string(q, name, value);              \
	}                                                                      \
	static int addAncillaryBuffer(FrameType *q,                            \
				      const char *name,                        \
				      const void *buffer,                      \
				      size_t len)                              \
	{                                                                      \
		return _T##_add_ancillary_buffer(q, name, buffer, len);        \
	}                                                                      \
	static int addAncillaryBufferWithCbs(                                  \
		FrameType *q,                                                  \
		const char *name,                                              \
		const void *buffer,                                            \
		size_t len,                                                    \
		const struct mbuf_ancillary_data_cbs *cbs)                     \
	{                                                                      \
		return _T##_add_ancillary_buffer_with_cbs(                     \
			q, name, buffer, len, cbs);                            \
	}                                                                      \
	static int addAncillaryData(FrameType *q,                              \
				    struct mbuf_ancillary_data *data)          \
	{                                                                      \
		return _T##_add_ancillary_data(q, data);                       \
	}                                                                      \
	static int getAncillaryData(FrameType *q,                              \
				    const char *name,                          \
				    struct mbuf_ancillary_data **data)         \
	{                                                                      \
		return _T##_get_ancillary_data(q, name, data);                 \
	}                                                                      \
	static int removeAncillaryData(FrameType *q, const char *name)         \
	{                                                                      \
		return _T##_remove_ancillary_data(q, name);                    \
	}                                                                      \
	static int getFrameInfo(FrameType *q, InfoType *info)                  \
	{                                                                      \
		return _T##_get_frame_info(q, info);                           \
	}


#define MBUF_VIDEO_FRAME_TRAITS_DEF(_T, _info, _cbs)                           \
	MBUF_FRAME_TRAITS_DEF(_T, _info, _cbs)                                 \
	static int setMetadata(FrameType *q, struct vmeta_frame *meta)         \
	{                                                                      \
		return _T##_set_metadata(q, meta);                             \
	}                                                                      \
	static int getMetadata(FrameType *q, struct vmeta_frame **meta)        \
	{                                                                      \
		return _T##_get_metadata(q, meta);                             \
	}                                                                      \
	static int usesMemFromPool(FrameType *q,                               \
				   const struct mbuf_pool *pool,               \
				   bool *any,                                  \
				   bool *all)                                  \
	{                                                                      \
		return _T##_uses_mem_from_pool(q, pool, any, all);             \
	}                                                                      \
	static int getBuffer(FrameType *q, const void **data, size_t *len)     \
	{                                                                      \
		return _T##_get_packed_buffer(q, data, len);                   \
	}                                                                      \
	static int releaseBuffer(FrameType *q, const void *data)               \
	{                                                                      \
		return _T##_release_packed_buffer(q, data);                    \
	}                                                                      \
	static int getRWBuffer(FrameType *q, void **data, size_t *len)         \
	{                                                                      \
		return _T##_get_rw_packed_buffer(q, data, len);                \
	}                                                                      \
	static int releaseRWBuffer(FrameType *q, const void *data)             \
	{                                                                      \
		return _T##_release_rw_packed_buffer(q, data);                 \
	}                                                                      \
	/* Not implemented (Audio) */                                          \
	static int setBuffer(                                                  \
		FrameType *q, struct mbuf_mem *mem, size_t offset, size_t len) \
	{                                                                      \
		return -ENOSYS;                                                \
	}


#define MBUF_AUDIO_FRAME_TRAITS_DEF(_T, _info, _cbs)                           \
	template <> struct FrameTraits<struct _T> {                            \
		using FrameType = struct _T;                                   \
		using InfoType = struct _info;                                 \
		using CbsType = struct _cbs;                                   \
		static constexpr Frame::Type Type = Frame::Type::AUDIO;        \
		MBUF_FRAME_TRAITS_DEF(_T, _info, _cbs)                         \
		static int                                                     \
		getBuffer(FrameType *q, const void **data, size_t *len)        \
		{                                                              \
			return _T##_get_buffer(q, data, len);                  \
		}                                                              \
		static int releaseBuffer(FrameType *q, const void *data)       \
		{                                                              \
			return _T##_release_buffer(q, data);                   \
		}                                                              \
		static int getRWBuffer(FrameType *q, void **data, size_t *len) \
		{                                                              \
			return _T##_get_rw_buffer(q, data, len);               \
		}                                                              \
		static int releaseRWBuffer(FrameType *q, const void *data)     \
		{                                                              \
			return _T##_release_rw_buffer(q, data);                \
		}                                                              \
		/* Specific */                                                 \
		static int setBuffer(FrameType *q,                             \
				     struct mbuf_mem *mem,                     \
				     size_t offset,                            \
				     size_t len)                               \
		{                                                              \
			return _T##_set_buffer(q, mem, offset, len);           \
		}                                                              \
		/* Not implemented (Video) */                                  \
		static int setMetadata(FrameType *q, struct vmeta_frame *meta) \
		{                                                              \
			return -ENOSYS;                                        \
		}                                                              \
		static int getMetadata(FrameType *q,                           \
				       struct vmeta_frame **meta)              \
		{                                                              \
			return -ENOSYS;                                        \
		}                                                              \
		static int usesMemFromPool(FrameType *q,                       \
					   const struct mbuf_pool *pool,       \
					   bool *any,                          \
					   bool *all)                          \
		{                                                              \
			return -ENOSYS;                                        \
		}                                                              \
		/* Not implemented (Coded video) */                            \
		static int addNalu(FrameType *q,                               \
				   struct mbuf_mem *mem,                       \
				   size_t offset,                              \
				   const struct vdef_nalu *nalu)               \
		{                                                              \
			return -ENOSYS;                                        \
		}                                                              \
		static int insertNalu(FrameType *q,                            \
				      struct mbuf_mem *mem,                    \
				      size_t offset,                           \
				      const struct vdef_nalu *nalu,            \
				      unsigned int index)                      \
		{                                                              \
			return -ENOSYS;                                        \
		}                                                              \
		/* Not implemented (Raw video) */                              \
		static int setPlane(FrameType *q,                              \
				    unsigned int plane,                        \
				    struct mbuf_mem *mem,                      \
				    size_t offset,                             \
				    size_t len)                                \
		{                                                              \
			return -ENOSYS;                                        \
		}                                                              \
	};


#define MBUF_CODED_VIDEO_FRAME_TRAITS_DEF(_T, _info, _cbs)                     \
	template <> struct FrameTraits<struct _T> {                            \
		using FrameType = struct _T;                                   \
		using InfoType = struct _info;                                 \
		using CbsType = struct _cbs;                                   \
		static constexpr Frame::Type Type = Frame::Type::CODED_VIDEO;  \
		MBUF_VIDEO_FRAME_TRAITS_DEF(_T, _info, _cbs)                   \
		static int addNalu(FrameType *q,                               \
				   struct mbuf_mem *mem,                       \
				   size_t offset,                              \
				   const struct vdef_nalu *nalu)               \
		{                                                              \
			return _T##_add_nalu(q, mem, offset, nalu);            \
		}                                                              \
		static int insertNalu(FrameType *q,                            \
				      struct mbuf_mem *mem,                    \
				      size_t offset,                           \
				      const struct vdef_nalu *nalu,            \
				      unsigned int index)                      \
		{                                                              \
			return _T##_insert_nalu(q, mem, offset, nalu, index);  \
		}                                                              \
		/* Not implemented (Raw video) */                              \
		static int setPlane(FrameType *q,                              \
				    unsigned int plane,                        \
				    struct mbuf_mem *mem,                      \
				    size_t offset,                             \
				    size_t len)                                \
		{                                                              \
			return -ENOSYS;                                        \
		}                                                              \
	};


#define MBUF_RAW_VIDEO_FRAME_TRAITS_DEF(_T, _info, _cbs)                       \
	template <> struct FrameTraits<struct _T> {                            \
		using FrameType = struct _T;                                   \
		using InfoType = struct _info;                                 \
		using CbsType = struct _cbs;                                   \
		static constexpr Frame::Type Type = Frame::Type::RAW_VIDEO;    \
		MBUF_VIDEO_FRAME_TRAITS_DEF(_T, _info, _cbs)                   \
		static int setPlane(FrameType *q,                              \
				    unsigned int plane,                        \
				    struct mbuf_mem *mem,                      \
				    size_t offset,                             \
				    size_t len)                                \
		{                                                              \
			return _T##_set_plane(q, plane, mem, offset, len);     \
		}                                                              \
		/* Not implemented (Coded video) */                            \
		static int addNalu(FrameType *q,                               \
				   struct mbuf_mem *mem,                       \
				   size_t offset,                              \
				   const struct vdef_nalu *nalu)               \
		{                                                              \
			return -ENOSYS;                                        \
		}                                                              \
		static int insertNalu(FrameType *q,                            \
				      struct mbuf_mem *mem,                    \
				      size_t offset,                           \
				      const struct vdef_nalu *nalu,            \
				      unsigned int index)                      \
		{                                                              \
			return -ENOSYS;                                        \
		}                                                              \
	};


MBUF_CODED_VIDEO_FRAME_TRAITS_DEF(mbuf_coded_video_frame,
				  vdef_coded_frame,
				  mbuf_coded_video_frame_cbs)
MBUF_RAW_VIDEO_FRAME_TRAITS_DEF(mbuf_raw_video_frame,
				vdef_raw_frame,
				mbuf_raw_video_frame_cbs)
MBUF_AUDIO_FRAME_TRAITS_DEF(mbuf_audio_frame, adef_frame, mbuf_audio_frame_cbs)


template <typename T, typename InfoType, typename CbsType>
class FrameImpl : public Frame {
public:
	using Traits = FrameTraits<T>;
	using NativeFrame = typename Traits::FrameType;

	explicit FrameImpl(const InfoType *info) : Frame()
	{
		Traits::create(&mFrame, info);
	}

	FrameImpl(T *existing, bool owner) :
			Frame(), mFrame(existing), mOwner(owner)
	{
	}

	~FrameImpl()
	{
		if (mFrame && mOwner)
			FrameImpl::unref();
	}

	void *getFramePtr() const override
	{
		return mFrame;
	}

	int ref() override
	{
		return Traits::ref(mFrame);
	}

	int unref() override
	{
		return Traits::unref(mFrame);
	}

	Type getType() const override
	{
		return Traits::Type;
	}

	int rdLock() override
	{
		return Traits::rdLock(mFrame);
	}

	int rdUnlock() override
	{
		return Traits::rdUnlock(mFrame);
	}

	int wrLock() override
	{
		return Traits::wrLock(mFrame);
	}

	int wrUnlock() override
	{
		return Traits::wrUnlock(mFrame);
	}

	int setMetadata(struct vmeta_frame *metadata) override
	{
		return Traits::setMetadata(mFrame, metadata);
	}

	int getMetadata(struct vmeta_frame **metadata) override
	{
		return Traits::getMetadata(mFrame, metadata);
	}

	int usesMemFromPool(const struct mbuf_pool *pool,
			    bool *any,
			    bool *all) override
	{
		return Traits::usesMemFromPool(mFrame, pool, any, all);
	}

	int finalize() override
	{
		return Traits::finalize(mFrame);
	}

	int getBuffer(const void **data, size_t *len) override
	{
		return Traits::getBuffer(mFrame, data, len);
	}

	int releaseBuffer(const void *data) override
	{
		return Traits::releaseBuffer(mFrame, data);
	}

	int getRWBuffer(void **data, size_t *len) override
	{
		return Traits::getRWBuffer(mFrame, data, len);
	}

	int releaseRWBuffer(const void *data) override
	{
		return Traits::releaseRWBuffer(mFrame, data);
	}


	int addAncillaryString(const char *name, const char *value) override
	{
		return Traits::addAncillaryString(mFrame, name, value);
	}

	int addAncillaryBuffer(const char *name,
			       const void *buffer,
			       size_t len) override
	{
		return Traits::addAncillaryBuffer(mFrame, name, buffer, len);
	}

	int addAncillaryBufferWithCbs(
		const char *name,
		const void *buffer,
		size_t len,
		const struct mbuf_ancillary_data_cbs *cbs) override
	{
		return Traits::addAncillaryBufferWithCbs(
			mFrame, name, buffer, len, cbs);
	}

	int addAncillaryData(struct mbuf_ancillary_data *data) override
	{
		return Traits::addAncillaryData(mFrame, data);
	}

	int getAncillaryData(const char *name,
			     struct mbuf_ancillary_data **data) override
	{
		return Traits::getAncillaryData(mFrame, name, data);
	}

	int removeAncillaryData(const char *name) override
	{
		return Traits::removeAncillaryData(mFrame, name);
	}

	/* Coded only */
	int addNalu(struct mbuf_mem *mem,
		    size_t offset,
		    const struct vdef_nalu *nalu) override
	{
		return Traits::addNalu(mFrame, mem, offset, nalu);
	}
	int insertNalu(struct mbuf_mem *mem,
		       size_t offset,
		       const struct vdef_nalu *nalu,
		       unsigned int index) override
	{
		return Traits::insertNalu(mFrame, mem, offset, nalu, index);
	}

	/* Raw only */
	int setPlane(unsigned int plane,
		     struct mbuf_mem *mem,
		     size_t offset,
		     size_t len) override
	{
		return Traits::setPlane(mFrame, plane, mem, offset, len);
	}

	/* Audio only */
	int setBuffer(struct mbuf_mem *mem, size_t offset, size_t len) override
	{
		return Traits::setBuffer(mFrame, mem, offset, len);
	}

	/* Get frame info overrides */
	int getFrameInfo(struct vdef_coded_frame *frame_info) const override
	{
		return getFrameInfoT(frame_info);
	}
	int getFrameInfo(struct vdef_raw_frame *frame_info) const override
	{
		return getFrameInfoT(frame_info);
	}
	int getFrameInfo(struct adef_frame *frame_info) const override
	{
		return getFrameInfoT(frame_info);
	}

private:
	NativeFrame *mFrame = nullptr;
	bool mOwner = true;

	template <typename U> int getFrameInfoT([[maybe_unused]] U *info) const
	{
		return -ENOSYS;
	}
	int getFrameInfoT(InfoType *info) const
	{
		return Traits::getFrameInfo(mFrame, info);
	}
};


} /* namespace mbuf */
