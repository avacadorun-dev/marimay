#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/check/service.h"
#include "ruda/syntax/syntax.h"

typedef struct RudaTestCancelState {
    size_t polls;
    size_t cancel_at;
} RudaTestCancelState;

static bool ruda_test_cancel_poll(void *context) {
    RudaTestCancelState *state = context;
    if (state == NULL) {
        return false;
    }
    state->polls += 1;
    return state->polls >= state->cancel_at;
}

static RudaCompilerService ruda_test_incremental_service(
    RudaAllocator allocator
) {
    RudaCompilerService service;
    memset(&service, 0, sizeof(service));
    RUDA_EXPECT(ruda_compiler_service_init(
        &service,
        allocator,
        ruda_compiler_default_options()
    ) == RUDA_COMPILER_OK);
    return service;
}

static const RudaGreenNode *ruda_test_direct_node(
    const RudaGreenNode *root,
    RudaSyntaxKind kind,
    size_t occurrence
) {
    size_t index = 0;
    size_t found = 0;
    for (index = 0; root != NULL && index < root->child_count; index += 1) {
        const RudaSyntaxElement *element = ruda_syntax_child(root, index);
        if (element != NULL && element->kind == RUDA_SYNTAX_ELEMENT_NODE
            && element->as.node != NULL && element->as.node->kind == kind) {
            if (found == occurrence) {
                return element->as.node;
            }
            found += 1;
        }
    }
    return NULL;
}

static bool ruda_test_function_i64(
    const RudaCheckResult *checked,
    size_t index,
    int64_t expected
) {
    RudaMirValue value;
    memset(&value, 0, sizeof(value));
    return ruda_check_interpret_function(
            checked,
            index,
            NULL,
            0,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &value
        ) == RUDA_MIR_INTERPRET_OK
        && value.type == RUDA_TYPE_I64 && value.as.i64 == expected;
}

