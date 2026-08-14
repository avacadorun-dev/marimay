#include "test_support.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/check/service.h"

static RudaCompilerService ruda_test_compiler_service_create(
    RudaAllocator allocator,
    RudaCompilerOptions options
) {
    RudaCompilerService service;
    memset(&service, 0, sizeof(service));
    RUDA_EXPECT(ruda_compiler_service_init(&service, allocator, options)
        == RUDA_COMPILER_OK);
    return service;
}

static void ruda_test_compiler_service_multifile(void) {
    static const char source_a[] =
        "fn first(): i64 {\n"
        "    return 40;\n"
        "}\n";
    static const char source_b[] =
        "fn second(): i64 {\n"
        "    return first() + 2;\n"
        "}\n";
    RudaCompilerOptions options = ruda_compiler_default_options();
    RudaCompilerService service = ruda_test_compiler_service_create(
        ruda_system_allocator(),
        options
    );
    RudaCompilerResult result;
    RudaFileId file_a = 0;
    RudaFileId file_b = 0;
    const RudaCompilerUnit *unit = NULL;
    memset(&result, 0, sizeof(result));

    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service,
        "src/b.ruda",
        strlen("src/b.ruda"),
        7,
        source_b,
        strlen(source_b),
        &file_b
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service,
        "src/a.ruda",
        strlen("src/a.ruda"),
        3,
        source_a,
        strlen(source_a),
        &file_a
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(file_a != 0 && file_b != 0 && file_a != file_b);
    RUDA_EXPECT(ruda_compiler_service_check(&service, "preview", &result)
        == RUDA_COMPILER_OK);
    RUDA_EXPECT(result.initialized);
    RUDA_EXPECT(!ruda_compiler_result_has_errors(&result));
    RUDA_EXPECT(result.units.length == 2);
    RUDA_EXPECT(result.checked.initialized);
    RUDA_EXPECT(result.checked.functions.length == 2);
    unit = ruda_compiler_result_unit(&result, 0);
    RUDA_EXPECT(unit != NULL);
    if (unit != NULL) {
        RUDA_EXPECT(unit->file_id == file_a);
        RUDA_EXPECT(unit->version == 3);
        RUDA_EXPECT(strcmp(ruda_source_path(unit->snapshot), "src/a.ruda") == 0);
    }
    unit = ruda_compiler_result_unit(&result, 1);
    RUDA_EXPECT(unit != NULL);
    if (unit != NULL) {
        RUDA_EXPECT(unit->file_id == file_b);
        RUDA_EXPECT(unit->version == 7);
        RUDA_EXPECT(strcmp(ruda_source_path(unit->snapshot), "src/b.ruda") == 0);
    }

    ruda_compiler_result_dispose(&result);
    ruda_compiler_service_dispose(&service);
}

