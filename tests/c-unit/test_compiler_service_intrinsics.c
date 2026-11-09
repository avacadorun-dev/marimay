#include "test_support.h"

#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/check/service.h"

/* Metadata/import fixture, not a substitute for the real typed-JSON corpus. */
static const char *const mmay_metadata_paths[] = {
    "json/model.my", "other/model.my", "root/namespace.my", "root/named.my",
    "root/type.my", "root/none.my", "root/other.my"
};
static const char *const mmay_metadata_sources[] = {
    "public type Marker {}\n",
    "public fn decode(value: int): int { return value; }\n",
    "import * as codec from \"std/json\";\n"
    "import * as copy from \"std/json\";\n"
    "fn localShadow(codec: int): int { return codec; }\n",
    "import { encode as save, decodeWithLimits as read, decode as alsoRead } from \"std/json\";\n",
    "import { Marker } from \"std/json\"; fn marker(value: Marker): Marker { return value; }\n",
    "fn plain(): int { return 0; }\n",
    "import * as json from \"other/json\"; fn pretend(): int { return json.decode(1); }\n"
};
enum { MMAY_METADATA_FILE_COUNT = 7 };

typedef struct MarimayMetadataFixture {
    MarimayCompilerService service;
    MarimayFileId files[MMAY_METADATA_FILE_COUNT];
} MarimayMetadataFixture;

static bool mmay_metadata_init(MarimayMetadataFixture *fixture,
    MarimayAllocator allocator, size_t max_completions
) {
    MarimayCompilerOptions options = mmay_compiler_default_options();
    size_t index;
    memset(fixture, 0, sizeof(*fixture));
    options.check_options.max_completions = max_completions;
    if (mmay_compiler_service_init(&fixture->service, allocator, options) != MMAY_COMPILER_OK) return false;
    for (index = 0; index < MMAY_METADATA_FILE_COUNT; index += 1) {
        if (mmay_compiler_service_add_document(&fixture->service,
                mmay_metadata_paths[index], strlen(mmay_metadata_paths[index]), 1,
                mmay_metadata_sources[index], strlen(mmay_metadata_sources[index]),
                &fixture->files[index]) != MMAY_COMPILER_OK) {
            mmay_compiler_service_dispose(&fixture->service);
            return false;
        }
    }
    return true;
}

static MarimayCompilerStatus mmay_metadata_check(MarimayMetadataFixture *fixture,
    bool reverse, const MarimayCompilerCheckRequest *request, MarimayCompilerResult *result
) {
    MarimayFileId root[5];
    MarimayCompilerPackage packages[3];
    size_t index;
    memset(packages, 0, sizeof(packages));
    for (index = 0; index < 5; index += 1) root[index] = fixture->files[reverse ? 6 - index : 2 + index];
    packages[reverse ? 2 : 0] = (MarimayCompilerPackage){"std/json", 8, &fixture->files[0], 1};
    packages[1] = (MarimayCompilerPackage){"other/json", 10, &fixture->files[1], 1};
    packages[reverse ? 0 : 2] = (MarimayCompilerPackage){"app/root", 8, root, 5};
    return mmay_compiler_service_check_graph_request(&fixture->service,
        packages, 3, "app/root", 8, request, result);
}

static const MarimaySourceSnapshot *mmay_metadata_snapshot(
    const MarimayCompilerResult *result, size_t file_index
) {
    size_t index;
    for (index = 0; index < result->units.length; index += 1) {
        const MarimayCompilerUnit *unit = mmay_compiler_result_unit(result, index);
        if (strcmp(mmay_source_path(unit->snapshot), mmay_metadata_paths[file_index]) == 0) return unit->snapshot;
    }
    return NULL;
}

