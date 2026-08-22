#ifndef MMAY_CHECK_ENTRY_H
#define MMAY_CHECK_ENTRY_H

#include "mmay/check/result.h"

typedef enum MarimayCheckEntryStatus {
    MMAY_CHECK_ENTRY_OK,
    MMAY_CHECK_ENTRY_INVALID_ARGUMENT,
    MMAY_CHECK_ENTRY_NOT_FOUND,
    MMAY_CHECK_ENTRY_INVALID_SIGNATURE,
    MMAY_CHECK_ENTRY_ERROR_CONTRACT_MISSING
} MarimayCheckEntryStatus;

typedef struct MarimayCheckedEntry {
    const MarimayCheckedFunction *function;
    const MarimayCheckedSymbol *symbol;
    const MarimayCheckedFunction *error_message;
    MarimayTypeId error_type;
    bool returns_result;
} MarimayCheckedEntry;

/*
 * Selects the exact root-package `main` and validates the normative entry
 * signatures. For Result<void, E>, `message(E): string` is the structural
 * prelude Error contract and is returned for expected-error rendering.
 * Returned pointers are borrowed from result.
 */
MarimayCheckEntryStatus mmay_check_select_entry(
    const MarimayCheckResult *result,
    const char *root_package,
    size_t root_package_length,
    MarimayCheckedEntry *entry
);

#endif
