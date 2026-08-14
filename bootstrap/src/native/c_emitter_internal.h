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
    RUDA_NATIVE_C_ARRAY_UNSUPPORTED
} RudaNativeCArrayKind;

int ruda_native_c_symbol_compare(RudaSymbolId left, RudaSymbolId right);
int ruda_native_c_function_pointer_compare(const void *left, const void *right);
const RudaMirArray *ruda_native_c_array(const RudaMirFunction *function,
    RudaTypeId type);
RudaNativeCArrayKind ruda_native_c_array_kind(const RudaMirFunction *function,
    RudaTypeId type);
bool ruda_native_c_type_supported(const RudaMirFunction *function,
    RudaTypeId type, bool result);
const char *ruda_native_c_type(const RudaMirFunction *function, RudaTypeId type);
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
bool ruda_native_c_call_supported(const RudaMirFunction *function,
    RudaMirCallId call_id, const RudaMirFunction *const *functions,
    size_t count, RudaTypeId required_result);
RudaNativeCEmitStatus ruda_native_c_validate_function(
    const RudaMirFunction *function, const RudaMirFunction *const *functions,
    size_t count);
bool ruda_native_c_function_index(const RudaMirFunction *const *functions,
    size_t count, RudaSymbolId symbol, size_t *result);
bool ruda_native_c_has_cycle(const RudaMirFunction *const *functions,
    size_t count, unsigned char *states, RudaNativeCCycleFrame *stack);
bool ruda_native_c_emit_signature(RudaNativeCBuffer *buffer,
    const RudaMirFunction *function, bool prototype);
bool ruda_native_c_emit_function(RudaNativeCBuffer *buffer,
    const RudaMirFunction *function, const RudaMirFunction *const *functions,
    size_t count);
bool ruda_native_c_emit_symbol_and_strings(RudaNativeCBuffer *buffer,
    const RudaMirFunction *function);
bool ruda_native_c_emit_runtime(RudaNativeCBuffer *buffer, size_t max_frames);
bool ruda_native_c_emit_keep_assets(RudaNativeCBuffer *buffer,
    const RudaMirFunction *const *functions, size_t count);

#endif