static void ruda_test_incremental_same_and_shifted(void) {
    static const char initial[] =
        "fn first(): i64 { return 1; }\n"
        "fn middle(): i64 { return 2; }\n"
        "fn last(): i64 { return 3; }\n";
    static const char same_length[] =
        "fn first(): i64 { return 1; }\n"
        "fn middle(): i64 { return 4; }\n"
        "fn last(): i64 { return 3; }\n";
    static const char shifted[] =
        "fn first(): i64 { return 1; }\n"
        "fn middle(): i64 { return 400; }\n"
        "fn last(): i64 { return 3; }\n";
    RudaCompilerService service = ruda_test_incremental_service(
        ruda_system_allocator()
    );
    RudaCompilerResult result;
    RudaCompilerResult fresh;
    RudaCompilerCheckRequest request;
    RudaCompilerCheckStats stats;
    RudaCompilerEdit edit;
    RudaFileId file = 0;
    const RudaCompilerUnit *unit = NULL;
    const RudaGreenNode *first = NULL;
    const RudaGreenNode *middle = NULL;
    const RudaGreenNode *last = NULL;
    size_t offset = (size_t)(strstr(initial, "return 2") - initial) + 7;
    memset(&result, 0, sizeof(result));
    memset(&fresh, 0, sizeof(fresh));
    memset(&request, 0, sizeof(request));
    memset(&stats, 0, sizeof(stats));
    memset(&edit, 0, sizeof(edit));

    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service,
        "main.ruda",
        strlen("main.ruda"),
        1,
        initial,
        strlen(initial),
        &file
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_check(&service, "preview", &result)
        == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_result_is_current(&service, &result));
    unit = ruda_compiler_result_unit(&result, 0);
    RUDA_EXPECT(unit != NULL);
    if (unit == NULL) {
        ruda_compiler_result_dispose(&result);
        ruda_compiler_service_dispose(&service);
        return;
    }
    first = ruda_test_direct_node(
        unit->parsed.tree.root,
        RUDA_SYNTAX_FUNCTION_DECL,
        0
    );
    middle = ruda_test_direct_node(
        unit->parsed.tree.root,
        RUDA_SYNTAX_FUNCTION_DECL,
        1
    );
    last = ruda_test_direct_node(
        unit->parsed.tree.root,
        RUDA_SYNTAX_FUNCTION_DECL,
        2
    );
    RUDA_EXPECT(first != NULL && middle != NULL && last != NULL);

    edit.base_version = 1;
    edit.start = offset;
    edit.previous_end = offset + 1;
    edit.new_end = offset + 1;
    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service,
        file,
        2,
        same_length,
        strlen(same_length),
        edit
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(!ruda_compiler_result_is_current(&service, &result));
    request.previous = &result;
    request.stats = &stats;
    RUDA_EXPECT(ruda_compiler_service_check_request(
        &service,
        "preview",
        &request,
        &result
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(stats.incremental_parse_count == 1);
    RUDA_EXPECT(stats.full_parse_count == 0);
    RUDA_EXPECT(stats.reused_top_level_nodes == 2);
    RUDA_EXPECT(ruda_compiler_result_is_current(&service, &result));
    unit = ruda_compiler_result_unit(&result, 0);
    RUDA_EXPECT(unit != NULL);
    if (unit != NULL) {
        RUDA_EXPECT(ruda_test_direct_node(
            unit->parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            0
        ) == first);
        RUDA_EXPECT(ruda_test_direct_node(
            unit->parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            1
        ) != middle);
        RUDA_EXPECT(ruda_test_direct_node(
            unit->parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            2
        ) == last);
    }
    RUDA_EXPECT(ruda_test_function_i64(&result.checked, 1, 4));
    RUDA_EXPECT(ruda_compiler_service_check(&service, "preview", &fresh)
        == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_test_function_i64(&fresh.checked, 1, 4));
    RUDA_EXPECT(fresh.checked.diagnostics.length
        == result.checked.diagnostics.length);
    ruda_compiler_result_dispose(&fresh);

    unit = ruda_compiler_result_unit(&result, 0);
    first = unit != NULL ? ruda_test_direct_node(
        unit->parsed.tree.root,
        RUDA_SYNTAX_FUNCTION_DECL,
        0
    ) : NULL;
    last = unit != NULL ? ruda_test_direct_node(
        unit->parsed.tree.root,
        RUDA_SYNTAX_FUNCTION_DECL,
        2
    ) : NULL;
    offset = (size_t)(strstr(same_length, "return 4") - same_length) + 7;
    edit.base_version = 2;
    edit.start = offset;
    edit.previous_end = offset + 1;
    edit.new_end = offset + 3;
    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service,
        file,
        3,
        shifted,
        strlen(shifted),
        edit
    ) == RUDA_COMPILER_OK);
    memset(&stats, 0, sizeof(stats));
    request.previous = &result;
    request.stats = &stats;
    RUDA_EXPECT(ruda_compiler_service_check_request(
        &service,
        "preview",
        &request,
        &result
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(stats.incremental_parse_count == 1);
    RUDA_EXPECT(stats.reused_top_level_nodes == 1);
    unit = ruda_compiler_result_unit(&result, 0);
    RUDA_EXPECT(unit != NULL);
    if (unit != NULL) {
        RUDA_EXPECT(ruda_test_direct_node(
            unit->parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            0
        ) == first);
        RUDA_EXPECT(ruda_test_direct_node(
            unit->parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            2
        ) != last);
    }
    RUDA_EXPECT(ruda_test_function_i64(&result.checked, 1, 400));

    ruda_compiler_result_dispose(&result);
    ruda_compiler_service_dispose(&service);
}

static void ruda_test_incremental_edit_validation(void) {
    static const char initial[] = "fn value(): i64 { return 1; }\n";
    static const char changed[] = "fn value(): i64 { return 2; }\n";
    static const char wrong_prefix[] = "Fn value(): i64 { return 2; }\n";
    static const char wrong_suffix[] = "fn value(): i64 { return 2; } ";
    RudaCompilerService service = ruda_test_incremental_service(
        ruda_system_allocator()
    );
    RudaCompilerEdit edit;
    RudaCompilerDocument document;
    RudaFileId file = 0;
    uint64_t generation = 0;
    size_t offset = (size_t)(strstr(initial, "return 1") - initial) + 7;
    memset(&edit, 0, sizeof(edit));
    memset(&document, 0, sizeof(document));
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service,
        "main.ruda",
        strlen("main.ruda"),
        4,
        initial,
        strlen(initial),
        &file
    ) == RUDA_COMPILER_OK);
    generation = service.generation;
    edit.base_version = 3;
    edit.start = offset;
    edit.previous_end = offset + 1;
    edit.new_end = offset + 1;
    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service, file, 5, changed, strlen(changed), edit
    ) == RUDA_COMPILER_STALE_VERSION);
    RUDA_EXPECT(service.generation == generation);

    edit.base_version = 4;
    edit.start = offset + 1;
    edit.previous_end = offset;
    edit.new_end = offset + 1;
    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service, file, 5, changed, strlen(changed), edit
    ) == RUDA_COMPILER_INVALID_ARGUMENT);
    RUDA_EXPECT(service.generation == generation);

    edit.previous_end = offset + 1;
    edit.new_end = offset;
    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service, file, 5, changed, strlen(changed), edit
    ) == RUDA_COMPILER_INVALID_ARGUMENT);
    RUDA_EXPECT(service.generation == generation);

    edit.start = offset;
    edit.new_end = strlen(changed) + 1;
    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service, file, 5, changed, strlen(changed), edit
    ) == RUDA_COMPILER_INVALID_ARGUMENT);
    RUDA_EXPECT(service.generation == generation);

    edit.new_end = offset + 1;
    edit.previous_end = strlen(initial) + 1;
    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service, file, 5, changed, strlen(changed), edit
    ) == RUDA_COMPILER_INVALID_ARGUMENT);
    RUDA_EXPECT(service.generation == generation);

    edit.previous_end = offset + 1;
    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service, file, 4, changed, strlen(changed), edit
    ) == RUDA_COMPILER_STALE_VERSION);
    RUDA_EXPECT(service.generation == generation);

    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service, file, 5, wrong_suffix, strlen(wrong_suffix), edit
    ) == RUDA_COMPILER_INVALID_ARGUMENT);
    RUDA_EXPECT(service.generation == generation);

    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service, file, 5, wrong_prefix, strlen(wrong_prefix), edit
    ) == RUDA_COMPILER_INVALID_ARGUMENT);
    RUDA_EXPECT(service.generation == generation);
    RUDA_EXPECT(ruda_compiler_service_document(&service, file, &document));
    RUDA_EXPECT(document.version == 4 && !document.has_edit);

    RUDA_EXPECT(ruda_compiler_service_overlay(
        &service, file, 5, changed, strlen(changed)
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_document(&service, file, &document));
    RUDA_EXPECT(document.version == 5 && !document.has_edit);
    RUDA_EXPECT(document.previous_snapshot == NULL);
    ruda_compiler_service_dispose(&service);
}

