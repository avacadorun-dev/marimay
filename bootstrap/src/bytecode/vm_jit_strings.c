#include "vm_jit_internal.h"
#include "mmay/runtime/string.h"

/* Array bounds and the exact parent descriptor have already been checked by
 * the emitter. Preserve the cold string-cell conversion's checked child access
 * and failure transaction; publish the destination and its byte view together.
 * This bridge cannot allocate, collect, call Marimay or relocate VM frames. */
bool mmay_bytecode_jit_string_load(MarimayBytecodeJitContext *context,
    uint32_t target, const MarimayManagedCell *cell
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    MarimayManagedReference reference = cell->reference;
    const MarimayManagedHeap *heap = &context->execution->managed_heap;
    if (mmay_managed_string_bytes(heap, reference, &bytes, &length)
            != MMAY_MANAGED_OK) return false;
    MarimayBytecodeJitBinding *binding = &context->bindings[target];
    const uint32_t *layouts = context->function->register_layouts.data;
    binding->view.string.reference = reference;
    binding->view.string.bytes = bytes;
    binding->view.string.length = length;
    binding->execution = context->execution;
    binding->function = context->function;
    binding->view_epoch = heap->view_epoch;
    binding->layout = layouts[target];
    binding->type = MMAY_BYTECODE_TYPE_STRING;
    binding->epoch = context->epoch;
    context->registers[target].as.managed_reference = reference;
    return true;
}
