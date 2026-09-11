#ifndef MMAY_BYTECODE_VM_JIT_INTERNAL_H
#define MMAY_BYTECODE_VM_JIT_INTERNAL_H

#include "vm_jit.h"
#include "vm_activation.h"

#define MMAY_JIT_MAX_REGISTERS 1024
#define MMAY_JIT_MAX_CODE_BYTES (8U * 1024U * 1024U)
#define MMAY_JIT_MAX_VIEW_BYTES (2U * 1024U * 1024U)
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
    MarimayBytecodeFrameLayout frame_layout;
    MarimayBytecodePreparedCall *calls;
    struct MarimayBytecodeJitBinding *bindings;
    size_t binding_bytes;
};

typedef struct MarimayBytecodeJitBinding {
    size_t epoch;
    union {
        MarimayManagedArrayView array;
        MarimayManagedRecordView record;
        struct {
            MarimayManagedReference reference;
            const unsigned char *bytes;
            size_t length;
        } string;
        struct {
            const MarimayManagedEnumDescriptor *descriptor;
            uint64_t heap_provenance;
        } enumeration;
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
    void *activation_metadata;
    size_t activation_metadata_bytes;
};

MarimayBytecodeRunStatus mmay_bytecode_jit_prepare_activations(MarimayBytecodeJit *jit,
    const MarimayBytecodeProgram *program, const MarimayBytecodeExecutionImage *image,
    const unsigned char *selected);
void *mmay_bytecode_jit_call(MarimayBytecodeJitContext *context, uint32_t pc,
    const MarimayBytecodeInstruction *instruction, const MarimayBytecodePreparedCall *call);
void *mmay_bytecode_jit_return(MarimayBytecodeJitContext *context, uint32_t pc,
    const MarimayBytecodeInstruction *instruction);
/* Enter a marked segment start. Native call continuations reset emission proofs
 * separately before transferring through their internal labels. */
void mmay_bytecode_jit_enter(const MarimayBytecodeJitFunction *compiled,
    MarimayBytecodeJitContext *context, size_t pc);

void *mmay_bytecode_jit_bind(MarimayBytecodeJitContext *context, uint32_t reg);
bool mmay_bytecode_jit_string_load(MarimayBytecodeJitContext *context,
    uint32_t target, const MarimayManagedCell *cell);
void mmay_bytecode_jit_clear_bindings(MarimayBytecodeJit *jit);
bool mmay_bytecode_jit_enum(MarimayBytecodeJitContext *context, uint32_t pc);
bool mmay_bytecode_jit_enum_new_supported(const MarimayBytecodeFunction *function,
    const MarimayBytecodeImageOperation *operation);
bool mmay_bytecode_jit_enum_new(MarimayBytecodeJitContext *context, uint32_t pc);
bool mmay_bytecode_jit_array_push(MarimayBytecodeJitContext *context, uint32_t pc);
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
