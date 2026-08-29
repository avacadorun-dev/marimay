#ifndef MMAY_BYTECODE_VM_INTERNAL_H
#define MMAY_BYTECODE_VM_INTERNAL_H

#include "mmay/bytecode/vm.h"

#include "bytecode_internal.h"

typedef struct MarimayBytecodeManagedRoots {
    MarimayManagedRootFrame frame;
    MarimayManagedReference **locations;
    size_t location_bytes;
} MarimayBytecodeManagedRoots;

bool mmay_bytecode_managed_value_valid(MarimayBytecodeExecution *execution,
    MarimayBytecodeValue value);
bool mmay_bytecode_managed_value_valid_for(
    MarimayBytecodeExecution *execution, const MarimayBytecodeFunction *function,
    MarimayBytecodeValue value, uint32_t layout);
const MarimayManagedArrayDescriptor *mmay_bytecode_array_descriptor(
    MarimayBytecodeType type);
const MarimayManagedArrayDescriptor *mmay_bytecode_array_descriptor_for(
    MarimayBytecodeExecution *execution, const MarimayBytecodeFunction *function,
    MarimayBytecodeType type, uint32_t layout);
bool mmay_bytecode_array_cache_init(
    MarimayBytecodeExecution *execution, MarimayAllocator allocator);
void mmay_bytecode_array_cache_dispose(MarimayBytecodeExecution *execution);
void mmay_bytecode_array_cache_rollback(
    MarimayBytecodeExecution *execution, size_t length);
MarimayManagedObjectKind mmay_bytecode_nominal_object_kind(
    MarimayBytecodeType type);
const void *mmay_bytecode_nominal_reference_descriptor(
    MarimayBytecodeExecution *execution, const MarimayBytecodeFunction *function,
    MarimayBytecodeType type, uint32_t layout);
MarimayBytecodeRunStatus mmay_bytecode_run_managed_status(
    MarimayManagedStatus status);
MarimayBytecodeRunStatus mmay_bytecode_execution_intern(
    MarimayBytecodeExecution *execution, const unsigned char *bytes,
    size_t length, MarimayBytecodeReference *result);
bool mmay_bytecode_nominal_cache_init(
    MarimayBytecodeExecution *execution, MarimayAllocator allocator);
void mmay_bytecode_nominal_cache_dispose(
    MarimayBytecodeExecution *execution);
MarimayBytecodeRunStatus mmay_bytecode_run_nominal_descriptor(
    MarimayBytecodeExecution *execution, const MarimayBytecodeFunction *function,
    MarimayBytecodeType type, uint32_t layout, const void **descriptor);
MarimayBytecodeRunStatus mmay_bytecode_run_nominal_value_validate(
    MarimayBytecodeExecution *execution, const MarimayBytecodeFunction *function,
    MarimayBytecodeValue value, MarimayBytecodeType type, uint32_t layout);
MarimayBytecodeRunStatus mmay_bytecode_value_to_managed_cell(
    MarimayBytecodeExecution *execution, const MarimayBytecodeValue *value,
    MarimayManagedCell *cell);
MarimayBytecodeRunStatus mmay_bytecode_managed_cell_to_value(
    MarimayBytecodeExecution *execution, MarimayManagedCell cell,
    MarimayBytecodeValue *value);
MarimayBytecodeRunStatus mmay_bytecode_run_nominal_instruction(
    const MarimayBytecodeInstruction *instruction, MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution, const MarimayBytecodeFunction *function,
    MarimayAllocator allocator);
MarimayBytecodeRunStatus mmay_bytecode_execution_new_numeric_option(
    MarimayBytecodeExecution *execution, const MarimayBytecodeFunction *function,
    uint32_t enum_id, bool some, MarimayBytecodeValue payload,
    MarimayBytecodeValue *target);
MarimayBytecodeRunStatus mmay_bytecode_run_numeric_instruction(
    const MarimayBytecodeInstruction *instruction, MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution, const MarimayBytecodeFunction *function);
MarimayBytecodeRunStatus mmay_bytecode_run_format_instruction(
    const MarimayBytecodeInstruction *instruction, MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution);
MarimayBytecodeRunStatus mmay_bytecode_run_value_instruction(
    const MarimayBytecodeInstruction *instruction, MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution, const MarimayBytecodeFunction *function,
    MarimayAllocator allocator, const MarimayIoStdout *stdout_sink,
    MarimayBytecodePanic *panic, MarimaySymbolId symbol,
    int64_t *bounds_index, size_t *bounds_length);
MarimayBytecodeRunStatus mmay_bytecode_run_array_mutation_instruction(
    const MarimayBytecodeInstruction *instruction, MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution, const MarimayBytecodeFunction *function,
    MarimayAllocator allocator, MarimayBytecodePanic *panic, MarimaySymbolId symbol,
    bool *handled
);
MarimayBytecodeRunStatus mmay_bytecode_run_string_concat_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution
);
MarimayBytecodeRunStatus mmay_bytecode_run_text_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    const MarimayIoStdout *stdout_sink
);
MarimayBytecodeRunStatus mmay_bytecode_run_parse_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function
);
MarimayBytecodeRunStatus mmay_bytecode_run_range_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function
);
MarimayBytecodeRunStatus mmay_bytecode_run_io_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    const MarimayIoStdout *stdout_sink
);
MarimayBytecodeRunStatus mmay_bytecode_run_fs_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function
);
MarimayBytecodeRunStatus mmay_bytecode_run_context_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function
);
MarimayBytecodeRunStatus mmay_bytecode_run_test_output_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    MarimayBytecodePanic *panic,
    MarimaySymbolId symbol
);
MarimayBytecodeRunStatus mmay_bytecode_run_begin_test_panic(
    MarimayBytecodePanic *panic, MarimayAllocator allocator,
    const unsigned char *message, size_t message_length,
    MarimaySymbolId symbol, MarimayBytecodeSpan span
);

