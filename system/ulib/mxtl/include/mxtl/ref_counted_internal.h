// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <magenta/assert.h>
#include <magenta/compiler.h>
#include <mxtl/atomic.h>
#include <mxtl/canary.h>

namespace mxtl {
namespace internal {

template <bool Enabled>
class AdoptionValidator;

// This will catch:
// - Double-adoptions
// - AddRef/Release without adopting first
// - Wrapping bad pointers
// - Re-wrapping raw pointers to destroyed objects
template <>
class AdoptionValidator<true> {
public:
    ~AdoptionValidator() {
        magic_ = 0;
    }

    void Adopt() {
        AssertMagic(kStartingMagic);
        magic_ = kAdoptedMagic;
    }
    void ValidateAddRef() {
        AssertMagic(kAdoptedMagic);
    }
    void ValidateRelease() {
        AssertMagic(kAdoptedMagic);
    }

private:
    void AssertMagic(const uint32_t expected) const {
        MX_DEBUG_ASSERT_MSG(magic_ == expected,
                            "Invalid magic (expect: 0x%02x, got: 0x%02x)\n",
                            static_cast<unsigned int>(expected),
                            static_cast<unsigned int>(magic_));
    }

    // The object has been constructed, but not yet adopted or destroyed.
    static constexpr uint32_t kStartingMagic = mxtl::magic("RcST");

    // The object has been constructed and adopted, but not destroyed.
    static constexpr uint32_t kAdoptedMagic = mxtl::magic("RcAD");

    volatile uint32_t magic_ = kStartingMagic;
};

template <>
class AdoptionValidator<false> {
public:
    void Adopt() {}
    void ValidateAddRef() {}
    void ValidateRelease() {}
};

template <bool EnableAdoptionValidator>
class RefCountedBase {
protected:
    constexpr RefCountedBase()
        : ref_count_(1) {}
    ~RefCountedBase() {}
    void AddRef() {
        adoption_validator_.ValidateAddRef();
        const int rc = ref_count_.fetch_add(1, memory_order_relaxed);
        if (EnableAdoptionValidator) {
            // This assertion will fire if someone calls AddRef() on a
            // ref-counted object that has reached ref_count_ == 0 but has not
            // been destroyed yet. This could happen by manually calling
            // AddRef(), or re-wrapping such a pointer with WrapRefPtr() or
            // RefPtr<T>(T*) (both of which call AddRef()). If the object has
            // already been destroyed, the magic check in
            // adoption_validator_.ValidateAddRef() should have caught it before
            // this point.
            MX_DEBUG_ASSERT_MSG(rc >= 1, "count %d < 1\n", rc);
        }
    }
    // Returns true if the object should self-delete.
    bool Release() __WARN_UNUSED_RESULT {
        adoption_validator_.ValidateRelease();
        const int rc = ref_count_.fetch_sub(1, memory_order_release);
        if (EnableAdoptionValidator) {
            // This assertion will fire if someone manually calls Release()
            // on a ref-counted object too many times.
            MX_DEBUG_ASSERT_MSG(rc >= 1, "count %d < 1\n", rc);
        }
        if (rc == 1) {
            atomic_thread_fence(memory_order_acquire);
            return true;
        }
        return false;
    }

    void Adopt() {
        adoption_validator_.Adopt();
    }

    // Current ref count. Only to be used for debugging purposes.
    int ref_count_debug() const {
        return ref_count_.load(memory_order_relaxed);
    }

private:
    mxtl::atomic_int ref_count_;
    AdoptionValidator<EnableAdoptionValidator> adoption_validator_;
};

} // namespace internal
} // namespace mxtl
