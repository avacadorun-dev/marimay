#include "vm_frame_layout.h"
#include "bytecode_internal.h"
#include <stdalign.h>
#include <string.h>

static bool dimensions(const MarimayBytecodeProgramFunction *function,
    MarimayBytecodeFrameLayout *layout, size_t *bytes) {
    if (function == NULL || layout == NULL || bytes == NULL) return false;
    const MarimayBytecodeFunction *body = &function->body;
    const MarimayBytecodeType *types = body->register_types.data;
    if (body->register_types.length != 0 && types == NULL) return false;
    *layout = (MarimayBytecodeFrameLayout){.function = function};
    for (size_t i = 0; i < body->register_types.length; ++i)
        layout->managed_count += mmay_bytecode_type_is_managed(types[i]) ? 1 : 0;
    size_t root_bytes = 0;
    return mmay_size_multiply(body->register_types.length == 0 ? 1
            : body->register_types.length, sizeof(MarimayBytecodeValue), &layout->register_bytes)
        && mmay_size_multiply(body->calls.length, sizeof(MarimayBytecodeCallId), &layout->defer_bytes)
        && mmay_size_multiply(layout->managed_count, sizeof(size_t), &root_bytes)
        && mmay_size_add(layout->register_bytes, root_bytes, bytes);
}

bool mmay_bytecode_frame_layout_size(const MarimayBytecodeProgramFunction *function,
    size_t *bytes) {
    MarimayBytecodeFrameLayout layout;
    return dimensions(function, &layout, bytes);
}

bool mmay_bytecode_frame_layout_init(const MarimayBytecodeProgramFunction *function,
    void *storage, size_t bytes, MarimayBytecodeFrameLayout *layout) {
    MarimayBytecodeFrameLayout result;
    size_t required = 0;
    if (layout == NULL || storage == NULL || !dimensions(function, &result, &required)
        || bytes < required) return false;
    _Static_assert(sizeof(MarimayBytecodeValue) % alignof(size_t) == 0,
        "root indexes follow initialized registers");
    memset(storage, 0, required);
    MarimayBytecodeValue *values = storage;
    size_t *roots = (size_t *)((unsigned char *)storage + result.register_bytes);
    const MarimayBytecodeType *types = function->body.register_types.data;
    size_t count = 0;
    for (size_t i = 0; i < function->body.register_types.length; ++i) {
        values[i].type = types[i];
        if (mmay_bytecode_type_is_managed(types[i])) {
            values[i].as.managed_reference = mmay_managed_invalid_reference();
            roots[count++] = i;
        }
    }
    result.initial_registers = values;
    result.managed_registers = roots;
    *layout = result;
    return true;
}