static void ruda_test_compiler_service_overlays(void) {
    static const char valid[] = "fn value(): i64 { return 1; }\n";
    static const char syntax_error[] = "fn value(: i64 { return 1; }\n";
    static const char type_error[] = "fn value(): i64 { return missing; }\n";
    RudaCompilerOptions options = ruda_compiler_default_options();
    RudaCompilerService service = ruda_test_compiler_service_create(
        ruda_system_allocator(),
        options
    );
    RudaCompilerResult old_result;
    RudaCompilerResult syntax_result;
    RudaCompilerResult type_result;
    RudaFileId file = 0;
    RudaCompilerDocument document;
    RudaSnapshotId initial_snapshot = 0;
    memset(&old_result, 0, sizeof(old_result));
    memset(&syntax_result, 0, sizeof(syntax_result));
    memset(&type_result, 0, sizeof(type_result));
    memset(&document, 0, sizeof(document));

    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service,
        "main.ruda",
        strlen("main.ruda"),
        10,
        valid,
        strlen(valid),
        &file
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_document(&service, file, &document));
    if (document.snapshot != NULL) {
        initial_snapshot = ruda_source_snapshot_id(document.snapshot);
    }
    RUDA_EXPECT(ruda_compiler_service_check(&service, "preview", &old_result)
        == RUDA_COMPILER_OK);
    RUDA_EXPECT(!ruda_compiler_result_has_errors(&old_result));

    RUDA_EXPECT(ruda_compiler_service_overlay(
        &service,
        file,
        10,
        syntax_error,
        strlen(syntax_error)
    ) == RUDA_COMPILER_STALE_VERSION);
    RUDA_EXPECT(ruda_compiler_service_document(&service, file, &document));
    RUDA_EXPECT(document.version == 10);
    if (document.snapshot != NULL) {
        RUDA_EXPECT(ruda_source_snapshot_id(document.snapshot) == initial_snapshot);
    }
    RUDA_EXPECT(ruda_compiler_service_overlay(
        &service,
        file,
        11,
        syntax_error,
        strlen(syntax_error)
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_check(&service, "preview", &syntax_result)
        == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_result_has_errors(&syntax_result));
    RUDA_EXPECT(syntax_result.has_frontend_errors);
    RUDA_EXPECT(!syntax_result.checked.initialized);
    RUDA_EXPECT(ruda_compiler_result_unit(&syntax_result, 0) != NULL);

    RUDA_EXPECT(ruda_compiler_service_overlay(
        &service,
        file,
        12,
        type_error,
        strlen(type_error)
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_check(&service, "preview", &type_result)
        == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_result_has_errors(&type_result));
    RUDA_EXPECT(!type_result.has_frontend_errors);
    RUDA_EXPECT(type_result.checked.initialized);
    RUDA_EXPECT(type_result.checked.diagnostics.length == 1);

    /* Prior snapshots/results stay readable after newer overlays. */
    RUDA_EXPECT(ruda_compiler_result_unit(&old_result, 0) != NULL);
    if (ruda_compiler_result_unit(&old_result, 0) != NULL) {
        RUDA_EXPECT(ruda_compiler_result_unit(&old_result, 0)->version == 10);
        RUDA_EXPECT(ruda_source_snapshot_id(
            ruda_compiler_result_unit(&old_result, 0)->snapshot
        ) == initial_snapshot);
    }

    ruda_compiler_result_dispose(&type_result);
    ruda_compiler_result_dispose(&syntax_result);
    ruda_compiler_result_dispose(&old_result);
    ruda_compiler_service_dispose(&service);
}

static void ruda_test_compiler_service_boundaries(void) {
    static const char source[] = "fn value(): i64 { return 1; }\n";
    RudaCompilerOptions options = ruda_compiler_default_options();
    RudaCompilerService service;
    RudaFileId file = 0;
    memset(&service, 0, sizeof(service));
    options.lex_options.max_tokens = 1;
    RUDA_EXPECT(ruda_compiler_service_init(
        &service,
        ruda_system_allocator(),
        options
    ) == RUDA_COMPILER_INVALID_ARGUMENT);
    options = ruda_compiler_default_options();
    options.parse_options.max_events = 1;
    RUDA_EXPECT(ruda_compiler_service_init(
        &service,
        ruda_system_allocator(),
        options
    ) == RUDA_COMPILER_INVALID_ARGUMENT);
    options = ruda_compiler_default_options();
    options.ast_options.max_nodes = 0;
    RUDA_EXPECT(ruda_compiler_service_init(
        &service,
        ruda_system_allocator(),
        options
    ) == RUDA_COMPILER_INVALID_ARGUMENT);
    options = ruda_compiler_default_options();
    options.check_options.max_functions = 0;
    RUDA_EXPECT(ruda_compiler_service_init(
        &service,
        ruda_system_allocator(),
        options
    ) == RUDA_COMPILER_INVALID_ARGUMENT);
    options = ruda_compiler_default_options();
    options.max_documents = 1;
    RUDA_EXPECT(ruda_compiler_service_init(
        &service,
        ruda_system_allocator(),
        options
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service,
        "a.ruda",
        strlen("a.ruda"),
        1,
        source,
        strlen(source),
        &file
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service,
        "a.ruda",
        strlen("a.ruda"),
        1,
        source,
        strlen(source),
        NULL
    ) == RUDA_COMPILER_LIMIT_REACHED);
    RUDA_EXPECT(ruda_compiler_service_overlay(
        &service,
        file + 99,
        2,
        source,
        strlen(source)
    ) == RUDA_COMPILER_UNKNOWN_DOCUMENT);
    ruda_compiler_service_dispose(&service);

    options = ruda_compiler_default_options();
    options.max_snapshots_per_document = 2;
    service = ruda_test_compiler_service_create(
        ruda_system_allocator(),
        options
    );
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service,
        "a.ruda",
        strlen("a.ruda"),
        1,
        source,
        strlen(source),
        NULL
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service,
        "a.ruda",
        strlen("a.ruda"),
        2,
        source,
        strlen(source),
        NULL
    ) == RUDA_COMPILER_DUPLICATE_PATH);
    RUDA_EXPECT(ruda_compiler_service_overlay(
        &service,
        1,
        2,
        source,
        strlen(source)
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_overlay(
        &service,
        1,
        3,
        source,
        strlen(source)
    ) == RUDA_COMPILER_LIMIT_REACHED);
    {
        RudaCompilerDocument document;
        RUDA_EXPECT(ruda_compiler_service_document(&service, 1, &document));
        RUDA_EXPECT(document.version == 2);
        RUDA_EXPECT(document.snapshot_count == 2);
    }
    ruda_compiler_service_dispose(&service);
}

