#ifndef MMAY_CLI_TEST_NATIVE_H
#define MMAY_CLI_TEST_NATIVE_H

#include "mmay/check/result.h"

enum { MMAY_CLI_TEST_SELECTOR_BYTES = 3 * sizeof(size_t) + 1 };

/* Output vectors must be zeroed or owned; failures reset them. Symbols are
 * copied in strict order; no checked-result pointer escapes the call. */
bool mmay_cli_test_native_symbols(
    const MarimayCheckResult *checked, MarimayAllocator allocator,
    MarimayVector *output
);
/* Allocation-free lookup into the canonical symbol table. */
bool mmay_cli_test_native_selector(
    const MarimayVector *symbols, MarimaySymbolId symbol,
    char output[MMAY_CLI_TEST_SELECTOR_BYTES]
);
/* Owns only the pointer vector (including its final NULL). Artifact, selector
 * and user argument strings are borrowed through the synchronous child run. */
bool mmay_cli_test_native_arguments(
    MarimayAllocator allocator, const char *artifact, const char *selector,
    const char *const *arguments, size_t count, MarimayVector *output
);

#endif
