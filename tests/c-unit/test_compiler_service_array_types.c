#include "test_support.h"

#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/check/service.h"

static const RudaCheckedSymbol *ruda_service_array_signature(
    const RudaCompilerService *service,
    const RudaCompilerResult *result
) {
    const RudaCheckedSymbol *symbol = NULL;
    size_t count = 0;
    RUDA_EXPECT(ruda_compiler_service_symbol_search_count(
        service, result, "signature", strlen("signature"), &count
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(count == 1);
    RUDA_EXPECT(ruda_compiler_service_symbol_search(
        service,
        result,
        "signature",
        strlen("signature"),
        0,
        &symbol
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(symbol != NULL);
    return symbol;
}

static void ruda_test_service_array_type_spelling_query(void) {
    static const char source[] =
        "fn signature(rows: [][]int, maybe: Option<[]int>): [][]int {\n"
        "  return rows;\n"
        "}\n";
    static const char updated[] =
        "fn signature(rows: []int): []int { return rows; }\n";
    static const char expected[] =
        "([][]int, Option<[]int>) => [][]int";
    RudaCompilerService service;
    RudaCompilerResult result;
    RudaFileId file = 0;
    const RudaCheckedSymbol *symbol = NULL;
    RudaOwnedBytes spelling;
    RudaTypeSpellingOptions options = ruda_type_spelling_default_options();
    memset(&service, 0, sizeof(service));
    memset(&result, 0, sizeof(result));
    memset(&spelling, 0, sizeof(spelling));
    RUDA_EXPECT(ruda_compiler_service_init(
        &service,
        ruda_system_allocator(),
        ruda_compiler_default_options()
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service,
        "main.ruda",
        strlen("main.ruda"),
        1,
        source,
        strlen(source),
        &file
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_check(
        &service, "cp18-service", &result
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(!ruda_compiler_result_has_errors(&result));
    symbol = ruda_service_array_signature(&service, &result);
    if (symbol != NULL) {
        RUDA_EXPECT(ruda_compiler_service_symbol_type_spelling(
            &service,
            &result,
            symbol->symbol,
            ruda_system_allocator(),
            options,
            &spelling
        ) == RUDA_COMPILER_OK);
        RUDA_EXPECT(spelling.length == sizeof(expected) - 1);
        RUDA_EXPECT(spelling.length != sizeof(expected) - 1
            || memcmp(spelling.data, expected, sizeof(expected) - 1) == 0);
        ruda_owned_bytes_dispose(&spelling);

        options.max_bytes = 2;
        RUDA_EXPECT(ruda_compiler_service_symbol_type_spelling(
            &service,
            &result,
            symbol->symbol,
            ruda_system_allocator(),
            options,
            &spelling
        ) == RUDA_COMPILER_LIMIT_REACHED);
        RUDA_EXPECT(spelling.data == NULL && spelling.length == 0);

        options = ruda_type_spelling_default_options();
        options.max_depth = 1;
        RUDA_EXPECT(ruda_compiler_service_symbol_type_spelling(
            &service,
            &result,
            symbol->symbol,
            ruda_system_allocator(),
            options,
            &spelling
        ) == RUDA_COMPILER_LIMIT_REACHED);
        RUDA_EXPECT(spelling.data == NULL && spelling.length == 0);
    }

    RUDA_EXPECT(ruda_compiler_service_overlay(
        &service, file, 2, updated, strlen(updated)
    ) == RUDA_COMPILER_OK);
    if (symbol != NULL) {
        options = ruda_type_spelling_default_options();
        RUDA_EXPECT(ruda_compiler_service_symbol_type_spelling(
            &service,
            &result,
            symbol->symbol,
            ruda_system_allocator(),
            options,
            &spelling
        ) == RUDA_COMPILER_STALE_RESULT);
        RUDA_EXPECT(spelling.data == NULL && spelling.length == 0);
    }
    ruda_compiler_result_dispose(&result);
    ruda_compiler_service_dispose(&service);
}

static void ruda_test_service_array_type_spelling_allocations(void) {
    static const char source[] =
        "fn signature(rows: [][]int): [][]int { return rows; }\n";
    size_t failure_index = 0;
    bool reached_success = false;
    RudaCompilerService service;
    RudaCompilerResult result;
    RudaFileId file = 0;
    const RudaCheckedSymbol *symbol = NULL;
    memset(&service, 0, sizeof(service));
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(ruda_compiler_service_init(
        &service,
        ruda_system_allocator(),
        ruda_compiler_default_options()
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service,
        "main.ruda",
        strlen("main.ruda"),
        1,
        source,
        strlen(source),
        &file
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(file != 0);
    RUDA_EXPECT(ruda_compiler_service_check(
        &service, "cp18-service", &result
    ) == RUDA_COMPILER_OK);
    symbol = ruda_service_array_signature(&service, &result);
    for (failure_index = 0; symbol != NULL && failure_index < 32;
        failure_index += 1) {
        RudaFailAllocator state;
        RudaOwnedBytes spelling;
        RudaCompilerStatus status;
        memset(&spelling, 0, sizeof(spelling));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        status = ruda_compiler_service_symbol_type_spelling(
            &service,
            &result,
            symbol->symbol,
            ruda_fail_allocator(&state),
            ruda_type_spelling_default_options(),
            &spelling
        );
        if (status == RUDA_COMPILER_OK) {
            reached_success = true;
            RUDA_EXPECT(spelling.length != 0);
            ruda_owned_bytes_dispose(&spelling);
        } else {
            RUDA_EXPECT(status == RUDA_COMPILER_ALLOCATION_FAILED);
            RUDA_EXPECT(spelling.data == NULL && spelling.length == 0);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
    ruda_compiler_result_dispose(&result);
    ruda_compiler_service_dispose(&service);
}

void ruda_test_compiler_service_array_types(void) {
    ruda_test_service_array_type_spelling_query();
    ruda_test_service_array_type_spelling_allocations();
}
