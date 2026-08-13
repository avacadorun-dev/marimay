#include "ruda/base/provenance.h"

#include <stdint.h>

#if defined(_MSC_VER)

#include <intrin.h>

static volatile __int64 ruda_next_provenance_value = 1;

bool ruda_new_provenance(uint64_t *result) {
    __int64 current_bits = _InterlockedCompareExchange64(
        &ruda_next_provenance_value,
        0,
        0
    );
    uint64_t current = (uint64_t)current_bits;
    if (result == NULL) {
        return false;
    }
    while (current != 0 && current != UINT64_MAX) {
        const uint64_t next = current + UINT64_C(1);
        const __int64 observed = _InterlockedCompareExchange64(
            &ruda_next_provenance_value,
            (__int64)next,
            (__int64)current
        );
        if ((uint64_t)observed == current) {
            *result = current;
            return true;
        }
        current = (uint64_t)observed;
    }
    return false;
}

#else

#include <stdatomic.h>

static _Atomic uint64_t ruda_next_provenance_value = UINT64_C(1);

bool ruda_new_provenance(uint64_t *result) {
    uint64_t current = atomic_load_explicit(
        &ruda_next_provenance_value,
        memory_order_relaxed
    );
    if (result == NULL) {
        return false;
    }
    while (current != 0 && current != UINT64_MAX) {
        const uint64_t next = current + UINT64_C(1);
        if (atomic_compare_exchange_weak_explicit(
                &ruda_next_provenance_value,
                &current,
                next,
                memory_order_relaxed,
                memory_order_relaxed
            )) {
            *result = current;
            return true;
        }
    }
    return false;
}

#endif
