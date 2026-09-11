#ifndef MMAY_BYTECODE_VM_FRAME_LAYOUT_H
#define MMAY_BYTECODE_VM_FRAME_LAYOUT_H

#include "mmay/bytecode/vm.h"

/* Immutable initialization metadata for one verified function. The owner of
 * storage keeps it alive for the invocation; it contains no live values/roots. */
typedef struct MarimayBytecodeFrameLayout {
    const MarimayBytecodeProgramFunction *function;
    const MarimayBytecodeValue *initial_registers;
    const size_t *managed_registers;
    size_t register_bytes;
    size_t defer_bytes;
    size_t managed_count;
} MarimayBytecodeFrameLayout;

bool mmay_bytecode_frame_layout_size(const MarimayBytecodeProgramFunction *function,
    size_t *bytes);
bool mmay_bytecode_frame_layout_init(const MarimayBytecodeProgramFunction *function,
    void *storage, size_t bytes, MarimayBytecodeFrameLayout *layout);

#endif