static void ruda_test_result_currentness_is_service_scoped(void) {
    static const char source[] = "fn value(): i64 { return 1; }\n";
    RudaCompilerService first = ruda_test_incremental_service(
        ruda_system_allocator()
    );
    RudaCompilerService second = ruda_test_incremental_service(
        ruda_system_allocator()
    );
    RudaCompilerResult result;
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &first, "main.ruda", 9, 1, source, strlen(source), NULL
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &second, "main.ruda", 9, 1, source, strlen(source), NULL
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(first.generation == second.generation);
    RUDA_EXPECT(ruda_compiler_service_check(&first, "preview", &result)
        == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_result_is_current(&first, &result));
    RUDA_EXPECT(!ruda_compiler_result_is_current(&second, &result));
    ruda_compiler_result_dispose(&result);
    ruda_compiler_service_dispose(&second);
    ruda_compiler_service_dispose(&first);
}

static void ruda_test_incremental_chain_falls_back_to_full_parse(void) {
    static const char first[] = "fn value(): i64 { return 1; }\n";
    static const char second[] = "fn value(): i64 { return 2; }\n";
    static const char third[] = "fn value(): i64 { return 3; }\n";
    RudaCompilerService service = ruda_test_incremental_service(
        ruda_system_allocator()
    );
    RudaCompilerResult result;
    RudaCompilerCheckRequest request;
    RudaCompilerCheckStats stats;
    RudaCompilerEdit edit;
    RudaFileId file = 0;
    size_t offset = (size_t)(strstr(first, "return 1") - first) + 7;
    memset(&result, 0, sizeof(result));
    memset(&request, 0, sizeof(request));
    memset(&stats, 0, sizeof(stats));
    memset(&edit, 0, sizeof(edit));
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service, "main.ruda", 9, 1, first, strlen(first), &file
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_check(&service, "preview", &result)
        == RUDA_COMPILER_OK);
    edit.base_version = 1;
    edit.start = offset;
    edit.previous_end = offset + 1;
    edit.new_end = offset + 1;
    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service, file, 2, second, strlen(second), edit
    ) == RUDA_COMPILER_OK);
    edit.base_version = 2;
    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service, file, 3, third, strlen(third), edit
    ) == RUDA_COMPILER_OK);
    request.previous = &result;
    request.stats = &stats;
    RUDA_EXPECT(ruda_compiler_service_check_request(
        &service, "preview", &request, &result
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(stats.full_parse_count == 1);
    RUDA_EXPECT(stats.incremental_parse_count == 0);
    RUDA_EXPECT(ruda_test_function_i64(&result.checked, 0, 3));
    ruda_compiler_result_dispose(&result);
    ruda_compiler_service_dispose(&service);
}

static void ruda_test_incremental_distinct_result_lifetimes(void) {
    static const char initial[] = "fn value(): i64 { return 1; }\n";
    static const char changed[] = "fn value(): i64 { return 2; }\n";
    RudaCompilerService service = ruda_test_incremental_service(
        ruda_system_allocator()
    );
    RudaCompilerResult previous;
    RudaCompilerResult current;
    RudaCompilerCheckRequest request;
    RudaCompilerCheckStats stats;
    RudaCompilerEdit edit;
    RudaFileId file = 0;
    size_t offset = (size_t)(strstr(initial, "return 1") - initial) + 7;
    memset(&previous, 0, sizeof(previous));
    memset(&current, 0, sizeof(current));
    memset(&request, 0, sizeof(request));
    memset(&stats, 0, sizeof(stats));
    memset(&edit, 0, sizeof(edit));
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service, "main.ruda", 9, 1, initial, strlen(initial), &file
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_check(&service, "preview", &previous)
        == RUDA_COMPILER_OK);
    edit.base_version = 1;
    edit.start = offset;
    edit.previous_end = offset + 1;
    edit.new_end = offset + 1;
    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service, file, 2, changed, strlen(changed), edit
    ) == RUDA_COMPILER_OK);
    request.previous = &previous;
    request.stats = &stats;
    RUDA_EXPECT(ruda_compiler_service_check_request(
        &service, "preview", &request, &current
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(stats.incremental_parse_count == 1);
    RUDA_EXPECT(ruda_test_function_i64(&previous.checked, 0, 1));
    RUDA_EXPECT(ruda_test_function_i64(&current.checked, 0, 2));
    ruda_compiler_result_dispose(&previous);
    RUDA_EXPECT(ruda_test_function_i64(&current.checked, 0, 2));
    ruda_compiler_result_dispose(&current);
    ruda_compiler_service_dispose(&service);
}

static void ruda_test_overlay_edit_failure_atomicity(void) {
    static const char initial[] = "fn value(): i64 { return 1; }\n";
    static const char changed[] = "fn value(): i64 { return 2; }\n";
    RudaFailAllocator allocator_state;
    RudaAllocator allocator;
    RudaCompilerService service;
    RudaCompilerDocument before;
    RudaCompilerDocument after;
    RudaCompilerEdit edit;
    RudaFileId file = 0;
    size_t snapshot_count = 0;
    size_t source_bytes = 0;
    uint64_t generation = 0;
    size_t offset = (size_t)(strstr(initial, "return 1") - initial) + 7;
    memset(&before, 0, sizeof(before));
    memset(&after, 0, sizeof(after));
    memset(&edit, 0, sizeof(edit));
    ruda_fail_allocator_init(
        &allocator_state,
        ruda_system_allocator(),
        SIZE_MAX
    );
    allocator = ruda_fail_allocator(&allocator_state);
    service = ruda_test_incremental_service(allocator);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service, "main.ruda", 9, 1, initial, strlen(initial), &file
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_document(&service, file, &before));
    snapshot_count = service.retained_snapshot_count;
    source_bytes = service.retained_source_bytes;
    generation = service.generation;
    edit.base_version = 1;
    edit.start = offset;
    edit.previous_end = offset + 1;
    edit.new_end = offset + 1;
    allocator_state.failure_index = allocator_state.attempts;
    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service, file, 2, changed, strlen(changed), edit
    ) == RUDA_COMPILER_ALLOCATION_FAILED);
    RUDA_EXPECT(ruda_compiler_service_document(&service, file, &after));
    RUDA_EXPECT(after.version == before.version);
    RUDA_EXPECT(after.snapshot == before.snapshot);
    RUDA_EXPECT(after.previous_snapshot == before.previous_snapshot);
    RUDA_EXPECT(after.has_edit == before.has_edit);
    RUDA_EXPECT(service.retained_snapshot_count == snapshot_count);
    RUDA_EXPECT(service.retained_source_bytes == source_bytes);
    RUDA_EXPECT(service.generation == generation);
    allocator_state.failure_index = SIZE_MAX;
    ruda_compiler_service_dispose(&service);
    RUDA_EXPECT(allocator_state.live_allocations == 0);
    RUDA_EXPECT(allocator_state.live_bytes == 0);

    {
        RudaCompilerOptions options = ruda_compiler_default_options();
        RudaCompilerService limited;
        memset(&limited, 0, sizeof(limited));
        options.max_snapshots_per_document = 1;
        RUDA_EXPECT(ruda_compiler_service_init(
            &limited,
            ruda_system_allocator(),
            options
        ) == RUDA_COMPILER_OK);
        RUDA_EXPECT(ruda_compiler_service_add_document(
            &limited, "main.ruda", 9, 1, initial, strlen(initial), &file
        ) == RUDA_COMPILER_OK);
        RUDA_EXPECT(ruda_compiler_service_document(&limited, file, &before));
        snapshot_count = limited.retained_snapshot_count;
        source_bytes = limited.retained_source_bytes;
        generation = limited.generation;
        RUDA_EXPECT(ruda_compiler_service_overlay_edit(
            &limited, file, 2, changed, strlen(changed), edit
        ) == RUDA_COMPILER_LIMIT_REACHED);
        RUDA_EXPECT(ruda_compiler_service_document(&limited, file, &after));
        RUDA_EXPECT(after.version == before.version);
        RUDA_EXPECT(after.snapshot == before.snapshot);
        RUDA_EXPECT(after.previous_snapshot == before.previous_snapshot);
        RUDA_EXPECT(after.has_edit == before.has_edit);
        RUDA_EXPECT(limited.retained_snapshot_count == snapshot_count);
        RUDA_EXPECT(limited.retained_source_bytes == source_bytes);
        RUDA_EXPECT(limited.generation == generation);
        ruda_compiler_service_dispose(&limited);
    }
}

