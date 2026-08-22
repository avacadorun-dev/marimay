#include "test_support.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/check/service.h"

static MarimayCompilerService mmay_test_compiler_service_create(
    MarimayAllocator allocator,
    MarimayCompilerOptions options
) {
    MarimayCompilerService service;
    memset(&service, 0, sizeof(service));
    MMAY_EXPECT(mmay_compiler_service_init(&service, allocator, options)
        == MMAY_COMPILER_OK);
    return service;
}

static void mmay_test_compiler_service_multifile(void) {
    static const char source_a[] =
        "fn first(): i64 {\n"
        "    return 40;\n"
        "}\n";
    static const char source_b[] =
        "fn second(): i64 {\n"
        "    return first() + 2;\n"
        "}\n";
    MarimayCompilerOptions options = mmay_compiler_default_options();
    MarimayCompilerService service = mmay_test_compiler_service_create(
        mmay_system_allocator(),
        options
    );
    MarimayCompilerResult result;
    MarimayFileId file_a = 0;
    MarimayFileId file_b = 0;
    const MarimayCompilerUnit *unit = NULL;
    memset(&result, 0, sizeof(result));

    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service,
        "src/b.my",
        strlen("src/b.my"),
        7,
        source_b,
        strlen(source_b),
        &file_b
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service,
        "src/a.my",
        strlen("src/a.my"),
        3,
        source_a,
        strlen(source_a),
        &file_a
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(file_a != 0 && file_b != 0 && file_a != file_b);
    MMAY_EXPECT(mmay_compiler_service_check(&service, "preview", &result)
        == MMAY_COMPILER_OK);
    MMAY_EXPECT(result.initialized);
    MMAY_EXPECT(!mmay_compiler_result_has_errors(&result));
    MMAY_EXPECT(result.units.length == 2);
    MMAY_EXPECT(result.checked.initialized);
    MMAY_EXPECT(result.checked.functions.length == 2);
    unit = mmay_compiler_result_unit(&result, 0);
    MMAY_EXPECT(unit != NULL);
    if (unit != NULL) {
        MMAY_EXPECT(unit->file_id == file_a);
        MMAY_EXPECT(unit->version == 3);
        MMAY_EXPECT(strcmp(mmay_source_path(unit->snapshot), "src/a.my") == 0);
    }
    unit = mmay_compiler_result_unit(&result, 1);
    MMAY_EXPECT(unit != NULL);
    if (unit != NULL) {
        MMAY_EXPECT(unit->file_id == file_b);
        MMAY_EXPECT(unit->version == 7);
        MMAY_EXPECT(strcmp(mmay_source_path(unit->snapshot), "src/b.my") == 0);
    }

    mmay_compiler_result_dispose(&result);
    mmay_compiler_service_dispose(&service);
}

