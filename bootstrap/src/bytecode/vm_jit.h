#ifndef MMAY_BYTECODE_VM_JIT_H
#define MMAY_BYTECODE_VM_JIT_H

#include "vm_image.h"
#include "mmay/base/code_memory.h"

typedef struct MarimayBytecodeJitFunction MarimayBytecodeJitFunction;

typedef struct MarimayBytecodeJit {
    MarimayAllocator allocator;
    bool ready;
    bool meters_fuel;
    MarimayBytecodeJitFunction *functions;
    size_t function_count;
    void *metadata;
    size_t metadata_bytes;
    void *bindings;
    size_t binding_bytes;
    size_t epoch;
} MarimayBytecodeJit;

/* Verified image and original function metadata must outlive this owner.
 * Unsupported hosts/regions retain ordinary execution. Allocation failure is
 * observable; code-size eligibility is a bounded optimization decision. */
MarimayBytecodeRunStatus mmay_bytecode_jit_init(MarimayBytecodeJit *jit,
    const MarimayBytecodeProgram *program,
    const MarimayBytecodeExecutionImage *image, MarimayAllocator allocator,
    MarimayCodeMemoryFault *fault, bool meter_fuel);
void mmay_bytecode_jit_dispose(MarimayBytecodeJit *jit);
/* True requests one original cold instruction at the returned source PC before
 * re-entry. False leaves the ordinary VM hot tier responsible for execution. */
bool mmay_bytecode_jit_run(MarimayBytecodeJit *jit, size_t function_id,
    const MarimayBytecodeImageFunction *image,
    const MarimayBytecodeFunction *function, MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution, size_t *block, size_t *instruction,
    size_t *steps, MarimayBytecodeRunOptions options);

#endif
