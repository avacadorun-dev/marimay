#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/check/service.h"
#include "mmay/syntax/syntax.h"

typedef struct MarimayTestCancelState {
    size_t polls;
    size_t cancel_at;
} MarimayTestCancelState;

static bool mmay_test_cancel_poll(void *context) {
    MarimayTestCancelState *state = context;
    if (state == NULL) {
        return false;
    }
    state->polls += 1;
    return state->polls >= state->cancel_at;
}

static MarimayCompilerService mmay_test_incremental_service(
    MarimayAllocator allocator
) {
    MarimayCompilerService service;
    memset(&service, 0, sizeof(service));
    MMAY_EXPECT(mmay_compiler_service_init(
        &service,
        allocator,
        mmay_compiler_default_options()
    ) == MMAY_COMPILER_OK);
    return service;
}

static const MarimayGreenNode *mmay_test_direct_node(
    const MarimayGreenNode *root,
    MarimaySyntaxKind kind,
    size_t occurrence
) {
    size_t index = 0;
    size_t found = 0;
    for (index = 0; root != NULL && index < root->child_count; index += 1) {
        const MarimaySyntaxElement *element = mmay_syntax_child(root, index);
        if (element != NULL && element->kind == MMAY_SYNTAX_ELEMENT_NODE
            && element->as.node != NULL && element->as.node->kind == kind) {
            if (found == occurrence) {
                return element->as.node;
            }
            found += 1;
        }
    }
    return NULL;
}

static bool mmay_test_function_i64(
    const MarimayCheckResult *checked,
    size_t index,
    int64_t expected
) {
    MarimayMirValue value;
    memset(&value, 0, sizeof(value));
    return mmay_check_interpret_function(
            checked,
            index,
            NULL,
            0,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &value
        ) == MMAY_MIR_INTERPRET_OK
        && value.type == MMAY_TYPE_I64 && value.as.i64 == expected;
}

