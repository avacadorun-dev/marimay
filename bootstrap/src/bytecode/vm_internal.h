#ifndef RUDA_BYTECODE_VM_INTERNAL_H
#define RUDA_BYTECODE_VM_INTERNAL_H

#include "ruda/bytecode/vm.h"

#include "bytecode_internal.h"

typedef struct RudaBytecodeManagedRoots {
    RudaManagedRootFrame frame;
    RudaManagedReference **locations;
    size_t location_bytes;
} RudaBytecodeManagedRoots;

bool ruda_bytecode_managed_value_valid(RudaBytecodeExecution *execution,
    RudaBytecodeValue value);
RudaBytecodeRunStatus ruda_bytecode_run_value_instruction(
    const RudaBytecodeInstruction *instruction, RudaBytecodeValue *registers,
    RudaBytecodeExecution *execution, const RudaBytecodeFunction *function,
    RudaAllocator allocator, int64_t *bounds_index, size_t *bounds_length);

bool ruda_bytecode_run_consume_step(size_t *steps,
    RudaBytecodeRunOptions options);
void ruda_bytecode_run_set_trap(RudaBytecodeRunOptions options,
    RudaBytecodeSpan span);
void ruda_bytecode_run_reset_panic(RudaBytecodePanic *panic);
RudaBytecodeRunStatus ruda_bytecode_run_begin_panic(RudaBytecodePanic *panic,
    RudaAllocator allocator, RudaBytecodeExecution *execution,
    RudaBytecodeValue message, RudaSymbolId symbol, RudaBytecodeSpan span);
RudaBytecodeRunStatus ruda_bytecode_run_begin_bounds_panic(
    RudaBytecodePanic *panic, RudaAllocator allocator, int64_t index,
    size_t length, RudaSymbolId symbol, RudaBytecodeSpan span);
RudaBytecodeRunStatus ruda_bytecode_run_append_panic_frame(
    RudaBytecodePanic *panic, RudaSymbolId symbol,
    RudaBytecodeSpan call_span);
bool ruda_bytecode_run_string_value_valid(RudaBytecodeExecution *execution,
    RudaBytecodeValue value);
bool ruda_bytecode_run_function_uses_strings(
    const RudaBytecodeFunction *function);
bool ruda_bytecode_run_program_uses_strings(const RudaBytecodeProgram *program);
bool ruda_bytecode_run_function_uses_arrays(
    const RudaBytecodeFunction *function);
bool ruda_bytecode_run_program_uses_arrays(const RudaBytecodeProgram *program);
RudaBytecodeRunStatus ruda_bytecode_run_roots_init(
    RudaBytecodeManagedRoots *roots, RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function, RudaBytecodeValue *registers,
    RudaAllocator allocator);
void ruda_bytecode_run_roots_dispose(RudaBytecodeManagedRoots *roots,
    RudaBytecodeExecution *execution, RudaAllocator allocator);
bool ruda_bytecode_run_function_uses_f64(
    const RudaBytecodeFunction *function);
bool ruda_bytecode_run_program_uses_f64(const RudaBytecodeProgram *program);
bool ruda_bytecode_run_function_has_calls(
    const RudaBytecodeFunction *function);

#endif
