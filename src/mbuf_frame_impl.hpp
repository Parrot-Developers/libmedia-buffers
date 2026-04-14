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


namespace mbuf {

#include "media-buffers/mbuf_frame.hpp"

#if __cplusplus >= 201402L
using std::make_unique;
#else
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args &&...args)
{
	return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#endif


template <typename T, typename InfoType, typename CbsType>
class FrameImpl : public Frame {
	FrameImpl(const FrameImpl &) = delete;
	FrameImpl &operator=(const FrameImpl &) = delete;

	using NewFunc = int (*)(const InfoType *, T **);
	using UnrefFunc = int (*)(T *);
	using CbsFunc = int (*)(T *, const CbsType *);
	using RdLockFunc = int (*)(T *);
	using RdUnlockFunc = int (*)(T *);
	using WrLockFunc = int (*)(T *);
	using WrUnlockFunc = int (*)(T *);
	using SetMetadataFunc = int (*)(T *, struct vmeta_frame *);
	using GetMetadataFunc = int (*)(T *, struct vmeta_frame **);
	using UsesMemFromPool = int (*)(const T *,
					const struct mbuf_pool *,
					bool *,
					bool *);
	using FinalizeFunc = int (*)(T *);
	using GetBufferFunc = int (*)(T *, const void **, size_t *);
	using ReleaseBufferFunc = int (*)(T *, const void *);
	using GetRWBufferFunc = int (*)(T *, void **, size_t *);
	using AddAncillaryStringFunc = int (*)(T *, const char *, const char *);
	using AddAncillaryBufferFunc = int (*)(T *,
					       const char *,
					       const void *,
					       size_t len);
	using AddAncillaryBufferWithCbsFunc =
		int (*)(T *,
			const char *,
			const void *,
			size_t len,
			const struct mbuf_ancillary_data_cbs *cbs);
	using AddAncillaryDataFunc = int (*)(T *, mbuf_ancillary_data *);
	using GetAncillaryDataFunc = int (*)(T *,
					     const char *,
					     mbuf_ancillary_data **);
	using RemoveAncillaryDataFunc = int (*)(T *, const char *);
	using AddNaluFunc = int (*)(T *,
				    struct mbuf_mem *,
				    size_t,
				    const struct vdef_nalu *);
	using InsertNaluFunc = int (*)(T *,
				       struct mbuf_mem *,
				       size_t,
				       const struct vdef_nalu *,
				       unsigned int);
	using SetPlaneFunc = int (*)(T *,
				     unsigned int plane,
				     struct mbuf_mem *mem,
				     size_t offset,
				     size_t len);
	using SetBufferFunc = int (*)(T *,
				      struct mbuf_mem *mem,
				      size_t offset,
				      size_t len);
	using GetFrameInfoCodedFunc = int (*)(const T *,
					      struct vdef_coded_frame *);
	using GetFrameInfoRawFunc = int (*)(const T *, struct vdef_raw_frame *);
	using GetFrameInfoAudioFunc = int (*)(const T *, struct adef_frame *);

public:
	FrameImpl(Type type, const InfoType *info) :
			Frame(), mType(type), mF(nullptr)
	{
		enable();
		if (newF)
			newF(info, &mF);
	}

	FrameImpl(Type type, T *existing) : Frame(), mType(type), mF(existing)
	{
		enable();
	}

	~FrameImpl()
	{
		if (mF && unrefF)
			unref();
	}

	FrameImpl(FrameImpl &) noexcept = delete;

	FrameImpl(FrameImpl &&) noexcept = delete;

	FrameImpl &operator=(FrameImpl &) = delete;

	FrameImpl &operator=(FrameImpl &&) = delete;

	int unref() override
	{
		return unrefF ? unrefF(mF) : -ENOSYS;
	}

	Type getType() const override
	{
		return mType;
	}

	int rdLock() override
	{
		return rdLockF ? rdLockF(mF) : -ENOSYS;
	}

	int rdUnlock() override
	{
		return rdUnlockF ? rdUnlockF(mF) : -ENOSYS;
	}

	int wrLock() override
	{
		return wrLockF ? wrLockF(mF) : -ENOSYS;
	}

	int wrUnlock() override
	{
		return wrUnlockF ? wrUnlockF(mF) : -ENOSYS;
	}

	int setMetadata(struct vmeta_frame *frame) override
	{
		return setMetadataF ? setMetadataF(mF, frame) : -ENOSYS;
	}

	int getMetadata(struct vmeta_frame **frame) override
	{
		return getMetadataF ? getMetadataF(mF, frame) : -ENOSYS;
	}

	int addNalu(struct mbuf_mem *mem,
		    size_t offset,
		    const struct vdef_nalu *nalu) override
	{
		return addNaluF ? addNaluF(mF, mem, offset, nalu) : -ENOSYS;
	}

	int insertNalu(struct mbuf_mem *mem,
		       size_t offset,
		       const struct vdef_nalu *nalu,
		       unsigned int index) override
	{
		return insertNaluF ? insertNaluF(mF, mem, offset, nalu, index)
				   : -ENOSYS;
	}

	int usesMemFromPool(const struct mbuf_pool *pool,
			    bool *any,
			    bool *all) override
	{
		return usesMemFromPoolF ? usesMemFromPoolF(mF, pool, any, all)
					: -ENOSYS;
	}

	int finalize() override
	{
		return finalizeF ? finalizeF(mF) : -ENOSYS;
	}


	int getBuffer(const void **data, size_t *len) override
	{
		return getBufferF ? getBufferF(mF, data, len) : -ENOSYS;
	}

	int releaseBuffer(const void *data) override
	{
		return releaseBufferF ? releaseBufferF(mF, data) : -ENOSYS;
	}

	int getRWBuffer(void **data, size_t *len) override
	{
		return getRWBufferF ? getRWBufferF(mF, data, len) : -ENOSYS;
	}

	int releaseRWBuffer(const void *data) override
	{
		return releaseBufferF ? releaseBufferF(mF, data) : -ENOSYS;
	}


	int addAncillaryString(const char *name, const char *value) override
	{
		return addAncillaryStringF
			       ? addAncillaryStringF(mF, name, value)
			       : -ENOSYS;
	}

	int addAncillaryBuffer(const char *name,
			       const void *buffer,
			       size_t len) override
	{
		return addAncillaryBufferF
			       ? addAncillaryBufferF(mF, name, buffer, len)
			       : -ENOSYS;
	}

	int addAncillaryBufferWithCbs(
		const char *name,
		const void *buffer,
		size_t len,
		const struct mbuf_ancillary_data_cbs *cbs) override
	{
		return addAncillaryBufferWithCbsF
			       ? addAncillaryBufferWithCbsF(
					 mF, name, buffer, len, cbs)
			       : -ENOSYS;
	}

	int addAncillaryData(struct mbuf_ancillary_data *data) override
	{
		return addAncillaryDataF ? addAncillaryDataF(mF, data)
					 : -ENOSYS;
	}

	int getAncillaryData(const char *name,
			     struct mbuf_ancillary_data **data) override
	{
		return getAncillaryDataF ? getAncillaryDataF(mF, name, data)
					 : -ENOSYS;
	}

	int removeAncillaryData(const char *name) override
	{
		return removeAncillaryDataF ? removeAncillaryDataF(mF, name)
					    : -ENOSYS;
	}

	int setPlane(unsigned int plane,
		     struct mbuf_mem *mem,
		     size_t offset,
		     size_t len) override
	{
		return setPlaneF ? setPlaneF(mF, plane, mem, offset, len)
				 : -ENOSYS;
	}

	int setBuffer(struct mbuf_mem *mem, size_t offset, size_t len) override
	{
		return setBufferF ? setBufferF(mF, mem, offset, len) : -ENOSYS;
	}

	int getFrameInfo(struct vdef_coded_frame *frame_info) const override
	{
		return getFrameInfoF.coded ? getFrameInfoF.coded(mF, frame_info)
					   : -ENOSYS;
	}

	int getFrameInfo(struct vdef_raw_frame *frame_info) const override
	{
		return getFrameInfoF.raw ? getFrameInfoF.raw(mF, frame_info)
					 : -ENOSYS;
	}

	int getFrameInfo(struct adef_frame *frame_info) const override
	{
		return getFrameInfoF.audio ? getFrameInfoF.audio(mF, frame_info)
					   : -ENOSYS;
	}

private:
	void enable();

	Type mType;
	T *mF;

	NewFunc newF = nullptr;
	UnrefFunc unrefF = nullptr;
	CbsFunc cbsF = nullptr;
	RdLockFunc rdLockF = nullptr;
	RdUnlockFunc rdUnlockF = nullptr;
	WrLockFunc wrLockF = nullptr;
	WrUnlockFunc wrUnlockF = nullptr;
	SetMetadataFunc setMetadataF = nullptr;
	GetMetadataFunc getMetadataF = nullptr;
	UsesMemFromPool usesMemFromPoolF = nullptr;
	FinalizeFunc finalizeF = nullptr;
	GetBufferFunc getBufferF = nullptr;
	GetRWBufferFunc getRWBufferF = nullptr;
	ReleaseBufferFunc releaseBufferF = nullptr;
	ReleaseBufferFunc releaseRWBufferF = nullptr;
	AddAncillaryStringFunc addAncillaryStringF = nullptr;
	AddAncillaryBufferFunc addAncillaryBufferF = nullptr;
	AddAncillaryBufferWithCbsFunc addAncillaryBufferWithCbsF = nullptr;
	AddAncillaryDataFunc addAncillaryDataF = nullptr;
	GetAncillaryDataFunc getAncillaryDataF = nullptr;
	RemoveAncillaryDataFunc removeAncillaryDataF = nullptr;
	AddNaluFunc addNaluF = nullptr;
	InsertNaluFunc insertNaluF = nullptr;
	SetPlaneFunc setPlaneF = nullptr;
	SetBufferFunc setBufferF = nullptr;

	struct {
		GetFrameInfoCodedFunc coded = nullptr;
		GetFrameInfoRawFunc raw = nullptr;
		GetFrameInfoAudioFunc audio = nullptr;
	} getFrameInfoF;
};


} /* namespace mbuf */
