#include "cli_test_native.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mmay/runtime/program_arguments.h"

static int mmay_cli_test_symbol_compare(const void *left, const void *right) {
    return memcmp(left, right, sizeof(MarimaySymbolId));
}

bool mmay_cli_test_native_symbols(
    const MarimayCheckResult *checked, MarimayAllocator allocator,
    MarimayVector *output
) {
    size_t index;
    if (output == NULL) return false;
    mmay_vector_dispose(output);
    if (checked == NULL || !checked->initialized
        || !mmay_vector_init(output, allocator, sizeof(MarimaySymbolId))) return false;
    for (index = 0; index < checked->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(checked, index);
        if (function != NULL && function->is_test
            && !mmay_vector_push(output, &function->mir.symbol)) goto failure;
    }
    if (output->length > 1) {
        qsort(output->data, output->length, output->element_size, mmay_cli_test_symbol_compare);
        for (index = 1; index < output->length; index += 1) {
            if (mmay_cli_test_symbol_compare(mmay_vector_at_const(output, index - 1),
                    mmay_vector_at_const(output, index)) == 0) goto failure;
        }
    }
    return true;
failure:
    mmay_vector_dispose(output);
    return false;
}

bool mmay_cli_test_native_selector(
    const MarimayVector *symbols, MarimaySymbolId symbol,
    char output[MMAY_CLI_TEST_SELECTOR_BYTES]
) {
    size_t low = 0;
    size_t high;
    if (symbols == NULL || output == NULL
        || symbols->element_size != sizeof(MarimaySymbolId)) return false;
    high = symbols->length;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        const MarimaySymbolId *candidate = mmay_vector_at_const(symbols, middle);
        int compared;
        if (candidate == NULL) return false;
        compared = mmay_cli_test_symbol_compare(candidate, &symbol);
        if (compared < 0) low = middle + 1;
        else if (compared > 0) high = middle;
        else {
            int written = snprintf(output, MMAY_CLI_TEST_SELECTOR_BYTES, "%zu", middle);
            return written > 0 && (size_t)written < MMAY_CLI_TEST_SELECTOR_BYTES;
        }
    }
    return false;
}

bool mmay_cli_test_native_arguments(
    MarimayAllocator allocator, const char *artifact, const char *selector,
    const char *const *arguments, size_t count, MarimayVector *output
) {
    size_t index;
    const char *end = NULL;
    if (output == NULL) return false;
    mmay_vector_dispose(output);
    if (artifact == NULL || selector == NULL || (count != 0 && arguments == NULL)
        || count > MMAY_PROGRAM_ARGUMENT_MAX_COUNT
        || !mmay_vector_init(output, allocator, sizeof(const char *))) return false;
    if (!mmay_vector_reserve(output, count + 3)
        || !mmay_vector_push(output, &artifact)
        || !mmay_vector_push(output, &selector)) goto failure;
    for (index = 0; index < count; index += 1) {
        if (arguments[index] == NULL || !mmay_vector_push(output, &arguments[index])) goto failure;
    }
    if (!mmay_vector_push(output, &end)) goto failure;
    return true;
failure:
    mmay_vector_dispose(output);
    return false;
}
