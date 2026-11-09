#include "test_support.h"

#include <stdint.h>
#include <string.h>
#include "mmay/check/service.h"
#include "mmay/check/visible_symbols.h"

static MarimayCompilerStatus visible_run(MarimayAllocator allocator, bool verify) {
    static const char library[] =
        "public type User { public id: int, hidden: int, }\n"
        "public fn alpha(): int { return 1; }\n"
        "fn hidden(): int { return 2; }\n";
    static const char application[] =
        "import { User as Person, alpha as renamed } from \"pkg/lib\";\n"
        "import * as ns from \"pkg/lib\";\n"
        "fn own(): int { return 0; }\n"
        "fn main(): int { return renamed() + ns.alpha(); }\n";
    MarimayCompilerService service = {0};
    MarimayCompilerResult result = {0};
    MarimayFileId files[2] = {0};
    MarimayCompilerPackage packages[2] = {
        {"pkg/lib", 7, &files[0], 1}, {"pkg/app", 7, &files[1], 1}
    };
    const MarimaySourceSnapshot *snapshot = NULL;
    MarimayCompilerStatus status = mmay_compiler_service_init(&service,
        allocator, mmay_compiler_default_options());
    if (status != MMAY_COMPILER_OK) goto cleanup;
    status = mmay_compiler_service_add_document(&service, "lib/api.my", 10,
        1, library, sizeof(library) - 1, &files[0]);
    if (status != MMAY_COMPILER_OK) goto cleanup;
    status = mmay_compiler_service_add_document(&service, "app/main.my", 11,
        1, application, sizeof(application) - 1, &files[1]);
    if (status != MMAY_COMPILER_OK) goto cleanup;
    status = mmay_compiler_service_check_graph(&service, packages, 2, "pkg/app", 7, &result);
    if (status != MMAY_COMPILER_OK) goto cleanup;
    if (verify) {
        size_t index = 0;
        static const char *const expected[] = {"own", "main", "Person", "renamed", "ns.User", "ns.alpha"};
        MMAY_EXPECT(!mmay_compiler_result_has_errors(&result));
        for (index = 0; index < result.units.length; index += 1) {
            const MarimayCompilerUnit *unit = mmay_compiler_result_unit(&result, index);
            if (unit->file_id == files[1]) snapshot = unit->snapshot;
        }
        MMAY_EXPECT(snapshot != NULL);
        MMAY_EXPECT(mmay_checked_visible_symbol_count(&result.checked, snapshot, NULL, 0) == 6);
        for (index = 0; index < sizeof(expected) / sizeof(expected[0]); index += 1) {
            const MarimayCheckedVisibleSymbol *item = mmay_checked_visible_symbol(
                &result.checked, snapshot, expected[index], strlen(expected[index]), 0);
            MMAY_EXPECT(item != NULL && item->spelling.length == strlen(expected[index]));
            if (item != NULL) MMAY_EXPECT(mmay_checked_symbol_by_id(&result.checked, item->symbol) != NULL);
        }
        MMAY_EXPECT(mmay_checked_visible_symbol_count(&result.checked, snapshot, "hidden", 6) == 0);
        MMAY_EXPECT(mmay_checked_visible_symbol_count(&result.checked, snapshot, "ns.hidden", 9) == 0);
        MMAY_EXPECT(mmay_checked_visible_symbol_count(&result.checked, snapshot, "alpha", 5) == 0);
        MMAY_EXPECT(mmay_checked_visible_symbol_count(&result.checked, snapshot, NULL, 1) == 0);
        MMAY_EXPECT(mmay_checked_visible_symbol(&result.checked, snapshot, NULL, 0, 100) == NULL);
    }
cleanup:
    mmay_compiler_result_dispose(&result);
    mmay_compiler_service_dispose(&service);
    return status;
}

void mmay_test_compiler_visible_symbols(void) {
    MarimayFailAllocator state;
    size_t index = 0, attempts;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(visible_run(mmay_fail_allocator(&state), true) == MMAY_COMPILER_OK);
    attempts = state.attempts;
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    for (index = 0; index < attempts; index += 1) {
        mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
        MMAY_EXPECT(visible_run(mmay_fail_allocator(&state), false) == MMAY_COMPILER_ALLOCATION_FAILED);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(mmay_checked_visible_symbol_count(NULL, NULL, NULL, 0) == 0);
    MMAY_EXPECT(mmay_checked_visible_symbol(NULL, NULL, NULL, 0, 0) == NULL);
}
