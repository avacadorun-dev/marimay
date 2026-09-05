#include "test_support.h"

#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/format/document.h"

static void mmay_document_expect_part(
    MarimayDocument *document,
    MarimayDocumentPartKind kind
) {
    MMAY_EXPECT(mmay_document_part(document, kind) == MMAY_DOCUMENT_OK);
}

static void mmay_document_expect_text(MarimayDocument *document, const char *text) {
    MMAY_EXPECT(mmay_document_text(
        document,
        (const unsigned char *)text,
        strlen(text)
    ) == MMAY_DOCUMENT_OK);
}

static void mmay_test_document_suffix_width(void) {
    MarimayDocument document;
    MarimayDocumentRenderOptions options = mmay_document_default_render_options();
    MarimayTestBuffer output;
    MMAY_EXPECT(mmay_document_init(&document, mmay_system_allocator(), 64));
    mmay_document_expect_text(&document, "root");
    mmay_document_expect_part(&document, MMAY_DOCUMENT_HARDLINE);
    mmay_document_expect_part(&document, MMAY_DOCUMENT_INDENT_BEGIN);
    mmay_document_expect_part(&document, MMAY_DOCUMENT_GROUP_BEGIN);
    mmay_document_expect_text(&document, "(");
    mmay_document_expect_part(&document, MMAY_DOCUMENT_INDENT_BEGIN);
    mmay_document_expect_part(&document, MMAY_DOCUMENT_SOFTBREAK);
    mmay_document_expect_text(&document, "界");
    mmay_document_expect_part(&document, MMAY_DOCUMENT_INDENT_END);
    mmay_document_expect_part(&document, MMAY_DOCUMENT_SOFTBREAK);
    mmay_document_expect_text(&document, ")");
    mmay_document_expect_part(&document, MMAY_DOCUMENT_GROUP_END);
    mmay_document_expect_text(&document, ";");
    mmay_document_expect_part(&document, MMAY_DOCUMENT_HARDLINE);
    mmay_document_expect_part(&document, MMAY_DOCUMENT_INDENT_END);
    options.width = 7;
    mmay_test_buffer_init(&output);
    MMAY_EXPECT(mmay_document_render(&document, mmay_test_buffer_writer(&output),
        options) == MMAY_DOCUMENT_OK);
    MMAY_EXPECT(strcmp(output.bytes, "root\n  (界);\n") == 0);
    options.width = 6;
    mmay_test_buffer_init(&output);
    MMAY_EXPECT(mmay_document_render(&document, mmay_test_buffer_writer(&output),
        options) == MMAY_DOCUMENT_OK);
    MMAY_EXPECT(strcmp(output.bytes, "root\n  (\n    界\n  );\n") == 0);
    mmay_document_dispose(&document);
}

void mmay_test_document(void) {
    MarimayDocument document;
    MarimayDocumentRenderOptions options = mmay_document_default_render_options();
    MarimayTestBuffer output;
    MarimayWriter writer;

    mmay_test_document_suffix_width();

    MMAY_EXPECT(mmay_document_init(&document, mmay_system_allocator(), 64));
    mmay_document_expect_part(&document, MMAY_DOCUMENT_GROUP_BEGIN);
    mmay_document_expect_text(&document, "call(");
    mmay_document_expect_part(&document, MMAY_DOCUMENT_INDENT_BEGIN);
    mmay_document_expect_part(&document, MMAY_DOCUMENT_SOFTBREAK);
    mmay_document_expect_text(&document, "alpha,");
    mmay_document_expect_part(&document, MMAY_DOCUMENT_SOFTLINE);
    mmay_document_expect_text(&document, "界");
    mmay_document_expect_part(&document, MMAY_DOCUMENT_INDENT_END);
    mmay_document_expect_part(&document, MMAY_DOCUMENT_SOFTBREAK);
    mmay_document_expect_text(&document, ")");
    mmay_document_expect_part(&document, MMAY_DOCUMENT_GROUP_END);
    mmay_document_expect_part(&document, MMAY_DOCUMENT_HARDLINE);

    mmay_test_buffer_init(&output);
    writer = mmay_test_buffer_writer(&output);
    options.width = 15;
    MMAY_EXPECT(mmay_document_render(&document, writer, options) == MMAY_DOCUMENT_OK);
    MMAY_EXPECT(strcmp(output.bytes, "call(alpha, 界)\n") == 0);

    mmay_test_buffer_init(&output);
    writer = mmay_test_buffer_writer(&output);
    options.width = 14;
    MMAY_EXPECT(mmay_document_render(&document, writer, options) == MMAY_DOCUMENT_OK);
    MMAY_EXPECT(strcmp(
        output.bytes,
        "call(\n  alpha,\n  界\n)\n"
    ) == 0);

    mmay_test_buffer_init(&output);
    output.failure_index = 0;
    writer = mmay_test_buffer_writer(&output);
    options = mmay_document_default_render_options();
    MMAY_EXPECT(mmay_document_render(
        &document,
        writer,
        options
    ) == MMAY_DOCUMENT_WRITE_FAILED);

    mmay_test_buffer_init(&output);
    writer = mmay_test_buffer_writer(&output);
    options = mmay_document_default_render_options();
    options.max_scan_steps = 1;
    MMAY_EXPECT(mmay_document_render(
        &document,
        writer,
        options
    ) == MMAY_DOCUMENT_LIMIT_REACHED);
    mmay_document_dispose(&document);

    MMAY_EXPECT(mmay_document_init(&document, mmay_system_allocator(), 1));
    mmay_document_expect_text(&document, "one");
    MMAY_EXPECT(mmay_document_text(
        &document,
        (const unsigned char *)"two",
        3
    ) == MMAY_DOCUMENT_LIMIT_REACHED);
    MMAY_EXPECT(mmay_document_text(
        &document,
        (const unsigned char *)"bad\ntext",
        strlen("bad\ntext")
    ) == MMAY_DOCUMENT_INVALID_ARGUMENT);
    mmay_document_dispose(&document);

    MMAY_EXPECT(mmay_document_init(&document, mmay_system_allocator(), 4));
    mmay_document_expect_part(&document, MMAY_DOCUMENT_GROUP_BEGIN);
    mmay_document_expect_text(&document, "open");
    mmay_test_buffer_init(&output);
    writer = mmay_test_buffer_writer(&output);
    MMAY_EXPECT(mmay_document_render(
        &document,
        writer,
        mmay_document_default_render_options()
    ) == MMAY_DOCUMENT_INVALID_ARGUMENT);
    mmay_document_dispose(&document);

    {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), 0);
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_document_init(&document, allocator, 4));
        MMAY_EXPECT(mmay_document_text(
            &document,
            (const unsigned char *)"allocation",
            strlen("allocation")
        ) == MMAY_DOCUMENT_ALLOCATION_FAILED);
        mmay_document_dispose(&document);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
}
