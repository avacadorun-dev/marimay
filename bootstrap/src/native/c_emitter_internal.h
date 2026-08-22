#ifndef MMAY_NATIVE_C_EMITTER_INTERNAL_H
#define MMAY_NATIVE_C_EMITTER_INTERNAL_H

#include "mmay/native/c_emitter.h"

#include "mmay/base/vector.h"

typedef struct MarimayNativeCBuffer {
    MarimayVector bytes;
    size_t limit;
    bool limited;
} MarimayNativeCBuffer;

typedef struct MarimayNativeCCycleFrame {
    size_t function_index;
    size_t block_index;
    size_t instruction_index;
} MarimayNativeCCycleFrame;

typedef enum MarimayNativeCArrayKind {
    MMAY_NATIVE_C_ARRAY_NONE,
    MMAY_NATIVE_C_ARRAY_F64,
    MMAY_NATIVE_C_ARRAY_ARRAY_F64,
    MMAY_NATIVE_C_ARRAY_GENERIC,
    MMAY_NATIVE_C_ARRAY_UNSUPPORTED
} MarimayNativeCArrayKind;

typedef enum MarimayNativeCManagedKind {
    MMAY_NATIVE_C_MANAGED_NONE,
    MMAY_NATIVE_C_MANAGED_ARRAY,
    MMAY_NATIVE_C_MANAGED_RECORD,
    MMAY_NATIVE_C_MANAGED_ENUM,
    MMAY_NATIVE_C_MANAGED_INTERFACE,
    MMAY_NATIVE_C_MANAGED_UNSUPPORTED
} MarimayNativeCManagedKind;

typedef struct MarimayNativeCManagedLayout {
    const MarimayMirFunction *function;
    MarimayNativeCManagedKind kind;
    size_t index;
} MarimayNativeCManagedLayout;

int mmay_native_c_symbol_compare(MarimaySymbolId left, MarimaySymbolId right);
int mmay_native_c_function_pointer_compare(const void *left, const void *right);
const MarimayMirArray *mmay_native_c_array(const MarimayMirFunction *function,
    MarimayTypeId type);
const MarimayMirArray *mmay_native_c_register_array(
    const MarimayMirFunction *function, MarimayMirRegister register_id);
MarimayNativeCArrayKind mmay_native_c_array_kind(const MarimayMirFunction *function,
    MarimayTypeId type);
bool mmay_native_c_program_type_supported(
    const MarimayMirFunction *const *functions, size_t count, MarimayTypeId type,
    bool result);
const char *mmay_native_c_type(const MarimayMirFunction *function, MarimayTypeId type);
const char *mmay_native_c_integer_kind_name(MarimayTypeId type);
const char *mmay_native_c_integer_conversion_name(MarimayTypeId type);
bool mmay_native_c_buffer_append(MarimayNativeCBuffer *buffer, const char *text,
    size_t length);
bool mmay_native_c_write(MarimayNativeCBuffer *buffer, const char *format, ...);
bool mmay_native_c_symbol(MarimayNativeCBuffer *buffer, MarimaySymbolId symbol);
bool mmay_native_c_symbol_hex(MarimayNativeCBuffer *buffer, MarimaySymbolId symbol);
bool mmay_native_c_symbol_data(MarimayNativeCBuffer *buffer, MarimaySymbolId symbol);
bool mmay_native_c_string_data(MarimayNativeCBuffer *buffer, MarimaySymbolId symbol,
    size_t index);
const MarimayMirFunction *mmay_native_c_find(const MarimayMirFunction *const *functions,
    size_t count, MarimaySymbolId symbol);
const MarimayMirCall *mmay_native_c_call(const MarimayMirFunction *function,
    MarimayMirCallId call);
const MarimayMirCallArgument *mmay_native_c_call_argument(
    const MarimayMirFunction *function, size_t index);
const MarimayMirArrayConstruction *mmay_native_c_array_construction(
    const MarimayMirFunction *function, MarimayMirArrayConstructionId construction);
const MarimayMirArrayArgument *mmay_native_c_array_argument(
    const MarimayMirFunction *function, size_t index);
const char *mmay_native_c_array_descriptor(const MarimayMirFunction *function,
    MarimayTypeId type);
size_t mmay_native_c_managed_register_count(const MarimayMirFunction *function);
bool mmay_native_c_function_requires_roots(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    size_t depth);
bool mmay_native_c_plan_root_registers(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimayAllocator allocator,
    unsigned char **registers,
    size_t *register_bytes,
    size_t *root_count);
bool mmay_native_c_find_managed_layout(
    const MarimayMirFunction *const *functions, size_t count, MarimayTypeId type,
    MarimayNativeCManagedLayout *result);
bool mmay_native_c_managed_layouts_valid(
    const MarimayMirFunction *const *functions, size_t count);
bool mmay_native_c_write_descriptor_name(
    MarimayNativeCBuffer *buffer, MarimayNativeCManagedKind kind, MarimayTypeId type);
bool mmay_native_c_write_interface_table_name(
    MarimayNativeCBuffer *buffer, MarimaySymbolId symbol, size_t construction);
bool mmay_native_c_emit_managed_descriptors(
    MarimayNativeCBuffer *buffer, const MarimayMirFunction *const *functions,
    size_t count);
bool mmay_native_c_emit_managed_parameter_check(
    MarimayNativeCBuffer *buffer, const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions, size_t count, size_t parameter);
bool mmay_native_c_call_supported(const MarimayMirFunction *function,
    MarimayMirCallId call_id, const MarimayMirFunction *const *functions,
    size_t count, MarimayTypeId required_result);