static void mmay_test_compiler_service_overlays(void) {
    static const char valid[] = "fn value(): i64 { return 1; }\n";
    static const char syntax_error[] = "fn value(: i64 { return 1; }\n";
    static const char type_error[] = "fn value(): i64 { return missing; }\n";
    MarimayCompilerOptions options = mmay_compiler_default_options();
    MarimayCompilerService service = mmay_test_compiler_service_create(
        mmay_system_allocator(),
        options
    );
    MarimayCompilerResult old_result;
    MarimayCompilerResult syntax_result;
    MarimayCompilerResult type_result;
    MarimayFileId file = 0;
    MarimayCompilerDocument document;
    MarimaySnapshotId initial_snapshot = 0;
    memset(&old_result, 0, sizeof(old_result));
    memset(&syntax_result, 0, sizeof(syntax_result));
    memset(&type_result, 0, sizeof(type_result));
    memset(&document, 0, sizeof(document));

    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service,
        "main.my",
        strlen("main.my"),
        10,
        valid,
        strlen(valid),
        &file
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_document(&service, file, &document));
    if (document.snapshot != NULL) {
        initial_snapshot = mmay_source_snapshot_id(document.snapshot);
    }
    MMAY_EXPECT(mmay_compiler_service_check(&service, "preview", &old_result)
        == MMAY_COMPILER_OK);
    MMAY_EXPECT(!mmay_compiler_result_has_errors(&old_result));

    MMAY_EXPECT(mmay_compiler_service_overlay(
        &service,
        file,
        10,
        syntax_error,
        strlen(syntax_error)
    ) == MMAY_COMPILER_STALE_VERSION);
    MMAY_EXPECT(mmay_compiler_service_document(&service, file, &document));
    MMAY_EXPECT(document.version == 10);
    if (document.snapshot != NULL) {
        MMAY_EXPECT(mmay_source_snapshot_id(document.snapshot) == initial_snapshot);
    }
    MMAY_EXPECT(mmay_compiler_service_overlay(
        &service,
        file,
        11,
        syntax_error,
        strlen(syntax_error)
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_check(&service, "preview", &syntax_result)
        == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_result_has_errors(&syntax_result));
    MMAY_EXPECT(syntax_result.has_frontend_errors);
    MMAY_EXPECT(!syntax_result.checked.initialized);
    MMAY_EXPECT(mmay_compiler_result_unit(&syntax_result, 0) != NULL);

    MMAY_EXPECT(mmay_compiler_service_overlay(
        &service,
        file,
        12,
        type_error,
        strlen(type_error)
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_check(&service, "preview", &type_result)
        == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_result_has_errors(&type_result));
    MMAY_EXPECT(!type_result.has_frontend_errors);
    MMAY_EXPECT(type_result.checked.initialized);
    MMAY_EXPECT(type_result.checked.diagnostics.length == 1);

    /* Prior snapshots/results stay readable after newer overlays. */
    MMAY_EXPECT(mmay_compiler_result_unit(&old_result, 0) != NULL);
    if (mmay_compiler_result_unit(&old_result, 0) != NULL) {
        MMAY_EXPECT(mmay_compiler_result_unit(&old_result, 0)->version == 10);
        MMAY_EXPECT(mmay_source_snapshot_id(
            mmay_compiler_result_unit(&old_result, 0)->snapshot
        ) == initial_snapshot);
    }

    mmay_compiler_result_dispose(&type_result);
    mmay_compiler_result_dispose(&syntax_result);
    mmay_compiler_result_dispose(&old_result);
    mmay_compiler_service_dispose(&service);
}

static void mmay_test_compiler_service_boundaries(void) {
    static const char source[] = "fn value(): i64 { return 1; }\n";
    MarimayCompilerOptions options = mmay_compiler_default_options();
    MarimayCompilerService service;
    MarimayFileId file = 0;
    memset(&service, 0, sizeof(service));
    options.lex_options.max_tokens = 1;
    MMAY_EXPECT(mmay_compiler_service_init(
        &service,
        mmay_system_allocator(),
        options
    ) == MMAY_COMPILER_INVALID_ARGUMENT);
    options = mmay_compiler_default_options();
    options.parse_options.max_events = 1;
    MMAY_EXPECT(mmay_compiler_service_init(
        &service,
        mmay_system_allocator(),
        options
    ) == MMAY_COMPILER_INVALID_ARGUMENT);
    options = mmay_compiler_default_options();
    options.ast_options.max_nodes = 0;
    MMAY_EXPECT(mmay_compiler_service_init(
        &service,
        mmay_system_allocator(),
        options
    ) == MMAY_COMPILER_INVALID_ARGUMENT);
    options = mmay_compiler_default_options();
    options.check_options.max_functions = 0;
    MMAY_EXPECT(mmay_compiler_service_init(
        &service,
        mmay_system_allocator(),
        options
    ) == MMAY_COMPILER_INVALID_ARGUMENT);
    options = mmay_compiler_default_options();
    options.max_documents = 1;
    MMAY_EXPECT(mmay_compiler_service_init(
        &service,
        mmay_system_allocator(),
        options
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service,
        "a.my",
        strlen("a.my"),
        1,
        source,
        strlen(source),
        &file
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service,
        "a.my",
        strlen("a.my"),
        1,
        source,
        strlen(source),
        NULL
    ) == MMAY_COMPILER_LIMIT_REACHED);
    MMAY_EXPECT(mmay_compiler_service_overlay(
        &service,
        file + 99,
        2,
        source,
        strlen(source)
    ) == MMAY_COMPILER_UNKNOWN_DOCUMENT);
    mmay_compiler_service_dispose(&service);

    options = mmay_compiler_default_options();
    options.max_snapshots_per_document = 2;
    service = mmay_test_compiler_service_create(
        mmay_system_allocator(),
        options
    );
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service,
        "a.my",
        strlen("a.my"),
        1,
        source,
        strlen(source),
        NULL
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service,
        "a.my",
        strlen("a.my"),
        2,
        source,
        strlen(source),
        NULL
    ) == MMAY_COMPILER_DUPLICATE_PATH);
    MMAY_EXPECT(mmay_compiler_service_overlay(
        &service,
        1,
        2,
        source,
        strlen(source)
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_overlay(
        &service,
        1,
        3,
        source,
        strlen(source)
    ) == MMAY_COMPILER_LIMIT_REACHED);
    {
        MarimayCompilerDocument document;
        MMAY_EXPECT(mmay_compiler_service_document(&service, 1, &document));
        MMAY_EXPECT(document.version == 2);
        MMAY_EXPECT(document.snapshot_count == 2);
    }
    mmay_compiler_service_dispose(&service);
}