static void mmay_metadata_expect_scope(const MarimayMetadataFixture *fixture,
    const MarimayCompilerResult *result, size_t file_index,
    const char *const *expected, size_t expected_count
) {
    const MarimaySourceSnapshot *snapshot = mmay_metadata_snapshot(result, file_index);
    const MarimayCheckedIntrinsicCompletion *item = NULL;
    size_t count = SIZE_MAX, index;
    MMAY_EXPECT(snapshot != NULL);
    MMAY_EXPECT(mmay_compiler_service_intrinsic_completion_count(&fixture->service,
        result, snapshot, NULL, 0, &count) == MMAY_COMPILER_OK);
    MMAY_EXPECT(count == expected_count);
    for (index = 0; index < count && index < expected_count; index += 1) {
        MMAY_EXPECT(mmay_compiler_service_intrinsic_completion(&fixture->service,
            result, snapshot, NULL, 0, index, &item) == MMAY_COMPILER_OK);
        MMAY_EXPECT(item != NULL);
        if (item == NULL) continue;
        MMAY_EXPECT(item->snapshot == snapshot && item->spelling.length == strlen(expected[index]));
        MMAY_EXPECT(strcmp((const char *)item->spelling.data, expected[index]) == 0);
        MMAY_EXPECT(strcmp(item->intrinsic->package_name, "std/json") == 0);
        MMAY_EXPECT(item->intrinsic->direct_call_only && item->intrinsic->max_type_arguments == 1);
        MMAY_EXPECT(strstr(item->intrinsic->constraint, "explicit codec") != NULL);
    }
    MMAY_EXPECT(mmay_compiler_service_intrinsic_completion(&fixture->service,
        result, snapshot, NULL, 0, count, &item) == MMAY_COMPILER_OK);
    MMAY_EXPECT(item == NULL);
}

static void mmay_metadata_imports_and_signatures(void) {
    static const char *const names[] = {
        "codec.decode", "codec.decodeWithLimits", "codec.encode", "codec.encodeWithLimits",
        "copy.decode", "copy.decodeWithLimits", "copy.encode", "copy.encodeWithLimits"
    };
    static const char *const named[] = {"alsoRead", "read", "save"};
    static const char *const signatures[] = {
        "fn decode<T>(contents: bytes.Bytes): Result<T, json.Error>",
        "fn decodeWithLimits<T>(contents: bytes.Bytes, limits: json.Limits): Result<T, json.Error>",
        "fn encode<T>(value: T): Result<bytes.Bytes, json.Error>",
        "fn encodeWithLimits<T>(value: T, limits: json.Limits): Result<bytes.Bytes, json.Error>"
    };
    MarimayFailAllocator state;
    MarimayMetadataFixture fixture;
    MarimayCompilerResult result = {0}, reversed = {0};
    const MarimaySourceSnapshot *snapshot;
    const MarimayCheckedIntrinsicCompletion *item = NULL;
    size_t index, count = 0, attempts;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_metadata_init(&fixture, mmay_fail_allocator(&state), 100));
    MMAY_EXPECT(mmay_metadata_check(&fixture, false, NULL, &result) == MMAY_COMPILER_OK);
    MMAY_EXPECT(!mmay_compiler_result_has_errors(&result));
    MMAY_EXPECT(mmay_metadata_check(&fixture, true, NULL, &reversed) == MMAY_COMPILER_OK);
    attempts = state.attempts;
    mmay_metadata_expect_scope(&fixture, &result, 2, names, 8);
    mmay_metadata_expect_scope(&fixture, &result, 3, named, 3);
    mmay_metadata_expect_scope(&fixture, &reversed, 2, names, 8);
    mmay_metadata_expect_scope(&fixture, &reversed, 3, named, 3);
    for (index = 4; index < MMAY_METADATA_FILE_COUNT; index += 1) {
        mmay_metadata_expect_scope(&fixture, &result, index, NULL, 0);
    }
    snapshot = mmay_metadata_snapshot(&result, 2);
    for (index = 0; index < 4; index += 1) {
        item = mmay_checked_intrinsic_completion(&result.checked, snapshot, "codec.", 6, index);
        MMAY_EXPECT(item != NULL);
        if (item == NULL) continue;
        MMAY_EXPECT(item->intrinsic->kind == (MarimayIntrinsicKind)index);
        MMAY_EXPECT(strcmp(item->intrinsic->signature, signatures[index]) == 0);
        MMAY_EXPECT(item->intrinsic->min_type_arguments == (index < 2 ? 1U : 0U));
        MMAY_EXPECT(item->intrinsic->argument_count == (index % 2 == 0 ? 1U : 2U));
    }
    MMAY_EXPECT(mmay_checked_intrinsic_completion_count(&result.checked, snapshot, "codec.d", 7) == 2);
    MMAY_EXPECT(mmay_checked_intrinsic_completion_count(&result.checked, snapshot, "decode", 6) == 0);
    MMAY_EXPECT(mmay_checked_intrinsic_completion_count(&result.checked, snapshot, "Codec", 5) == 0);
    MMAY_EXPECT(mmay_compiler_service_intrinsic_completion_count(&fixture.service,
        &result, snapshot, NULL, 1, &count) == MMAY_COMPILER_INVALID_ARGUMENT);
    MMAY_EXPECT(count == 0);
    MMAY_EXPECT(mmay_compiler_service_intrinsic_completion(&fixture.service,
        &result, snapshot, NULL, 1, 0, &item) == MMAY_COMPILER_INVALID_ARGUMENT);
    MMAY_EXPECT(item == NULL);
    MMAY_EXPECT(mmay_checked_intrinsic_completion(NULL, snapshot, NULL, 0, 0) == NULL);
    MMAY_EXPECT(result.checked.intrinsic_completions.length == 11);
    /* No invented JSON function declarations, source locations or dot candidates. */
    for (index = 0; index < result.checked.symbols.length; index += 1) {
        const MarimayCheckedSymbol *symbol = mmay_checked_symbol(&result.checked, index);
        if (strcmp((const char *)symbol->package_name.data, "std/json") == 0) {
            MMAY_EXPECT(symbol->kind == MMAY_CHECKED_SYMBOL_RECORD
                || symbol->kind == MMAY_CHECKED_SYMBOL_FIELD);
            if (symbol->kind == MMAY_CHECKED_SYMBOL_FIELD) {
                const MarimayCheckedSymbol *owner = mmay_checked_symbol_by_id(
                    &result.checked, symbol->owner_symbol);
                MMAY_EXPECT(owner != NULL && owner->kind == MMAY_CHECKED_SYMBOL_RECORD);
            }
        }
    }
    MMAY_EXPECT(result.checked.completions.length == 0);
    MMAY_EXPECT(state.attempts == attempts);
    mmay_compiler_result_dispose(&reversed);
    mmay_compiler_result_dispose(&result);
    mmay_compiler_service_dispose(&fixture.service);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

