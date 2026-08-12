#ifndef RUDA_FORMAT_DOCUMENT_H
#define RUDA_FORMAT_DOCUMENT_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/allocator.h"
#include "ruda/base/vector.h"
#include "ruda/base/writer.h"

typedef enum RudaDocumentStatus {
    RUDA_DOCUMENT_OK,
    RUDA_DOCUMENT_INVALID_ARGUMENT,
    RUDA_DOCUMENT_ALLOCATION_FAILED,
    RUDA_DOCUMENT_LIMIT_REACHED,
    RUDA_DOCUMENT_WRITE_FAILED
} RudaDocumentStatus;

typedef enum RudaDocumentPartKind {
    RUDA_DOCUMENT_TEXT,
    RUDA_DOCUMENT_BREAK_TEXT,
    RUDA_DOCUMENT_HARDLINE,
    RUDA_DOCUMENT_SOFTLINE,
    RUDA_DOCUMENT_SOFTBREAK,
    RUDA_DOCUMENT_GROUP_BEGIN,
    RUDA_DOCUMENT_GROUP_END,
    RUDA_DOCUMENT_INDENT_BEGIN,
    RUDA_DOCUMENT_INDENT_END,
    RUDA_DOCUMENT_LITERAL_LINE
} RudaDocumentPartKind;

typedef struct RudaDocumentPart {
    RudaDocumentPartKind kind;
    const unsigned char *text;
    size_t length;
} RudaDocumentPart;

typedef struct RudaDocument {
    RudaVector parts;
    size_t max_parts;
    bool initialized;
} RudaDocument;

typedef struct RudaDocumentRenderOptions {
    size_t width;
    size_t indent_width;
    size_t max_nesting;
    size_t max_scan_steps;
} RudaDocumentRenderOptions;

bool ruda_document_init(
    RudaDocument *document,
    RudaAllocator allocator,
    size_t max_parts
);
void ruda_document_dispose(RudaDocument *document);

RudaDocumentStatus ruda_document_text(
    RudaDocument *document,
    const unsigned char *text,
    size_t length
);
RudaDocumentStatus ruda_document_break_text(
    RudaDocument *document,
    const unsigned char *text,
    size_t length
);
RudaDocumentStatus ruda_document_part(
    RudaDocument *document,
    RudaDocumentPartKind kind
);

RudaDocumentRenderOptions ruda_document_default_render_options(void);
RudaDocumentStatus ruda_document_render(
    const RudaDocument *document,
    RudaWriter writer,
    RudaDocumentRenderOptions options
);

#endif
