#include "test_parser_support.h"

static bool mmay_parser_read_file(
    const char *path,
    unsigned char *bytes,
    size_t capacity,
    size_t *length
) {
    FILE *file = fopen(path, "rb");
    size_t count = 0;
    bool read_ok = false;
    bool close_ok = false;
    if (file == NULL || capacity == 0) {
        return false;
    }
    count = fread(bytes, 1, capacity, file);
    read_ok = ferror(file) == 0 && feof(file) != 0;
    close_ok = fclose(file) == 0;
    if (!read_ok || !close_ok) {
        return false;
    }
    *length = count;
    return true;
}

static void mmay_parser_append(
    char *output,
    size_t capacity,
    size_t *length,
    const char *text
) {
    size_t text_length = strlen(text);
    if (*length >= capacity || text_length >= capacity - *length) {
        return;
    }
    memcpy(output + *length, text, text_length);
    *length += text_length;
    output[*length] = '\0';
}

static void mmay_dump_syntax_node(
    const MarimayGreenNode *node,
    size_t depth,
    char *output,
    size_t capacity,
    size_t *length
) {
    size_t index = 0;
    char line[160];
    int written = 0;
    for (index = 0; index < depth; index += 1) {
        mmay_parser_append(output, capacity, length, "  ");
    }
    written = snprintf(
        line,
        sizeof(line),
        "%s len=%zu\n",
        mmay_syntax_kind_name(node->kind),
        node->text_length
    );
    if (written < 0 || (size_t)written >= sizeof(line)) {
        return;
    }
    mmay_parser_append(output, capacity, length, line);
    for (index = 0; index < node->child_count; index += 1) {
        const MarimaySyntaxElement *element = mmay_syntax_child(node, index);
        size_t indent = 0;
        if (element == NULL) {
            return;
        }
        if (element->kind == MMAY_SYNTAX_ELEMENT_NODE) {
            mmay_dump_syntax_node(
                element->as.node,
                depth + 1,
                output,
                capacity,
                length
            );
        } else {
            for (indent = 0; indent < depth + 1; indent += 1) {
                mmay_parser_append(output, capacity, length, "  ");
            }
            written = snprintf(
                line,
                sizeof(line),
                "%s %zu..%zu%s\n",
                mmay_token_kind_name(element->as.token.kind),
                element->as.token.start,
                element->as.token.end,
                element->as.token.synthetic ? " synthetic" : ""
            );
            if (written < 0 || (size_t)written >= sizeof(line)) {
                return;
            }
            mmay_parser_append(output, capacity, length, line);
        }
    }
}

void mmay_test_parser_snapshot(void) {
    unsigned char source[4096];
    size_t source_length = 0;
    MarimayParseFixture fixture;
    char dump[16384] = {0};
    size_t dump_length = 0;
    MMAY_EXPECT(mmay_parser_read_file(
        "syntax/lexer/lossless.my",
        source,
        sizeof(source),
        &source_length
    ));
    MMAY_EXPECT(mmay_parse_fixture_init_bytes(&fixture, source, source_length));
    if (!fixture.parsed.initialized) {
        return;
    }
    MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
    MMAY_EXPECT(fixture.parsed.diagnostics.length == 0);
    MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
    mmay_dump_syntax_node(
        fixture.parsed.tree.root,
        0,
        dump,
        sizeof(dump),
        &dump_length
    );
    MMAY_EXPECT_SNAPSHOT("parser-lossless.tree", dump);
    mmay_parse_fixture_dispose(&fixture);
}

void mmay_test_parser_empty_source(void) {
    MarimayParseFixture fixture;
    bool initialized = mmay_parse_fixture_init_bytes(
        &fixture,
        (const unsigned char *)"",
        0
    );
    MMAY_EXPECT(initialized);
    if (!initialized) {
        return;
    }
    MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
    MMAY_EXPECT(fixture.parsed.diagnostics.length == 0);
    MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
    MMAY_EXPECT(fixture.parsed.tree.root->text_length == 0);
    mmay_parse_fixture_dispose(&fixture);
}

