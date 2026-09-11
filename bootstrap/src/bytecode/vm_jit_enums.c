#include "vm_jit_internal.h"
#include "vm_hot.h"
#include "vm_internal.h"

/* Construction admission is structural. Neither a new nominal descriptor nor
 * an immediate descriptor ID may be allocated by this segment bridge. */
bool mmay_bytecode_jit_enum_new_supported(const MarimayBytecodeFunction *function,
    const MarimayBytecodeImageOperation *operation) {
    if (function == NULL || operation == NULL || operation->opcode != MMAY_OPCODE_ENUM_NEW)
        return false;
    const MarimayBytecodeEnumConstruction *construction =
        mmay_bytecode_enum_construction(function, operation->second);
    const MarimayBytecodeEnumLayout *layout = construction != NULL
        ? mmay_bytecode_enum_layout(function, construction->enum_id) : NULL;
    if (layout == NULL || layout->variant_count != 2 || construction->variant >= 2
        || construction->argument_count > 1) return false;
    if (construction->argument_count == 0) return true;
    const MarimayBytecodeEnumArgument *argument = mmay_vector_at_const(
        &function->enum_arguments, construction->argument_start);
    return argument != NULL && (mmay_bytecode_type_is_integer(argument->type)
        || argument->type == MMAY_BYTECODE_TYPE_BOOL
        || argument->type == MMAY_BYTECODE_TYPE_RUNE
        || argument->type == MMAY_BYTECODE_TYPE_F64);
}

static bool registered(const MarimayManagedHeap *heap,
    const MarimayManagedEnumDescriptor *descriptor) {
    if (heap->immediate_enum_last_descriptor == descriptor
        && heap->immediate_enum_last_id != 0) return true;
    for (size_t i = 0; i < heap->immediate_enum_descriptor_count; ++i)
        if (heap->immediate_enum_descriptors[i] == descriptor) return true;
    return false;
}

bool mmay_bytecode_jit_enum_new(MarimayBytecodeJitContext *context, uint32_t pc) {
    if (context == NULL || context->execution == NULL || !context->execution->initialized
        || !context->execution->managed_heap.initialized) return false;
    const MarimayBytecodeImageOperation *op = &context->image->operations[pc];
    const MarimayBytecodeEnumConstruction *construction =
        mmay_bytecode_enum_construction(context->function, op->second);
    if (construction == NULL || construction->variant >= 2
        || construction->argument_count > 1) return false;
    MarimayManagedHeap *heap = &context->execution->managed_heap;
    MarimayBytecodeJitBinding *binding = &context->bindings[op->first];
    const MarimayManagedEnumDescriptor *descriptor = NULL;
    /* Complete nominal descriptors and registered IDs live until execution
     * teardown. Collection cannot free them; a fresh heap changes provenance. */
    if (binding->type == MMAY_BYTECODE_TYPE_ENUM
        && binding->execution == context->execution
        && binding->function == context->function
        && binding->layout == construction->enum_id
        && binding->view.enumeration.heap_provenance == heap->provenance) {
        descriptor = binding->view.enumeration.descriptor;
    } else {
        descriptor = mmay_bytecode_enum_descriptor_cached(context->execution,
            context->function, construction->enum_id);
        if (descriptor == NULL || !registered(heap, descriptor)) return false;
        binding->type = MMAY_BYTECODE_TYPE_ENUM;
        binding->execution = context->execution;
        binding->function = context->function;
        binding->layout = construction->enum_id;
        binding->view.enumeration.descriptor = descriptor;
        binding->view.enumeration.heap_provenance = heap->provenance;
    }
    /* Prove the runtime's exact immediate representation before invoking its
     * validation/creation owner. Every other shape exits wholly uncommitted. */
    if (descriptor == NULL || descriptor->variant_count != 2 || descriptor->variants == NULL)
        return false;
    const MarimayManagedEnumVariantDescriptor *variant = &descriptor->variants[construction->variant];
    if (variant->payload_count != construction->argument_count) return false;
    if (variant->payload_count == 0) {
        if (variant->payloads != NULL) return false;
    } else if (variant->payloads == NULL
        || variant->payloads[0].cell_kind != MMAY_MANAGED_CELL_RAW64
        || variant->payloads[0].is_mutable
        || variant->payloads[0].reference_kind != MMAY_MANAGED_OBJECT_INVALID
        || variant->payloads[0].reference_descriptor != NULL) return false;
    MarimayManagedCell cell = {0};
    if (construction->argument_count != 0) {
        const MarimayBytecodeEnumArgument *argument = mmay_vector_at_const(
            &context->function->enum_arguments, construction->argument_start);
        if (argument == NULL || mmay_bytecode_value_to_managed_cell(context->execution,
                &context->registers[argument->register_id], &cell) != MMAY_BYTECODE_RUN_OK)
            return false;
    }
    MarimayManagedReference result;
    if (mmay_managed_enum_new(heap, descriptor, construction->variant,
            construction->argument_count != 0 ? &cell : NULL,
            construction->argument_count, &result) != MMAY_MANAGED_OK) return false;
    context->registers[op->first].as.managed_reference = result;
    return true;
}
