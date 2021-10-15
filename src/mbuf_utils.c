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

#include "mbuf_utils.h"


void mbuf_rwlock_init(mbuf_rwlock_t *lock)
{
	atomic_store(lock, RWLOCK_FREE);
}


bool mbuf_rwlock_is_wrlocked(mbuf_rwlock_t *lock)
{
	return atomic_load(lock) == RWLOCK_WRLOCKED;
}


bool mbuf_rwlock_is_free(mbuf_rwlock_t *lock)
{
	return atomic_load(lock) == RWLOCK_FREE;
}


bool mbuf_rwlock_is_rdlocked(mbuf_rwlock_t *lock)
{
	return atomic_load(lock) > RWLOCK_FREE;
}


int mbuf_rwlock_get_value(mbuf_rwlock_t *lock)
{
	return atomic_load(lock);
}


int mbuf_rwlock_wrlock(mbuf_rwlock_t *lock)
{
	int zero = RWLOCK_FREE;
	if (atomic_compare_exchange_strong(lock, &zero, RWLOCK_WRLOCKED))
		return 0;
	if (zero == RWLOCK_WRLOCKED)
		return -EALREADY;
	return -EBUSY;
}


int mbuf_rwlock_wrunlock(mbuf_rwlock_t *lock)
{
	int wrlocked = RWLOCK_WRLOCKED;
	if (atomic_compare_exchange_strong(lock, &wrlocked, RWLOCK_FREE))
		return 0;
	if (wrlocked == RWLOCK_FREE)
		return -EALREADY;
	return -EBUSY;
}


int mbuf_rwlock_rdlock(mbuf_rwlock_t *lock)
{
	while (true) {
		int curr = atomic_load(lock);
		if (curr == RWLOCK_WRLOCKED)
			return -EBUSY;
		int next = curr + 1;
		if (atomic_compare_exchange_weak(lock, &curr, next))
			break;
	}
	return 0;
}


int mbuf_rwlock_rdunlock(mbuf_rwlock_t *lock)
{
	while (true) {
		int curr = atomic_load(lock);
		if (curr <= RWLOCK_FREE)
			return -EALREADY;
		int next = curr - 1;
		if (atomic_compare_exchange_weak(lock, &curr, next))
			break;
	}
	return 0;
}