bool mmay_native_c_interface_call_supported(
    const MarimayMirFunction *function, MarimayMirCallId call_id,
    const MarimayMirFunction *const *functions, size_t count,
    MarimayTypeId required_result);
MarimayNativeCEmitStatus mmay_native_c_validate_function(
    const MarimayMirFunction *function, const MarimayMirFunction *const *functions,
    size_t count);
bool mmay_native_c_function_index(const MarimayMirFunction *const *functions,
    size_t count, MarimaySymbolId symbol, size_t *result);
bool mmay_native_c_has_cycle(const MarimayMirFunction *const *functions,
    size_t count, unsigned char *states, MarimayNativeCCycleFrame *stack);
bool mmay_native_c_emit_signature(MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions, size_t count, bool prototype);
bool mmay_native_c_emit_call_arguments(
    MarimayNativeCBuffer *buffer, const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions, size_t count,
    const MarimayMirCall *call);
bool mmay_native_c_emit_propagate_call(
    MarimayNativeCBuffer *buffer, MarimaySymbolId symbol, MarimayMirSpan span,
    bool returns_never);
bool mmay_native_c_emit_managed_instruction(
    MarimayNativeCBuffer *buffer, const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions, size_t count,
    const MarimayMirInstruction *instruction);
bool mmay_native_c_emit_array_mutation_runtime(MarimayNativeCBuffer *buffer);
bool mmay_native_c_emit_array_mutation_instruction(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
);
bool mmay_native_c_emit_range_instruction(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t function_count,
    const MarimayMirBlock *block,
    size_t instruction_index,
    const MarimayMirInstruction *instruction
);
bool mmay_native_c_batch_scalar_mutation(
    const MarimayMirFunction *function);
bool mmay_native_c_array_register_unmodified(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id);
bool mmay_native_c_array_register_origin(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id,
    MarimayMirRegister *origin);
bool mmay_native_c_array_view_known(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id);
bool mmay_native_c_array_children_parameter(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t function_count,
    MarimayMirRegister parameter);
bool mmay_native_c_array_generation_unobserved(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id);
bool mmay_native_c_array_fill_plan(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayMirRegister *count,
    MarimayMirRegister *index,
    uint64_t *bits,
    MarimayMirBlockId *exit_block);
bool mmay_native_c_array_fill_count(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayMirRegister array_register,
    MarimayMirRegister *count);
bool mmay_native_c_array_filled_index_proven(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayMirRegister array_register,
    MarimayMirRegister index_register);
bool mmay_native_c_array_guarded_index_proven(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayMirRegister array_register,
    MarimayMirRegister index_register);
bool mmay_native_c_array_relational_index_proven(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t function_count,
    const MarimayMirInstruction *instruction,
    MarimayMirRegister array_register,
    MarimayMirRegister index_register);
bool mmay_native_c_array_loaded_index_proven(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t function_count,
    const MarimayMirInstruction *instruction,
    MarimayMirRegister array_register,
    MarimayMirRegister index_register);
bool mmay_native_c_subtract_one_proven(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t function_count,
    const MarimayMirInstruction *instruction);
bool mmay_native_c_add_one_proven(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t function_count,
    const MarimayMirInstruction *instruction);
bool mmay_native_c_negate_proven(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction);
bool mmay_native_c_method_possible(
    const MarimayMirFunction *const *functions, size_t count,
    MarimayTypeId interface_type, uint32_t slot, size_t target);
bool mmay_native_c_emit_defer_instruction(
    MarimayNativeCBuffer *buffer, const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions, size_t count,
    const MarimayMirInstruction *instruction);
bool mmay_native_c_emit_defer_cleanup(
    MarimayNativeCBuffer *buffer, const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions, size_t count);
bool mmay_native_c_write_from_managed_cell(
    MarimayNativeCBuffer *buffer, MarimayTypeId type, MarimayMirRegister target);
bool mmay_native_c_write_to_managed_cell(
    MarimayNativeCBuffer *buffer, MarimayTypeId type, MarimayMirRegister source);
bool mmay_native_c_enum_construction_immediate(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
);
bool mmay_native_c_immediate_enum_id(
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimayTypeId type,
    uint32_t *result);
bool mmay_native_c_emit_numeric_instruction(
    MarimayNativeCBuffer *buffer, const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction);
bool mmay_native_c_emit_format_instruction(
    MarimayNativeCBuffer *buffer, const MarimayMirInstruction *instruction);
bool mmay_native_c_emit_io_runtime(MarimayNativeCBuffer *buffer);
bool mmay_native_c_emit_io_instruction(
    MarimayNativeCBuffer *buffer, const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction);
bool mmay_native_c_emit_test_output_instruction(
    MarimayNativeCBuffer *buffer, const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction);
bool mmay_native_c_emit_function(MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function, const MarimayMirFunction *const *functions,
    size_t count, const unsigned char *root_registers, size_t root_count);
bool mmay_native_c_emit_symbol_and_strings(MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function);
bool mmay_native_c_emit_runtime(
    MarimayNativeCBuffer *buffer, size_t max_frames, size_t max_suppressed);
bool mmay_native_c_emit_keep_assets(MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *const *functions, size_t count);
MarimayNativeCEmitStatus mmay_native_c_validate_executable_entry(
    const MarimayMirFunction *entry,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayNativeCEmitOptions *options
);
bool mmay_native_c_emit_public_helpers(MarimayNativeCBuffer *buffer);
bool mmay_native_c_emit_executable_entry(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *entry,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayNativeCEmitOptions *options
);

#endif
