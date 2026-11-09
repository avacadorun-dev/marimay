#include "intrinsic_calls.h"

#include "checker_internal.h"
#include "checker_package_context.h"

static int mmay_intrinsic_call_compare(const MarimayCheckedIntrinsicCall *left,
    const MarimayCheckedIntrinsicCall *right) {
    if (left->snapshot != right->snapshot) {
        size_t left_length = mmay_source_path_length(left->snapshot);
        size_t right_length = mmay_source_path_length(right->snapshot);
        size_t shared = left_length < right_length ? left_length : right_length;
        int order = memcmp(mmay_source_path(left->snapshot),
            mmay_source_path(right->snapshot), shared);
        if (order != 0) return order;
        if (left_length != right_length) return left_length > right_length ? 1 : -1;
        if (mmay_source_snapshot_id(left->snapshot) != mmay_source_snapshot_id(right->snapshot))
            return mmay_source_snapshot_id(left->snapshot)
                > mmay_source_snapshot_id(right->snapshot) ? 1 : -1;
    }
    if (left->start != right->start) return left->start > right->start ? 1 : -1;
    if (left->end != right->end) return left->end > right->end ? 1 : -1;
    return (left->intrinsic->kind > right->intrinsic->kind)
        - (left->intrinsic->kind < right->intrinsic->kind);
}

bool mmay_check_record_intrinsic_call(MarimayChecker *checker,
    const MarimayIntrinsicSignature *intrinsic, size_t start, size_t end) {
    MarimayCheckedIntrinsicCall call;
    MarimayVector *calls = NULL;
    size_t low = 0, high = 0;
    if (checker == NULL || checker->result == NULL || checker->ast == NULL
        || intrinsic == NULL || start >= end || end > mmay_source_length(checker->ast->snapshot))
        return false;
    calls = &checker->result->intrinsic_calls;
    call = (MarimayCheckedIntrinsicCall){intrinsic, checker->ast->snapshot, start, end};
    high = calls->length;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        const MarimayCheckedIntrinsicCall *existing = mmay_vector_at_const(calls, middle);
        int order = mmay_intrinsic_call_compare(existing, &call);
        if (order == 0) return true;
        if (order < 0) low = middle + 1;
        else high = middle;
    }
    if (calls->length >= checker->options.max_symbols) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!mmay_vector_push(calls, &call)) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    if (low + 1 < calls->length) {
        MarimayCheckedIntrinsicCall *items = calls->data;
        memmove(items + low + 1, items + low,
            (calls->length - low - 1) * sizeof(*items));
        items[low] = call;
    }
    return true;
}

const MarimayCheckedIntrinsicCall *mmay_checked_intrinsic_call(
    const MarimayCheckResult *result, size_t index) {
    return result != NULL && result->initialized && result->error_count == 0
        ? mmay_vector_at_const(&result->intrinsic_calls, index) : NULL;
}

const MarimayCheckedIntrinsicCall *mmay_checked_intrinsic_call_at(
    const MarimayCheckResult *result, const MarimaySourceSnapshot *snapshot,
    size_t offset) {
    size_t index = 0;
    if (result == NULL || !result->initialized || result->error_count != 0) return NULL;
    for (index = 0; index < result->intrinsic_calls.length; index += 1) {
        const MarimayCheckedIntrinsicCall *call = mmay_vector_at_const(
            &result->intrinsic_calls, index);
        if (call->snapshot == snapshot && call->start <= offset && offset < call->end)
            return call;
    }
    return NULL;
}
