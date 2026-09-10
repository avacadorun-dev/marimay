#include "vm_jit_internal.h"

static bool mmay_jit_plan_supported(const MarimayBytecodeFunction *function,
    const MarimayBytecodeImageOperation *op
) {
    const MarimayBytecodeType *types = function->register_types.data;
    switch (op->opcode) {
        case MMAY_OPCODE_CONST_I64: case MMAY_OPCODE_CONST_BOOL:
        case MMAY_OPCODE_CONST_INTEGER: case MMAY_OPCODE_CONST_F64:
        case MMAY_OPCODE_COPY: case MMAY_OPCODE_BOOL_NOT:
        case MMAY_OPCODE_BOOL_EQUAL:
        case MMAY_OPCODE_JUMP: case MMAY_OPCODE_BRANCH_BOOL:
        case MMAY_OPCODE_ARRAY_LENGTH: case MMAY_OPCODE_ARRAY_REQUIRE_INDEX:
        case MMAY_OPCODE_ARRAY_GENERATION: case MMAY_OPCODE_ARRAY_REQUIRE_GENERATION:
        case MMAY_OPCODE_ENUM_IS_VARIANT: case MMAY_OPCODE_ENUM_REQUIRE_VARIANT:
        case MMAY_OPCODE_ENUM_GET_PAYLOAD: return true;
        case MMAY_OPCODE_I64_EQUAL:
        case MMAY_OPCODE_I64_LESS: case MMAY_OPCODE_I64_LESS_EQUAL:
        case MMAY_OPCODE_I64_ADD_CHECKED: case MMAY_OPCODE_I64_SUBTRACT_CHECKED:
        case MMAY_OPCODE_I64_DIVIDE_CHECKED: case MMAY_OPCODE_I64_REMAINDER_CHECKED:
        case MMAY_OPCODE_I64_MULTIPLY_CHECKED: case MMAY_OPCODE_I64_NEGATE_CHECKED:
        case MMAY_OPCODE_I64_ADD_PROVEN: case MMAY_OPCODE_I64_SUBTRACT_PROVEN:
        case MMAY_OPCODE_I64_MULTIPLY_PROVEN:
            /* These historical opcodes also carry other integer widths. The
             * current emitter implements signed i64, not their typed policies. */
            return types[op->second] == MMAY_BYTECODE_TYPE_I64;
        case MMAY_OPCODE_RECORD_GET:
            return types[op->first] != MMAY_BYTECODE_TYPE_STRING;
        case MMAY_OPCODE_ARRAY_GET: case MMAY_OPCODE_ARRAY_GET_IN_BOUNDS:
            return mmay_bytecode_jit_array_width(function, op->second) != 0;
        case MMAY_OPCODE_ARRAY_SET: case MMAY_OPCODE_ARRAY_SET_IN_BOUNDS:
            return mmay_bytecode_jit_array_width(function, op->first) != 0;
        default: return false;
    }
}

void mmay_bytecode_jit_plan(const MarimayBytecodeImageFunction *image,
    const MarimayBytecodeFunction *function, MarimayBytecodeJitPlanOperation *plan
) {
    size_t pc = 0;
    while (pc < image->operation_count) {
        const MarimayBytecodeImageOperation *operation = &image->operations[pc];
        if (!mmay_jit_plan_supported(function, operation)) {
            plan[pc++] = (MarimayBytecodeJitPlanOperation){0, false, false};
            continue;
        }
        size_t end = pc + 1;
        while (end < image->operation_count
            && image->operations[end].source_block == operation->source_block
            && mmay_jit_plan_supported(function, &image->operations[end])) end += 1;
        bool starts = true;
        while (pc < end) {
            plan[pc++] = (MarimayBytecodeJitPlanOperation){(uint32_t)end, true, starts};
            starts = false;
        }
    }
}
