#ifndef RUDA_TYPES_TYPE_H
#define RUDA_TYPES_TYPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"
#include "ruda/base/map.h"
#include "ruda/base/vector.h"

typedef uint32_t RudaTypeId;
typedef uint32_t RudaInferenceId;

enum {
    RUDA_TYPE_INVALID = 0,
    RUDA_TYPE_BOOL,
    RUDA_TYPE_I8,
    RUDA_TYPE_I16,
    RUDA_TYPE_I32,
    RUDA_TYPE_I64,
    RUDA_TYPE_U8,
    RUDA_TYPE_U16,
    RUDA_TYPE_U32,
    RUDA_TYPE_U64,
    RUDA_TYPE_F32,
    RUDA_TYPE_F64,
    RUDA_TYPE_RUNE,
    RUDA_TYPE_STRING,
    RUDA_TYPE_VOID,
    RUDA_TYPE_NEVER,
    RUDA_TYPE_FIRST_DYNAMIC
};

#define RUDA_INFERENCE_INVALID UINT32_MAX

typedef enum RudaTypeKind {
    RUDA_TYPE_KIND_INVALID,
    RUDA_TYPE_KIND_BOOL,
    RUDA_TYPE_KIND_I8,
    RUDA_TYPE_KIND_I16,
    RUDA_TYPE_KIND_I32,
    RUDA_TYPE_KIND_I64,
    RUDA_TYPE_KIND_U8,
    RUDA_TYPE_KIND_U16,
    RUDA_TYPE_KIND_U32,
    RUDA_TYPE_KIND_U64,
    RUDA_TYPE_KIND_F32,
    RUDA_TYPE_KIND_F64,
    RUDA_TYPE_KIND_RUNE,
    RUDA_TYPE_KIND_STRING,
    RUDA_TYPE_KIND_VOID,
    RUDA_TYPE_KIND_NEVER,
    RUDA_TYPE_KIND_NOMINAL,
    RUDA_TYPE_KIND_INTERFACE,
    RUDA_TYPE_KIND_TUPLE,
    RUDA_TYPE_KIND_FUNCTION,
    RUDA_TYPE_KIND_TYPE_PARAMETER
} RudaTypeKind;

typedef struct RudaSymbolId {
    unsigned char bytes[16];
} RudaSymbolId;

typedef struct RudaTypeDescriptor {
    RudaTypeKind kind;
    RudaSymbolId symbol;
    size_t parameter_count;
    size_t argument_count;
    size_t allocation_size;
    RudaTypeId arguments[];
} RudaTypeDescriptor;

typedef struct RudaTypeTableOptions {
    size_t max_types;
    size_t max_type_arguments;
} RudaTypeTableOptions;

typedef struct RudaTypeTable {
    RudaAllocator allocator;
    RudaVector descriptors;
    RudaMap canonical;
    RudaTypeTableOptions options;
    bool initialized;
} RudaTypeTable;

typedef struct RudaInferenceVariable {
    RudaInferenceId parent;
    uint32_t rank;
    RudaTypeId binding;
} RudaInferenceVariable;

typedef struct RudaInferenceContext {
    RudaAllocator allocator;
    RudaVector variables;
    size_t max_variables;
    bool initialized;
} RudaInferenceContext;

RudaTypeTableOptions ruda_type_table_default_options(void);

/* Stable across hosts for the same exact UTF-8 parts and discriminator. */
bool ruda_symbol_id_from_parts(
    const char *package_name,
    const char *source_path,
    const char *declaration_name,
    uint32_t discriminator,
    RudaSymbolId *result
);
bool ruda_symbol_id_equal(RudaSymbolId left, RudaSymbolId right);

bool ruda_type_table_init(
    RudaTypeTable *table,
    RudaAllocator allocator,
    RudaTypeTableOptions options
);
void ruda_type_table_dispose(RudaTypeTable *table);

RudaTypeKind ruda_type_kind(const RudaTypeTable *table, RudaTypeId type);
const RudaTypeDescriptor *ruda_type_descriptor(
    const RudaTypeTable *table,
    RudaTypeId type
);

bool ruda_type_intern_nominal(
    RudaTypeTable *table,
    RudaTypeKind kind,
    RudaSymbolId symbol,
    const RudaTypeId *arguments,
    size_t argument_count,
    RudaTypeId *result
);
bool ruda_type_intern_tuple(
    RudaTypeTable *table,
    const RudaTypeId *elements,
    size_t element_count,
    RudaTypeId *result
);
bool ruda_type_intern_function(
    RudaTypeTable *table,
    const RudaTypeId *parameters,
    size_t parameter_count,
    RudaTypeId return_type,
    RudaTypeId *result
);

bool ruda_inference_context_init(
    RudaInferenceContext *context,
    RudaAllocator allocator,
    size_t max_variables
);
void ruda_inference_context_dispose(RudaInferenceContext *context);
bool ruda_inference_new(RudaInferenceContext *context, RudaInferenceId *result);
bool ruda_inference_unify_variables(
    RudaInferenceContext *context,
    RudaInferenceId left,
    RudaInferenceId right
);
bool ruda_inference_bind(
    RudaInferenceContext *context,
    RudaInferenceId variable,
    RudaTypeId type
);
RudaTypeId ruda_inference_resolve(
    RudaInferenceContext *context,
    RudaInferenceId variable
);

#endif
