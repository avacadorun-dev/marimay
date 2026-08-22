#ifndef MMAY_FORMAT_DOCUMENT_H
#define MMAY_FORMAT_DOCUMENT_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/vector.h"
#include "mmay/base/writer.h"

typedef enum MarimayDocumentStatus {
    MMAY_DOCUMENT_OK,
    MMAY_DOCUMENT_INVALID_ARGUMENT,
    MMAY_DOCUMENT_ALLOCATION_FAILED,
    MMAY_DOCUMENT_LIMIT_REACHED,
    MMAY_DOCUMENT_WRITE_FAILED
} MarimayDocumentStatus;

typedef enum MarimayDocumentPartKind {
    MMAY_DOCUMENT_TEXT,
    MMAY_DOCUMENT_BREAK_TEXT,
    MMAY_DOCUMENT_HARDLINE,
    MMAY_DOCUMENT_SOFTLINE,
    MMAY_DOCUMENT_SOFTBREAK,
    MMAY_DOCUMENT_GROUP_BEGIN,
    MMAY_DOCUMENT_GROUP_END,
    MMAY_DOCUMENT_INDENT_BEGIN,
    MMAY_DOCUMENT_INDENT_END,
    MMAY_DOCUMENT_LITERAL_LINE
} MarimayDocumentPartKind;

typedef struct MarimayDocumentPart {
    MarimayDocumentPartKind kind;
    const unsigned char *text;
    size_t length;
} MarimayDocumentPart;

typedef struct MarimayDocument {
    MarimayVector parts;
    size_t max_parts;
    bool initialized;
} MarimayDocument;

typedef struct MarimayDocumentRenderOptions {
    size_t width;
    size_t indent_width;
    size_t max_nesting;
    size_t max_scan_steps;
} MarimayDocumentRenderOptions;

bool mmay_document_init(
    MarimayDocument *document,
    MarimayAllocator allocator,
    size_t max_parts
);
void mmay_document_dispose(MarimayDocument *document);

MarimayDocumentStatus mmay_document_text(
    MarimayDocument *document,
    const unsigned char *text,
    size_t length
);
MarimayDocumentStatus mmay_document_break_text(
    MarimayDocument *document,
    const unsigned char *text,
    size_t length
);
MarimayDocumentStatus mmay_document_part(
    MarimayDocument *document,
    MarimayDocumentPartKind kind
);

MarimayDocumentRenderOptions mmay_document_default_render_options(void);
MarimayDocumentStatus mmay_document_render(
    const MarimayDocument *document,
    MarimayWriter writer,
    MarimayDocumentRenderOptions options
);

#endif