static void mmay_test_incremental_same_and_shifted(void) {
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
    MarimayCompilerService service = mmay_test_incremental_service(
        mmay_system_allocator()
    );
    MarimayCompilerResult result;
    MarimayCompilerResult fresh;
    MarimayCompilerCheckRequest request;
    MarimayCompilerCheckStats stats;
    MarimayCompilerEdit edit;
    MarimayFileId file = 0;
    const MarimayCompilerUnit *unit = NULL;
    const MarimayGreenNode *first = NULL;
    const MarimayGreenNode *middle = NULL;
    const MarimayGreenNode *last = NULL;
    MarimaySymbolId middle_symbol;
    size_t offset = (size_t)(strstr(initial, "return 2") - initial) + 7;
    memset(&result, 0, sizeof(result));
    memset(&fresh, 0, sizeof(fresh));
    memset(&request, 0, sizeof(request));
    memset(&stats, 0, sizeof(stats));
    memset(&edit, 0, sizeof(edit));
    memset(&middle_symbol, 0, sizeof(middle_symbol));

    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service,
        "main.my",
        strlen("main.my"),
        1,
        initial,
        strlen(initial),
        &file
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_check(&service, "preview", &result)
        == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_result_is_current(&service, &result));
    unit = mmay_compiler_result_unit(&result, 0);
    MMAY_EXPECT(unit != NULL);
    if (unit == NULL) {
        mmay_compiler_result_dispose(&result);
        mmay_compiler_service_dispose(&service);
        return;
    }
    first = mmay_test_direct_node(
        unit->parsed.tree.root,
        MMAY_SYNTAX_FUNCTION_DECL,
        0
    );
    middle = mmay_test_direct_node(
        unit->parsed.tree.root,
        MMAY_SYNTAX_FUNCTION_DECL,
        1
    );
    last = mmay_test_direct_node(
        unit->parsed.tree.root,
        MMAY_SYNTAX_FUNCTION_DECL,
        2
    );
    MMAY_EXPECT(first != NULL && middle != NULL && last != NULL);
    MMAY_EXPECT(mmay_checked_symbol(&result.checked, 1) != NULL);
    if (mmay_checked_symbol(&result.checked, 1) != NULL) {
        middle_symbol = mmay_checked_symbol(&result.checked, 1)->symbol;
    }

    edit.base_version = 1;
    edit.start = offset;
    edit.previous_end = offset + 1;
    edit.new_end = offset + 1;
    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service,
        file,
        2,
        same_length,
        strlen(same_length),
        edit
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(!mmay_compiler_result_is_current(&service, &result));
    request.previous = &result;
    request.stats = &stats;
    MMAY_EXPECT(mmay_compiler_service_check_request(
        &service,
        "preview",
        &request,
        &result
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(stats.incremental_parse_count == 1);
    MMAY_EXPECT(stats.full_parse_count == 0);
    MMAY_EXPECT(stats.reused_top_level_nodes == 2);
    MMAY_EXPECT(mmay_compiler_result_is_current(&service, &result));
    unit = mmay_compiler_result_unit(&result, 0);
    MMAY_EXPECT(mmay_checked_symbol(&result.checked, 1) != NULL);
    if (unit != NULL && mmay_checked_symbol(&result.checked, 1) != NULL) {
        MMAY_EXPECT(mmay_symbol_id_equal(
            mmay_checked_symbol(&result.checked, 1)->symbol,
            middle_symbol
        ));
        MMAY_EXPECT(mmay_checked_symbol(&result.checked, 1)->snapshot
            == unit->snapshot);
    }
    MMAY_EXPECT(unit != NULL);
    if (unit != NULL) {
        MMAY_EXPECT(mmay_test_direct_node(
            unit->parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL,
            0
        ) == first);
        MMAY_EXPECT(mmay_test_direct_node(
            unit->parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL,
            1
        ) != middle);
        MMAY_EXPECT(mmay_test_direct_node(
            unit->parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL,
            2
        ) == last);
    }
    MMAY_EXPECT(mmay_test_function_i64(&result.checked, 1, 4));
    MMAY_EXPECT(mmay_compiler_service_check(&service, "preview", &fresh)
        == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_test_function_i64(&fresh.checked, 1, 4));
    MMAY_EXPECT(fresh.checked.diagnostics.length
        == result.checked.diagnostics.length);
    mmay_compiler_result_dispose(&fresh);

    unit = mmay_compiler_result_unit(&result, 0);
    first = unit != NULL ? mmay_test_direct_node(
        unit->parsed.tree.root,
        MMAY_SYNTAX_FUNCTION_DECL,
        0
    ) : NULL;
    last = unit != NULL ? mmay_test_direct_node(
        unit->parsed.tree.root,
        MMAY_SYNTAX_FUNCTION_DECL,
        2
    ) : NULL;
    offset = (size_t)(strstr(same_length, "return 4") - same_length) + 7;
    edit.base_version = 2;
    edit.start = offset;
    edit.previous_end = offset + 1;
    edit.new_end = offset + 3;
    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service,
        file,
        3,
        shifted,
        strlen(shifted),
        edit
    ) == MMAY_COMPILER_OK);
    memset(&stats, 0, sizeof(stats));
    request.previous = &result;
    request.stats = &stats;
    MMAY_EXPECT(mmay_compiler_service_check_request(
        &service,
        "preview",
        &request,
        &result
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(stats.incremental_parse_count == 1);
    MMAY_EXPECT(stats.reused_top_level_nodes == 1);
    unit = mmay_compiler_result_unit(&result, 0);
    MMAY_EXPECT(unit != NULL);
    if (unit != NULL) {
        MMAY_EXPECT(mmay_test_direct_node(
            unit->parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL,
            0
        ) == first);
        MMAY_EXPECT(mmay_test_direct_node(
            unit->parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL,
            2
        ) != last);
    }
    MMAY_EXPECT(mmay_test_function_i64(&result.checked, 1, 400));

    mmay_compiler_result_dispose(&result);
    mmay_compiler_service_dispose(&service);
}

static void mmay_test_incremental_edit_validation(void) {
    static const char initial[] = "fn value(): i64 { return 1; }\n";
    static const char changed[] = "fn value(): i64 { return 2; }\n";
    static const char wrong_prefix[] = "Fn value(): i64 { return 2; }\n";
    static const char wrong_suffix[] = "fn value(): i64 { return 2; } ";
    MarimayCompilerService service = mmay_test_incremental_service(
        mmay_system_allocator()
    );
    MarimayCompilerEdit edit;
    MarimayCompilerDocument document;
    MarimayFileId file = 0;
    uint64_t generation = 0;
    size_t offset = (size_t)(strstr(initial, "return 1") - initial) + 7;
    memset(&edit, 0, sizeof(edit));
    memset(&document, 0, sizeof(document));
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service,
        "main.my",
        strlen("main.my"),
        4,
        initial,
        strlen(initial),
        &file
    ) == MMAY_COMPILER_OK);
    generation = service.generation;
    edit.base_version = 3;
    edit.start = offset;
    edit.previous_end = offset + 1;
    edit.new_end = offset + 1;
    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service, file, 5, changed, strlen(changed), edit
    ) == MMAY_COMPILER_STALE_VERSION);
    MMAY_EXPECT(service.generation == generation);

    edit.base_version = 4;
    edit.start = offset + 1;
    edit.previous_end = offset;
    edit.new_end = offset + 1;
    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service, file, 5, changed, strlen(changed), edit
    ) == MMAY_COMPILER_INVALID_ARGUMENT);
    MMAY_EXPECT(service.generation == generation);

    edit.previous_end = offset + 1;
    edit.new_end = offset;
    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service, file, 5, changed, strlen(changed), edit
    ) == MMAY_COMPILER_INVALID_ARGUMENT);
    MMAY_EXPECT(service.generation == generation);

    edit.start = offset;
    edit.new_end = strlen(changed) + 1;
    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service, file, 5, changed, strlen(changed), edit
    ) == MMAY_COMPILER_INVALID_ARGUMENT);
    MMAY_EXPECT(service.generation == generation);

    edit.new_end = offset + 1;
    edit.previous_end = strlen(initial) + 1;
    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service, file, 5, changed, strlen(changed), edit
    ) == MMAY_COMPILER_INVALID_ARGUMENT);
    MMAY_EXPECT(service.generation == generation);

    edit.previous_end = offset + 1;
    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service, file, 4, changed, strlen(changed), edit
    ) == MMAY_COMPILER_STALE_VERSION);
    MMAY_EXPECT(service.generation == generation);

    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service, file, 5, wrong_suffix, strlen(wrong_suffix), edit
    ) == MMAY_COMPILER_INVALID_ARGUMENT);
    MMAY_EXPECT(service.generation == generation);

    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service, file, 5, wrong_prefix, strlen(wrong_prefix), edit
    ) == MMAY_COMPILER_INVALID_ARGUMENT);
    MMAY_EXPECT(service.generation == generation);
    MMAY_EXPECT(mmay_compiler_service_document(&service, file, &document));
    MMAY_EXPECT(document.version == 4 && !document.has_edit);

    MMAY_EXPECT(mmay_compiler_service_overlay(
        &service, file, 5, changed, strlen(changed)
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_document(&service, file, &document));
    MMAY_EXPECT(document.version == 5 && !document.has_edit);
    MMAY_EXPECT(document.previous_snapshot == NULL);
    mmay_compiler_service_dispose(&service);
}

