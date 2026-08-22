#ifndef MMAY_MIR_NOMINAL_H
#define MMAY_MIR_NOMINAL_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/mir/model.h"
#include "mmay/types/type.h"

typedef struct MarimayMirRecordField {
    MarimayTypeId type;
    bool is_mutable;
} MarimayMirRecordField;

typedef struct MarimayMirRecord {
    MarimayTypeId type;
    size_t field_start;
    size_t field_count;
} MarimayMirRecord;

typedef struct MarimayMirRecordArgument {
    MarimayMirRegister register_id;
    MarimayTypeId type;
} MarimayMirRecordArgument;

typedef struct MarimayMirRecordConstruction {
    MarimayMirRecordId record;
    size_t argument_start;
    size_t argument_count;
} MarimayMirRecordConstruction;

typedef enum MarimayMirEnumVariantKind {
    MMAY_MIR_ENUM_VARIANT_UNIT,
    MMAY_MIR_ENUM_VARIANT_POSITIONAL,
    MMAY_MIR_ENUM_VARIANT_NAMED
} MarimayMirEnumVariantKind;

typedef struct MarimayMirEnumPayload {
    MarimayTypeId type;
} MarimayMirEnumPayload;

typedef struct MarimayMirEnumVariant {
    MarimayMirEnumVariantKind kind;
    size_t payload_start;
    size_t payload_count;
} MarimayMirEnumVariant;

typedef struct MarimayMirEnum {
    MarimaySymbolId symbol;
    MarimayTypeId type;
    size_t variant_start;
    size_t variant_count;
    size_t payload_start;
    size_t payload_count;
} MarimayMirEnum;

typedef struct MarimayMirEnumArgument {
    MarimayMirRegister register_id;
    MarimayTypeId type;
} MarimayMirEnumArgument;

typedef struct MarimayMirEnumConstruction {
    MarimayMirEnumId enum_id;
    size_t variant;
    size_t argument_start;
    size_t argument_count;
} MarimayMirEnumConstruction;

typedef struct MarimayMirInterface {
    MarimayTypeId type;
    size_t method_count;
} MarimayMirInterface;

typedef struct MarimayMirInterfaceConstruction {
    MarimayMirInterfaceId interface_id;
    MarimayMirRegister payload;
    MarimayTypeId payload_type;
    size_t method_start;
    size_t method_count;
} MarimayMirInterfaceConstruction;

#endif