static bool mmay_metadata_cancel(void *context) {
    (void)context;
    return true;
}

static void mmay_metadata_overlays_and_errors(void) {
    static const char renamed[] = "import * as fresh from \"std/json\";\n";
    static const char absent[] = "fn unrelated(): int { return 0; }\n";
    static const char invalid[] = "import { decode as read } from \"std/json\"; fn broken(): int { return missing; }\n";
    static const char *const expected[] = {"fresh.decode", "fresh.decodeWithLimits", "fresh.encode", "fresh.encodeWithLimits"};
    MarimayFailAllocator state;
    MarimayMetadataFixture fixture;
    MarimayCompilerResult original = {0}, changed = {0}, removed = {0}, errors = {0}, cancelled = {0};
    MarimayCompilerCheckRequest request = {0};
    const MarimaySourceSnapshot *old_snapshot;
    const MarimayCheckedIntrinsicCompletion *item = NULL;
    size_t count = 10;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_metadata_init(&fixture, mmay_fail_allocator(&state), 100));
    MMAY_EXPECT(mmay_metadata_check(&fixture, false, NULL, &original) == MMAY_COMPILER_OK);
    old_snapshot = mmay_metadata_snapshot(&original, 2);
    MMAY_EXPECT(mmay_compiler_service_overlay(&fixture.service, fixture.files[2], 2,
        renamed, strlen(renamed)) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_intrinsic_completion_count(&fixture.service,
        &original, old_snapshot, NULL, 0, &count) == MMAY_COMPILER_STALE_RESULT);
    MMAY_EXPECT(count == 0);
    MMAY_EXPECT(mmay_compiler_service_intrinsic_completion(&fixture.service,
        &original, old_snapshot, NULL, 0, 0, &item) == MMAY_COMPILER_STALE_RESULT);
    MMAY_EXPECT(item == NULL);
    MMAY_EXPECT(mmay_checked_intrinsic_completion_count(&original.checked, old_snapshot, NULL, 0) == 8);
    MMAY_EXPECT(mmay_metadata_check(&fixture, false, NULL, &changed) == MMAY_COMPILER_OK);
    mmay_metadata_expect_scope(&fixture, &changed, 2, expected, 4);
    MMAY_EXPECT(mmay_compiler_service_intrinsic_completion(&fixture.service,
        &changed, old_snapshot, NULL, 0, 0, &item) == MMAY_COMPILER_STALE_RESULT);
    MMAY_EXPECT(item == NULL);
    MMAY_EXPECT(mmay_compiler_service_overlay(&fixture.service, fixture.files[2], 3,
        absent, strlen(absent)) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_metadata_check(&fixture, false, NULL, &removed) == MMAY_COMPILER_OK);
    mmay_metadata_expect_scope(&fixture, &removed, 2, NULL, 0);
    MMAY_EXPECT(mmay_compiler_service_overlay(&fixture.service, fixture.files[3], 2,
        invalid, strlen(invalid)) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_metadata_check(&fixture, false, NULL, &errors) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_result_has_errors(&errors));
    MMAY_EXPECT(errors.checked.intrinsic_completions.length == 0);
    request.cancel_poll = mmay_metadata_cancel;
    MMAY_EXPECT(mmay_metadata_check(&fixture, false, &request, &cancelled) == MMAY_COMPILER_CANCELLED);
    MMAY_EXPECT(!cancelled.initialized);
    mmay_compiler_result_dispose(&cancelled);
    mmay_compiler_result_dispose(&errors);
    mmay_compiler_result_dispose(&removed);
    mmay_compiler_result_dispose(&changed);
    mmay_compiler_result_dispose(&original);
    mmay_compiler_service_dispose(&fixture.service);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