static void mmay_test_result_currentness_is_service_scoped(void) {
    static const char source[] = "fn value(): i64 { return 1; }\n";
    MarimayCompilerService first = mmay_test_incremental_service(
        mmay_system_allocator()
    );
    MarimayCompilerService second = mmay_test_incremental_service(
        mmay_system_allocator()
    );
    MarimayCompilerResult result;
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &first, "main.my", sizeof("main.my") - 1, 1, source, strlen(source), NULL
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &second, "main.my", sizeof("main.my") - 1, 1, source, strlen(source), NULL
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(first.generation == second.generation);
    MMAY_EXPECT(mmay_compiler_service_check(&first, "preview", &result)
        == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_result_is_current(&first, &result));
    MMAY_EXPECT(!mmay_compiler_result_is_current(&second, &result));
    mmay_compiler_result_dispose(&result);
    mmay_compiler_service_dispose(&second);
    mmay_compiler_service_dispose(&first);
}

static void mmay_test_incremental_chain_falls_back_to_full_parse(void) {
    static const char first[] = "fn value(): i64 { return 1; }\n";
    static const char second[] = "fn value(): i64 { return 2; }\n";
    static const char third[] = "fn value(): i64 { return 3; }\n";
    MarimayCompilerService service = mmay_test_incremental_service(
        mmay_system_allocator()
    );
    MarimayCompilerResult result;
    MarimayCompilerCheckRequest request;
    MarimayCompilerCheckStats stats;
    MarimayCompilerEdit edit;
    MarimayFileId file = 0;
    size_t offset = (size_t)(strstr(first, "return 1") - first) + 7;
    memset(&result, 0, sizeof(result));
    memset(&request, 0, sizeof(request));
    memset(&stats, 0, sizeof(stats));
    memset(&edit, 0, sizeof(edit));
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service, "main.my", sizeof("main.my") - 1, 1, first, strlen(first), &file
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_check(&service, "preview", &result)
        == MMAY_COMPILER_OK);
    edit.base_version = 1;
    edit.start = offset;
    edit.previous_end = offset + 1;
    edit.new_end = offset + 1;
    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service, file, 2, second, strlen(second), edit
    ) == MMAY_COMPILER_OK);
    edit.base_version = 2;
    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service, file, 3, third, strlen(third), edit
    ) == MMAY_COMPILER_OK);
    request.previous = &result;
    request.stats = &stats;
    MMAY_EXPECT(mmay_compiler_service_check_request(
        &service, "preview", &request, &result
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(stats.full_parse_count == 1);
    MMAY_EXPECT(stats.incremental_parse_count == 0);
    MMAY_EXPECT(mmay_test_function_i64(&result.checked, 0, 3));
    mmay_compiler_result_dispose(&result);
    mmay_compiler_service_dispose(&service);
}

static void mmay_test_incremental_distinct_result_lifetimes(void) {
    static const char initial[] = "fn value(): i64 { return 1; }\n";
    static const char changed[] = "fn value(): i64 { return 2; }\n";
    MarimayCompilerService service = mmay_test_incremental_service(
        mmay_system_allocator()
    );
    MarimayCompilerResult previous;
    MarimayCompilerResult current;
    MarimayCompilerCheckRequest request;
    MarimayCompilerCheckStats stats;
    MarimayCompilerEdit edit;
    MarimayFileId file = 0;
    size_t offset = (size_t)(strstr(initial, "return 1") - initial) + 7;
    memset(&previous, 0, sizeof(previous));
    memset(&current, 0, sizeof(current));
    memset(&request, 0, sizeof(request));
    memset(&stats, 0, sizeof(stats));
    memset(&edit, 0, sizeof(edit));
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service, "main.my", sizeof("main.my") - 1, 1, initial, strlen(initial), &file
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_check(&service, "preview", &previous)
        == MMAY_COMPILER_OK);
    edit.base_version = 1;
    edit.start = offset;
    edit.previous_end = offset + 1;
    edit.new_end = offset + 1;
    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service, file, 2, changed, strlen(changed), edit
    ) == MMAY_COMPILER_OK);
    request.previous = &previous;
    request.stats = &stats;
    MMAY_EXPECT(mmay_compiler_service_check_request(
        &service, "preview", &request, &current
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(stats.incremental_parse_count == 1);
    MMAY_EXPECT(mmay_test_function_i64(&previous.checked, 0, 1));
    MMAY_EXPECT(mmay_test_function_i64(&current.checked, 0, 2));
    mmay_compiler_result_dispose(&previous);
    MMAY_EXPECT(mmay_test_function_i64(&current.checked, 0, 2));
    mmay_compiler_result_dispose(&current);
    mmay_compiler_service_dispose(&service);
}

static void mmay_test_overlay_edit_failure_atomicity(void) {
    static const char initial[] = "fn value(): i64 { return 1; }\n";
    static const char changed[] = "fn value(): i64 { return 2; }\n";
    MarimayFailAllocator allocator_state;
    MarimayAllocator allocator;
    MarimayCompilerService service;
    MarimayCompilerDocument before;
    MarimayCompilerDocument after;
    MarimayCompilerEdit edit;
    MarimayFileId file = 0;
    size_t snapshot_count = 0;
    size_t source_bytes = 0;
    uint64_t generation = 0;
    size_t offset = (size_t)(strstr(initial, "return 1") - initial) + 7;
    memset(&before, 0, sizeof(before));
    memset(&after, 0, sizeof(after));
    memset(&edit, 0, sizeof(edit));
    mmay_fail_allocator_init(
        &allocator_state,
        mmay_system_allocator(),
        SIZE_MAX
    );
    allocator = mmay_fail_allocator(&allocator_state);
    service = mmay_test_incremental_service(allocator);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service, "main.my", sizeof("main.my") - 1, 1, initial, strlen(initial), &file
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_document(&service, file, &before));
    snapshot_count = service.retained_snapshot_count;
    source_bytes = service.retained_source_bytes;
    generation = service.generation;
    edit.base_version = 1;
    edit.start = offset;
    edit.previous_end = offset + 1;
    edit.new_end = offset + 1;
    allocator_state.failure_index = allocator_state.attempts;
    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service, file, 2, changed, strlen(changed), edit
    ) == MMAY_COMPILER_ALLOCATION_FAILED);
    MMAY_EXPECT(mmay_compiler_service_document(&service, file, &after));
    MMAY_EXPECT(after.version == before.version);
    MMAY_EXPECT(after.snapshot == before.snapshot);
    MMAY_EXPECT(after.previous_snapshot == before.previous_snapshot);
    MMAY_EXPECT(after.has_edit == before.has_edit);
    MMAY_EXPECT(service.retained_snapshot_count == snapshot_count);
    MMAY_EXPECT(service.retained_source_bytes == source_bytes);
    MMAY_EXPECT(service.generation == generation);
    allocator_state.failure_index = SIZE_MAX;
    mmay_compiler_service_dispose(&service);
    MMAY_EXPECT(allocator_state.live_allocations == 0);
    MMAY_EXPECT(allocator_state.live_bytes == 0);

    {
        MarimayCompilerOptions options = mmay_compiler_default_options();
        MarimayCompilerService limited;
        memset(&limited, 0, sizeof(limited));
        options.max_snapshots_per_document = 1;
        MMAY_EXPECT(mmay_compiler_service_init(
            &limited,
            mmay_system_allocator(),
            options
        ) == MMAY_COMPILER_OK);
        MMAY_EXPECT(mmay_compiler_service_add_document(
            &limited, "main.my", sizeof("main.my") - 1, 1,
            initial, strlen(initial), &file
        ) == MMAY_COMPILER_OK);
        MMAY_EXPECT(mmay_compiler_service_document(&limited, file, &before));
        snapshot_count = limited.retained_snapshot_count;
        source_bytes = limited.retained_source_bytes;
        generation = limited.generation;
        MMAY_EXPECT(mmay_compiler_service_overlay_edit(
            &limited, file, 2, changed, strlen(changed), edit
        ) == MMAY_COMPILER_LIMIT_REACHED);
        MMAY_EXPECT(mmay_compiler_service_document(&limited, file, &after));
        MMAY_EXPECT(after.version == before.version);
        MMAY_EXPECT(after.snapshot == before.snapshot);
        MMAY_EXPECT(after.previous_snapshot == before.previous_snapshot);
        MMAY_EXPECT(after.has_edit == before.has_edit);
        MMAY_EXPECT(limited.retained_snapshot_count == snapshot_count);
        MMAY_EXPECT(limited.retained_source_bytes == source_bytes);
        MMAY_EXPECT(limited.generation == generation);
        mmay_compiler_service_dispose(&limited);
    }
}

