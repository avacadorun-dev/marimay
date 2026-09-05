#include "test_formatter_support.h"

/* Owns source-independent D031 braced-list layout and two-pass cleanup. */
static void mmay_formatter_braced_case(const char *source, const char *expected) {
    MarimayFormatterFixture original = {0};
    size_t iteration = 0, attempts = 0;
    if (!mmay_formatter_fixture_init(&original, "braces-input.my",
            (const unsigned char *)source, strlen(source))) {
        MMAY_EXPECT(false);
        return;
    }
    MMAY_EXPECT(original.lexed.diagnostics.length == 0);
    MMAY_EXPECT(original.parsed.diagnostics.length == 0);
    do {
        MarimayFailAllocator state;
        MarimayFormatResult first = {0}, second = {0};
        MarimayFormatterFixture formatted = {0};
        MarimayFormatStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(),
            iteration == 0 ? SIZE_MAX : iteration - 1);
        status = mmay_format_source(&original.lexed, &original.parsed,
            mmay_fail_allocator(&state), mmay_format_default_options(), &first);
        if (status == MMAY_FORMAT_OK) {
            if (!mmay_formatter_fixture_init(&formatted, "braces-output.my",
                    first.output.data, first.output.length)) {
                MMAY_EXPECT(false);
            } else {
                MMAY_EXPECT(formatted.lexed.diagnostics.length == 0);
                MMAY_EXPECT(formatted.parsed.diagnostics.length == 0);
                status = mmay_format_source(&formatted.lexed, &formatted.parsed,
                    mmay_fail_allocator(&state), mmay_format_default_options(), &second);
            }
        }
        if (iteration == 0) {
            MarimayAstTree before = {0}, after = {0};
            attempts = state.attempts;
            MMAY_EXPECT(status == MMAY_FORMAT_OK);
            MMAY_EXPECT(first.initialized && second.initialized);
            if (first.initialized && second.initialized) {
                MMAY_EXPECT(first.output.length == strlen(expected));
                MMAY_EXPECT(first.output.length != strlen(expected)
                    || memcmp(first.output.data, expected, strlen(expected)) == 0);
                if (second.changed) {
                    fprintf(stderr, "braced layout changed on second pass:\n%.*s\n=>\n%.*s\n",
                        (int)first.output.length, first.output.data,
                        (int)second.output.length, second.output.data);
                }
                MMAY_EXPECT(!second.changed);
                MMAY_EXPECT(second.output.length == first.output.length);
                MMAY_EXPECT(second.output.length != first.output.length
                    || memcmp(second.output.data, first.output.data, first.output.length) == 0);
                MMAY_EXPECT(mmay_formatter_comments_equal(&original.lexed, &formatted.lexed));
                MMAY_EXPECT(mmay_ast_project(&original.parsed.tree, mmay_system_allocator(),
                    mmay_ast_default_options(), &before) == MMAY_AST_OK);
                MMAY_EXPECT(mmay_ast_project(&formatted.parsed.tree, mmay_system_allocator(),
                    mmay_ast_default_options(), &after) == MMAY_AST_OK);
                if (before.initialized && after.initialized) {
                    MMAY_EXPECT(mmay_formatter_ast_semantically_equal(
                        before.root, original.snapshot, after.root, formatted.snapshot));
                }
            }
            if (after.initialized) mmay_ast_tree_dispose(&after);
            if (before.initialized) mmay_ast_tree_dispose(&before);
        } else {
            MMAY_EXPECT(status == MMAY_FORMAT_ALLOCATION_FAILED);
            MMAY_EXPECT(!second.initialized);
        }
        if (second.initialized) mmay_format_result_dispose(&second);
        mmay_formatter_fixture_dispose(&formatted);
        if (first.initialized) mmay_format_result_dispose(&first);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        iteration += 1;
    } while (iteration <= attempts);
    MMAY_EXPECT(attempts > 0);
    mmay_formatter_fixture_dispose(&original);
}

void mmay_test_formatter_braced_groups(void) {
    static const char compact[] =
        "fn inspect(value: Item): int {\n"
        "  return match value {\n"
        "    Item { aaaaaaaaaaaaaaaaaaa: a, bbbbbbbbbbbbbbbbbbb: b, cccccccccccccccccccccc: c } => a,\n"
        "  };\n"
        "}\n";
    mmay_formatter_braced_case(
        "fn inspect(value:Item):int{return match value{"
        "Item{aaaaaaaaaaaaaaaaaaa:a,bbbbbbbbbbbbbbbbbbb:b,cccccccccccccccccccccc:c}=>a,};}",
        compact);
    mmay_formatter_braced_case(compact, compact);
    mmay_formatter_braced_case(
        "fn inspect(value:Item):int{return match value{Item{\n"
        "aaaaaaaaaaaaaaaaaaa: a,\n"
        "bbbbbbbbbbbbbbbbbbb: b,\n"
        "cccccccccccccccccccccc: c,\n"
        "}=>a,};}", compact);
    mmay_formatter_braced_case(
        "fn inspect(value:Item):int{return match value{Item{"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:a,bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb:b,"
        "cccccccccccccccccccccccccccccccccccccc:c}=>a,};}",
        "fn inspect(value: Item): int {\n"
        "  return match value {\n"
        "    Item {\n"
        "      aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa: a,\n"
        "      bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb: b,\n"
        "      cccccccccccccccccccccccccccccccccccccc: c,\n"
        "    } => a,\n"
        "  };\n"
        "}\n");
    mmay_formatter_braced_case(
        "fn inspect(value:Item):int{return match value{Item{a, // attached\n b}=>a,};}",
        "fn inspect(value: Item): int {\n"
        "  return match value {\n"
        "    Item {\n"
        "      a,\n"
        "      // attached\n"
        "      b,\n"
        "    } => a,\n"
        "  };\n"
        "}\n");
}
