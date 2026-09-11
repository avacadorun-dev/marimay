#include "vm_jit_internal.h"
#include <stdalign.h>
#include <string.h>

/* Per-invocation projection of verified call and frame metadata. This owner
 * never initializes a live frame and never copies dynamic managed values. */
MarimayBytecodeRunStatus mmay_bytecode_jit_prepare_activations(MarimayBytecodeJit *jit,
    const MarimayBytecodeProgram *program, const MarimayBytecodeExecutionImage *image,
    const unsigned char *selected) {
    if (jit->meters_fuel) return MMAY_BYTECODE_RUN_OK;
    const MarimayBytecodeProgramFunction *functions = program->functions.data;
    size_t bytes = 0, view_bytes = 0;
    _Static_assert(sizeof(MarimayBytecodeValue) % alignof(MarimayBytecodePreparedCall) == 0
        && sizeof(size_t) % alignof(MarimayBytecodePreparedCall) == 0
        && sizeof(MarimayBytecodePreparedCall) % alignof(MarimayBytecodeValue) == 0
        && sizeof(MarimayBytecodePreparedCall) % alignof(MarimayBytecodeJitBinding) == 0
        && sizeof(MarimayBytecodeJitBinding) % alignof(MarimayBytecodeValue) == 0,
        "activation arena alignment");
    for (size_t id = 0; id < image->function_count; ++id) {
        if (!selected[id]) continue;
        size_t layout_bytes = 0, call_bytes = 0, binding_bytes = 0;
        if (!mmay_bytecode_frame_layout_size(&functions[id], &layout_bytes)
            || !mmay_size_multiply(functions[id].body.calls.length,
                sizeof(MarimayBytecodePreparedCall), &call_bytes)
            || !mmay_size_add(bytes, layout_bytes, &bytes)
            || !mmay_size_add(bytes, call_bytes, &bytes))
            return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        if (!mmay_size_multiply(functions[id].body.register_types.length,
                sizeof(MarimayBytecodeJitBinding), &binding_bytes))
            return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        /* A bounded view cache is optional. Excess functions retain the shared
         * checked table; no logical activation or managed object is cached. */
        if (binding_bytes <= MMAY_JIT_MAX_VIEW_BYTES - view_bytes) {
            jit->functions[id].binding_bytes = binding_bytes;
            view_bytes += binding_bytes;
            if (!mmay_size_add(bytes, binding_bytes, &bytes))
                return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        }
    }
    void *storage = mmay_allocate(&jit->allocator, bytes);
    if (storage == NULL) return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    jit->state->activation_metadata = storage;
    jit->state->activation_metadata_bytes = bytes;
    memset(storage, 0, bytes);
    size_t position = 0;
    for (size_t id = 0; id < image->function_count; ++id) {
        if (!selected[id]) continue;
        size_t layout_bytes = 0;
        MarimayBytecodeJitFunction *compiled = &jit->functions[id];
        if (!mmay_bytecode_frame_layout_size(&functions[id], &layout_bytes)
            || !mmay_bytecode_frame_layout_init(&functions[id],
                (unsigned char *)storage + position, layout_bytes, &compiled->frame_layout))
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        position += layout_bytes;
        compiled->calls = (MarimayBytecodePreparedCall *)((unsigned char *)storage + position);
        position += functions[id].body.calls.length * sizeof(*compiled->calls);
        if (compiled->binding_bytes != 0) {
            compiled->bindings = (MarimayBytecodeJitBinding *)((unsigned char *)storage + position);
            position += compiled->binding_bytes;
        }
    }
    for (size_t id = 0; id < image->function_count; ++id) {
        if (!selected[id]) continue;
        const MarimayBytecodeImageFunction *body = &image->functions[id];
        const MarimayBytecodeCall *calls = functions[id].body.calls.data;
        for (size_t pc = 0; pc < body->operation_count; ++pc) {
            const MarimayBytecodeImageOperation *op = &body->operations[pc];
            if (op->opcode != MMAY_OPCODE_CALL_DIRECT
                && op->opcode != MMAY_OPCODE_CALL_DIRECT_VOID) continue;
            uint32_t wire = op->opcode == MMAY_OPCODE_CALL_DIRECT_VOID ? op->first : op->second;
            if (wire >= functions[id].body.calls.length || op->third >= image->function_count)
                return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
            if (!selected[op->third]) continue;
            const MarimayBytecodeFunction *function = &functions[id].body;
            const MarimayBytecodeCall *call = &calls[wire];
            const MarimayBytecodeCallArgument *arguments = function->call_arguments.data;
            if (call->argument_count != 0 && (arguments == NULL
                || call->argument_start > function->call_arguments.length
                || call->argument_count > function->call_arguments.length - call->argument_start))
                return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
            jit->functions[id].calls[wire] = (MarimayBytecodePreparedCall){
                &functions[id], &functions[op->third], &calls[wire],
                &jit->functions[op->third].frame_layout,
                image->functions[op->third].borrows_caller_roots,
                call->argument_count != 0 ? arguments + call->argument_start : NULL,
                op->third
            };
        }
    }
    return MMAY_BYTECODE_RUN_OK;
}
