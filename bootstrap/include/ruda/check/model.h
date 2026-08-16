#ifndef RUDA_CHECK_MODEL_H
#define RUDA_CHECK_MODEL_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/owned_bytes.h"
#include "ruda/mir/function.h"
#include "ruda/mir/interpret.h"
#include "ruda/source/source_manager.h"
#include "ruda/types/type.h"

typedef struct RudaCheckDiagnostic {
    const char *code;
    const char *message;
    RudaOwnedBytes owned_message;
    const RudaSourceSnapshot *snapshot;
    size_t start;
    size_t end;
} RudaCheckDiagnostic;

typedef struct RudaCheckedFunction {
    RudaOwnedBytes name;
    RudaTypeId signature_type;
    RudaSymbolId receiver_symbol;
    RudaTypeId receiver_type;
    RudaMirFunction mir;
    bool dot_eligible;
    bool is_public;
    bool is_test;
    bool is_fuzz;
} RudaCheckedFunction;

typedef struct RudaCheckedConstant {
    RudaOwnedBytes name;
    RudaSymbolId symbol;
    RudaMirValue value;
    bool is_public;
} RudaCheckedConstant;

typedef struct RudaCheckedTypeAlias {
    RudaOwnedBytes name;
    RudaSymbolId symbol;
    RudaTypeId target_type;
    bool is_public;
} RudaCheckedTypeAlias;

#endif