static void ruda_test_incremental_insert_and_delete(void) {
    static const char initial[] = "fn value(): i64 { return 1; }\n";
    static const char inserted[] = "fn value(): i64 { return 10; }\n";
    RudaCompilerService service = ruda_test_incremental_service(
        ruda_system_allocator()
    );
    RudaCompilerResult result;
    RudaCompilerCheckRequest request;
    RudaCompilerCheckStats stats;
    RudaCompilerEdit edit;
    RudaFileId file = 0;
    size_t offset = (size_t)(strstr(initial, "return 1") - initial) + 8;
    memset(&result, 0, sizeof(result));
    memset(&request, 0, sizeof(request));
    memset(&stats, 0, sizeof(stats));
    memset(&edit, 0, sizeof(edit));
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service, "main.ruda", 9, 1, initial, strlen(initial), &file
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_check(&service, "preview", &result)
        == RUDA_COMPILER_OK);
    request.previous = &result;
    request.stats = &stats;
    edit.base_version = 1;
    edit.start = offset;
    edit.previous_end = offset;
    edit.new_end = offset + 1;
    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service, file, 2, inserted, strlen(inserted), edit
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_check_request(
        &service, "preview", &request, &result
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(stats.incremental_parse_count == 1);
    RUDA_EXPECT(ruda_test_function_i64(&result.checked, 0, 10));

    memset(&stats, 0, sizeof(stats));
    edit.base_version = 2;
    edit.previous_end = offset + 1;
    edit.new_end = offset;
    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service, file, 3, initial, strlen(initial), edit
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_check_request(
        &service, "preview", &request, &result
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(stats.incremental_parse_count == 1);
    RUDA_EXPECT(ruda_test_function_i64(&result.checked, 0, 1));
    ruda_compiler_result_dispose(&result);
    ruda_compiler_service_dispose(&service);
}

static void ruda_test_incremental_package_recheck(void) {
    static const char first[] = "fn first(): i64 { return 40; }\n";
    static const char changed[] = "fn other(): i64 { return 40; }\n";
    static const char consumer[] =
        "fn second(): i64 { return first() + 2; }\n";
    RudaCompilerService service = ruda_test_incremental_service(
        ruda_system_allocator()
    );
    RudaCompilerResult result;
    RudaCompilerResult fresh;
    RudaCompilerCheckRequest request;
    RudaCompilerCheckStats stats;
    RudaCompilerEdit edit;
    RudaFileId file = 0;
    const RudaCheckDiagnostic *diagnostic = NULL;
    const RudaCheckDiagnostic *fresh_diagnostic = NULL;
    const char *old_start = strstr(first, "first");
    const char *old_end = old_start + strlen("first");
    const char *new_start = strstr(changed, "other");
    const char *new_end = new_start + strlen("other");
    memset(&result, 0, sizeof(result));
    memset(&fresh, 0, sizeof(fresh));
    memset(&request, 0, sizeof(request));
    memset(&stats, 0, sizeof(stats));
    memset(&edit, 0, sizeof(edit));
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service, "a.ruda", 6, 1, first, strlen(first), &file
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service, "b.ruda", 6, 1, consumer, strlen(consumer), NULL
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_check(&service, "preview", &result)
        == RUDA_COMPILER_OK);
    RUDA_EXPECT(!ruda_compiler_result_has_errors(&result));
    edit.base_version = 1;
    edit.start = (size_t)(old_start - first);
    edit.previous_end = (size_t)(old_end - first);
    edit.new_end = (size_t)(new_end - changed);
    RUDA_EXPECT(edit.start == (size_t)(new_start - changed));
    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service, file, 2, changed, strlen(changed), edit
    ) == RUDA_COMPILER_OK);
    request.previous = &result;
    request.stats = &stats;
    RUDA_EXPECT(ruda_compiler_service_check_request(
        &service, "preview", &request, &result
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(stats.incremental_parse_count == 1);
    RUDA_EXPECT(stats.full_parse_count == 1);
    RUDA_EXPECT(ruda_compiler_result_has_errors(&result));
    RUDA_EXPECT(result.checked.diagnostics.length == 1);
    diagnostic = ruda_check_diagnostic(&result.checked, 0);
    RUDA_EXPECT(diagnostic != NULL && diagnostic->snapshot != NULL);
    if (diagnostic != NULL && diagnostic->snapshot != NULL) {
        RUDA_EXPECT(strcmp(
            ruda_source_path(diagnostic->snapshot),
            "b.ruda"
        ) == 0);
    }
    RUDA_EXPECT(ruda_compiler_service_check(&service, "preview", &fresh)
        == RUDA_COMPILER_OK);
    RUDA_EXPECT(fresh.checked.diagnostics.length
        == result.checked.diagnostics.length);
    fresh_diagnostic = ruda_check_diagnostic(&fresh.checked, 0);
    RUDA_EXPECT(diagnostic != NULL && fresh_diagnostic != NULL);
    if (diagnostic != NULL && fresh_diagnostic != NULL) {
        RUDA_EXPECT(strcmp(diagnostic->code, fresh_diagnostic->code) == 0);
        RUDA_EXPECT(strcmp(diagnostic->message, fresh_diagnostic->message) == 0);
        RUDA_EXPECT(diagnostic->snapshot == fresh_diagnostic->snapshot);
        RUDA_EXPECT(diagnostic->start == fresh_diagnostic->start);
        RUDA_EXPECT(diagnostic->end == fresh_diagnostic->end);
    }
    ruda_compiler_result_dispose(&fresh);
    ruda_compiler_result_dispose(&result);
    ruda_compiler_service_dispose(&service);
}

static void ruda_test_compiler_service_cancellation(void) {
    static const char first[] = "fn first(): i64 { return 40; }\n";
    static const char changed[] = "fn first(): i64 { return 41; }\n";
    static const char second[] =
        "fn second(): i64 { return first() + 2; }\n";
    RudaFailAllocator allocator_state;
    RudaAllocator allocator;
    RudaCompilerService service;
    RudaCompilerResult result;
    RudaCompilerCheckRequest request;
    RudaCompilerCheckStats stats;
    RudaCompilerCheckStats unchanged_stats;
    RudaCompilerEdit edit;
    RudaTestCancelState cancel;
    RudaFileId file = 0;
    const RudaSourceSnapshot *snapshot = NULL;
    size_t cancel_at = 0;
    bool reached_success = false;
    memset(&result, 0, sizeof(result));
    memset(&request, 0, sizeof(request));
    memset(&edit, 0, sizeof(edit));
    ruda_fail_allocator_init(
        &allocator_state,
        ruda_system_allocator(),
        SIZE_MAX
    );
    allocator = ruda_fail_allocator(&allocator_state);
    service = ruda_test_incremental_service(allocator);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service, "a.ruda", 6, 1, first, strlen(first), &file
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service, "b.ruda", 6, 1, second, strlen(second), NULL
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_check(&service, "preview", &result)
        == RUDA_COMPILER_OK);
    if (ruda_compiler_result_unit(&result, 0) != NULL) {
        snapshot = ruda_compiler_result_unit(&result, 0)->snapshot;
    }
    edit.base_version = 1;
    edit.start = (size_t)(strstr(first, "return 40") - first) + 7;
    edit.previous_end = edit.start + 2;
    edit.new_end = edit.start + 2;
    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service, file, 2, changed, strlen(changed), edit
    ) == RUDA_COMPILER_OK);
    request.previous = &result;
    request.cancel_poll = ruda_test_cancel_poll;
    request.cancel_context = &cancel;
    request.stats = &stats;
    memset(&unchanged_stats, 0x5a, sizeof(unchanged_stats));
    for (cancel_at = 1; cancel_at < 32; cancel_at += 1) {
        RudaCompilerStatus status;
        memset(&cancel, 0, sizeof(cancel));
        cancel.cancel_at = cancel_at;
        memset(&stats, 0x5a, sizeof(stats));
        status = ruda_compiler_service_check_request(
            &service,
            "preview",
            &request,
            &result
        );
        if (status == RUDA_COMPILER_OK) {
            reached_success = true;
            RUDA_EXPECT(stats.incremental_parse_count == 1);
            RUDA_EXPECT(stats.full_parse_count == 1);
            RUDA_EXPECT(ruda_test_function_i64(&result.checked, 0, 41));
            break;
        }
        RUDA_EXPECT(status == RUDA_COMPILER_CANCELLED);
        RUDA_EXPECT(ruda_compiler_result_unit(&result, 0) != NULL);
        if (ruda_compiler_result_unit(&result, 0) != NULL) {
            RUDA_EXPECT(ruda_compiler_result_unit(&result, 0)->snapshot
                == snapshot);
        }
        RUDA_EXPECT(ruda_test_function_i64(&result.checked, 0, 40));
        RUDA_EXPECT(memcmp(
            &stats,
            &unchanged_stats,
            sizeof(stats)
        ) == 0);
    }
    RUDA_EXPECT(reached_success);
    ruda_compiler_result_dispose(&result);
    ruda_compiler_service_dispose(&service);
    RUDA_EXPECT(allocator_state.live_allocations == 0);
    RUDA_EXPECT(allocator_state.live_bytes == 0);
}

