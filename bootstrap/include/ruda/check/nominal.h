#ifndef RUDA_CHECK_NOMINAL_H
#define RUDA_CHECK_NOMINAL_H

#include <stdbool.h>

#include "ruda/base/owned_bytes.h"
#include "ruda/base/vector.h"
#include "ruda/types/type.h"

typedef struct RudaCheckedRecordField {
    RudaOwnedBytes name;
    RudaTypeId type;
    bool is_public;
    bool is_mutable;
} RudaCheckedRecordField;

typedef struct RudaCheckedRecord {
    RudaOwnedBytes name;
    RudaSymbolId symbol;
    RudaTypeId type;
    /* Declaration-order generic parameter TypeIds for imported substitution. */
    RudaVector type_parameters;
    RudaVector fields;
    bool is_public;
} RudaCheckedRecord;

typedef enum RudaCheckedEnumVariantKind {
    RUDA_CHECKED_ENUM_VARIANT_UNIT,
    RUDA_CHECKED_ENUM_VARIANT_POSITIONAL,
    RUDA_CHECKED_ENUM_VARIANT_NAMED
} RudaCheckedEnumVariantKind;

typedef struct RudaCheckedEnumPayloadItem {
    RudaOwnedBytes name;
    RudaTypeId type;
    bool has_name;
} RudaCheckedEnumPayloadItem;

typedef struct RudaCheckedEnumVariant {
    RudaOwnedBytes name;
    RudaCheckedEnumVariantKind kind;
    RudaVector payload;
} RudaCheckedEnumVariant;

typedef struct RudaCheckedEnum {
    RudaOwnedBytes name;
    RudaSymbolId symbol;
    RudaTypeId type;
    /* Declaration-order generic parameter TypeIds for imported substitution. */
    RudaVector type_parameters;
    RudaVector variants;
    bool is_public;
} RudaCheckedEnum;

typedef struct RudaCheckedInterfaceMethod {
    RudaOwnedBytes name;
    RudaVector parameter_types;
    RudaTypeId result_type;
} RudaCheckedInterfaceMethod;

typedef struct RudaCheckedInterface {
    RudaOwnedBytes name;
    RudaSymbolId symbol;
    RudaTypeId type;
    RudaVector methods;
    bool is_public;
} RudaCheckedInterface;

#endif
