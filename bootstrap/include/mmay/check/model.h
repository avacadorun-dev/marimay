#ifndef MMAY_CHECK_MODEL_H
#define MMAY_CHECK_MODEL_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/owned_bytes.h"
#include "mmay/mir/function.h"
#include "mmay/mir/interpret.h"
#include "mmay/source/source_manager.h"
#include "mmay/types/type.h"

typedef struct MarimayCheckDiagnostic {
    const char *code;
    const char *message;
    MarimayOwnedBytes owned_message;
    const MarimaySourceSnapshot *snapshot;
    size_t start;
    size_t end;
} MarimayCheckDiagnostic;

typedef struct MarimayCheckedFunction {
    MarimayOwnedBytes name;
    MarimayTypeId signature_type;
    MarimaySymbolId receiver_symbol;
    MarimayTypeId receiver_type;
    MarimayMirFunction mir;
    bool dot_eligible;
    bool is_public;
    bool is_external;
    bool is_test;
    bool is_fuzz;
} MarimayCheckedFunction;

typedef struct MarimayCheckedConstant {
    MarimayOwnedBytes name;
    MarimaySymbolId symbol;
    MarimayMirValue value;
    bool is_public;
} MarimayCheckedConstant;

typedef struct MarimayCheckedTypeAlias {
    MarimayOwnedBytes name;
    MarimaySymbolId symbol;
    MarimayTypeId target_type;
    bool is_public;
} MarimayCheckedTypeAlias;

#endif
