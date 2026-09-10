#ifndef MMAY_BYTECODE_VM_JIT_INTERNAL_H
#define MMAY_BYTECODE_VM_JIT_INTERNAL_H

#include "vm_jit.h"

#define MMAY_JIT_MAX_REGISTERS 1024
#define MMAY_JIT_MAX_CODE_BYTES (8U * 1024U * 1024U)
#define MMAY_JIT_ENTRY UINT32_C(0x80000000)
#define MMAY_JIT_OFFSET UINT32_C(0x7fffffff)

typedef struct MarimayBytecodeJitBinding {
    size_t epoch;
    union {
        MarimayManagedArrayView array;
        MarimayManagedRecordView record;
    } view;
} MarimayBytecodeJitBinding;

typedef struct MarimayBytecodeJitContext {
    MarimayBytecodeValue *registers;
    MarimayBytecodeJitBinding *bindings;
    const MarimayBytecodeFunction *function;
    const MarimayBytecodeImageFunction *image;
    MarimayBytecodeExecution *execution;
    size_t epoch;
    size_t budget;
    size_t pc;
} MarimayBytecodeJitContext;

void *mmay_bytecode_jit_bind(MarimayBytecodeJitContext *context, uint32_t reg);
bool mmay_bytecode_jit_enum(MarimayBytecodeJitContext *context, uint32_t pc);
bool mmay_bytecode_jit_shift(MarimayBytecodeJitContext *context, uint32_t pc);
size_t mmay_bytecode_jit_array_width(const MarimayBytecodeFunction *function,
    uint32_t reg);
void mmay_bytecode_jit_plan(const MarimayBytecodeImageFunction *image,
    const MarimayBytecodeFunction *function, MarimayBytecodeJitPlanOperation *plan);

MarimayCodeMemoryStatus mmay_bytecode_jit_sljit(const MarimayBytecodeImageFunction *image,
    const MarimayBytecodeFunction *function, MarimayBytecodeJitFunction *compiled,
    MarimayAllocator *allocator, size_t capacity, MarimayCodeMemoryFault *fault, bool meter_fuel);

#endif
