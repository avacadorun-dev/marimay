#ifndef MMAY_MIR_SCALAR_FLOW_INTERNAL_H
#define MMAY_MIR_SCALAR_FLOW_INTERNAL_H

#include "mmay/mir/scalar_flow.h"

/* No analysis cache crosses a transformation. Only the privately cloned
 * blocks are mutable; function metadata remains borrowed until publication. */
typedef struct MarimayMirScalarFlow {
    MarimayMirFunction *function;
    size_t work;
    bool changed;
} MarimayMirScalarFlow;

bool mmay_mir_scalar_flow_work(MarimayMirScalarFlow *flow, size_t amount);
bool mmay_mir_scalar_flow_scalar(MarimayTypeId type);
bool mmay_mir_scalar_flow_literal(MarimayMirInstructionKind kind);
bool mmay_mir_scalar_flow_removable(const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction);
MarimayMirScalarFlowStatus mmay_mir_scalar_flow_hoist(MarimayMirScalarFlow *flow);
MarimayMirScalarFlowStatus mmay_mir_scalar_flow_values(MarimayMirScalarFlow *flow);
MarimayMirScalarFlowStatus mmay_mir_scalar_flow_liveness(MarimayMirScalarFlow *flow);
MarimayMirScalarFlowStatus mmay_mir_scalar_flow_control(MarimayMirScalarFlow *flow);

#endif
