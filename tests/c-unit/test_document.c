#include "test_support.h"

#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/format/document.h"

static void ruda_document_expect_part(
    RudaDocument *document,
    RudaDocumentPartKind kind
) {
    RUDA_EXPECT(ruda_document_part(document, kind) == RUDA_DOCUMENT_OK);
}

static void ruda_document_expect_text(RudaDocument *document, const char *text) {
    RUDA_EXPECT(ruda_document_text(
        document,
        (const unsigned char *)text,
        strlen(text)
    ) == RUDA_DOCUMENT_OK);
}

void ruda_test_document(void) {
    RudaDocument document;
    RudaDocumentRenderOptions options = ruda_document_default_render_options();
    RudaTestBuffer output;
    RudaWriter writer;

    RUDA_EXPECT(ruda_document_init(&document, ruda_system_allocator(), 64));
    ruda_document_expect_part(&document, RUDA_DOCUMENT_GROUP_BEGIN);
    ruda_document_expect_text(&document, "call(");
    ruda_document_expect_part(&document, RUDA_DOCUMENT_INDENT_BEGIN);
    ruda_document_expect_part(&document, RUDA_DOCUMENT_SOFTBREAK);
    ruda_document_expect_text(&document, "alpha,");
    ruda_document_expect_part(&document, RUDA_DOCUMENT_SOFTLINE);
    ruda_document_expect_text(&document, "界");
    ruda_document_expect_part(&document, RUDA_DOCUMENT_INDENT_END);
    ruda_document_expect_part(&document, RUDA_DOCUMENT_SOFTBREAK);
    ruda_document_expect_text(&document, ")");
    ruda_document_expect_part(&document, RUDA_DOCUMENT_GROUP_END);
    ruda_document_expect_part(&document, RUDA_DOCUMENT_HARDLINE);

    ruda_test_buffer_init(&output);
    writer = ruda_test_buffer_writer(&output);
    options.width = 15;
    RUDA_EXPECT(ruda_document_render(&document, writer, options) == RUDA_DOCUMENT_OK);
    RUDA_EXPECT(strcmp(output.bytes, "call(alpha, 界)\n") == 0);

    ruda_test_buffer_init(&output);
    writer = ruda_test_buffer_writer(&output);
    options.width = 14;
    RUDA_EXPECT(ruda_document_render(&document, writer, options) == RUDA_DOCUMENT_OK);
    RUDA_EXPECT(strcmp(
        output.bytes,
        "call(\n  alpha,\n  界\n)\n"
    ) == 0);

    ruda_test_buffer_init(&output);
    output.failure_index = 0;
    writer = ruda_test_buffer_writer(&output);
    options = ruda_document_default_render_options();
    RUDA_EXPECT(ruda_document_render(
        &document,
        writer,
        options
    ) == RUDA_DOCUMENT_WRITE_FAILED);

    ruda_test_buffer_init(&output);
    writer = ruda_test_buffer_writer(&output);
    options = ruda_document_default_render_options();
    options.max_scan_steps = 1;
    RUDA_EXPECT(ruda_document_render(
        &document,
        writer,
        options
    ) == RUDA_DOCUMENT_LIMIT_REACHED);
    ruda_document_dispose(&document);

    RUDA_EXPECT(ruda_document_init(&document, ruda_system_allocator(), 1));
    ruda_document_expect_text(&document, "one");
    RUDA_EXPECT(ruda_document_text(
        &document,
        (const unsigned char *)"two",
        3
    ) == RUDA_DOCUMENT_LIMIT_REACHED);
    RUDA_EXPECT(ruda_document_text(
        &document,
        (const unsigned char *)"bad\ntext",
        strlen("bad\ntext")
    ) == RUDA_DOCUMENT_INVALID_ARGUMENT);
    ruda_document_dispose(&document);

    RUDA_EXPECT(ruda_document_init(&document, ruda_system_allocator(), 4));
    ruda_document_expect_part(&document, RUDA_DOCUMENT_GROUP_BEGIN);
    ruda_document_expect_text(&document, "open");
    ruda_test_buffer_init(&output);
    writer = ruda_test_buffer_writer(&output);
    RUDA_EXPECT(ruda_document_render(
        &document,
        writer,
        ruda_document_default_render_options()
    ) == RUDA_DOCUMENT_INVALID_ARGUMENT);
    ruda_document_dispose(&document);

    {
        RudaFailAllocator state;
        RudaAllocator allocator;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), 0);
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_document_init(&document, allocator, 4));
        RUDA_EXPECT(ruda_document_text(
            &document,
            (const unsigned char *)"allocation",
            strlen("allocation")
        ) == RUDA_DOCUMENT_ALLOCATION_FAILED);
        ruda_document_dispose(&document);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
}
