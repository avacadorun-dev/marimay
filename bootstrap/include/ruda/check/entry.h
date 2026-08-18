#ifndef RUDA_CHECK_ENTRY_H
#define RUDA_CHECK_ENTRY_H

#include "ruda/check/result.h"

typedef enum RudaCheckEntryStatus {
    RUDA_CHECK_ENTRY_OK,
    RUDA_CHECK_ENTRY_INVALID_ARGUMENT,
    RUDA_CHECK_ENTRY_NOT_FOUND,
    RUDA_CHECK_ENTRY_INVALID_SIGNATURE,
    RUDA_CHECK_ENTRY_ERROR_CONTRACT_MISSING
} RudaCheckEntryStatus;

typedef struct RudaCheckedEntry {
    const RudaCheckedFunction *function;
    const RudaCheckedSymbol *symbol;
    const RudaCheckedFunction *error_message;
    RudaTypeId error_type;
    bool returns_result;
} RudaCheckedEntry;

/*
 * Selects the exact root-package `main` and validates the normative entry
 * signatures. For Result<void, E>, `message(E): string` is the structural
 * prelude Error contract and is returned for expected-error rendering.
 * Returned pointers are borrowed from result.
 */
RudaCheckEntryStatus ruda_check_select_entry(
    const RudaCheckResult *result,
    const char *root_package,
    size_t root_package_length,
    RudaCheckedEntry *entry
);

#endif
