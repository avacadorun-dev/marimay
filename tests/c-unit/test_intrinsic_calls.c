#include "test_support.h"

#include <string.h>

#include "../../bootstrap/src/check/checker_package_context.h"
#include "../../bootstrap/src/check/intrinsic_calls.h"
#include "../../bootstrap/src/lsp/intrinsic_queries.h"

static const char source[] = "json.encode(42); save(43); encode(44);\n";

static void intrinsic_record_test(const MarimaySourceSnapshot *snapshot) {
    MarimayFailAllocator state;
    MarimayCheckResult result = {0};
    MarimayAstTree ast = {0};
    MarimayChecker checker = {0};
    const MarimayIntrinsicSignature *intrinsic = mmay_intrinsic_signature(2);
    const MarimayCheckedIntrinsicCall *call = NULL;
    size_t attempts = 0;
    MMAY_EXPECT(intrinsic != NULL && strcmp(intrinsic->name, "encode") == 0);
    ast.snapshot = snapshot;
    checker.ast = &ast;
    checker.result = &result;
    checker.options = mmay_check_default_options();
    result.initialized = true;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_vector_init(&result.intrinsic_calls, mmay_fail_allocator(&state),
        sizeof(MarimayCheckedIntrinsicCall)));
    MMAY_EXPECT(mmay_check_record_intrinsic_call(&checker, intrinsic, 17, 21));
    MMAY_EXPECT(mmay_check_record_intrinsic_call(&checker, intrinsic, 5, 11));
    attempts = state.attempts;
    call = mmay_checked_intrinsic_call_at(&result, snapshot, 8);
    MMAY_EXPECT(call != NULL && call->intrinsic == intrinsic);
    MMAY_EXPECT(call != NULL && call->start == 5 && call->end == 11);
    MMAY_EXPECT(mmay_checked_intrinsic_call_at(&result, snapshot, 11) == NULL);
    MMAY_EXPECT(mmay_checked_intrinsic_call_at(&result, snapshot, 0) == NULL);
    MMAY_EXPECT(mmay_checked_intrinsic_call(&result, 0)->start == 5);
    MMAY_EXPECT(mmay_checked_intrinsic_call(&result, 1)->start == 17);
    MMAY_EXPECT(mmay_checked_intrinsic_call(&result, 2) == NULL);
    result.error_count = 1;
    MMAY_EXPECT(mmay_checked_intrinsic_call_at(&result, snapshot, 8) == NULL);
    MMAY_EXPECT(mmay_checked_intrinsic_call(&result, 0) == NULL);
    result.error_count = 0;
    checker.options.max_symbols = 2;
    MMAY_EXPECT(mmay_check_record_intrinsic_call(&checker, intrinsic, 5, 11));
    MMAY_EXPECT(result.intrinsic_calls.length == 2);
    MMAY_EXPECT(!mmay_check_record_intrinsic_call(&checker, intrinsic, 27, 33));
    MMAY_EXPECT(checker.failure == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(result.intrinsic_calls.length == 2);
    mmay_vector_dispose(&result.intrinsic_calls);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    for (size_t index = 0; index < attempts; index += 1) {
        checker.failure = MMAY_CHECK_OK;
        checker.options.max_symbols = 100;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
        MMAY_EXPECT(mmay_vector_init(&result.intrinsic_calls, mmay_fail_allocator(&state),
            sizeof(MarimayCheckedIntrinsicCall)));
        MMAY_EXPECT(!mmay_check_record_intrinsic_call(&checker, intrinsic, 5, 11));
        MMAY_EXPECT(checker.failure == MMAY_CHECK_ALLOCATION_FAILED);
        MMAY_EXPECT(result.intrinsic_calls.length == 0 && state.attempts > index);
        mmay_vector_dispose(&result.intrinsic_calls);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
}

static bool intrinsic_view_run(const MarimaySourceSnapshot *snapshot,
    MarimayAllocator allocator, MarimayTestBuffer *output, bool inspect) {
    LspAnalysis analysis = {0};
    MarimayCheckedIntrinsicCall call = {mmay_intrinsic_signature(2), snapshot, 5, 11};
    LspStatus status;
    bool found = false, ok = false;
    analysis.target.allocator = allocator;
    analysis.result.checked.initialized = true;
    analysis.result.checked.intrinsic_calls.data = &call;
    analysis.result.checked.intrinsic_calls.length = 1;
    analysis.result.checked.intrinsic_calls.capacity = 1;
    analysis.result.checked.intrinsic_calls.element_size = sizeof(call);
    status = lsp_intrinsic_views_init(&analysis);
    ok = status == LSP_OK;
    if (ok) ok = lsp_intrinsic_query(&analysis, snapshot, 11, LSP_INTRINSIC_HOVER,
        false, mmay_test_buffer_writer(output), &found);
    if (ok && inspect) {
        const MarimaySourceSnapshot *view = mmay_source_manager_current(
            &analysis.intrinsic_sources, 1);
        const char *signature = strstr((const char *)mmay_source_bytes(view), "fn encode<T>");
        MMAY_EXPECT(found && signature != NULL);
        MMAY_EXPECT(strstr(output->bytes, "interface/signature only") != NULL);
        MMAY_EXPECT(strstr(output->bytes, "fn encode<T>(value: T)") != NULL);
        MMAY_EXPECT(strstr(output->bytes, "explicit codec") != NULL);
        MMAY_EXPECT(strcmp(mmay_source_path(view), "std/json/intrinsics.my") == 0);
        mmay_test_buffer_init(output);
        MMAY_EXPECT(lsp_intrinsic_query(&analysis, view,
            (size_t)(signature - (const char *)mmay_source_bytes(view)) + 3,
            LSP_INTRINSIC_HOVER, false, mmay_test_buffer_writer(output), &found));
        MMAY_EXPECT(found && strstr(output->bytes, "fn encode<T>") != NULL);
        mmay_test_buffer_init(output);
        MMAY_EXPECT(lsp_intrinsic_query(&analysis, snapshot, 12,
            LSP_INTRINSIC_HOVER, false, mmay_test_buffer_writer(output), &found));
        MMAY_EXPECT(!found && output->length == 0);
        analysis.result.checked.error_count = 1;
        MMAY_EXPECT(lsp_intrinsic_query(&analysis, snapshot, 8,
            LSP_INTRINSIC_HOVER, false, mmay_test_buffer_writer(output), &found));
        MMAY_EXPECT(!found && output->length == 0);
    }
    if (analysis.intrinsic_sources_initialized)
        mmay_source_manager_dispose(&analysis.intrinsic_sources);
    return ok;
}

static void intrinsic_view_test(const MarimaySourceSnapshot *snapshot) {
    MarimayFailAllocator state;
    MarimayTestBuffer output;
    size_t attempts = 0, writes = 0;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    mmay_test_buffer_init(&output);
    MMAY_EXPECT(intrinsic_view_run(snapshot, mmay_fail_allocator(&state), &output, false));
    attempts = state.attempts;
    writes = output.calls;
    MMAY_EXPECT(attempts != 0 && writes != 0);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    for (size_t index = 0; index < attempts; index += 1) {
        mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
        mmay_test_buffer_init(&output);
        MMAY_EXPECT(!intrinsic_view_run(snapshot, mmay_fail_allocator(&state), &output, false));
        MMAY_EXPECT(state.attempts > index);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    for (size_t index = 0; index < writes; index += 1) {
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        mmay_test_buffer_init(&output);
        output.failure_index = index;
        MMAY_EXPECT(!intrinsic_view_run(snapshot, mmay_fail_allocator(&state), &output, false));
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    mmay_test_buffer_init(&output);
    MMAY_EXPECT(intrinsic_view_run(snapshot, mmay_system_allocator(), &output, true));
}

void mmay_test_intrinsic_calls(void) {
    MarimayFailAllocator state;
    MarimaySourceManager sources = {0};
    const MarimaySourceSnapshot *snapshot = NULL;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_source_manager_init(&sources, mmay_fail_allocator(&state)));
    MMAY_EXPECT(mmay_source_manager_add(&sources, "/intrinsic-calls.my",
        strlen("/intrinsic-calls.my"), source, sizeof(source) - 1,
        &snapshot) == MMAY_SOURCE_OK);
    if (snapshot != NULL) {
        intrinsic_record_test(snapshot);
        intrinsic_view_test(snapshot);
    }
    mmay_source_manager_dispose(&sources);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}
