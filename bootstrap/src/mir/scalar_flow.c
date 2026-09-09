#include "scalar_flow_internal.h"
#include "mir_internal.h"

#include <string.h>

bool mmay_mir_scalar_flow_work(MarimayMirScalarFlow *flow, size_t amount) {
    return mmay_mir_consume_verify_work(&flow->work, amount,
        flow->function->options.max_verify_work);
}

bool mmay_mir_scalar_flow_scalar(MarimayTypeId type) {
    return type >= MMAY_TYPE_BOOL && type <= MMAY_TYPE_RUNE;
}

bool mmay_mir_scalar_flow_literal(MarimayMirInstructionKind kind) {
    return kind == MMAY_MIR_CONSTANT_I64 || kind == MMAY_MIR_CONSTANT_INTEGER
        || kind == MMAY_MIR_CONSTANT_BOOL || kind == MMAY_MIR_CONSTANT_F64;
}

bool mmay_mir_scalar_flow_removable(const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction) {
    if (!mmay_mir_scalar_flow_scalar(mmay_mir_register_type(function,
            instruction->target))) return false;
    if (mmay_mir_scalar_flow_literal(instruction->kind)) return true;
    switch (instruction->kind) {
        case MMAY_MIR_COPY:
        case MMAY_MIR_NOT_BOOL:
        case MMAY_MIR_EQUAL_BOOL:
        case MMAY_MIR_EQUAL_I64:
        case MMAY_MIR_LESS_I64:
        case MMAY_MIR_LESS_EQUAL_I64:
        case MMAY_MIR_BITWISE_NOT_I64:
        case MMAY_MIR_BITWISE_AND_I64:
        case MMAY_MIR_BITWISE_OR_I64:
        case MMAY_MIR_BITWISE_XOR_I64:
        case MMAY_MIR_ADD_PROVEN_I64:
        case MMAY_MIR_SUBTRACT_PROVEN_I64:
        case MMAY_MIR_MULTIPLY_PROVEN_I64:
            return true;
        default: return false;
    }
}

static void scalar_flow_blocks_dispose(MarimayVector *blocks) {
    for (size_t index = 0; index < blocks->length; index += 1) {
        MarimayMirBlock *block = mmay_vector_at(blocks, index);
        mmay_vector_dispose(&block->instructions);
    }
    mmay_vector_dispose(blocks);
}

static bool scalar_flow_blocks_clone(const MarimayMirFunction *function,
    MarimayVector *blocks) {
    if (!mmay_vector_init(blocks, function->allocator, sizeof(MarimayMirBlock))
        || !mmay_vector_reserve(blocks, function->blocks.length)) return false;
    for (size_t index = 0; index < function->blocks.length; index += 1) {
        const MarimayMirBlock *source = mmay_vector_at_const(&function->blocks, index);
        MarimayMirBlock block = {0};
        block.allocator = function->allocator;
        block.terminator = source->terminator;
        block.initialized = true;
        if (!mmay_vector_init(&block.instructions, function->allocator,
                sizeof(MarimayMirInstruction))
            || !mmay_vector_reserve(&block.instructions, source->instructions.length)) {
            mmay_vector_dispose(&block.instructions);
            return false;
        }
        if (source->instructions.length != 0) {
            memcpy(block.instructions.data, source->instructions.data,
                source->instructions.length * sizeof(MarimayMirInstruction));
        }
        block.instructions.length = source->instructions.length;
        if (!mmay_vector_push(blocks, &block)) {
            mmay_vector_dispose(&block.instructions);
            return false;
        }
    }
    return true;
}

static MarimayMirScalarFlowStatus scalar_flow_verify_status(MarimayMirVerifyStatus status) {
    if (status == MMAY_MIR_VERIFY_OK) return MMAY_MIR_SCALAR_FLOW_OK;
    if (status == MMAY_MIR_VERIFY_ALLOCATION_FAILED) return MMAY_MIR_SCALAR_FLOW_ALLOCATION_FAILED;
    if (status == MMAY_MIR_VERIFY_LIMIT_REACHED) return MMAY_MIR_SCALAR_FLOW_LIMIT_REACHED;
    return MMAY_MIR_SCALAR_FLOW_INVALID_MIR;
}

MarimayMirScalarFlowStatus mmay_mir_simplify_scalar_flow(MarimayMirFunction *function) {
    if (function == NULL || !function->initialized) return MMAY_MIR_SCALAR_FLOW_INVALID_ARGUMENT;
    MarimayMirScalarFlowStatus status = scalar_flow_verify_status(mmay_mir_verify(function));
    if (status != MMAY_MIR_SCALAR_FLOW_OK) return status;
    MarimayMirFunction candidate = *function;
    memset(&candidate.blocks, 0, sizeof(candidate.blocks));
    if (!scalar_flow_blocks_clone(function, &candidate.blocks)) {
        scalar_flow_blocks_dispose(&candidate.blocks);
        return MMAY_MIR_SCALAR_FLOW_ALLOCATION_FAILED;
    }
    MarimayMirScalarFlow flow = {&candidate, 0, false};
    status = mmay_mir_scalar_flow_control(&flow);
    if (status == MMAY_MIR_SCALAR_FLOW_OK) status = mmay_mir_scalar_flow_hoist(&flow);
    do {
        flow.changed = false;
        if (status == MMAY_MIR_SCALAR_FLOW_OK) status = mmay_mir_scalar_flow_values(&flow);
        if (status == MMAY_MIR_SCALAR_FLOW_OK) status = mmay_mir_scalar_flow_liveness(&flow);
        if (status == MMAY_MIR_SCALAR_FLOW_OK) status = mmay_mir_scalar_flow_control(&flow);
    } while (status == MMAY_MIR_SCALAR_FLOW_OK && flow.changed);
    if (status == MMAY_MIR_SCALAR_FLOW_OK) {
        status = scalar_flow_verify_status(mmay_mir_verify(&candidate));
    }
    if (status == MMAY_MIR_SCALAR_FLOW_OK) {
        scalar_flow_blocks_dispose(&function->blocks);
        function->blocks = candidate.blocks;
        function->instruction_count = candidate.instruction_count;
    } else {
        scalar_flow_blocks_dispose(&candidate.blocks);
    }
    return status;
}
