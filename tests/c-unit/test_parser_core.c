#include "test_parser_support.h"

static bool ruda_parser_read_file(
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

static void ruda_parser_append(
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

static void ruda_dump_syntax_node(
    const RudaGreenNode *node,
    size_t depth,
    char *output,
    size_t capacity,
    size_t *length
) {
    size_t index = 0;
    char line[160];
    int written = 0;
    for (index = 0; index < depth; index += 1) {
        ruda_parser_append(output, capacity, length, "  ");
    }
    written = snprintf(
        line,
        sizeof(line),
        "%s len=%zu\n",
        ruda_syntax_kind_name(node->kind),
        node->text_length
    );
    if (written < 0 || (size_t)written >= sizeof(line)) {
        return;
    }
    ruda_parser_append(output, capacity, length, line);
    for (index = 0; index < node->child_count; index += 1) {
        const RudaSyntaxElement *element = ruda_syntax_child(node, index);
        size_t indent = 0;
        if (element == NULL) {
            return;
        }
        if (element->kind == RUDA_SYNTAX_ELEMENT_NODE) {
            ruda_dump_syntax_node(
                element->as.node,
                depth + 1,
                output,
                capacity,
                length
            );
        } else {
            for (indent = 0; indent < depth + 1; indent += 1) {
                ruda_parser_append(output, capacity, length, "  ");
            }
            written = snprintf(
                line,
                sizeof(line),
                "%s %zu..%zu%s\n",
                ruda_token_kind_name(element->as.token.kind),
                element->as.token.start,
                element->as.token.end,
                element->as.token.synthetic ? " synthetic" : ""
            );
            if (written < 0 || (size_t)written >= sizeof(line)) {
                return;
            }
            ruda_parser_append(output, capacity, length, line);
        }
    }
}

void ruda_test_parser_snapshot(void) {
    unsigned char source[4096];
    size_t source_length = 0;
    RudaParseFixture fixture;
    char dump[16384] = {0};
    size_t dump_length = 0;
    RUDA_EXPECT(ruda_parser_read_file(
        "syntax/lexer/lossless.ruda",
        source,
        sizeof(source),
        &source_length
    ));
    RUDA_EXPECT(ruda_parse_fixture_init_bytes(&fixture, source, source_length));
    if (!fixture.parsed.initialized) {
        return;
    }
    RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
    RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
    RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
    ruda_dump_syntax_node(
        fixture.parsed.tree.root,
        0,
        dump,
        sizeof(dump),
        &dump_length
    );
    RUDA_EXPECT_SNAPSHOT("parser-lossless.tree", dump);
    ruda_parse_fixture_dispose(&fixture);
}

void ruda_test_parser_empty_source(void) {
    RudaParseFixture fixture;
    bool initialized = ruda_parse_fixture_init_bytes(
        &fixture,
        (const unsigned char *)"",
        0
    );
    RUDA_EXPECT(initialized);
    if (!initialized) {
        return;
    }
    RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
    RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
    RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
    RUDA_EXPECT(fixture.parsed.tree.root->text_length == 0);
    ruda_parse_fixture_dispose(&fixture);
}

void ruda_test_parser_examples(void) {
    static const char *const paths[] = {
        "examples/concurrency.ruda",
        "examples/matrix.ruda",
        "examples/nominal_cp1.ruda",
        "examples/service.ruda",
        "examples/tour.ruda",
        "examples/tour_test.ruda",
        "syntax/parser/stdout-preview.ruda"
    };
    unsigned char source[32768];
    size_t path_index = 0;
    for (path_index = 0; path_index < sizeof(paths) / sizeof(paths[0]); path_index += 1) {
        size_t source_length = 0;
        RudaParseFixture fixture;
        RUDA_EXPECT(ruda_parser_read_file(
            paths[path_index],
            source,
            sizeof(source),
            &source_length
        ));
        RUDA_EXPECT(ruda_parse_fixture_init_bytes(&fixture, source, source_length));
        if (fixture.parsed.initialized) {
            RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
            RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
            RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
            ruda_parse_fixture_dispose(&fixture);
        }
    }
}

void ruda_test_parser_all_productions(void) {
    unsigned char source[32768];
    size_t source_length = 0;
    size_t kind = 0;
    RudaParseFixture fixture;
    bool initialized = false;
    RUDA_EXPECT(ruda_parser_read_file(
        "syntax/parser/all-productions.ruda",
        source,
        sizeof(source),
        &source_length
    ));
    initialized = ruda_parse_fixture_init_bytes(
        &fixture,
        source,
        source_length
    );
    RUDA_EXPECT(initialized);
    if (!initialized) {
        return;
    }
    RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
    RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
    RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
    for (kind = 0; kind < RUDA_SYNTAX_KIND_COUNT; kind += 1) {
        size_t count = ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            (RudaSyntaxKind)kind
        );
        if ((RudaSyntaxKind)kind == RUDA_SYNTAX_ERROR) {
            RUDA_EXPECT(count == 0);
        } else if (count == 0) {
            fprintf(
                stderr,
                "all-productions.ruda misses syntax kind %s\n",
                ruda_syntax_kind_name((RudaSyntaxKind)kind)
            );
            RUDA_EXPECT(count != 0);
        }
    }
    ruda_parse_fixture_dispose(&fixture);
}

void ruda_test_parser_recovery(void) {
    static const char source[] =
        "fn broken(value int): Result<int, E> {\n"
        "  const = 1;\n"
        "  return value\n"
        "}\n"
        "enum Bad { 123, Good }\n"
        "nonsense;\n"
        "fn after(): void {}\n";
    RudaParseFixture fixture;
    RUDA_EXPECT(ruda_parse_fixture_init(&fixture, source));
    if (!fixture.parsed.initialized) {
        return;
    }
    RUDA_EXPECT(fixture.parsed.diagnostics.length >= 4);
    RUDA_EXPECT(ruda_parse_has_diagnostic(
        &fixture.parsed,
        RUDA_DIAGNOSTIC_EXPECTED_TOKEN
    ));
    RUDA_EXPECT(ruda_parse_has_diagnostic(
        &fixture.parsed,
        RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
    ));
    RUDA_EXPECT(ruda_parse_has_diagnostic(
        &fixture.parsed,
        RUDA_DIAGNOSTIC_INVALID_DECLARATION
    ));
    RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
    ruda_parse_fixture_dispose(&fixture);
}

void ruda_test_parser_delimiter_recovery(void) {
    static const char source[] = "fn broken(): void { call((1); }";
    RudaParseFixture fixture;
    bool initialized = ruda_parse_fixture_init(&fixture, source);
    RUDA_EXPECT(initialized);
    if (!initialized) {
        return;
    }
    RUDA_EXPECT(ruda_parse_has_diagnostic(
        &fixture.parsed,
        RUDA_DIAGNOSTIC_UNTERMINATED_DELIMITER
    ));
    RUDA_EXPECT(ruda_parse_has_diagnostic(
        &fixture.parsed,
        RUDA_DIAGNOSTIC_EXPECTED_TOKEN
    ));
    RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
    ruda_parse_fixture_dispose(&fixture);
}
