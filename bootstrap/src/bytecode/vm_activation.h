#ifndef MMAY_BYTECODE_VM_ACTIVATION_H
#define MMAY_BYTECODE_VM_ACTIVATION_H

#include "vm_frame.h"
#include "vm_image.h"

/* Nonrecursive logical activations. Storage and root lifetimes stay with the
 * frame owner; call/return transitions own their source-visible protocol. */
typedef struct MarimayBytecodeRunStack {
    MarimayBytecodeRunFrame *frames;
    size_t count;
    size_t capacity;
} MarimayBytecodeRunStack;

/* Active verified caller/instruction; the instruction's fuel is already
 * charged. A failed transition remains owned by normal stack teardown. */
MarimayBytecodeRunStatus mmay_bytecode_run_call(
    MarimayBytecodeRunStack *stack, const MarimayBytecodeProgram *program,
    const MarimayBytecodeExecutionImage *image,
    const MarimayBytecodeInstruction *instruction, MarimayAllocator allocator,
    const MarimayBytecodeRunOptions *options);
MarimayBytecodeRunStatus mmay_bytecode_run_return(
    MarimayBytecodeRunStack *stack, const MarimayBytecodeInstruction *instruction,
    const MarimayBytecodeRunOptions *options, MarimayBytecodeValue *result,
    bool *finished);

#endif