void mmay_test_parser_examples(void) {
    static const char *const paths[] = {
        "examples/concurrency.my",
        "examples/matrix.my",
        "examples/nominal_cp1.my",
        "examples/service.my",
        "examples/tour.my",
        "examples/tour_test.my",
        "syntax/parser/stdout-preview.my"
    };
    unsigned char source[32768];
    size_t path_index = 0;
    for (path_index = 0; path_index < sizeof(paths) / sizeof(paths[0]); path_index += 1) {
        size_t source_length = 0;
        MarimayParseFixture fixture;
        MMAY_EXPECT(mmay_parser_read_file(
            paths[path_index],
            source,
            sizeof(source),
            &source_length
        ));
        MMAY_EXPECT(mmay_parse_fixture_init_bytes(&fixture, source, source_length));
        if (fixture.parsed.initialized) {
            MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
            MMAY_EXPECT(fixture.parsed.diagnostics.length == 0);
            MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
            mmay_parse_fixture_dispose(&fixture);
        }
    }
}

void mmay_test_parser_all_productions(void) {
    unsigned char source[32768];
    size_t source_length = 0;
    size_t kind = 0;
    MarimayParseFixture fixture;
    bool initialized = false;
    MMAY_EXPECT(mmay_parser_read_file(
        "syntax/parser/all-productions.my",
        source,
        sizeof(source),
        &source_length
    ));
    initialized = mmay_parse_fixture_init_bytes(
        &fixture,
        source,
        source_length
    );
    MMAY_EXPECT(initialized);
    if (!initialized) {
        return;
    }
    MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
    MMAY_EXPECT(fixture.parsed.diagnostics.length == 0);
    MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
    for (kind = 0; kind < MMAY_SYNTAX_KIND_COUNT; kind += 1) {
        size_t count = mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            (MarimaySyntaxKind)kind
        );
        if ((MarimaySyntaxKind)kind == MMAY_SYNTAX_ERROR) {
            MMAY_EXPECT(count == 0);
        } else if (count == 0) {
            fprintf(
                stderr,
                "all-productions.my misses syntax kind %s\n",
                mmay_syntax_kind_name((MarimaySyntaxKind)kind)
            );
            MMAY_EXPECT(count != 0);
        }
    }
    mmay_parse_fixture_dispose(&fixture);
}

void mmay_test_parser_recovery(void) {
    static const char source[] =
        "fn broken(value int): Result<int, E> {\n"
        "  const = 1;\n"
        "  return value\n"
        "}\n"
        "enum Bad { 123, Good }\n"
        "nonsense;\n"
        "fn after(): void {}\n";
    MarimayParseFixture fixture;
    MMAY_EXPECT(mmay_parse_fixture_init(&fixture, source));
    if (!fixture.parsed.initialized) {
        return;
    }
    MMAY_EXPECT(fixture.parsed.diagnostics.length >= 4);
    MMAY_EXPECT(mmay_parse_has_diagnostic(
        &fixture.parsed,
        MMAY_DIAGNOSTIC_EXPECTED_TOKEN
    ));
    MMAY_EXPECT(mmay_parse_has_diagnostic(
        &fixture.parsed,
        MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
    ));
    MMAY_EXPECT(mmay_parse_has_diagnostic(
        &fixture.parsed,
        MMAY_DIAGNOSTIC_INVALID_DECLARATION
    ));
    MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
    mmay_parse_fixture_dispose(&fixture);
}

void mmay_test_parser_delimiter_recovery(void) {
    static const char source[] = "fn broken(): void { call((1); }";
    MarimayParseFixture fixture;
    bool initialized = mmay_parse_fixture_init(&fixture, source);
    MMAY_EXPECT(initialized);
    if (!initialized) {
        return;
    }
    MMAY_EXPECT(mmay_parse_has_diagnostic(
        &fixture.parsed,
        MMAY_DIAGNOSTIC_UNTERMINATED_DELIMITER
    ));
    MMAY_EXPECT(mmay_parse_has_diagnostic(
        &fixture.parsed,
        MMAY_DIAGNOSTIC_EXPECTED_TOKEN
    ));
    MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
    mmay_parse_fixture_dispose(&fixture);
}
