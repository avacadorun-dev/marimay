#include "vm_jit_internal.h"
#include "vm_internal.h"

/* Nonallocating mutation bridge. The checked runtime owner retains value,
 * length, generation, publication and resource-diagnostic semantics. */
bool mmay_bytecode_jit_array_push(MarimayBytecodeJitContext *context, uint32_t pc) {
    const MarimayBytecodeImageOperation *op = &context->image->operations[pc];
    MarimayBytecodeJitBinding *binding = &context->bindings[op->first];
    if (binding->epoch != context->epoch
        && mmay_bytecode_jit_bind(context, op->first) == NULL) return false;
    const MarimayManagedArrayView *view = &binding->view.array;
    /* No runtime call is allowed to reach growth or a managed safepoint.
     * On a full array, leave this original instruction wholly uncommitted. */
    if (*view->length >= *view->capacity) return false;
    MarimayManagedCell cell;
    if (mmay_bytecode_value_to_managed_cell(context->execution,
            &context->registers[op->second], &cell) != MMAY_BYTECODE_RUN_OK) return false;
    return mmay_managed_array_view_push(&context->execution->managed_heap, view, cell)
        == MMAY_MANAGED_OK;
}
