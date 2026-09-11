#include "vm_jit_internal.h"
#include "vm_hot.h"
#include "vm_internal.h"
#include <string.h>

void mmay_bytecode_jit_clear_bindings(MarimayBytecodeJit *jit) {
    memset(jit->bindings, 0, jit->binding_bytes);
    for (size_t id = 0; id < jit->function_count; ++id) {
        MarimayBytecodeJitFunction *function = &jit->functions[id];
        if (function->bindings != NULL)
            memset(function->bindings, 0, function->binding_bytes);
    }
}

size_t mmay_bytecode_jit_array_width(const MarimayBytecodeFunction *function,
    uint32_t reg
) {
    const MarimayBytecodeType *types = function->register_types.data;
    const uint32_t *layouts = function->register_layouts.data;
    uint32_t ignored = 0;
    MarimayBytecodeType type = mmay_bytecode_function_array_element_type(
        function, types[reg], layouts[reg], &ignored);
    switch (type) {
        case MMAY_BYTECODE_TYPE_BOOL:
        case MMAY_BYTECODE_TYPE_I8: case MMAY_BYTECODE_TYPE_U8: return 1;
        case MMAY_BYTECODE_TYPE_I16: case MMAY_BYTECODE_TYPE_U16: return 2;
        case MMAY_BYTECODE_TYPE_I32: case MMAY_BYTECODE_TYPE_U32:
        case MMAY_BYTECODE_TYPE_F32: case MMAY_BYTECODE_TYPE_RUNE: return 4;
        case MMAY_BYTECODE_TYPE_I64: case MMAY_BYTECODE_TYPE_U64:
        case MMAY_BYTECODE_TYPE_F64: return 8;
        default:
            return mmay_bytecode_type_is_managed(type) ? sizeof(MarimayManagedCell) : 0;
    }
}

void *mmay_bytecode_jit_bind(MarimayBytecodeJitContext *context, uint32_t reg) {
    if (context->execution == NULL) return NULL;
    const MarimayBytecodeValue value = context->registers[reg];
    MarimayBytecodeJitBinding *binding = &context->bindings[reg];
    const uint32_t *layouts = context->function->register_layouts.data;
    const MarimayManagedHeap *heap = &context->execution->managed_heap;
    /* The view points at nonmoving object fields, never copied array storage.
     * Revalidate its lifetime before reading any retained object pointer. */
    if (context->execution->initialized && heap->initialized && !heap->collecting
        && heap->view_epoch != UINT64_MAX
        && binding->view_epoch == heap->view_epoch
        && binding->execution == context->execution
        && binding->function == context->function
        && binding->layout == layouts[reg] && binding->type == value.type
        && heap->provenance == value.as.managed_reference.heap_provenance
        && mmay_managed_reference_equal(value.as.managed_reference,
            value.type == MMAY_BYTECODE_TYPE_STRING ? binding->view.string.reference
                : value.type == MMAY_BYTECODE_TYPE_RECORD
                    ? binding->view.record.reference : binding->view.array.reference)) {
        binding->epoch = context->epoch;
        return &binding->view;
    }
    /* A failed checked miss cannot leave a reusable partial view. */
    binding->function = NULL;
    binding->epoch = 0;
    if (value.type == MMAY_BYTECODE_TYPE_STRING) {
        if (mmay_managed_string_bytes(heap, value.as.managed_reference,
                &binding->view.string.bytes, &binding->view.string.length)
                != MMAY_MANAGED_OK) return NULL;
        binding->view.string.reference = value.as.managed_reference;
    } else if (value.type == MMAY_BYTECODE_TYPE_RECORD) {
        const MarimayManagedRecordDescriptor *descriptor =
            mmay_bytecode_record_descriptor_cached(context->execution,
                context->function, layouts[reg]);
        if (descriptor == NULL || mmay_managed_record_view_bind(
                &context->execution->managed_heap, value.as.managed_reference,
                descriptor, &binding->view.record) != MMAY_MANAGED_OK) return NULL;
    } else {
        const MarimayManagedArrayDescriptor *descriptor =
            mmay_bytecode_array_descriptor_cached(context->execution,
                context->function, value.type, layouts[reg]);
        if (descriptor == NULL || mmay_managed_array_view_bind(
                &context->execution->managed_heap, value.as.managed_reference,
                descriptor, &binding->view.array) != MMAY_MANAGED_OK) return NULL;
    }
    binding->execution = context->execution;
    binding->function = context->function;
    binding->layout = layouts[reg];
    binding->type = value.type;
    binding->view_epoch = heap->view_epoch;
    binding->epoch = context->epoch;
    return &binding->view;
}

bool mmay_bytecode_jit_enum(MarimayBytecodeJitContext *context, uint32_t pc) {
    const MarimayBytecodeImageOperation *operation = &context->image->operations[pc];
    const MarimayBytecodeBlock *block = mmay_vector_at_const(
        &context->function->blocks, operation->source_block);
    const MarimayBytecodeInstruction *instruction = mmay_vector_at_const(
        &block->instructions, operation->source_instruction);
    /* Only these nonallocating read instructions may enter the shared owner. */
    if (instruction->opcode != MMAY_OPCODE_ENUM_IS_VARIANT
        && instruction->opcode != MMAY_OPCODE_ENUM_REQUIRE_VARIANT
        && instruction->opcode != MMAY_OPCODE_ENUM_GET_PAYLOAD) return false;
    return mmay_bytecode_run_nominal_instruction(instruction, context->registers,
        context->execution, context->function, context->execution->allocator)
        == MMAY_BYTECODE_RUN_OK;
}
