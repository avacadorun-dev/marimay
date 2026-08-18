#ifndef RUDA_NATIVE_C_EMITTER_INTERNAL_H
#define RUDA_NATIVE_C_EMITTER_INTERNAL_H

#include "ruda/native/c_emitter.h"

#include "ruda/base/vector.h"

typedef struct RudaNativeCBuffer {
    RudaVector bytes;
    size_t limit;
    bool limited;
} RudaNativeCBuffer;

typedef struct RudaNativeCCycleFrame {
    size_t function_index;
    size_t block_index;
    size_t instruction_index;
} RudaNativeCCycleFrame;

typedef enum RudaNativeCArrayKind {
    RUDA_NATIVE_C_ARRAY_NONE,
    RUDA_NATIVE_C_ARRAY_F64,
    RUDA_NATIVE_C_ARRAY_ARRAY_F64,
    RUDA_NATIVE_C_ARRAY_GENERIC,
    RUDA_NATIVE_C_ARRAY_UNSUPPORTED
} RudaNativeCArrayKind;

typedef enum RudaNativeCManagedKind {
    RUDA_NATIVE_C_MANAGED_NONE,
    RUDA_NATIVE_C_MANAGED_ARRAY,
    RUDA_NATIVE_C_MANAGED_RECORD,
    RUDA_NATIVE_C_MANAGED_ENUM,
    RUDA_NATIVE_C_MANAGED_INTERFACE,
    RUDA_NATIVE_C_MANAGED_UNSUPPORTED
} RudaNativeCManagedKind;

typedef struct RudaNativeCManagedLayout {
    const RudaMirFunction *function;
    RudaNativeCManagedKind kind;
    size_t index;
} RudaNativeCManagedLayout;

int ruda_native_c_symbol_compare(RudaSymbolId left, RudaSymbolId right);
int ruda_native_c_function_pointer_compare(const void *left, const void *right);
const RudaMirArray *ruda_native_c_array(const RudaMirFunction *function,
    RudaTypeId type);
RudaNativeCArrayKind ruda_native_c_array_kind(const RudaMirFunction *function,
    RudaTypeId type);
bool ruda_native_c_program_type_supported(
    const RudaMirFunction *const *functions, size_t count, RudaTypeId type,
    bool result);
const char *ruda_native_c_type(const RudaMirFunction *function, RudaTypeId type);
const char *ruda_native_c_integer_kind_name(RudaTypeId type);
const char *ruda_native_c_integer_conversion_name(RudaTypeId type);
bool ruda_native_c_buffer_append(RudaNativeCBuffer *buffer, const char *text,
    size_t length);
bool ruda_native_c_write(RudaNativeCBuffer *buffer, const char *format, ...);
bool ruda_native_c_symbol(RudaNativeCBuffer *buffer, RudaSymbolId symbol);
bool ruda_native_c_symbol_hex(RudaNativeCBuffer *buffer, RudaSymbolId symbol);
bool ruda_native_c_symbol_data(RudaNativeCBuffer *buffer, RudaSymbolId symbol);
bool ruda_native_c_string_data(RudaNativeCBuffer *buffer, RudaSymbolId symbol,
    size_t index);
const RudaMirFunction *ruda_native_c_find(const RudaMirFunction *const *functions,
    size_t count, RudaSymbolId symbol);
const RudaMirCall *ruda_native_c_call(const RudaMirFunction *function,
    RudaMirCallId call);
const RudaMirCallArgument *ruda_native_c_call_argument(
    const RudaMirFunction *function, size_t index);
const RudaMirArrayConstruction *ruda_native_c_array_construction(
    const RudaMirFunction *function, RudaMirArrayConstructionId construction);
const RudaMirArrayArgument *ruda_native_c_array_argument(
    const RudaMirFunction *function, size_t index);
const char *ruda_native_c_array_descriptor(const RudaMirFunction *function,
    RudaTypeId type);
size_t ruda_native_c_managed_register_count(const RudaMirFunction *function);
bool ruda_native_c_find_managed_layout(
    const RudaMirFunction *const *functions, size_t count, RudaTypeId type,
    RudaNativeCManagedLayout *result);
bool ruda_native_c_managed_layouts_valid(
    const RudaMirFunction *const *functions, size_t count);
bool ruda_native_c_write_descriptor_name(
    RudaNativeCBuffer *buffer, RudaNativeCManagedKind kind, RudaTypeId type);
bool ruda_native_c_write_interface_table_name(
    RudaNativeCBuffer *buffer, RudaSymbolId symbol, size_t construction);