bool mmay_bytecode_run_consume_step(size_t *steps,
    MarimayBytecodeRunOptions options);
void mmay_bytecode_run_set_trap(MarimayBytecodeRunOptions options,
    MarimayBytecodeSpan span);
void mmay_bytecode_run_reset_panic(MarimayBytecodePanic *panic);
MarimayBytecodeRunStatus mmay_bytecode_run_begin_panic(MarimayBytecodePanic *panic,
    MarimayAllocator allocator, MarimayBytecodeExecution *execution,
    MarimayBytecodeValue message, MarimaySymbolId symbol, MarimayBytecodeSpan span);
MarimayBytecodeRunStatus mmay_bytecode_run_begin_bounds_panic(
    MarimayBytecodePanic *panic, MarimayAllocator allocator, int64_t index,
    size_t length, MarimaySymbolId symbol, MarimayBytecodeSpan span);
MarimayBytecodeRunStatus mmay_bytecode_run_begin_iterator_invalidated_panic(
    MarimayBytecodePanic *panic, MarimayAllocator allocator,
    MarimaySymbolId symbol, MarimayBytecodeSpan span
);
MarimayBytecodeRunStatus mmay_bytecode_run_append_panic_frame(
    MarimayBytecodePanic *panic, MarimaySymbolId symbol,
    MarimayBytecodeSpan call_span);
bool mmay_bytecode_run_append_suppressed_panic(
    MarimayBytecodePanic *primary, MarimayBytecodePanic *secondary);
bool mmay_bytecode_run_string_value_valid(MarimayBytecodeExecution *execution,
    MarimayBytecodeValue value);
bool mmay_bytecode_run_function_uses_strings(
    const MarimayBytecodeFunction *function);
bool mmay_bytecode_run_program_uses_strings(const MarimayBytecodeProgram *program);
bool mmay_bytecode_run_function_uses_arrays(
    const MarimayBytecodeFunction *function);
bool mmay_bytecode_run_program_uses_arrays(const MarimayBytecodeProgram *program);
bool mmay_bytecode_run_function_uses_managed(
    const MarimayBytecodeFunction *function);
bool mmay_bytecode_run_program_uses_managed(
    const MarimayBytecodeProgram *program);
MarimayBytecodeRunStatus mmay_bytecode_run_roots_init(
    MarimayBytecodeManagedRoots *roots, MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function, MarimayBytecodeValue *registers,
    MarimayAllocator allocator);
void mmay_bytecode_run_roots_dispose(MarimayBytecodeManagedRoots *roots,
    MarimayBytecodeExecution *execution, MarimayAllocator allocator);
bool mmay_bytecode_run_function_uses_f64(
    const MarimayBytecodeFunction *function);
bool mmay_bytecode_run_program_uses_f64(const MarimayBytecodeProgram *program);
bool mmay_bytecode_run_function_has_calls(
    const MarimayBytecodeFunction *function);
MarimayBytecodeRunStatus mmay_bytecode_run_arguments_validate(
    MarimayBytecodeExecution *execution, const MarimayBytecodeFunction *function,
    const MarimayBytecodeValue *arguments, size_t argument_count);
MarimayBytecodeRunStatus mmay_bytecode_run_deferred_program_call(
    const MarimayBytecodeProgram *program, const MarimayBytecodeFunction *caller,
    const MarimayBytecodeCall *call, const MarimayBytecodeInstruction *instruction,
    const MarimayBytecodeValue *registers, MarimayAllocator allocator,
    MarimayBytecodeRunOptions options, bool preserve_primary);

MarimayBytecodeRunStatus mmay_bytecode_run_interface_new(
    const MarimayBytecodeInstruction *instruction, MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution, const MarimayBytecodeFunction *function);
MarimayBytecodeRunStatus mmay_bytecode_run_interface_resolve(
    const MarimayBytecodeInstruction *instruction, const MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution, const MarimayBytecodeFunction *function,
    MarimaySymbolId *method, MarimayManagedReference *payload);
bool mmay_bytecode_run_interface_value_valid(
    MarimayBytecodeExecution *execution, const MarimayBytecodeFunction *function,
    MarimayBytecodeValue value, uint32_t layout);
const MarimayManagedInterfaceDescriptor *mmay_bytecode_run_interface_descriptor(
    MarimayBytecodeExecution *execution, const MarimayBytecodeFunction *function,
    MarimayBytecodeInterfaceId interface_id);
void mmay_bytecode_run_interface_tables_dispose(
    MarimayBytecodeExecution *execution);

#endif
