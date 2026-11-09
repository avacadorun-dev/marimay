#include "test_support.h"

#include <stdint.h>
#include <string.h>
#include "../../bootstrap/src/lsp/analysis_overrides.h"

static void overrides_run(MarimayAllocator allocator, bool verify) {
    MarimayCliCheckTarget target = {0};
    MarimayCompilerDocument document;
    MarimayFileId file = 0;
    const char path[] = "override.my";
    LspSourceOverride edits[] = {
        {path, sizeof(path) - 1, "new", 3}, {path, sizeof(path) - 1, "other", 5}
    };
    LspStatus status;
    if (mmay_compiler_service_init(&target.service, allocator,
            mmay_compiler_default_options()) != MMAY_COMPILER_OK) return;
    target.service_initialized = true;
    if (mmay_compiler_service_add_document(&target.service, path, sizeof(path) - 1,
            1, "old", 3, &file) != MMAY_COMPILER_OK) goto cleanup;
    if (verify) {
        MMAY_EXPECT(lsp_apply_source_overrides(&target, NULL, 1) == LSP_INVALID);
        MMAY_EXPECT(lsp_apply_source_overrides(&target, edits, 2) == LSP_INVALID);
        MMAY_EXPECT(mmay_compiler_service_document(&target.service, file, &document));
        MMAY_EXPECT(document.version == 1);
        edits[0].text_length = 16U * 1024U * 1024U + 1U;
        MMAY_EXPECT(lsp_apply_source_overrides(&target, edits, 1) == LSP_LIMIT);
        edits[0].text_length = 3;
    }
    status = lsp_apply_source_overrides(&target, edits, 1);
    if (verify) {
        MMAY_EXPECT(status == LSP_OK);
        MMAY_EXPECT(mmay_compiler_service_document(&target.service, file, &document));
        MMAY_EXPECT(document.version == 2);
        MMAY_EXPECT(mmay_source_length(document.snapshot) == 3);
        MMAY_EXPECT(memcmp(mmay_source_bytes(document.snapshot), "new", 3) == 0);
        edits[0].path = "missing.my";
        edits[0].path_length = 10;
        MMAY_EXPECT(lsp_apply_source_overrides(&target, edits, 1) == LSP_INVALID);
    }
cleanup:
    mmay_cli_check_target_dispose(&target);
}

void mmay_test_lsp_overrides(void) {
    MarimayFailAllocator state;
    size_t index = 0, attempts;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    overrides_run(mmay_fail_allocator(&state), true);
    attempts = state.attempts;
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    for (index = 0; index < attempts; index += 1) {
        mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
        overrides_run(mmay_fail_allocator(&state), false);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
}
