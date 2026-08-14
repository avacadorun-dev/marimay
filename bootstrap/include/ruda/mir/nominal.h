#ifndef RUDA_MIR_NOMINAL_H
#define RUDA_MIR_NOMINAL_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/mir/model.h"
#include "ruda/types/type.h"

typedef struct RudaMirRecordField {
    RudaTypeId type;
    bool is_mutable;
} RudaMirRecordField;

typedef struct RudaMirRecord {
    RudaTypeId type;
    size_t field_start;
    size_t field_count;
} RudaMirRecord;

typedef struct RudaMirRecordArgument {
    RudaMirRegister register_id;
    RudaTypeId type;
} RudaMirRecordArgument;

typedef struct RudaMirRecordConstruction {
    RudaMirRecordId record;
    size_t argument_start;
    size_t argument_count;
} RudaMirRecordConstruction;

typedef enum RudaMirEnumVariantKind {
    RUDA_MIR_ENUM_VARIANT_UNIT,
    RUDA_MIR_ENUM_VARIANT_POSITIONAL,
    RUDA_MIR_ENUM_VARIANT_NAMED
} RudaMirEnumVariantKind;

typedef struct RudaMirEnumPayload {
    RudaTypeId type;
} RudaMirEnumPayload;

typedef struct RudaMirEnumVariant {
    RudaMirEnumVariantKind kind;
    size_t payload_start;
    size_t payload_count;
} RudaMirEnumVariant;

typedef struct RudaMirEnum {
    RudaTypeId type;
    size_t variant_start;
    size_t variant_count;
    size_t payload_start;
    size_t payload_count;
} RudaMirEnum;

typedef struct RudaMirEnumArgument {
    RudaMirRegister register_id;
    RudaTypeId type;
} RudaMirEnumArgument;

typedef struct RudaMirEnumConstruction {
    RudaMirEnumId enum_id;
    size_t variant;
    size_t argument_start;
    size_t argument_count;
} RudaMirEnumConstruction;

typedef struct RudaMirInterface {
    RudaTypeId type;
    size_t method_count;
} RudaMirInterface;

typedef struct RudaMirInterfaceConstruction {
    RudaMirInterfaceId interface_id;
    RudaMirRegister payload;
    RudaTypeId payload_type;
    size_t method_start;
    size_t method_count;
} RudaMirInterfaceConstruction;

#endif
