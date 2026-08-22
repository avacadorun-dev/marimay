#ifndef RUDA_MIR_RANGE_H
#define RUDA_MIR_RANGE_H

#include "ruda/mir/function.h"

typedef enum RudaMirRangeOptimizeStatus {
    RUDA_MIR_RANGE_OPTIMIZE_OK,
    RUDA_MIR_RANGE_OPTIMIZE_INVALID_ARGUMENT,
    RUDA_MIR_RANGE_OPTIMIZE_INVALID_MIR,
    RUDA_MIR_RANGE_OPTIMIZE_ALLOCATION_FAILED,
    RUDA_MIR_RANGE_OPTIMIZE_LIMIT_REACHED
} RudaMirRangeOptimizeStatus;

/*
 * Rewrites only verifier-backed duplicate checked array accesses. On every
 * non-OK result, `function` is unchanged so callers retain checked MIR.
 */
RudaMirRangeOptimizeStatus ruda_mir_optimize_ranges(
    RudaMirFunction *function
);

#endif
