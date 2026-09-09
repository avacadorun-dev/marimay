#ifndef MMAY_MIR_SCALAR_FLOW_H
#define MMAY_MIR_SCALAR_FLOW_H

#include "mmay/mir/function.h"

typedef enum MarimayMirScalarFlowStatus {
    MMAY_MIR_SCALAR_FLOW_OK,
    MMAY_MIR_SCALAR_FLOW_INVALID_ARGUMENT,
    MMAY_MIR_SCALAR_FLOW_INVALID_MIR,
    MMAY_MIR_SCALAR_FLOW_ALLOCATION_FAILED,
    MMAY_MIR_SCALAR_FLOW_LIMIT_REACHED
} MarimayMirScalarFlowStatus;

/* D106: verify, privately transform block storage, reverify, then publish.
 * Every non-OK result leaves the original function and metadata unchanged. */
MarimayMirScalarFlowStatus mmay_mir_simplify_scalar_flow(
    MarimayMirFunction *function);

#endif