static void ruda_test_incremental_allocation_failures(void) {
    static const char initial[] = "fn value(): i64 { return 1; }\n";
    static const char changed[] = "fn value(): i64 { return 2; }\n";
    RudaFailAllocator allocator_state;
    RudaAllocator allocator;
    RudaCompilerService service;
    RudaCompilerResult result;
    RudaCompilerCheckRequest request;
    RudaCompilerCheckStats stats;
    RudaCompilerCheckStats unchanged_stats;
    RudaCompilerEdit edit;
    RudaFileId file = 0;
    size_t relative_failure = 0;
    size_t offset = (size_t)(strstr(initial, "return 1") - initial) + 7;
    bool reached_success = false;
    memset(&result, 0, sizeof(result));
    memset(&request, 0, sizeof(request));
    memset(&edit, 0, sizeof(edit));
    ruda_fail_allocator_init(
        &allocator_state,
        ruda_system_allocator(),
        SIZE_MAX
    );
    allocator = ruda_fail_allocator(&allocator_state);
    service = ruda_test_incremental_service(allocator);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service, "main.ruda", 9, 1, initial, strlen(initial), &file
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_check(&service, "preview", &result)
        == RUDA_COMPILER_OK);
    edit.base_version = 1;
    edit.start = offset;
    edit.previous_end = offset + 1;
    edit.new_end = offset + 1;
    RUDA_EXPECT(ruda_compiler_service_overlay_edit(
        &service, file, 2, changed, strlen(changed), edit
    ) == RUDA_COMPILER_OK);
    request.previous = &result;
    request.stats = &stats;
    memset(&unchanged_stats, 0xa5, sizeof(unchanged_stats));
    for (relative_failure = 0; relative_failure < 512;
        relative_failure += 1) {
        RudaCompilerStatus status;
        memset(&stats, 0xa5, sizeof(stats));
        allocator_state.failure_index = allocator_state.attempts
            + relative_failure;
        status = ruda_compiler_service_check_request(
            &service,
            "preview",
            &request,
            &result
        );
        if (status == RUDA_COMPILER_OK) {
            const RudaCompilerUnit *current = ruda_compiler_result_unit(
                &result,
                0
            );
            reached_success = true;
            RUDA_EXPECT(stats.incremental_parse_count == 1);
            RUDA_EXPECT(current != NULL && current->version == 2);
            break;
        }
        RUDA_EXPECT(status == RUDA_COMPILER_ALLOCATION_FAILED);
        RUDA_EXPECT(memcmp(
            &stats,
            &unchanged_stats,
            sizeof(stats)
        ) == 0);
        RUDA_EXPECT(ruda_compiler_result_unit(&result, 0) != NULL);
        if (ruda_compiler_result_unit(&result, 0) != NULL) {
            RUDA_EXPECT(ruda_compiler_result_unit(&result, 0)->version == 1);
        }
        RUDA_EXPECT(!ruda_compiler_result_is_current(&service, &result));
    }
    RUDA_EXPECT(reached_success);
    ruda_compiler_result_dispose(&result);
    ruda_compiler_service_dispose(&service);
    RUDA_EXPECT(allocator_state.live_allocations == 0);
    RUDA_EXPECT(allocator_state.live_bytes == 0);
}

void ruda_test_compiler_service_incremental(void) {
    ruda_test_incremental_same_and_shifted();
    ruda_test_incremental_edit_validation();
    ruda_test_result_currentness_is_service_scoped();
    ruda_test_incremental_chain_falls_back_to_full_parse();
    ruda_test_incremental_distinct_result_lifetimes();
    ruda_test_overlay_edit_failure_atomicity();
    ruda_test_incremental_insert_and_delete();
    ruda_test_incremental_package_recheck();
    ruda_test_compiler_service_cancellation();
    ruda_test_incremental_allocation_failures();
}
