#include "vm_jit_internal.h"
#include "vm_hot.h"
#include "vm_internal.h"

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
        default: return 0;
    }
}

void *mmay_bytecode_jit_bind(MarimayBytecodeJitContext *context, uint32_t reg) {
    if (context->execution == NULL) return NULL;
    const MarimayBytecodeValue value = context->registers[reg];
    MarimayBytecodeJitBinding *binding = &context->bindings[reg];
    const uint32_t *layouts = context->function->register_layouts.data;
    if (value.type == MMAY_BYTECODE_TYPE_RECORD) {
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