static void mmay_test_compiler_service_allocation_failures(void) {
    static const char source[] = "fn value(): i64 { return 42; }\n";
    size_t relative_failure = 0;
    bool reached_success = false;
    for (relative_failure = 0; relative_failure < 256; relative_failure += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayCompilerOptions options = mmay_compiler_default_options();
        MarimayCompilerService service;
        MarimayCompilerResult result;
        MarimayCompilerStatus status;
        memset(&service, 0, sizeof(service));
        memset(&result, 0, sizeof(result));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_compiler_service_init(&service, allocator, options)
            == MMAY_COMPILER_OK);
        MMAY_EXPECT(mmay_compiler_service_add_document(
            &service,
            "main.my",
            strlen("main.my"),
            1,
            source,
            strlen(source),
            NULL
        ) == MMAY_COMPILER_OK);
        state.failure_index = state.attempts + relative_failure;
        status = mmay_compiler_service_check(&service, "preview", &result);
        if (status == MMAY_COMPILER_OK) {
            reached_success = true;
            MMAY_EXPECT(result.initialized);
            MMAY_EXPECT(!mmay_compiler_result_has_errors(&result));
        } else {
            MMAY_EXPECT(status == MMAY_COMPILER_ALLOCATION_FAILED);
            MMAY_EXPECT(!result.initialized);
        }
        mmay_compiler_result_dispose(&result);
        mmay_compiler_service_dispose(&service);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_compiler_service_aggregate_limits(void) {
    static const char source[] = "fn value(): i64 { return 42; }\n";
    MarimayCompilerOptions options = mmay_compiler_default_options();
    MarimayCompilerService service;
    MarimayFileId first = 0;
    memset(&service, 0, sizeof(service));
    options.max_documents = 2;
    options.max_snapshots_per_document = 4;
    options.max_total_snapshots = 2;
    MMAY_EXPECT(mmay_compiler_service_init(
        &service,
        mmay_system_allocator(),
        options
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service,
        "a.my",
        strlen("a.my"),
        1,
        source,
        strlen(source),
        &first
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service,
        "b.my",
        strlen("b.my"),
        1,
        source,
        strlen(source),
        NULL
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(service.retained_snapshot_count == 2);
    MMAY_EXPECT(service.retained_source_bytes == strlen(source) * 2);
    MMAY_EXPECT(mmay_compiler_service_overlay(
        &service,
        first,
        2,
        source,
        strlen(source)
    ) == MMAY_COMPILER_LIMIT_REACHED);
    MMAY_EXPECT(service.retained_snapshot_count == 2);
    MMAY_EXPECT(service.retained_source_bytes == strlen(source) * 2);
    mmay_compiler_service_dispose(&service);

    options = mmay_compiler_default_options();
    options.max_total_source_bytes = strlen(source) + 1;
    MMAY_EXPECT(mmay_compiler_service_init(
        &service,
        mmay_system_allocator(),
        options
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service,
        "a.my",
        strlen("a.my"),
        1,
        source,
        strlen(source),
        &first
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_overlay(
        &service,
        first,
        2,
        source,
        strlen(source)
    ) == MMAY_COMPILER_LIMIT_REACHED);
    MMAY_EXPECT(service.retained_snapshot_count == 1);
    MMAY_EXPECT(service.retained_source_bytes == strlen(source));
    mmay_compiler_service_dispose(&service);
}

static void mmay_test_compiler_service_reinitialize(void) {
    static const char source[] = "fn value(): i64 { return 42; }\n";
    MarimayFailAllocator state;
    MarimayAllocator allocator;
    MarimayCompilerOptions options = mmay_compiler_default_options();
    MarimayCompilerOptions invalid = options;
    MarimayCompilerService service;
    MarimayCompilerDocument document;
    MarimayFileId file = 0;
    memset(&service, 0, sizeof(service));
    memset(&document, 0, sizeof(document));
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    allocator = mmay_fail_allocator(&state);
    MMAY_EXPECT(mmay_compiler_service_init(&service, allocator, options)
        == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service,
        "main.my",
        strlen("main.my"),
        1,
        source,
        strlen(source),
        &file
    ) == MMAY_COMPILER_OK);
    invalid.max_total_snapshots = 1;
    MMAY_EXPECT(mmay_compiler_service_init(&service, allocator, invalid)
        == MMAY_COMPILER_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_compiler_service_document(&service, file, &document));
    MMAY_EXPECT(document.version == 1);

    MMAY_EXPECT(mmay_compiler_service_init(&service, allocator, options)
        == MMAY_COMPILER_OK);
    MMAY_EXPECT(service.documents.length == 0);
    MMAY_EXPECT(service.retained_snapshot_count == 0);
    MMAY_EXPECT(service.retained_source_bytes == 0);
    mmay_compiler_service_dispose(&service);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
}

static void mmay_test_compiler_service_document_failures(void) {
    static const char source[] = "fn value(): i64 { return 42; }\n";
    size_t failure_index = 0;
    bool reached_add_success = false;
    bool reached_overlay_success = false;
    for (failure_index = 0; failure_index < 64; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayCompilerOptions options = mmay_compiler_default_options();
        MarimayCompilerService service;
        MarimayCompilerStatus status;
        memset(&service, 0, sizeof(service));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_compiler_service_init(&service, allocator, options)
            == MMAY_COMPILER_OK);
        status = mmay_compiler_service_add_document(
            &service,
            "main.my",
            strlen("main.my"),
            1,
            source,
            strlen(source),
            NULL
        );
        if (status == MMAY_COMPILER_OK) {
            reached_add_success = true;
        } else {
            MMAY_EXPECT(status == MMAY_COMPILER_ALLOCATION_FAILED);
            MMAY_EXPECT(service.documents.length == 0);
            MMAY_EXPECT(service.sources.files.length == 0);
        }
        mmay_compiler_service_dispose(&service);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_add_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_add_success);

    for (failure_index = 0; failure_index < 64; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayCompilerOptions options = mmay_compiler_default_options();
        MarimayCompilerService service;
        MarimayCompilerStatus status;
        MarimayFileId file = 0;
        MarimayCompilerDocument before;
        const MarimaySourceSnapshot *before_snapshot = NULL;
        memset(&service, 0, sizeof(service));
        memset(&before, 0, sizeof(before));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_compiler_service_init(&service, allocator, options)
            == MMAY_COMPILER_OK);
        MMAY_EXPECT(mmay_compiler_service_add_document(
            &service,
            "main.my",
            strlen("main.my"),
            1,
            source,
            strlen(source),
            &file
        ) == MMAY_COMPILER_OK);
        MMAY_EXPECT(mmay_compiler_service_document(&service, file, &before));
        before_snapshot = before.snapshot;
        state.failure_index = state.attempts + failure_index;
        status = mmay_compiler_service_overlay(
            &service,
            file,
            2,
            source,
            strlen(source)
        );
        if (status == MMAY_COMPILER_OK) {
            reached_overlay_success = true;
        } else {
            MarimayCompilerDocument after;
            memset(&after, 0, sizeof(after));
            MMAY_EXPECT(mmay_compiler_service_document(
                &service,
                file,
                &after
            ));
            MMAY_EXPECT(status == MMAY_COMPILER_ALLOCATION_FAILED);
            MMAY_EXPECT(after.version == 1);
            MMAY_EXPECT(after.snapshot_count == 1);
            MMAY_EXPECT(after.snapshot == before_snapshot);
        }
        mmay_compiler_service_dispose(&service);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_overlay_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_overlay_success);
}

static void mmay_test_compiler_service_result_reuse(void) {
    static const char source[] = "fn value(): i64 { return 42; }\n";
    MarimayFailAllocator state;
    MarimayAllocator allocator;
    MarimayCompilerOptions options = mmay_compiler_default_options();
    MarimayCompilerService service;
    MarimayCompilerResult result;
    MarimayFileId file = 0;
    const MarimayCompilerUnit *unit = NULL;
    memset(&service, 0, sizeof(service));
    memset(&result, 0, sizeof(result));
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    allocator = mmay_fail_allocator(&state);
    MMAY_EXPECT(mmay_compiler_service_init(&service, allocator, options)
        == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service,
        "main.my",
        strlen("main.my"),
        1,
        source,
        strlen(source),
        &file
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_check(&service, "preview", &result)
        == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_overlay(
        &service,
        file,
        2,
        source,
        strlen(source)
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_check(&service, "preview", &result)
        == MMAY_COMPILER_OK);
    unit = mmay_compiler_result_unit(&result, 0);
    MMAY_EXPECT(unit != NULL && unit->version == 2);

    MMAY_EXPECT(mmay_compiler_service_check(&service, "", &result)
        == MMAY_COMPILER_INVALID_ARGUMENT);
    unit = mmay_compiler_result_unit(&result, 0);
    MMAY_EXPECT(unit != NULL && unit->version == 2);

    state.failure_index = state.attempts;
    MMAY_EXPECT(mmay_compiler_service_check(&service, "preview", &result)
        == MMAY_COMPILER_ALLOCATION_FAILED);
    unit = mmay_compiler_result_unit(&result, 0);
    MMAY_EXPECT(unit != NULL && unit->version == 2);

    mmay_compiler_result_dispose(&result);
    mmay_compiler_service_dispose(&service);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
}

void mmay_test_compiler_service(void) {
    mmay_test_compiler_service_multifile();
    mmay_test_compiler_service_overlays();
    mmay_test_compiler_service_boundaries();
    mmay_test_compiler_service_aggregate_limits();
    mmay_test_compiler_service_reinitialize();
    mmay_test_compiler_service_allocation_failures();
    mmay_test_compiler_service_document_failures();
    mmay_test_compiler_service_result_reuse();
}
