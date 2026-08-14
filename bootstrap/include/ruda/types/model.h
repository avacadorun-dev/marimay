#ifndef RUDA_TYPES_MODEL_H
#define RUDA_TYPES_MODEL_H

#include <stddef.h>
#include <stdint.h>

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
    RUDA_TYPE_KIND_TYPE_PARAMETER,
    /* Compiler-owned generic container kind; appended for API stability. */
    RUDA_TYPE_KIND_ARRAY
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

#endif