static void ruda_test_compiler_service_allocation_failures(void) {
    static const char source[] = "fn value(): i64 { return 42; }\n";
    size_t relative_failure = 0;
    bool reached_success = false;
    for (relative_failure = 0; relative_failure < 256; relative_failure += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaCompilerOptions options = ruda_compiler_default_options();
        RudaCompilerService service;
        RudaCompilerResult result;
        RudaCompilerStatus status;
        memset(&service, 0, sizeof(service));
        memset(&result, 0, sizeof(result));
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_compiler_service_init(&service, allocator, options)
            == RUDA_COMPILER_OK);
        RUDA_EXPECT(ruda_compiler_service_add_document(
            &service,
            "main.ruda",
            strlen("main.ruda"),
            1,
            source,
            strlen(source),
            NULL
        ) == RUDA_COMPILER_OK);
        state.failure_index = state.attempts + relative_failure;
        status = ruda_compiler_service_check(&service, "preview", &result);
        if (status == RUDA_COMPILER_OK) {
            reached_success = true;
            RUDA_EXPECT(result.initialized);
            RUDA_EXPECT(!ruda_compiler_result_has_errors(&result));
        } else {
            RUDA_EXPECT(status == RUDA_COMPILER_ALLOCATION_FAILED);
            RUDA_EXPECT(!result.initialized);
        }
        ruda_compiler_result_dispose(&result);
        ruda_compiler_service_dispose(&service);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_compiler_service_aggregate_limits(void) {
    static const char source[] = "fn value(): i64 { return 42; }\n";
    RudaCompilerOptions options = ruda_compiler_default_options();
    RudaCompilerService service;
    RudaFileId first = 0;
    memset(&service, 0, sizeof(service));
    options.max_documents = 2;
    options.max_snapshots_per_document = 4;
    options.max_total_snapshots = 2;
    RUDA_EXPECT(ruda_compiler_service_init(
        &service,
        ruda_system_allocator(),
        options
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service,
        "a.ruda",
        strlen("a.ruda"),
        1,
        source,
        strlen(source),
        &first
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service,
        "b.ruda",
        strlen("b.ruda"),
        1,
        source,
        strlen(source),
        NULL
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(service.retained_snapshot_count == 2);
    RUDA_EXPECT(service.retained_source_bytes == strlen(source) * 2);
    RUDA_EXPECT(ruda_compiler_service_overlay(
        &service,
        first,
        2,
        source,
        strlen(source)
    ) == RUDA_COMPILER_LIMIT_REACHED);
    RUDA_EXPECT(service.retained_snapshot_count == 2);
    RUDA_EXPECT(service.retained_source_bytes == strlen(source) * 2);
    ruda_compiler_service_dispose(&service);

    options = ruda_compiler_default_options();
    options.max_total_source_bytes = strlen(source) + 1;
    RUDA_EXPECT(ruda_compiler_service_init(
        &service,
        ruda_system_allocator(),
        options
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service,
        "a.ruda",
        strlen("a.ruda"),
        1,
        source,
        strlen(source),
        &first
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_overlay(
        &service,
        first,
        2,
        source,
        strlen(source)
    ) == RUDA_COMPILER_LIMIT_REACHED);
    RUDA_EXPECT(service.retained_snapshot_count == 1);
    RUDA_EXPECT(service.retained_source_bytes == strlen(source));
    ruda_compiler_service_dispose(&service);
}

static void ruda_test_compiler_service_reinitialize(void) {
    static const char source[] = "fn value(): i64 { return 42; }\n";
    RudaFailAllocator state;
    RudaAllocator allocator;
    RudaCompilerOptions options = ruda_compiler_default_options();
    RudaCompilerOptions invalid = options;
    RudaCompilerService service;
    RudaCompilerDocument document;
    RudaFileId file = 0;
    memset(&service, 0, sizeof(service));
    memset(&document, 0, sizeof(document));
    ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
    allocator = ruda_fail_allocator(&state);
    RUDA_EXPECT(ruda_compiler_service_init(&service, allocator, options)
        == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service,
        "main.ruda",
        strlen("main.ruda"),
        1,
        source,
        strlen(source),
        &file
    ) == RUDA_COMPILER_OK);
    invalid.max_total_snapshots = 1;
    RUDA_EXPECT(ruda_compiler_service_init(&service, allocator, invalid)
        == RUDA_COMPILER_INVALID_ARGUMENT);
    RUDA_EXPECT(ruda_compiler_service_document(&service, file, &document));
    RUDA_EXPECT(document.version == 1);

    RUDA_EXPECT(ruda_compiler_service_init(&service, allocator, options)
        == RUDA_COMPILER_OK);
    RUDA_EXPECT(service.documents.length == 0);
    RUDA_EXPECT(service.retained_snapshot_count == 0);
    RUDA_EXPECT(service.retained_source_bytes == 0);
    ruda_compiler_service_dispose(&service);
    RUDA_EXPECT(state.live_allocations == 0);
    RUDA_EXPECT(state.live_bytes == 0);
}

static void ruda_test_compiler_service_document_failures(void) {
    static const char source[] = "fn value(): i64 { return 42; }\n";
    size_t failure_index = 0;
    bool reached_add_success = false;
    bool reached_overlay_success = false;
    for (failure_index = 0; failure_index < 64; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaCompilerOptions options = ruda_compiler_default_options();
        RudaCompilerService service;
        RudaCompilerStatus status;
        memset(&service, 0, sizeof(service));
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_compiler_service_init(&service, allocator, options)
            == RUDA_COMPILER_OK);
        status = ruda_compiler_service_add_document(
            &service,
            "main.ruda",
            strlen("main.ruda"),
            1,
            source,
            strlen(source),
            NULL
        );
        if (status == RUDA_COMPILER_OK) {
            reached_add_success = true;
        } else {
            RUDA_EXPECT(status == RUDA_COMPILER_ALLOCATION_FAILED);
            RUDA_EXPECT(service.documents.length == 0);
            RUDA_EXPECT(service.sources.files.length == 0);
        }
        ruda_compiler_service_dispose(&service);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_add_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_add_success);

    for (failure_index = 0; failure_index < 64; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaCompilerOptions options = ruda_compiler_default_options();
        RudaCompilerService service;
        RudaCompilerStatus status;
        RudaFileId file = 0;
        RudaCompilerDocument before;
        const RudaSourceSnapshot *before_snapshot = NULL;
        memset(&service, 0, sizeof(service));
        memset(&before, 0, sizeof(before));
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_compiler_service_init(&service, allocator, options)
            == RUDA_COMPILER_OK);
        RUDA_EXPECT(ruda_compiler_service_add_document(
            &service,
            "main.ruda",
            strlen("main.ruda"),
            1,
            source,
            strlen(source),
            &file
        ) == RUDA_COMPILER_OK);
        RUDA_EXPECT(ruda_compiler_service_document(&service, file, &before));
        before_snapshot = before.snapshot;
        state.failure_index = state.attempts + failure_index;
        status = ruda_compiler_service_overlay(
            &service,
            file,
            2,
            source,
            strlen(source)
        );
        if (status == RUDA_COMPILER_OK) {
            reached_overlay_success = true;
        } else {
            RudaCompilerDocument after;
            memset(&after, 0, sizeof(after));
            RUDA_EXPECT(ruda_compiler_service_document(
                &service,
                file,
                &after
            ));
            RUDA_EXPECT(status == RUDA_COMPILER_ALLOCATION_FAILED);
            RUDA_EXPECT(after.version == 1);
            RUDA_EXPECT(after.snapshot_count == 1);
            RUDA_EXPECT(after.snapshot == before_snapshot);
        }
        ruda_compiler_service_dispose(&service);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_overlay_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_overlay_success);
}

static void ruda_test_compiler_service_result_reuse(void) {
    static const char source[] = "fn value(): i64 { return 42; }\n";
    RudaFailAllocator state;
    RudaAllocator allocator;
    RudaCompilerOptions options = ruda_compiler_default_options();
    RudaCompilerService service;
    RudaCompilerResult result;
    RudaFileId file = 0;
    const RudaCompilerUnit *unit = NULL;
    memset(&service, 0, sizeof(service));
    memset(&result, 0, sizeof(result));
    ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
    allocator = ruda_fail_allocator(&state);
    RUDA_EXPECT(ruda_compiler_service_init(&service, allocator, options)
        == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service,
        "main.ruda",
        strlen("main.ruda"),
        1,
        source,
        strlen(source),
        &file
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_check(&service, "preview", &result)
        == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_overlay(
        &service,
        file,
        2,
        source,
        strlen(source)
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_check(&service, "preview", &result)
        == RUDA_COMPILER_OK);
    unit = ruda_compiler_result_unit(&result, 0);
    RUDA_EXPECT(unit != NULL && unit->version == 2);

    RUDA_EXPECT(ruda_compiler_service_check(&service, "", &result)
        == RUDA_COMPILER_INVALID_ARGUMENT);
    unit = ruda_compiler_result_unit(&result, 0);
    RUDA_EXPECT(unit != NULL && unit->version == 2);

    state.failure_index = state.attempts;
    RUDA_EXPECT(ruda_compiler_service_check(&service, "preview", &result)
        == RUDA_COMPILER_ALLOCATION_FAILED);
    unit = ruda_compiler_result_unit(&result, 0);
    RUDA_EXPECT(unit != NULL && unit->version == 2);

    ruda_compiler_result_dispose(&result);
    ruda_compiler_service_dispose(&service);
    RUDA_EXPECT(state.live_allocations == 0);
    RUDA_EXPECT(state.live_bytes == 0);
}

void ruda_test_compiler_service(void) {
    ruda_test_compiler_service_multifile();
    ruda_test_compiler_service_overlays();
    ruda_test_compiler_service_boundaries();
    ruda_test_compiler_service_aggregate_limits();
    ruda_test_compiler_service_reinitialize();
    ruda_test_compiler_service_allocation_failures();
    ruda_test_compiler_service_document_failures();
    ruda_test_compiler_service_result_reuse();
}
