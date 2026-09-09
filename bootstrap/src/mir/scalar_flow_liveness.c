#include "scalar_flow_internal.h"
#include "mir_internal.h"

#include <string.h>

static bool flow_arguments(MarimayMirScalarFlow *flow, unsigned char *live,
    const MarimayVector *arguments, size_t start, size_t count) {
    if (!mmay_mir_scalar_flow_work(flow, count)) return false;
    for (size_t index = start; index < start + count; index += 1) {
        /* Every verified MIR construction/call argument starts with its exact
         * register id. memcpy avoids aliasing between their distinct types. */
        MarimayMirRegister reg;
        memcpy(&reg, (const unsigned char *)arguments->data
            + index * arguments->element_size, sizeof(reg));
        live[reg] = 1;
    }
    return true;
}

static bool flow_call_arguments(MarimayMirScalarFlow *flow, unsigned char *live,
    MarimayMirCallId id) {
    const MarimayMirCall *call = mmay_mir_call(flow->function, id);
    return flow_arguments(flow, live, &flow->function->call_arguments,
        call->argument_start, call->argument_count);
}

static bool flow_instruction_reads(MarimayMirScalarFlow *flow, unsigned char *live,
    const MarimayMirInstruction *instruction) {
    const MarimayMirFunction *function = flow->function;
    if (!mmay_mir_scalar_flow_work(flow, 1)) return false;
    if (mmay_mir_instruction_uses_first(instruction->kind)) live[instruction->first] = 1;
    if (mmay_mir_instruction_uses_second(instruction->kind)) live[instruction->second] = 1;
    if (mmay_mir_instruction_uses_third(instruction->kind)) live[instruction->third] = 1;
    switch (instruction->kind) {
        case MMAY_MIR_CALL:
        case MMAY_MIR_CALL_INTERFACE:
        case MMAY_MIR_DEFER_REGISTER:
        case MMAY_MIR_DEFER_CALL:
            return flow_call_arguments(flow, live, instruction->call);
        case MMAY_MIR_RECORD_NEW: {
            const MarimayMirRecordConstruction *c = mmay_mir_record_construction(function,
                instruction->record_construction);
            return flow_arguments(flow, live, &function->record_arguments,
                c->argument_start, c->argument_count);
        }
        case MMAY_MIR_ENUM_NEW: {
            const MarimayMirEnumConstruction *c = mmay_mir_enum_construction(function,
                instruction->enum_construction);
            return flow_arguments(flow, live, &function->enum_arguments,
                c->argument_start, c->argument_count);
        }
        case MMAY_MIR_ARRAY_NEW: {
            const MarimayMirArrayConstruction *c = mmay_mir_array_construction(function,
                instruction->array_construction);
            return flow_arguments(flow, live, &function->array_arguments,
                c->argument_start, c->argument_count);
        }
        case MMAY_MIR_INTERFACE_NEW: {
            const MarimayMirInterfaceConstruction *c = mmay_mir_interface_construction(
                function, instruction->interface_construction);
            live[c->payload] = 1;
            return true;
        }
        default: return true;
    }
}

static bool flow_live_out(MarimayMirScalarFlow *flow, const MarimayMirBlock *block,
    const unsigned char *incoming, unsigned char *live) {
    size_t count = flow->function->register_types.length;
    if (!mmay_mir_scalar_flow_work(flow, count)) return false;
    memset(live, 0, count);
    const MarimayMirTerminator *term = &block->terminator;
    if (term->kind == MMAY_MIR_GOTO || term->kind == MMAY_MIR_BRANCH) {
        memcpy(live, incoming + (size_t)term->target * count, count);
    }
    if (term->kind == MMAY_MIR_BRANCH) {
        const unsigned char *other = incoming + (size_t)term->otherwise * count;
        for (size_t index = 0; index < count; index += 1) live[index] |= other[index];
    }
    if (term->kind == MMAY_MIR_BRANCH || term->kind == MMAY_MIR_PANIC
        || (term->kind == MMAY_MIR_RETURN && term->value != MMAY_MIR_NO_REGISTER)) {
        live[term->value] = 1;
    }
    return term->kind != MMAY_MIR_CALL_NEVER || flow_call_arguments(flow, live, term->call);
}

