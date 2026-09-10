#ifndef MMAY_BYTECODE_VM_JIT_INTERNAL_H
#define MMAY_BYTECODE_VM_JIT_INTERNAL_H

#include "vm_jit.h"
#include "vm_activation.h"

#define MMAY_JIT_MAX_REGISTERS 1024
#define MMAY_JIT_MAX_CODE_BYTES (8U * 1024U * 1024U)
#define MMAY_JIT_ENTRY UINT32_C(0x80000000)
#define MMAY_JIT_OFFSET UINT32_C(0x7fffffff)

typedef struct MarimayBytecodeJitPlanOperation {
    uint32_t segment_end;
    bool supported;
    bool starts_segment;
} MarimayBytecodeJitPlanOperation;

struct MarimayBytecodeJitFunction {
    uint32_t *entries;
    MarimayBytecodeJitPlanOperation *plan;
    void *code;
    size_t code_bytes;
    MarimayCodeMemory memory;
};

typedef struct MarimayBytecodeJitBinding {
    size_t epoch;
    union {
        MarimayManagedArrayView array;
        MarimayManagedRecordView record;
    } view;
    const MarimayBytecodeExecution *execution;
    const MarimayBytecodeFunction *function;
    uint64_t view_epoch;
    uint32_t layout;
    MarimayBytecodeType type;
} MarimayBytecodeJitBinding;

typedef struct MarimayBytecodeJitActivation {
    MarimayBytecodeJit *jit;
    const MarimayBytecodeProgram *program;
    const MarimayBytecodeExecutionImage *image;
    MarimayBytecodeRunStack *stack;
    MarimayAllocator allocator;
    const MarimayBytecodeRunOptions *options;
    MarimayBytecodeValue *result;
    MarimayBytecodeRunStatus status;
    bool finished;
} MarimayBytecodeJitActivation;

typedef struct MarimayBytecodeJitContext {
    MarimayBytecodeValue *registers;
    MarimayBytecodeJitBinding *bindings;
    const MarimayBytecodeFunction *function;
    const MarimayBytecodeImageFunction *image;
    MarimayBytecodeExecution *execution;
    size_t epoch;
    size_t budget;
    size_t pc;
    MarimayBytecodeJitActivation *activation;
} MarimayBytecodeJitContext;

struct MarimayBytecodeJitState {
    MarimayBytecodeJitActivation activation;
    MarimayBytecodeJitContext context;
};

void *mmay_bytecode_jit_transition(MarimayBytecodeJitContext *context, uint32_t pc);
void mmay_bytecode_jit_enter(const MarimayBytecodeJitFunction *compiled,
    MarimayBytecodeJitContext *context, size_t pc);

void *mmay_bytecode_jit_bind(MarimayBytecodeJitContext *context, uint32_t reg);
bool mmay_bytecode_jit_enum(MarimayBytecodeJitContext *context, uint32_t pc);
bool mmay_bytecode_jit_shift(MarimayBytecodeJitContext *context, uint32_t pc);
size_t mmay_bytecode_jit_array_width(const MarimayBytecodeFunction *function,
    uint32_t reg);
MarimayBytecodeRunStatus mmay_bytecode_jit_select(
    const MarimayBytecodeProgram *program, const MarimayBytecodeExecutionImage *image,
    MarimayAllocator allocator, bool meter_fuel, void **storage, size_t *bytes,
    unsigned char **selected);
void mmay_bytecode_jit_plan(const MarimayBytecodeImageFunction *image,
    const MarimayBytecodeFunction *function, MarimayBytecodeJitPlanOperation *plan, bool meter_fuel);

MarimayCodeMemoryStatus mmay_bytecode_jit_sljit(const MarimayBytecodeImageFunction *image,
    const MarimayBytecodeFunction *function, MarimayBytecodeJitFunction *compiled,
    MarimayAllocator *allocator, size_t capacity, MarimayCodeMemoryFault *fault, bool meter_fuel);

#endif