static void mmay_test_incremental_insert_and_delete(void) {
    static const char initial[] = "fn value(): i64 { return 1; }\n";
    static const char inserted[] = "fn value(): i64 { return 10; }\n";
    MarimayCompilerService service = mmay_test_incremental_service(
        mmay_system_allocator()
    );
    MarimayCompilerResult result;
    MarimayCompilerCheckRequest request;
    MarimayCompilerCheckStats stats;
    MarimayCompilerEdit edit;
    MarimayFileId file = 0;
    size_t offset = (size_t)(strstr(initial, "return 1") - initial) + 8;
    memset(&result, 0, sizeof(result));
    memset(&request, 0, sizeof(request));
    memset(&stats, 0, sizeof(stats));
    memset(&edit, 0, sizeof(edit));
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service, "main.my", sizeof("main.my") - 1, 1, initial, strlen(initial), &file
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_check(&service, "preview", &result)
        == MMAY_COMPILER_OK);
    request.previous = &result;
    request.stats = &stats;
    edit.base_version = 1;
    edit.start = offset;
    edit.previous_end = offset;
    edit.new_end = offset + 1;
    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service, file, 2, inserted, strlen(inserted), edit
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_check_request(
        &service, "preview", &request, &result
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(stats.incremental_parse_count == 1);
    MMAY_EXPECT(mmay_test_function_i64(&result.checked, 0, 10));

    memset(&stats, 0, sizeof(stats));
    edit.base_version = 2;
    edit.previous_end = offset + 1;
    edit.new_end = offset;
    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service, file, 3, initial, strlen(initial), edit
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_check_request(
        &service, "preview", &request, &result
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(stats.incremental_parse_count == 1);
    MMAY_EXPECT(mmay_test_function_i64(&result.checked, 0, 1));
    mmay_compiler_result_dispose(&result);
    mmay_compiler_service_dispose(&service);
}

static void mmay_test_incremental_package_recheck(void) {
    static const char first[] = "fn first(): i64 { return 40; }\n";
    static const char changed[] = "fn other(): i64 { return 40; }\n";
    static const char consumer[] =
        "fn second(): i64 { return first() + 2; }\n";
    MarimayCompilerService service = mmay_test_incremental_service(
        mmay_system_allocator()
    );
    MarimayCompilerResult result;
    MarimayCompilerResult fresh;
    MarimayCompilerCheckRequest request;
    MarimayCompilerCheckStats stats;
    MarimayCompilerEdit edit;
    MarimayFileId file = 0;
    const MarimayCheckDiagnostic *diagnostic = NULL;
    const MarimayCheckDiagnostic *fresh_diagnostic = NULL;
    const char *old_start = strstr(first, "first");
    const char *old_end = old_start + strlen("first");
    const char *new_start = strstr(changed, "other");
    const char *new_end = new_start + strlen("other");
    memset(&result, 0, sizeof(result));
    memset(&fresh, 0, sizeof(fresh));
    memset(&request, 0, sizeof(request));
    memset(&stats, 0, sizeof(stats));
    memset(&edit, 0, sizeof(edit));
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service, "a.my", sizeof("a.my") - 1, 1, first, strlen(first), &file
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service, "b.my", sizeof("b.my") - 1, 1,
        consumer, strlen(consumer), NULL
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_check(&service, "preview", &result)
        == MMAY_COMPILER_OK);
    MMAY_EXPECT(!mmay_compiler_result_has_errors(&result));
    edit.base_version = 1;
    edit.start = (size_t)(old_start - first);
    edit.previous_end = (size_t)(old_end - first);
    edit.new_end = (size_t)(new_end - changed);
    MMAY_EXPECT(edit.start == (size_t)(new_start - changed));
    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service, file, 2, changed, strlen(changed), edit
    ) == MMAY_COMPILER_OK);
    request.previous = &result;
    request.stats = &stats;
    MMAY_EXPECT(mmay_compiler_service_check_request(
        &service, "preview", &request, &result
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(stats.incremental_parse_count == 1);
    MMAY_EXPECT(stats.full_parse_count == 1);
    MMAY_EXPECT(mmay_compiler_result_has_errors(&result));
    MMAY_EXPECT(result.checked.diagnostics.length == 1);
    diagnostic = mmay_check_diagnostic(&result.checked, 0);
    MMAY_EXPECT(diagnostic != NULL && diagnostic->snapshot != NULL);
    if (diagnostic != NULL && diagnostic->snapshot != NULL) {
        MMAY_EXPECT(strcmp(
            mmay_source_path(diagnostic->snapshot),
            "b.my"
        ) == 0);
    }
    MMAY_EXPECT(mmay_compiler_service_check(&service, "preview", &fresh)
        == MMAY_COMPILER_OK);
    MMAY_EXPECT(fresh.checked.diagnostics.length
        == result.checked.diagnostics.length);
    fresh_diagnostic = mmay_check_diagnostic(&fresh.checked, 0);
    MMAY_EXPECT(diagnostic != NULL && fresh_diagnostic != NULL);
    if (diagnostic != NULL && fresh_diagnostic != NULL) {
        MMAY_EXPECT(strcmp(diagnostic->code, fresh_diagnostic->code) == 0);
        MMAY_EXPECT(strcmp(diagnostic->message, fresh_diagnostic->message) == 0);
        MMAY_EXPECT(diagnostic->snapshot == fresh_diagnostic->snapshot);
        MMAY_EXPECT(diagnostic->start == fresh_diagnostic->start);
        MMAY_EXPECT(diagnostic->end == fresh_diagnostic->end);
    }
    mmay_compiler_result_dispose(&fresh);
    mmay_compiler_result_dispose(&result);
    mmay_compiler_service_dispose(&service);
}

static void mmay_test_compiler_service_cancellation(void) {
    static const char first[] = "fn first(): i64 { return 40; }\n";
    static const char changed[] = "fn first(): i64 { return 41; }\n";
    static const char second[] =
        "fn second(): i64 { return first() + 2; }\n";
    MarimayFailAllocator allocator_state;
    MarimayAllocator allocator;
    MarimayCompilerService service;
    MarimayCompilerResult result;
    MarimayCompilerCheckRequest request;
    MarimayCompilerCheckStats stats;
    MarimayCompilerCheckStats unchanged_stats;
    MarimayCompilerEdit edit;
    MarimayTestCancelState cancel;
    MarimayFileId file = 0;
    const MarimaySourceSnapshot *snapshot = NULL;
    size_t cancel_at = 0;
    bool reached_success = false;
    memset(&result, 0, sizeof(result));
    memset(&request, 0, sizeof(request));
    memset(&edit, 0, sizeof(edit));
    mmay_fail_allocator_init(
        &allocator_state,
        mmay_system_allocator(),
        SIZE_MAX
    );
    allocator = mmay_fail_allocator(&allocator_state);
    service = mmay_test_incremental_service(allocator);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service, "a.my", sizeof("a.my") - 1, 1, first, strlen(first), &file
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service, "b.my", sizeof("b.my") - 1, 1, second, strlen(second), NULL
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_check(&service, "preview", &result)
        == MMAY_COMPILER_OK);
    if (mmay_compiler_result_unit(&result, 0) != NULL) {
        snapshot = mmay_compiler_result_unit(&result, 0)->snapshot;
    }
    edit.base_version = 1;
    edit.start = (size_t)(strstr(first, "return 40") - first) + 7;
    edit.previous_end = edit.start + 2;
    edit.new_end = edit.start + 2;
    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service, file, 2, changed, strlen(changed), edit
    ) == MMAY_COMPILER_OK);
    request.previous = &result;
    request.cancel_poll = mmay_test_cancel_poll;
    request.cancel_context = &cancel;
    request.stats = &stats;
    memset(&unchanged_stats, 0x5a, sizeof(unchanged_stats));
    for (cancel_at = 1; cancel_at < 32; cancel_at += 1) {
        MarimayCompilerStatus status;
        memset(&cancel, 0, sizeof(cancel));
        cancel.cancel_at = cancel_at;
        memset(&stats, 0x5a, sizeof(stats));
        status = mmay_compiler_service_check_request(
            &service,
            "preview",
            &request,
            &result
        );
        if (status == MMAY_COMPILER_OK) {
            reached_success = true;
            MMAY_EXPECT(stats.incremental_parse_count == 1);
            MMAY_EXPECT(stats.full_parse_count == 1);
            MMAY_EXPECT(mmay_test_function_i64(&result.checked, 0, 41));
            break;
        }
        MMAY_EXPECT(status == MMAY_COMPILER_CANCELLED);
        MMAY_EXPECT(mmay_compiler_result_unit(&result, 0) != NULL);
        if (mmay_compiler_result_unit(&result, 0) != NULL) {
            MMAY_EXPECT(mmay_compiler_result_unit(&result, 0)->snapshot
                == snapshot);
        }
        MMAY_EXPECT(mmay_test_function_i64(&result.checked, 0, 40));
        MMAY_EXPECT(memcmp(
            &stats,
            &unchanged_stats,
            sizeof(stats)
        ) == 0);
    }
    MMAY_EXPECT(reached_success);
    mmay_compiler_result_dispose(&result);
    mmay_compiler_service_dispose(&service);
    MMAY_EXPECT(allocator_state.live_allocations == 0);
    MMAY_EXPECT(allocator_state.live_bytes == 0);
}

