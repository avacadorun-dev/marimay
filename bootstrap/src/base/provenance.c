#include "mmay/base/provenance.h"

#include <stddef.h>
#include <stdint.h>

#if defined(_MSC_VER)

#include <intrin.h>

static volatile long mmay_next_provenance_value = 1;

bool mmay_new_provenance(uint64_t *result) {
    long current_bits = _InterlockedCompareExchange(
        &mmay_next_provenance_value,
        0,
        0
    );
    uint32_t current = (uint32_t)current_bits;
    if (result == NULL) {
        return false;
    }
    while (current != 0 && current != UINT32_MAX) {
        const uint32_t next = current + UINT32_C(1);
        const long observed = _InterlockedCompareExchange(
            &mmay_next_provenance_value,
            (long)next,
            (long)current
        );
        if ((uint32_t)observed == current) {
            *result = (uint64_t)current << 32U;
            return true;
        }
        current = (uint32_t)observed;
    }
    return false;
}

#else

#include <stdatomic.h>

static _Atomic uint32_t mmay_next_provenance_value = UINT32_C(1);

bool mmay_new_provenance(uint64_t *result) {
    uint32_t current = atomic_load_explicit(
        &mmay_next_provenance_value,
        memory_order_relaxed
    );
    if (result == NULL) {
        return false;
    }
    while (current != 0 && current != UINT32_MAX) {
        const uint32_t next = current + UINT32_C(1);
        if (atomic_compare_exchange_weak_explicit(
                &mmay_next_provenance_value,
                &current,
                next,
                memory_order_relaxed,
                memory_order_relaxed
            )) {
            *result = (uint64_t)current << 32U;
            return true;
        }
    }
    return false;
}

#endif
