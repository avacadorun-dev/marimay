#ifndef RUDA_CHECK_TYPE_SPELLING_H
#define RUDA_CHECK_TYPE_SPELLING_H

#include <stddef.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/check/result.h"

typedef enum RudaTypeSpellingStatus {
    RUDA_TYPE_SPELLING_OK,
    RUDA_TYPE_SPELLING_INVALID_ARGUMENT,
    RUDA_TYPE_SPELLING_ALLOCATION_FAILED,
    RUDA_TYPE_SPELLING_LIMIT_REACHED,
    RUDA_TYPE_SPELLING_UNAVAILABLE
} RudaTypeSpellingStatus;

typedef struct RudaTypeSpellingOptions {
    size_t max_depth;
    size_t max_bytes;
} RudaTypeSpellingOptions;

RudaTypeSpellingOptions ruda_type_spelling_default_options(void);

/*
 * Produces canonical source spelling for a result-local checked type. output
 * must be zero-initialized. On success it owns a zero-terminated byte copy and
 * must be disposed. Failure leaves output empty. The output allocator may
 * differ from the check-result allocator.
 */
RudaTypeSpellingStatus ruda_checked_type_spelling(
    const RudaCheckResult *result,
    RudaTypeId type,
    RudaAllocator allocator,
    RudaTypeSpellingOptions options,
    RudaOwnedBytes *output
);

#endif
