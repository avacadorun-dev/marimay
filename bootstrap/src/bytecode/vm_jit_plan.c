#include "vm_jit_internal.h"

static bool eligible(const MarimayBytecodeImageFunction *image,
    const MarimayBytecodeFunction *function) {
    return function->register_types.length <= MMAY_JIT_MAX_REGISTERS
        && image->operation_count <= 32768;
}

static bool backedge(const MarimayBytecodeImageFunction *image) {
    for (size_t pc = 0; pc < image->operation_count; ++pc) {
        const MarimayBytecodeImageOperation *op = &image->operations[pc];
        if ((op->opcode == MMAY_OPCODE_JUMP && op->first <= pc)
            || (op->opcode == MMAY_OPCODE_BRANCH_BOOL
                && (op->second <= pc || op->third <= pc))) return true;
    }
    return false;
}

MarimayBytecodeRunStatus mmay_bytecode_jit_select(
    const MarimayBytecodeProgram *program, const MarimayBytecodeExecutionImage *image,
    MarimayAllocator allocator, bool meter_fuel, void **storage, size_t *bytes,
    unsigned char **selected
) {
    const MarimayBytecodeProgramFunction *functions = program->functions.data;
    size_t count = image->function_count;
    bool any = false;
    *storage = NULL; *selected = NULL; *bytes = 0;
    for (size_t id = 0; id < count && !any; ++id)
        any = eligible(&image->functions[id], &functions[id].body)
            && backedge(&image->functions[id]);
    if (!any || count > UINT32_MAX) return MMAY_BYTECODE_RUN_OK;
    if (!mmay_size_multiply(count, sizeof(uint32_t) + 1, bytes))
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    *storage = mmay_allocate(&allocator, *bytes);
    if (*storage == NULL) return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    uint32_t *queue = *storage;
    *selected = (unsigned char *)(queue + count);
    size_t tail = 0;
    for (size_t id = 0; id < count; ++id) {
        (*selected)[id] = (unsigned char)(eligible(&image->functions[id], &functions[id].body)
            && backedge(&image->functions[id]));
        if ((*selected)[id]) queue[tail++] = (uint32_t)id;
    }
    /* The unmetered native call tier needs the reachable direct callees too.
     * Each function enters the queue once; no repeated whole-program scans. */
    for (size_t head = 0; !meter_fuel && head < tail; ++head) {
        const MarimayBytecodeImageFunction *body = &image->functions[queue[head]];
        for (size_t pc = 0; pc < body->operation_count; ++pc) {
            const MarimayBytecodeImageOperation *op = &body->operations[pc];
            if (op->opcode != MMAY_OPCODE_CALL_DIRECT
                && op->opcode != MMAY_OPCODE_CALL_DIRECT_VOID) continue;
            size_t callee = op->third;
            if (callee >= count || (*selected)[callee]
                || !eligible(&image->functions[callee], &functions[callee].body)) continue;
            (*selected)[callee] = 1;
            queue[tail++] = (uint32_t)callee;
        }
    }
    return MMAY_BYTECODE_RUN_OK;
}

static bool mmay_jit_plan_supported(const MarimayBytecodeFunction *function,
    const MarimayBytecodeImageOperation *op, bool meter_fuel
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
        case MMAY_OPCODE_CALL_DIRECT: case MMAY_OPCODE_CALL_DIRECT_VOID:
        case MMAY_OPCODE_RETURN: case MMAY_OPCODE_RETURN_VOID:
            return !meter_fuel;
        case MMAY_OPCODE_RECORD_GET:
            return types[op->first] != MMAY_BYTECODE_TYPE_STRING;
        case MMAY_OPCODE_ARRAY_GET: case MMAY_OPCODE_ARRAY_GET_IN_BOUNDS:
            return mmay_bytecode_jit_array_width(function, op->second) != 0;
        case MMAY_OPCODE_ARRAY_PUSH:
        case MMAY_OPCODE_ARRAY_SET: case MMAY_OPCODE_ARRAY_SET_IN_BOUNDS:
            /* Reference stores still require the runtime child-type checks. */
            return mmay_bytecode_jit_array_width(function, op->first) != 0
                && mmay_bytecode_jit_array_width(function, op->first) <= 8;
        default: return false;
    }
}

void mmay_bytecode_jit_plan(const MarimayBytecodeImageFunction *image,
    const MarimayBytecodeFunction *function, MarimayBytecodeJitPlanOperation *plan, bool meter_fuel
) {
    size_t pc = 0;
    while (pc < image->operation_count) {
        const MarimayBytecodeImageOperation *operation = &image->operations[pc];
        if (!mmay_jit_plan_supported(function, operation, meter_fuel)) {
            plan[pc++] = (MarimayBytecodeJitPlanOperation){0, false, false};
            continue;
        }
        size_t end = pc + 1;
        while (end < image->operation_count
            && image->operations[end].source_block == operation->source_block
            && mmay_jit_plan_supported(function, &image->operations[end], meter_fuel)) end += 1;
        bool starts = true;
        while (pc < end) {
            plan[pc++] = (MarimayBytecodeJitPlanOperation){(uint32_t)end, true, starts};
            starts = false;
        }
    }
}
