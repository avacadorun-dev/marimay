#ifndef MMAY_CHECK_TYPE_SPELLING_H
#define MMAY_CHECK_TYPE_SPELLING_H

#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/check/result.h"

typedef enum MarimayTypeSpellingStatus {
    MMAY_TYPE_SPELLING_OK,
    MMAY_TYPE_SPELLING_INVALID_ARGUMENT,
    MMAY_TYPE_SPELLING_ALLOCATION_FAILED,
    MMAY_TYPE_SPELLING_LIMIT_REACHED,
    MMAY_TYPE_SPELLING_UNAVAILABLE
} MarimayTypeSpellingStatus;

typedef struct MarimayTypeSpellingOptions {
    size_t max_depth;
    size_t max_bytes;
} MarimayTypeSpellingOptions;

MarimayTypeSpellingOptions mmay_type_spelling_default_options(void);

/*
 * Produces canonical source spelling for a result-local checked type. output
 * must be zero-initialized. On success it owns a zero-terminated byte copy and
 * must be disposed. Failure leaves output empty. The output allocator may
 * differ from the check-result allocator.
 */
MarimayTypeSpellingStatus mmay_checked_type_spelling(
    const MarimayCheckResult *result,
    MarimayTypeId type,
    MarimayAllocator allocator,
    MarimayTypeSpellingOptions options,
    MarimayOwnedBytes *output
);

#endif
