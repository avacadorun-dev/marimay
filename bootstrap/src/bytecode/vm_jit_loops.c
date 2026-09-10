#include "vm_jit_internal.h"

/* Reuse the D109 owner: it publishes each successful source prefix and never
 * allocates, invokes host code, grows storage or changes a logical frame. */
bool mmay_bytecode_jit_shift(MarimayBytecodeJitContext *context, uint32_t pc) {
    if (context == NULL || context->image == NULL
        || pc >= context->image->operation_count
        || context->image->operations[pc].fusion != MMAY_BYTECODE_IMAGE_FUSION_ARRAY_SHIFT_LOOP) return false;
    size_t next = pc, ignored_steps = 0, remaining = context->budget;
    MarimayBytecodeRunOptions options = {0};
    options.max_steps = SIZE_MAX;
    options.remaining_shared_steps = &remaining;
    (void)mmay_bytecode_run_image_array_shift_loop(context->image,
        context->function, context->registers, context->execution,
        &next, &ignored_steps, options);
    bool advanced = next != pc || remaining != context->budget;
    context->budget = remaining;
    context->pc = next;
    /* Return to the VM after any committed progress, expiring all cached views
     * because the composed owner may have rebound managed register slots. */
    return advanced;
}
