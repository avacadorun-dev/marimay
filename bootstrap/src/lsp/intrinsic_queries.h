#ifndef MMAY_LSP_INTRINSIC_QUERIES_H
#define MMAY_LSP_INTRINSIC_QUERIES_H

#include "workspace.h"

typedef enum LspIntrinsicQueryKind {
    LSP_INTRINSIC_HOVER,
    LSP_INTRINSIC_DEFINITION,
    LSP_INTRINSIC_REFERENCES
} LspIntrinsicQueryKind;

/* Analysis owns the immutable registry API view; it is never checked as input. */
LspStatus lsp_intrinsic_views_init(LspAnalysis *analysis);
/* Unmatched positions write nothing and set found=false. */
bool lsp_intrinsic_query(const LspAnalysis *analysis,
    const MarimaySourceSnapshot *snapshot, size_t offset,
    LspIntrinsicQueryKind kind, bool include_declaration,
    MarimayWriter writer, bool *found);

#endif
