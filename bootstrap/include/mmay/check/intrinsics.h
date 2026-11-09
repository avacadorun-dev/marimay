#ifndef MMAY_CHECK_INTRINSICS_H
#define MMAY_CHECK_INTRINSICS_H

#include "mmay/check/result.h"

typedef enum MarimayIntrinsicKind {
    MMAY_INTRINSIC_JSON_DECODE,
    MMAY_INTRINSIC_JSON_DECODE_WITH_LIMITS,
    MMAY_INTRINSIC_JSON_ENCODE,
    MMAY_INTRINSIC_JSON_ENCODE_WITH_LIMITS
} MarimayIntrinsicKind;

/* Immutable compiler-owned text, not a source declaration or callable type. */
typedef struct MarimayIntrinsicSignature {
    MarimayIntrinsicKind kind;
    const char *package_name;
    const char *name;
    const char *signature;
    const char *constraint;
    size_t min_type_arguments;
    size_t max_type_arguments;
    size_t argument_count;
    bool direct_call_only;
} MarimayIntrinsicSignature;

/* File import scope only; no cursor-local availability is implied. */
typedef struct MarimayCheckedIntrinsicCompletion {
    const MarimayIntrinsicSignature *intrinsic;
    const MarimaySourceSnapshot *snapshot;
    MarimayOwnedBytes spelling;
} MarimayCheckedIntrinsicCompletion;

/* Actual resolved direct-call sites, without an invented function symbol/type. */
typedef struct MarimayCheckedIntrinsicCall {
    const MarimayIntrinsicSignature *intrinsic;
    const MarimaySourceSnapshot *snapshot;
    size_t start;
    size_t end;
} MarimayCheckedIntrinsicCall;

const MarimayIntrinsicSignature *mmay_intrinsic_signature(size_t index);
const MarimayCheckedIntrinsicCall *mmay_checked_intrinsic_call(
    const MarimayCheckResult *result, size_t index);
const MarimayCheckedIntrinsicCall *mmay_checked_intrinsic_call_at(
    const MarimayCheckResult *result, const MarimaySourceSnapshot *snapshot,
    size_t offset);

/* Borrowed, allocation-free exact byte-prefix queries, ordered by spelling. */
size_t mmay_checked_intrinsic_completion_count(
    const MarimayCheckResult *result, const MarimaySourceSnapshot *snapshot,
    const void *prefix, size_t prefix_length);
const MarimayCheckedIntrinsicCompletion *mmay_checked_intrinsic_completion(
    const MarimayCheckResult *result, const MarimaySourceSnapshot *snapshot,
    const void *prefix, size_t prefix_length, size_t match_index);

#endif