static bool flow_transfer(MarimayMirScalarFlow *flow, MarimayMirBlock *block,
    unsigned char *live, const unsigned char *pinned, bool rewrite) {
    MarimayMirInstruction *instructions = (MarimayMirInstruction *)block->instructions.data;
    size_t write = block->instructions.length;
    for (size_t index = block->instructions.length; index != 0; index -= 1) {
        if (!mmay_mir_scalar_flow_work(flow, 1)) return false;
        MarimayMirInstruction instruction = instructions[index - 1];
        if (!live[instruction.target] && !pinned[instruction.target]
            && mmay_mir_scalar_flow_removable(flow->function, &instruction)) continue;
        live[instruction.target] = 0;
        if (!flow_instruction_reads(flow, live, &instruction)) return false;
        if (rewrite) instructions[--write] = instruction;
    }
    if (rewrite && write != 0) {
        size_t kept = block->instructions.length - write;
        if (kept != 0) memmove(instructions, instructions + write, kept * sizeof(*instructions));
        block->instructions.length = kept;
        flow->function->instruction_count -= write;
        flow->changed = true;
    }
    return true;
}

MarimayMirScalarFlowStatus mmay_mir_scalar_flow_liveness(MarimayMirScalarFlow *flow) {
    MarimayMirFunction *function = flow->function;
    size_t registers = function->register_types.length;
    size_t row_bytes = registers == 0 ? 1 : registers;
    size_t bytes = 0;
    if (!mmay_size_multiply(function->blocks.length, row_bytes, &bytes)
        || bytes > MMAY_MIR_MAX_DATAFLOW_CELLS) return MMAY_MIR_SCALAR_FLOW_LIMIT_REACHED;
    unsigned char *incoming = mmay_allocate(&function->allocator, bytes);
    unsigned char *scratch = mmay_allocate(&function->allocator, row_bytes);
    unsigned char *pinned = mmay_allocate(&function->allocator, row_bytes);
    MarimayMirScalarFlowStatus status = MMAY_MIR_SCALAR_FLOW_ALLOCATION_FAILED;
    if (incoming == NULL || scratch == NULL || pinned == NULL) goto cleanup;
    memset(incoming, 0, bytes);
    memset(pinned, 0, row_bytes);
    status = MMAY_MIR_SCALAR_FLOW_LIMIT_REACHED;
    for (size_t b = 0; b < function->blocks.length; b += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(&function->blocks, b);
        for (size_t i = 0; i < block->instructions.length; i += 1) {
            if (!mmay_mir_scalar_flow_work(flow, 1)) goto cleanup;
            const MarimayMirInstruction *ins = mmay_vector_at_const(&block->instructions, i);
            /* Panic/resource unwinding can read these outside explicit CFG
             * edges. Keep every definition of their captured registers. */
            if ((ins->kind == MMAY_MIR_DEFER_REGISTER || ins->kind == MMAY_MIR_DEFER_CALL)
                && !flow_call_arguments(flow, pinned, ins->call)) goto cleanup;
        }
    }
    bool changed;
    do {
        changed = false;
        for (size_t b = function->blocks.length; b != 0; b -= 1) {
            MarimayMirBlock *block = mmay_vector_at(&function->blocks, b - 1);
            if (!flow_live_out(flow, block, incoming, scratch)
                || !flow_transfer(flow, block, scratch, pinned, false)) goto cleanup;
            unsigned char *previous = incoming + (b - 1) * registers;
            if (memcmp(previous, scratch, registers) != 0) {
                memcpy(previous, scratch, registers);
                changed = true;
            }
        }
    } while (changed);
    for (size_t b = 0; b < function->blocks.length; b += 1) {
        MarimayMirBlock *block = mmay_vector_at(&function->blocks, b);
        if (!flow_live_out(flow, block, incoming, scratch)
            || !flow_transfer(flow, block, scratch, pinned, true)) goto cleanup;
    }
    status = MMAY_MIR_SCALAR_FLOW_OK;
cleanup:
    mmay_deallocate(&function->allocator, pinned, row_bytes);
    mmay_deallocate(&function->allocator, scratch, row_bytes);
    mmay_deallocate(&function->allocator, incoming, bytes);
    return status;
}
