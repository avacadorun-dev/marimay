#ifndef MMAY_CHECK_NOMINAL_H
#define MMAY_CHECK_NOMINAL_H

#include <stdbool.h>

#include "mmay/base/owned_bytes.h"
#include "mmay/base/vector.h"
#include "mmay/types/type.h"

typedef struct MarimayCheckedRecordField {
    MarimayOwnedBytes name;
    MarimayTypeId type;
    bool is_public;
    bool is_mutable;
} MarimayCheckedRecordField;

typedef struct MarimayCheckedRecord {
    MarimayOwnedBytes name;
    MarimaySymbolId symbol;
    MarimayTypeId type;
    /* Declaration-order generic parameter TypeIds for imported substitution. */
    MarimayVector type_parameters;
    MarimayVector fields;
    bool is_public;
    bool derives_eq_hash;
} MarimayCheckedRecord;

typedef enum MarimayCheckedEnumVariantKind {
    MMAY_CHECKED_ENUM_VARIANT_UNIT,
    MMAY_CHECKED_ENUM_VARIANT_POSITIONAL,
    MMAY_CHECKED_ENUM_VARIANT_NAMED
} MarimayCheckedEnumVariantKind;

typedef struct MarimayCheckedEnumPayloadItem {
    MarimayOwnedBytes name;
    MarimayTypeId type;
    bool has_name;
} MarimayCheckedEnumPayloadItem;

typedef struct MarimayCheckedEnumVariant {
    MarimayOwnedBytes name;
    MarimayCheckedEnumVariantKind kind;
    MarimayVector payload;
} MarimayCheckedEnumVariant;

typedef struct MarimayCheckedEnum {
    MarimayOwnedBytes name;
    MarimaySymbolId symbol;
    MarimayTypeId type;
    /* Declaration-order generic parameter TypeIds for imported substitution. */
    MarimayVector type_parameters;
    MarimayVector variants;
    bool is_public;
    bool derives_eq_hash;
} MarimayCheckedEnum;

typedef struct MarimayCheckedInterfaceMethod {
    MarimayOwnedBytes name;
    MarimayVector parameter_types;
    MarimayTypeId result_type;
} MarimayCheckedInterfaceMethod;

typedef struct MarimayCheckedInterface {
    MarimayOwnedBytes name;
    MarimaySymbolId symbol;
    MarimayTypeId type;
    MarimayVector methods;
    bool is_public;
} MarimayCheckedInterface;

#endif