static void mmay_test_incremental_allocation_failures(void) {
    static const char initial[] = "fn value(): i64 { return 1; }\n";
    static const char changed[] = "fn value(): i64 { return 2; }\n";
    MarimayFailAllocator allocator_state;
    MarimayAllocator allocator;
    MarimayCompilerService service;
    MarimayCompilerResult result;
    MarimayCompilerCheckRequest request;
    MarimayCompilerCheckStats stats;
    MarimayCompilerCheckStats unchanged_stats;
    MarimayCompilerEdit edit;
    MarimayFileId file = 0;
    size_t relative_failure = 0;
    size_t offset = (size_t)(strstr(initial, "return 1") - initial) + 7;
    bool reached_success = false;
    memset(&result, 0, sizeof(result));
    memset(&request, 0, sizeof(request));
    memset(&edit, 0, sizeof(edit));
    mmay_fail_allocator_init(
        &allocator_state,
        mmay_system_allocator(),
        SIZE_MAX
    );
    allocator = mmay_fail_allocator(&allocator_state);
    service = mmay_test_incremental_service(allocator);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service, "main.my", sizeof("main.my") - 1, 1, initial, strlen(initial), &file
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_check(&service, "preview", &result)
        == MMAY_COMPILER_OK);
    edit.base_version = 1;
    edit.start = offset;
    edit.previous_end = offset + 1;
    edit.new_end = offset + 1;
    MMAY_EXPECT(mmay_compiler_service_overlay_edit(
        &service, file, 2, changed, strlen(changed), edit
    ) == MMAY_COMPILER_OK);
    request.previous = &result;
    request.stats = &stats;
    memset(&unchanged_stats, 0xa5, sizeof(unchanged_stats));
    for (relative_failure = 0; relative_failure < 512;
        relative_failure += 1) {
        MarimayCompilerStatus status;
        memset(&stats, 0xa5, sizeof(stats));
        allocator_state.failure_index = allocator_state.attempts
            + relative_failure;
        status = mmay_compiler_service_check_request(
            &service,
            "preview",
            &request,
            &result
        );
        if (status == MMAY_COMPILER_OK) {
            const MarimayCompilerUnit *current = mmay_compiler_result_unit(
                &result,
                0
            );
            reached_success = true;
            MMAY_EXPECT(stats.incremental_parse_count == 1);
            MMAY_EXPECT(current != NULL && current->version == 2);
            break;
        }
        MMAY_EXPECT(status == MMAY_COMPILER_ALLOCATION_FAILED);
        MMAY_EXPECT(memcmp(
            &stats,
            &unchanged_stats,
            sizeof(stats)
        ) == 0);
        MMAY_EXPECT(mmay_compiler_result_unit(&result, 0) != NULL);
        if (mmay_compiler_result_unit(&result, 0) != NULL) {
            MMAY_EXPECT(mmay_compiler_result_unit(&result, 0)->version == 1);
        }
        MMAY_EXPECT(!mmay_compiler_result_is_current(&service, &result));
    }
    MMAY_EXPECT(reached_success);
    mmay_compiler_result_dispose(&result);
    mmay_compiler_service_dispose(&service);
    MMAY_EXPECT(allocator_state.live_allocations == 0);
    MMAY_EXPECT(allocator_state.live_bytes == 0);
}

void mmay_test_compiler_service_incremental(void) {
    mmay_test_incremental_same_and_shifted();
    mmay_test_incremental_edit_validation();
    mmay_test_result_currentness_is_service_scoped();
    mmay_test_incremental_chain_falls_back_to_full_parse();
    mmay_test_incremental_distinct_result_lifetimes();
    mmay_test_overlay_edit_failure_atomicity();
    mmay_test_incremental_insert_and_delete();
    mmay_test_incremental_package_recheck();
    mmay_test_compiler_service_cancellation();
    mmay_test_incremental_allocation_failures();
}
