#include "test_support.h"

#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/check/service.h"

static const MarimayCheckedSymbol *mmay_service_array_signature(
    const MarimayCompilerService *service,
    const MarimayCompilerResult *result
) {
    const MarimayCheckedSymbol *symbol = NULL;
    size_t count = 0;
    MMAY_EXPECT(mmay_compiler_service_symbol_search_count(
        service, result, "signature", strlen("signature"), &count
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(count == 1);
    MMAY_EXPECT(mmay_compiler_service_symbol_search(
        service,
        result,
        "signature",
        strlen("signature"),
        0,
        &symbol
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(symbol != NULL);
    return symbol;
}

static void mmay_test_service_array_type_spelling_query(void) {
    static const char source[] =
        "fn signature(rows: [][]int, maybe: Option<[]int>): [][]int {\n"
        "  return rows;\n"
        "}\n";
    static const char updated[] =
        "fn signature(rows: []int): []int { return rows; }\n";
    static const char expected[] =
        "([][]int, Option<[]int>) => [][]int";
    MarimayCompilerService service;
    MarimayCompilerResult result;
    MarimayFileId file = 0;
    const MarimayCheckedSymbol *symbol = NULL;
    MarimayOwnedBytes spelling;
    MarimayTypeSpellingOptions options = mmay_type_spelling_default_options();
    memset(&service, 0, sizeof(service));
    memset(&result, 0, sizeof(result));
    memset(&spelling, 0, sizeof(spelling));
    MMAY_EXPECT(mmay_compiler_service_init(
        &service,
        mmay_system_allocator(),
        mmay_compiler_default_options()
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service,
        "main.my",
        strlen("main.my"),
        1,
        source,
        strlen(source),
        &file
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_check(
        &service, "cp18-service", &result
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(!mmay_compiler_result_has_errors(&result));
    symbol = mmay_service_array_signature(&service, &result);
    if (symbol != NULL) {
        MMAY_EXPECT(mmay_compiler_service_symbol_type_spelling(
            &service,
            &result,
            symbol->symbol,
            mmay_system_allocator(),
            options,
            &spelling
        ) == MMAY_COMPILER_OK);
        MMAY_EXPECT(spelling.length == sizeof(expected) - 1);
        MMAY_EXPECT(spelling.length != sizeof(expected) - 1
            || memcmp(spelling.data, expected, sizeof(expected) - 1) == 0);
        mmay_owned_bytes_dispose(&spelling);

        options.max_bytes = 2;
        MMAY_EXPECT(mmay_compiler_service_symbol_type_spelling(
            &service,
            &result,
            symbol->symbol,
            mmay_system_allocator(),
            options,
            &spelling
        ) == MMAY_COMPILER_LIMIT_REACHED);
        MMAY_EXPECT(spelling.data == NULL && spelling.length == 0);

        options = mmay_type_spelling_default_options();
        options.max_depth = 1;
        MMAY_EXPECT(mmay_compiler_service_symbol_type_spelling(
            &service,
            &result,
            symbol->symbol,
            mmay_system_allocator(),
            options,
            &spelling
        ) == MMAY_COMPILER_LIMIT_REACHED);
        MMAY_EXPECT(spelling.data == NULL && spelling.length == 0);
    }

    MMAY_EXPECT(mmay_compiler_service_overlay(
        &service, file, 2, updated, strlen(updated)
    ) == MMAY_COMPILER_OK);
    if (symbol != NULL) {
        options = mmay_type_spelling_default_options();
        MMAY_EXPECT(mmay_compiler_service_symbol_type_spelling(
            &service,
            &result,
            symbol->symbol,
            mmay_system_allocator(),
            options,
            &spelling
        ) == MMAY_COMPILER_STALE_RESULT);
        MMAY_EXPECT(spelling.data == NULL && spelling.length == 0);
    }
    mmay_compiler_result_dispose(&result);
    mmay_compiler_service_dispose(&service);
}

static void mmay_test_service_array_type_spelling_allocations(void) {
    static const char source[] =
        "fn signature(rows: [][]int): [][]int { return rows; }\n";
    size_t failure_index = 0;
    bool reached_success = false;
    MarimayCompilerService service;
    MarimayCompilerResult result;
    MarimayFileId file = 0;
    const MarimayCheckedSymbol *symbol = NULL;
    memset(&service, 0, sizeof(service));
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_compiler_service_init(
        &service,
        mmay_system_allocator(),
        mmay_compiler_default_options()
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service,
        "main.my",
        strlen("main.my"),
        1,
        source,
        strlen(source),
        &file
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(file != 0);
    MMAY_EXPECT(mmay_compiler_service_check(
        &service, "cp18-service", &result
    ) == MMAY_COMPILER_OK);
    symbol = mmay_service_array_signature(&service, &result);
    for (failure_index = 0; symbol != NULL && failure_index < 32;
        failure_index += 1) {
        MarimayFailAllocator state;
        MarimayOwnedBytes spelling;
        MarimayCompilerStatus status;
        memset(&spelling, 0, sizeof(spelling));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_compiler_service_symbol_type_spelling(
            &service,
            &result,
            symbol->symbol,
            mmay_fail_allocator(&state),
            mmay_type_spelling_default_options(),
            &spelling
        );
        if (status == MMAY_COMPILER_OK) {
            reached_success = true;
            MMAY_EXPECT(spelling.length != 0);
            mmay_owned_bytes_dispose(&spelling);
        } else {
            MMAY_EXPECT(status == MMAY_COMPILER_ALLOCATION_FAILED);
            MMAY_EXPECT(spelling.data == NULL && spelling.length == 0);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
    mmay_compiler_result_dispose(&result);
    mmay_compiler_service_dispose(&service);
}

void mmay_test_compiler_service_array_types(void) {
    mmay_test_service_array_type_spelling_query();
    mmay_test_service_array_type_spelling_allocations();
}
