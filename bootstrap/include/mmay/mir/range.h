#ifndef MMAY_MIR_RANGE_H
#define MMAY_MIR_RANGE_H

#include "mmay/mir/function.h"

typedef enum MarimayMirRangeOptimizeStatus {
    MMAY_MIR_RANGE_OPTIMIZE_OK,
    MMAY_MIR_RANGE_OPTIMIZE_INVALID_ARGUMENT,
    MMAY_MIR_RANGE_OPTIMIZE_INVALID_MIR,
    MMAY_MIR_RANGE_OPTIMIZE_ALLOCATION_FAILED,
    MMAY_MIR_RANGE_OPTIMIZE_LIMIT_REACHED
} MarimayMirRangeOptimizeStatus;

/*
 * Rewrites only verifier-backed duplicate checked array accesses. On every
 * non-OK result, `function` is unchanged so callers retain checked MIR.
 */
MarimayMirRangeOptimizeStatus mmay_mir_optimize_ranges(
    MarimayMirFunction *function
);

#endif
