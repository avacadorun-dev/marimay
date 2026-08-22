#ifndef MMAY_TYPES_MODEL_H
#define MMAY_TYPES_MODEL_H

#include <stddef.h>
#include <stdint.h>

typedef uint32_t MarimayTypeId;
typedef uint32_t MarimayInferenceId;

enum {
    MMAY_TYPE_INVALID = 0,
    MMAY_TYPE_BOOL,
    MMAY_TYPE_I8,
    MMAY_TYPE_I16,
    MMAY_TYPE_I32,
    MMAY_TYPE_I64,
    MMAY_TYPE_U8,
    MMAY_TYPE_U16,
    MMAY_TYPE_U32,
    MMAY_TYPE_U64,
    MMAY_TYPE_F32,
    MMAY_TYPE_F64,
    MMAY_TYPE_RUNE,
    MMAY_TYPE_STRING,
    MMAY_TYPE_VOID,
    MMAY_TYPE_NEVER,
    MMAY_TYPE_FIRST_DYNAMIC
};

#define MMAY_INFERENCE_INVALID UINT32_MAX

typedef enum MarimayTypeKind {
    MMAY_TYPE_KIND_INVALID,
    MMAY_TYPE_KIND_BOOL,
    MMAY_TYPE_KIND_I8,
    MMAY_TYPE_KIND_I16,
    MMAY_TYPE_KIND_I32,
    MMAY_TYPE_KIND_I64,
    MMAY_TYPE_KIND_U8,
    MMAY_TYPE_KIND_U16,
    MMAY_TYPE_KIND_U32,
    MMAY_TYPE_KIND_U64,
    MMAY_TYPE_KIND_F32,
    MMAY_TYPE_KIND_F64,
    MMAY_TYPE_KIND_RUNE,
    MMAY_TYPE_KIND_STRING,
    MMAY_TYPE_KIND_VOID,
    MMAY_TYPE_KIND_NEVER,
    MMAY_TYPE_KIND_NOMINAL,
    MMAY_TYPE_KIND_INTERFACE,
    MMAY_TYPE_KIND_TUPLE,
    MMAY_TYPE_KIND_FUNCTION,
    MMAY_TYPE_KIND_TYPE_PARAMETER,
    /* Compiler-owned generic container kind; appended for API stability. */
    MMAY_TYPE_KIND_ARRAY
} MarimayTypeKind;

typedef struct MarimaySymbolId {
    unsigned char bytes[16];
} MarimaySymbolId;

typedef struct MarimayTypeDescriptor {
    MarimayTypeKind kind;
    MarimaySymbolId symbol;
    size_t parameter_count;
    size_t argument_count;
    size_t allocation_size;
    MarimayTypeId arguments[];
} MarimayTypeDescriptor;

#endif
