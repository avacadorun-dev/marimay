#ifndef RUDA_CHECK_RESULT_H
#define RUDA_CHECK_RESULT_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/allocator.h"
#include "ruda/base/map.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/base/vector.h"
#include "ruda/check/facts.h"
#include "ruda/check/model.h"
#include "ruda/check/nominal.h"
#include "ruda/types/type.h"

typedef struct RudaCheckResult {
    RudaAllocator allocator;
    RudaOwnedBytes package_name;
    /* Borrowed first source snapshot in canonical path order. */
    const RudaSourceSnapshot *snapshot;
    RudaTypeTable types;
    RudaVector diagnostics;
    RudaVector functions;
    RudaVector constants;
    RudaVector type_aliases;
    RudaVector records;
    RudaVector enums;
    RudaVector interfaces;
    RudaVector symbols;
    RudaMap function_index;
    RudaMap symbol_index;
    bool initialized;
} RudaCheckResult;

const RudaCheckDiagnostic *ruda_check_diagnostic(
    const RudaCheckResult *result,
    size_t index
);
const RudaCheckedFunction *ruda_checked_function(
    const RudaCheckResult *result,
    size_t index
);
const RudaCheckedConstant *ruda_checked_constant(
    const RudaCheckResult *result,
    size_t index
);
const RudaCheckedTypeAlias *ruda_checked_type_alias(
    const RudaCheckResult *result,
    size_t index
);
const RudaCheckedRecord *ruda_checked_record(
    const RudaCheckResult *result,
    size_t index
);
const RudaCheckedEnum *ruda_checked_enum(
    const RudaCheckResult *result,
    size_t index
);
const RudaCheckedInterface *ruda_checked_interface(
    const RudaCheckResult *result,
    size_t index
);
const RudaCheckedSymbol *ruda_checked_symbol(
    const RudaCheckResult *result,
    size_t index
);
const RudaCheckedSymbol *ruda_checked_symbol_by_id(
    const RudaCheckResult *result,
    RudaSymbolId symbol
);
/* Exact declaration-name hit only; reference occurrences are not indexed yet. */
const RudaCheckedSymbol *ruda_checked_symbol_defined_at(
    const RudaCheckResult *result,
    const RudaSourceSnapshot *snapshot,
    size_t byte_offset
);
void ruda_check_result_dispose(RudaCheckResult *result);

#endif