bool ruda_native_c_emit_managed_descriptors(
    RudaNativeCBuffer *buffer, const RudaMirFunction *const *functions,
    size_t count);
bool ruda_native_c_emit_managed_parameter_check(
    RudaNativeCBuffer *buffer, const RudaMirFunction *function,
    const RudaMirFunction *const *functions, size_t count, size_t parameter);
bool ruda_native_c_call_supported(const RudaMirFunction *function,
    RudaMirCallId call_id, const RudaMirFunction *const *functions,
    size_t count, RudaTypeId required_result);
bool ruda_native_c_interface_call_supported(
    const RudaMirFunction *function, RudaMirCallId call_id,
    const RudaMirFunction *const *functions, size_t count,
    RudaTypeId required_result);
RudaNativeCEmitStatus ruda_native_c_validate_function(
    const RudaMirFunction *function, const RudaMirFunction *const *functions,
    size_t count);
bool ruda_native_c_function_index(const RudaMirFunction *const *functions,
    size_t count, RudaSymbolId symbol, size_t *result);
bool ruda_native_c_has_cycle(const RudaMirFunction *const *functions,
    size_t count, unsigned char *states, RudaNativeCCycleFrame *stack);
bool ruda_native_c_emit_signature(RudaNativeCBuffer *buffer,
    const RudaMirFunction *function, bool prototype);
bool ruda_native_c_emit_call_arguments(
    RudaNativeCBuffer *buffer, const RudaMirFunction *function,
    const RudaMirCall *call);
bool ruda_native_c_emit_propagate_call(
    RudaNativeCBuffer *buffer, RudaSymbolId symbol, RudaMirSpan span,
    bool returns_never);
bool ruda_native_c_emit_managed_instruction(
    RudaNativeCBuffer *buffer, const RudaMirFunction *function,
    const RudaMirFunction *const *functions, size_t count,
    const RudaMirInstruction *instruction);
bool ruda_native_c_method_possible(
    const RudaMirFunction *const *functions, size_t count,
    RudaTypeId interface_type, uint32_t slot, size_t target);
bool ruda_native_c_emit_defer_instruction(
    RudaNativeCBuffer *buffer, const RudaMirFunction *function,
    const RudaMirFunction *const *functions, size_t count,
    const RudaMirInstruction *instruction);
bool ruda_native_c_emit_defer_cleanup(
    RudaNativeCBuffer *buffer, const RudaMirFunction *function,
    const RudaMirFunction *const *functions, size_t count);
bool ruda_native_c_write_from_managed_cell(
    RudaNativeCBuffer *buffer, RudaTypeId type, RudaMirRegister target);
bool ruda_native_c_write_to_managed_cell(
    RudaNativeCBuffer *buffer, RudaTypeId type, RudaMirRegister source);
bool ruda_native_c_emit_numeric_instruction(
    RudaNativeCBuffer *buffer, const RudaMirFunction *function,
    const RudaMirInstruction *instruction);
bool ruda_native_c_emit_format_instruction(
    RudaNativeCBuffer *buffer, const RudaMirInstruction *instruction);
bool ruda_native_c_emit_io_runtime(RudaNativeCBuffer *buffer);
bool ruda_native_c_emit_io_instruction(
    RudaNativeCBuffer *buffer, const RudaMirFunction *function,
    const RudaMirInstruction *instruction);
bool ruda_native_c_emit_function(RudaNativeCBuffer *buffer,
    const RudaMirFunction *function, const RudaMirFunction *const *functions,
    size_t count);
bool ruda_native_c_emit_symbol_and_strings(RudaNativeCBuffer *buffer,
    const RudaMirFunction *function);
bool ruda_native_c_emit_runtime(
    RudaNativeCBuffer *buffer, size_t max_frames, size_t max_suppressed);
bool ruda_native_c_emit_keep_assets(RudaNativeCBuffer *buffer,
    const RudaMirFunction *const *functions, size_t count);
RudaNativeCEmitStatus ruda_native_c_validate_executable_entry(
    const RudaMirFunction *entry,
    const RudaMirFunction *const *functions,
    size_t count,
    const RudaNativeCEmitOptions *options
);
bool ruda_native_c_emit_public_helpers(RudaNativeCBuffer *buffer);
bool ruda_native_c_emit_executable_entry(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *entry,
    const RudaMirFunction *const *functions,
    size_t count,
    const RudaNativeCEmitOptions *options
);

#endif
