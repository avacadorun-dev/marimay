#include "analysis_overrides.h"

#include <string.h>

static bool lsp_override_path_equal(const LspSourceOverride *left,
    const LspSourceOverride *right) {
    return left->path_length == right->path_length
        && memcmp(left->path, right->path, left->path_length) == 0;
}

LspStatus lsp_apply_source_overrides(MarimayCliCheckTarget *target,
    const LspSourceOverride *overrides, size_t count) {
    size_t index = 0, total = 0;
    if (target == NULL || (overrides == NULL && count != 0)
        || count > MMAY_CLI_CHECK_MAX_FILES) return LSP_INVALID;
    /* Validate sizes and duplicate paths before replacing isolated snapshots. */
    for (index = 0; index < count; index += 1) {
        size_t earlier = 0;
        const LspSourceOverride *item = &overrides[index];
        if (item->path == NULL || item->path_length == 0
            || item->path_length > MMAY_CLI_CHECK_MAX_PATH_BYTES
            || memchr(item->path, 0, item->path_length) != NULL
            || (item->text == NULL && item->text_length != 0)) return LSP_INVALID;
        if (item->text_length > MMAY_CLI_CHECK_MAX_SOURCE_BYTES
            || !mmay_size_add(total, item->text_length, &total)
            || total > 64U * 1024U * 1024U) return LSP_LIMIT;
        for (earlier = 0; earlier < index; earlier += 1)
            if (lsp_override_path_equal(item, &overrides[earlier])) return LSP_INVALID;
    }
    for (index = 0; index < count; index += 1) {
        size_t document_index = 0;
        bool found = false;
        const LspSourceOverride *item = &overrides[index];
        for (document_index = 0; document_index < target->service.documents.length;
            document_index += 1) {
            const MarimayCompilerDocument *document = mmay_vector_at_const(
                &target->service.documents, document_index);
            MarimayCompilerStatus status;
            if (mmay_source_path_length(document->snapshot) != item->path_length
                || memcmp(mmay_source_path(document->snapshot), item->path,
                    item->path_length) != 0) continue;
            if (document->version == UINT64_MAX) return LSP_LIMIT;
            status = mmay_compiler_service_overlay(&target->service,
                document->file_id, document->version + 1, item->text, item->text_length);
            if (status == MMAY_COMPILER_ALLOCATION_FAILED) return LSP_ALLOCATION_FAILED;
            if (status == MMAY_COMPILER_LIMIT_REACHED) return LSP_LIMIT;
            if (status != MMAY_COMPILER_OK) return LSP_INTERNAL_ERROR;
            found = true;
            break;
        }
        if (!found) return LSP_INVALID;
    }
    return LSP_OK;
}
