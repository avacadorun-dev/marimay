#ifndef MMAY_BYTECODE_VM_FRAME_H
#define MMAY_BYTECODE_VM_FRAME_H

#include "mmay/bytecode/vm.h"

/* Activation storage: this owner allocates/releases registers, defer slots,
 * root frames and the nonrecursive frame stack. Dispatch owns the cursors.
 * Initialization receives a zeroed or inactive slot, never a linked frame. */
typedef struct MarimayBytecodeRunFrame {
    const MarimayBytecodeProgramFunction *function;
    MarimayBytecodeValue *registers;
    size_t register_bytes;
    size_t block;
    size_t instruction;
    size_t steps;
    uint32_t return_destination;
    bool return_is_void;
    bool return_is_never;
    MarimayBytecodeSpan call_span;
    struct MarimayBytecodeManagedRoots *managed_roots;
    MarimayBytecodeCallId *defer_stack;
    size_t defer_stack_bytes;
    size_t defer_count;
    size_t root_location_capacity;
    bool roots_prepared;
} MarimayBytecodeRunFrame;

void mmay_bytecode_run_frame_dispose(MarimayBytecodeRunFrame *frame,
    const MarimayAllocator *allocator, MarimayBytecodeExecution *execution);
void mmay_bytecode_run_frame_release(MarimayBytecodeRunFrame *frame,
    MarimayBytecodeExecution *execution);
void mmay_bytecode_run_frames_dispose(MarimayBytecodeRunFrame *frames,
    size_t capacity, const MarimayAllocator *allocator,
    MarimayBytecodeExecution *execution);
MarimayBytecodeRunStatus mmay_bytecode_run_frame_init(
    MarimayBytecodeRunFrame *frame, const MarimayBytecodeProgramFunction *function,
    const MarimayBytecodeValue *arguments, size_t argument_count,
    MarimayAllocator allocator, uint32_t return_destination, bool return_is_void,
    bool return_is_never, MarimayBytecodeSpan call_span);
MarimayBytecodeRunStatus mmay_bytecode_run_frame_roots_init(
    MarimayBytecodeRunFrame *frame, MarimayBytecodeExecution *execution,
    MarimayAllocator allocator);
void mmay_bytecode_run_frame_borrow_caller_roots(
    MarimayBytecodeRunFrame *frame);
bool mmay_bytecode_run_frames_reserve(MarimayBytecodeRunFrame **frames,
    size_t *capacity, size_t required, size_t maximum, MarimayAllocator allocator);

#endif
