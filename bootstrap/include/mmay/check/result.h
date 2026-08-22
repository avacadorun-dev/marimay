#ifndef MMAY_CHECK_RESULT_H
#define MMAY_CHECK_RESULT_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/map.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/base/vector.h"
#include "mmay/check/facts.h"
#include "mmay/check/model.h"
#include "mmay/check/nominal.h"
#include "mmay/types/type.h"

typedef struct MarimayCheckResult {
    MarimayAllocator allocator;
    MarimayOwnedBytes package_name;
    /* Borrowed first source snapshot in canonical path order. */
    const MarimaySourceSnapshot *snapshot;
    MarimayTypeTable types;
    MarimayVector diagnostics;
    MarimayVector functions;
    MarimayVector constants;
    MarimayVector type_aliases;
    MarimayVector records;
    MarimayVector enums;
    MarimayVector interfaces;
    MarimayVector symbols;
    MarimayVector occurrences;
    MarimayVector completions;
    MarimayMap function_index;
    MarimayMap symbol_index;
    bool initialized;
} MarimayCheckResult;

const MarimayCheckDiagnostic *mmay_check_diagnostic(
    const MarimayCheckResult *result,
    size_t index
);
const MarimayCheckedFunction *mmay_checked_function(
    const MarimayCheckResult *result,
    size_t index
);
const MarimayCheckedConstant *mmay_checked_constant(
    const MarimayCheckResult *result,
    size_t index
);
const MarimayCheckedTypeAlias *mmay_checked_type_alias(
    const MarimayCheckResult *result,
    size_t index
);
const MarimayCheckedRecord *mmay_checked_record(
    const MarimayCheckResult *result,
    size_t index
);
const MarimayCheckedEnum *mmay_checked_enum(
    const MarimayCheckResult *result,
    size_t index
);
const MarimayCheckedInterface *mmay_checked_interface(
    const MarimayCheckResult *result,
    size_t index
);
const MarimayCheckedSymbol *mmay_checked_symbol(
    const MarimayCheckResult *result,
    size_t index
);
const MarimayCheckedSymbol *mmay_checked_symbol_by_id(
    const MarimayCheckResult *result,
    MarimaySymbolId symbol
);
/* Exact declaration-name hit only; reference occurrences are not indexed yet. */
const MarimayCheckedSymbol *mmay_checked_symbol_defined_at(
    const MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot,
    size_t byte_offset
);
void mmay_check_result_dispose(MarimayCheckResult *result);

#endif