static void mmay_metadata_limits(void) {
    static const char shared[] = "type Local {} fn owned(value: Local): int { return 0; }\n";
    size_t index;
    for (index = 0; index < 4; index += 1) {
        MarimayFailAllocator state;
        MarimayMetadataFixture fixture;
        MarimayCompilerResult result = {0};
        size_t maximum = index < 2 ? 10 + index : 13 + index;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_metadata_init(&fixture, mmay_fail_allocator(&state), maximum));
        if (index >= 2) MMAY_EXPECT(mmay_compiler_service_overlay(&fixture.service, fixture.files[5],
            2, shared, strlen(shared)) == MMAY_COMPILER_OK);
        MMAY_EXPECT(mmay_metadata_check(&fixture, false, NULL, &result)
            == (index % 2 == 0 ? MMAY_COMPILER_LIMIT_REACHED : MMAY_COMPILER_OK));
        if (index % 2 == 0) MMAY_EXPECT(!result.initialized);
        else MMAY_EXPECT(result.checked.intrinsic_completions.length == 11);
        mmay_compiler_result_dispose(&result);
        mmay_compiler_service_dispose(&fixture.service);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
}

static void mmay_metadata_allocation_failures(void) {
    size_t index, attempts = 0;
    for (index = 0; index == 0 || index <= attempts + 1; index += 1) {
        MarimayFailAllocator state;
        MarimayMetadataFixture fixture;
        MarimayCompilerResult result = {0};
        MarimayCompilerStatus status;
        size_t before;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_metadata_init(&fixture, mmay_fail_allocator(&state), 100));
        before = state.attempts;
        if (index != 0) state.failure_index = before + index - 1;
        status = mmay_metadata_check(&fixture, false, NULL, &result);
        if (index == 0) attempts = state.attempts - before;
        if (index == 0 || index == attempts + 1) {
            MMAY_EXPECT(status == MMAY_COMPILER_OK && !mmay_compiler_result_has_errors(&result));
            MMAY_EXPECT(result.checked.intrinsic_completions.length == 11);
        } else {
            MMAY_EXPECT(status == MMAY_COMPILER_ALLOCATION_FAILED && !result.initialized);
        }
        mmay_compiler_result_dispose(&result);
        mmay_compiler_service_dispose(&fixture.service);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        if (mmay_test_failure_count() != 0) break;
    }
    MMAY_EXPECT(attempts != 0);
}

void mmay_test_compiler_service_intrinsics(void) {
    mmay_metadata_imports_and_signatures();
    mmay_metadata_overlays_and_errors();
    mmay_metadata_limits();
    mmay_metadata_allocation_failures();
}
