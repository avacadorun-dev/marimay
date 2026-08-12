#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/generated/diagnostic_codes.h"
#include "ruda/lex/lexer.h"
#include "ruda/parse/parser.h"
#include "ruda/source/source_manager.h"
#include "ruda/syntax/syntax.h"

typedef struct RudaParseFixture {
    RudaSourceManager sources;
    const RudaSourceSnapshot *snapshot;
    RudaLexResult lexed;
    RudaParseResult parsed;
} RudaParseFixture;

static bool ruda_parse_fixture_init_bytes(
    RudaParseFixture *fixture,
    const unsigned char *bytes,
    size_t length
) {
    memset(fixture, 0, sizeof(*fixture));
    if (!ruda_source_manager_init(&fixture->sources, ruda_system_allocator())) {
        return false;
    }
    if (ruda_source_manager_add(
            &fixture->sources,
            "parse.ruda",
            strlen("parse.ruda"),
            bytes,
            length,
            &fixture->snapshot
        ) != RUDA_SOURCE_OK) {
        ruda_source_manager_dispose(&fixture->sources);
        return false;
    }
    if (ruda_lex_snapshot(
            fixture->snapshot,
            ruda_system_allocator(),
            ruda_lex_default_options(),
            &fixture->lexed
        ) != RUDA_LEX_OK) {
        ruda_source_manager_dispose(&fixture->sources);
        return false;
    }
    if (ruda_parse_file(
            &fixture->lexed,
            ruda_system_allocator(),
            ruda_parse_default_options(),
            &fixture->parsed
        ) != RUDA_PARSE_OK) {
        ruda_lex_result_dispose(&fixture->lexed);
        ruda_source_manager_dispose(&fixture->sources);
        return false;
    }
    return true;
}

static bool ruda_parse_fixture_init(RudaParseFixture *fixture, const char *text) {
    return ruda_parse_fixture_init_bytes(
        fixture,
        (const unsigned char *)text,
        strlen(text)
    );
}

static void ruda_parse_fixture_dispose(RudaParseFixture *fixture) {
    ruda_parse_result_dispose(&fixture->parsed);
    ruda_lex_result_dispose(&fixture->lexed);
    ruda_source_manager_dispose(&fixture->sources);
}

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

static void ruda_test_parser_snapshot(void) {
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

static void ruda_test_parser_empty_source(void) {
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

static size_t ruda_syntax_count_kind(
    const RudaGreenNode *node,
    RudaSyntaxKind kind
);

static void ruda_test_parser_examples(void) {
    static const char *const paths[] = {
        "examples/concurrency.ruda",
        "examples/service.ruda",
        "examples/tour.ruda",
        "examples/tour_test.ruda"
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

static void ruda_test_parser_all_productions(void) {
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

static bool ruda_parse_has_diagnostic(
    const RudaParseResult *result,
    const char *code
) {
    size_t index = 0;
    for (index = 0; index < result->diagnostics.length; index += 1) {
        const RudaParseDiagnostic *diagnostic = ruda_parse_diagnostic(result, index);
        if (diagnostic != NULL && strcmp(diagnostic->code, code) == 0) {
            return true;
        }
    }
    return false;
}

static size_t ruda_syntax_count_kind(
    const RudaGreenNode *node,
    RudaSyntaxKind kind
) {
    size_t count = node != NULL && node->kind == kind ? 1 : 0;
    size_t index = 0;
    if (node == NULL) {
        return 0;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaSyntaxElement *element = ruda_syntax_child(node, index);
        if (element != NULL && element->kind == RUDA_SYNTAX_ELEMENT_NODE) {
            count += ruda_syntax_count_kind(element->as.node, kind);
        }
    }
    return count;
}

static size_t ruda_syntax_count_synthetic_tokens(const RudaGreenNode *node) {
    size_t count = 0;
    size_t index = 0;
    if (node == NULL) {
        return 0;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaSyntaxElement *element = ruda_syntax_child(node, index);
        if (element == NULL) {
            continue;
        }
        if (element->kind == RUDA_SYNTAX_ELEMENT_NODE) {
            count += ruda_syntax_count_synthetic_tokens(element->as.node);
        } else if (element->as.token.synthetic) {
            count += 1;
        }
    }
    return count;
}

static const RudaGreenNode *ruda_syntax_direct_nth_kind(
    const RudaGreenNode *node,
    RudaSyntaxKind kind,
    size_t ordinal
) {
    size_t index = 0;
    size_t found = 0;
    if (node == NULL) {
        return NULL;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaSyntaxElement *element = ruda_syntax_child(node, index);
        if (element == NULL || element->kind != RUDA_SYNTAX_ELEMENT_NODE
            || element->as.node->kind != kind) {
            continue;
        }
        if (found == ordinal) {
            return element->as.node;
        }
        found += 1;
    }
    return NULL;
}

static bool ruda_syntax_has_direct_node(
    const RudaGreenNode *node,
    RudaSyntaxKind kind
) {
    size_t index = 0;
    if (node == NULL) {
        return false;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaSyntaxElement *element = ruda_syntax_child(node, index);
        if (element != NULL && element->kind == RUDA_SYNTAX_ELEMENT_NODE
            && element->as.node->kind == kind) {
            return true;
        }
    }
    return false;
}

static const RudaGreenNode *ruda_syntax_find_kind_with_direct_token(
    const RudaGreenNode *node,
    RudaSyntaxKind kind,
    RudaTokenKind token_kind
) {
    size_t index = 0;
    bool has_token = false;
    if (node == NULL) {
        return NULL;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaSyntaxElement *element = ruda_syntax_child(node, index);
        if (element != NULL && element->kind == RUDA_SYNTAX_ELEMENT_TOKEN
            && element->as.token.kind == token_kind) {
            has_token = true;
        }
    }
    if (node->kind == kind && has_token) {
        return node;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaSyntaxElement *element = ruda_syntax_child(node, index);
        const RudaGreenNode *found = NULL;
        if (element == NULL || element->kind != RUDA_SYNTAX_ELEMENT_NODE) {
            continue;
        }
        found = ruda_syntax_find_kind_with_direct_token(
            element->as.node,
            kind,
            token_kind
        );
        if (found != NULL) {
            return found;
        }
    }
    return NULL;
}

static void ruda_test_parser_recovery(void) {
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

static void ruda_test_parser_delimiter_recovery(void) {
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

static void ruda_test_parser_declarations(void) {
    static const char source[] =
        "import { Item as Renamed, other, } from \"pkg\";\n"
        "type Id = i64;\n"
        "type Box<T> { public mutable value: T; }\n"
        "enum Maybe<T> { None, Some(T), Named { value: T; }, }\n"
        "interface Reader<T> { read(buffer: T): Result<int, Error>; Close; }\n"
        "@link(\"c\") extern \"c\" fn call(value: i32): i32;\n"
        "const answer: int = 42;\n"
        "@fuzz fn fuzzBytes(input: Array<byte>): void { const _ = input; }\n"
        "@test fn ok(): void { expect.equal(answer, 42); }\n";
    RudaParseFixture fixture;
    RUDA_EXPECT(ruda_parse_fixture_init(&fixture, source));
    if (!fixture.parsed.initialized) {
        return;
    }
    RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
    RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
    RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
    ruda_parse_fixture_dispose(&fixture);
}

static void ruda_test_parser_declaration_grammar(void) {
    static const char valid[] =
        "import * as http from \"std/http\";\n"
        "import { Item, Other as Alias, } from \"pkg\";\n"
        "@tag(1) public type Record<T> { public mutable value: T; }\n"
        "type Alias<T> = Result<T, Error>;\n"
        "enum Choice<T> { None, One(T), Named { value: T; }, }\n"
        "interface Reader<T> { read(value: T): Result<T, Error>; Close; }\n"
        "public unsafe fn local(value: int): int { return value; }\n"
        "extern \"c\" unsafe fn foreign(value: int): int;\n"
        "const answer: int = 42;\n"
        "@test fn emptyTest(): void {}\n"
        "@fuzz fn emptyFuzz(input: bytes): void {}\n";
    static const struct {
        const char *source;
        RudaSyntaxKind kind;
        const char *diagnostic;
    } invalid[] = {
        {
            "public import * as x from \"pkg\"; fn after(): void {}",
            RUDA_SYNTAX_IMPORT_DECL,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "@tag import * as x from \"pkg\"; fn after(): void {}",
            RUDA_SYNTAX_IMPORT_DECL,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "import * from \"pkg\"; fn after(): void {}",
            RUDA_SYNTAX_IMPORT_CLAUSE,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "import { First Second } from \"pkg\"; fn after(): void {}",
            RUDA_SYNTAX_IMPORT_DECL,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "type Bad { value: int } fn after(): void {}",
            RUDA_SYNTAX_FIELD_DECL,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "enum Bad { First Second } fn after(): void {}",
            RUDA_SYNTAX_ENUM_VARIANT,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "interface Bad { call(value: int) int; } fn after(): void {}",
            RUDA_SYNTAX_INTERFACE_ITEM,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "const missing: int; fn after(): void {}",
            RUDA_SYNTAX_CONST_DECL,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "extern \"c\" fn bad(): void {} fn after(): void {}",
            RUDA_SYNTAX_EXTERN_DECL,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        }
    };
    RudaParseFixture fixture;
    size_t index = 0;
    RUDA_EXPECT(ruda_parse_fixture_init(&fixture, valid));
    if (fixture.parsed.initialized) {
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_IMPORT_DECL
        ) == 2);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_ENUM_VARIANT
        ) == 3);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL
        ) == 3);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_EXTERN_DECL
        ) == 1);
        ruda_parse_fixture_dispose(&fixture);
    }
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        bool initialized = ruda_parse_fixture_init(&fixture, invalid[index].source);
        RUDA_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_parse_has_diagnostic(
            &fixture.parsed,
            invalid[index].diagnostic
        ));
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            invalid[index].kind
        ) != 0);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL
        ) >= 1);
        ruda_parse_fixture_dispose(&fixture);
    }
}

static void ruda_test_parser_keyword_boundaries(void) {
    static const char source[] =
        "type Contextual {\n"
        "  as: int; break: int; case: int; const: int; continue: int;\n"
        "  else: int; enum: int;\n"
        "  extern: int; fn: int; for: int; from: int; import: int;\n"
        "  interface: int; let: int; mutable: int; of: int; public: int;\n"
        "  scope: int; type: int; unsafe: int; yield: int;\n"
        "}\n"
        "fn type(as: int, const: int, scope: int, unsafe: int, yield: int): int {\n"
        "  const public = as;\n"
        "  task.yield();\n"
        "  yield();\n"
        "  break(); const(); continue(); for(); let(); scope(); unsafe();\n"
        "  for + scope + unsafe;\n"
        "  const + let;\n"
        "  if true {} else();\n"
        "  const scopeValue = scope {};\n"
        "  const unsafeValue = unsafe {};\n"
        "  return match true {\n"
        "    case true => { yield public; }\n"
        "    case false => yield;\n"
        "  };\n"
        "}\n"
        "fn break(): void {} fn const(): void {} fn continue(): void {}\n"
        "fn else(): void {} fn enum(): void {} fn extern(): void {}\n"
        "fn fn(): void {} fn for(): void {} fn import(): void {}\n"
        "fn interface(): void {} fn let(): void {} fn scope(): void {}\n"
        "fn unsafe(): void {}\n"
        "fn in(): void {} fn any(): void {} fn async(): void {}\n"
        "fn await(): void {} fn class(): void {} fn extends(): void {}\n"
        "fn impl(): void {} fn throws(): void {} fn test(): void {}\n"
        "fn fuzz(): void {} fn panic(): void {}\n";
    RudaParseFixture fixture;
    bool initialized = ruda_parse_fixture_init(&fixture, source);
    RUDA_EXPECT(initialized);
    if (!initialized) {
        return;
    }
    RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
    RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
    RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_FUNCTION_DECL
    ) == 25);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_YIELD_STATEMENT
    ) == 1);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_CALL_EXPRESSION
    ) == 10);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_RECORD_EXPRESSION
    ) == 2);
    ruda_parse_fixture_dispose(&fixture);

    {
        static const char *const hard_names[] = {
            "defer", "false", "if", "match", "return", "spawn", "true",
            "while"
        };
        size_t hard_index = 0;
        for (hard_index = 0;
            hard_index < sizeof(hard_names) / sizeof(hard_names[0]);
            hard_index += 1) {
            char invalid_source[96] = {0};
            int written = snprintf(
                invalid_source,
                sizeof(invalid_source),
                "fn %s(): void {} fn after(): void {}",
                hard_names[hard_index]
            );
            RUDA_EXPECT(written > 0
                && (size_t)written < sizeof(invalid_source));
            initialized = ruda_parse_fixture_init(&fixture, invalid_source);
            RUDA_EXPECT(initialized);
            if (!initialized) {
                continue;
            }
            RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
            RUDA_EXPECT(ruda_parse_has_diagnostic(
                &fixture.parsed,
                RUDA_DIAGNOSTIC_EXPECTED_TOKEN
            ));
            RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
            ruda_parse_fixture_dispose(&fixture);
        }
    }
}

static void ruda_test_parser_declaration_recovery(void) {
    static const struct {
        const char *source;
        RudaSyntaxKind kinds[4];
        size_t kind_count;
        size_t minimum_function_count;
    } cases[] = {
        {
            "@() fn decorated(): void {} fn after(): void {}",
            {RUDA_SYNTAX_ATTRIBUTE},
            1,
            2
        },
        {
            "import { , Good } from \"pkg\"; fn after(): void {}",
            {
                RUDA_SYNTAX_IMPORT_DECL,
                RUDA_SYNTAX_IMPORT_CLAUSE,
                RUDA_SYNTAX_IMPORT_ITEM
            },
            3,
            1
        },
        {
            "type = int; fn after(): void {}",
            {RUDA_SYNTAX_TYPE_ALIAS_DECL},
            1,
            1
        },
        {
            "type Bad { : int; } fn after(): void {}",
            {RUDA_SYNTAX_RECORD_DECL, RUDA_SYNTAX_FIELD_DECL},
            2,
            1
        },
        {
            "enum Bad { , Good } fn after(): void {}",
            {RUDA_SYNTAX_ENUM_DECL, RUDA_SYNTAX_ENUM_VARIANT},
            2,
            1
        },
        {
            "enum Bad { Broken { : int; }, } fn after(): void {}",
            {RUDA_SYNTAX_ENUM_DECL, RUDA_SYNTAX_NAMED_PAYLOAD},
            2,
            1
        },
        {
            "interface Bad { (): int; } fn after(): void {}",
            {RUDA_SYNTAX_INTERFACE_DECL, RUDA_SYNTAX_INTERFACE_ITEM},
            2,
            1
        },
        {
            "fn (value int): void {} fn after(): void {}",
            {
                RUDA_SYNTAX_FUNCTION_DECL,
                RUDA_SYNTAX_PARAMETERS,
                RUDA_SYNTAX_PARAMETER,
                RUDA_SYNTAX_BLOCK
            },
            4,
            2
        },
        {
            "fn broken(value: int: void {} fn after(): void {}",
            {RUDA_SYNTAX_FUNCTION_DECL, RUDA_SYNTAX_PARAMETERS},
            2,
            2
        },
        {
            "fn broken(): void { const value = 1;",
            {RUDA_SYNTAX_FUNCTION_DECL, RUDA_SYNTAX_BLOCK},
            2,
            1
        },
        {
            "extern fn foreign(): void; fn after(): void {}",
            {RUDA_SYNTAX_EXTERN_DECL},
            1,
            1
        },
        {
            "const : int = 1; fn after(): void {}",
            {RUDA_SYNTAX_CONST_DECL},
            1,
            1
        },
        {
            "fn broken(): void { let : int = 1; } fn after(): void {}",
            {RUDA_SYNTAX_LET_DECL},
            1,
            2
        }
    };
    static const char declaration_starters[] =
        "bad extern \"c\" fn foreign(): void;\n"
        "bad const kept: int = 1;\n"
        "bad @test fn test_kept(): void {}\n"
        "bad @fuzz fn fuzz_kept(input: bytes): void {}\n"
        "bad unsafe fn unsafe_kept(): void {}\n"
        "bad @tag fn tagged(): void {}\n"
        "bad public fn visible(): void {}\n";
    size_t case_index = 0;
    for (case_index = 0; case_index < sizeof(cases) / sizeof(cases[0]); case_index += 1) {
        RudaParseFixture fixture;
        size_t kind_index = 0;
        bool initialized = ruda_parse_fixture_init(&fixture, cases[case_index].source);
        RUDA_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(fixture.parsed.diagnostics.length != 0);
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
        RUDA_EXPECT(ruda_syntax_count_synthetic_tokens(
            fixture.parsed.tree.root
        ) != 0);
        for (kind_index = 0; kind_index < cases[case_index].kind_count; kind_index += 1) {
            RUDA_EXPECT(ruda_syntax_count_kind(
                fixture.parsed.tree.root,
                cases[case_index].kinds[kind_index]
            ) != 0);
        }
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL
        ) >= cases[case_index].minimum_function_count);
        ruda_parse_fixture_dispose(&fixture);
    }

    {
        RudaParseFixture fixture;
        bool initialized = ruda_parse_fixture_init(&fixture, declaration_starters);
        RUDA_EXPECT(initialized);
        if (!initialized) {
            return;
        }
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(fixture.parsed.diagnostics.length == 7);
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_ERROR
        ) == 7);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_EXTERN_DECL
        ) == 1);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_CONST_DECL
        ) == 1);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_ATTRIBUTE
        ) == 3);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL
        ) == 5);
        ruda_parse_fixture_dispose(&fixture);
    }
}

static void ruda_test_parser_pratt_expressions(void) {
    static const char source[] =
        "fn expressions(a: int, b: int, c: int): void {\n"
        "  const precedence = a + b * c;\n"
        "  const left = a - b - c;\n"
        "  let assigned = a;\n"
        "  assigned = b;\n"
        "  const value = service.load<Result<int, Error>>(a, b)[0]?;\n"
        "  const flag = !a == -b && true || false;\n"
        "  const array = [a, b + c,];\n"
        "  const record = Box { value: a };\n"
        "  const closure = (error: Error): Result<int, Error> => service.handle(error);\n"
        "  const spawned = spawn service.load(a);\n"
        "  const matched = match a { case 0 => b; case _ if c > 0 => c; };\n"
        "}\n";
    RudaParseFixture fixture;
    const RudaGreenNode *plus = NULL;
    const RudaGreenNode *assignment = NULL;
    bool initialized = ruda_parse_fixture_init(&fixture, source);
    RUDA_EXPECT(initialized);
    if (!initialized) {
        return;
    }
    RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
    RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
    RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_BINARY_EXPRESSION
    ) >= 7);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_ASSIGNMENT_EXPRESSION
    ) == 1);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_MEMBER_EXPRESSION
    ) >= 3);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_CALL_EXPRESSION
    ) >= 3);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_TYPE_ARGUMENTS
    ) >= 1);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_INDEX_EXPRESSION
    ) == 1);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_TRY_EXPRESSION
    ) == 1);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_UNARY_EXPRESSION
    ) == 2);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_ARRAY_EXPRESSION
    ) == 1);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_RECORD_EXPRESSION
    ) == 1);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_CLOSURE_EXPRESSION
    ) == 1);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_SPAWN_EXPRESSION
    ) == 1);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_MATCH_EXPRESSION
    ) == 1);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_MATCH_ARM
    ) == 2);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_PATTERN
    ) == 2);

    plus = ruda_syntax_find_kind_with_direct_token(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_BINARY_EXPRESSION,
        RUDA_TOKEN_PLUS
    );
    RUDA_EXPECT(plus != NULL);
    RUDA_EXPECT(ruda_syntax_has_direct_node(plus, RUDA_SYNTAX_BINARY_EXPRESSION));
    assignment = ruda_syntax_find_kind_with_direct_token(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_ASSIGNMENT_EXPRESSION,
        RUDA_TOKEN_EQUAL
    );
    RUDA_EXPECT(assignment != NULL);
    RUDA_EXPECT(ruda_syntax_has_direct_node(
        assignment,
        RUDA_SYNTAX_ASSIGNMENT_EXPRESSION
    ) == false);
    ruda_parse_fixture_dispose(&fixture);
}

static void ruda_test_parser_expression_grammar(void) {
    static const char valid[] =
        "fn operators(a: int, b: int, c: int): void {\n"
        "  let target = a;\n"
        "  target = b; target += b; target -= b; target *= b;\n"
        "  target /= b; target %= b; target &= b; target |= b;\n"
        "  target ^= b; target <<= b; target >>= b;\n"
        "  const arithmetic = a * b / c % a + b - c << a >> b;\n"
        "  const compared = a < b && a <= b && a > b && a >= b;\n"
        "  const equal = a == b || a != c;\n"
        "  const bits = a & b ^ c | a;\n"
        "  const unary = !true || -a < ~b;\n"
        "  const postfix = service.member<Result<int, Error>>(a)[b]?.next;\n"
        "  const values = [0, 1.5, \"s\", r\"\\\\\", b\"x\", 'x', true, false, $\"x {a}\"];\n"
        "  const grouped = (a + b);\n"
        "  const empty = [];\n"
        "  const record = pkg.Item {};\n"
        "  const closure = (value: int): int => { return value; };\n"
        "  const matched = match a { case 0 => b; case _ => { yield c; } };\n"
        "  const task = spawn service.call(a);\n"
        "}\n";
    static const struct {
        const char *source;
        RudaSyntaxKind kind;
        const char *diagnostic;
    } invalid[] = {
        {
            "fn broken(a: int, b: int, c: int): void { a = b = c; } "
            "fn after(): void {}",
            RUDA_SYNTAX_ASSIGNMENT_EXPRESSION,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(a: int, b: int, c: int): void { const x = a < b < c; } "
            "fn after(): void {}",
            RUDA_SYNTAX_BINARY_EXPRESSION,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = (); } fn after(): void {}",
            RUDA_SYNTAX_PAREN_GROUP,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(a: int, b: int): void { const x = (a, b); } "
            "fn after(): void {}",
            RUDA_SYNTAX_PAREN_GROUP,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = _; } fn after(): void {}",
            RUDA_SYNTAX_EXPRESSION,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(value: Value): void { const x = spawn value; } "
            "fn after(): void {}",
            RUDA_SYNTAX_SPAWN_EXPRESSION,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(value: Value): void { const x = (value) { field: value }; } "
            "fn after(): void {}",
            RUDA_SYNTAX_PAREN_GROUP,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(value: Value): void { const x = match value { "
            "case _ => return; }; } fn after(): void {}",
            RUDA_SYNTAX_MATCH_ARM,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        }
    };
    RudaParseFixture fixture;
    size_t index = 0;
    RUDA_EXPECT(ruda_parse_fixture_init(&fixture, valid));
    if (fixture.parsed.initialized) {
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_ASSIGNMENT_EXPRESSION
        ) == 11);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_UNARY_EXPRESSION
        ) >= 3);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_LITERAL_EXPRESSION
        ) >= 8);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_CLOSURE_EXPRESSION
        ) == 1);
        ruda_parse_fixture_dispose(&fixture);
    }
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        bool initialized = ruda_parse_fixture_init(&fixture, invalid[index].source);
        RUDA_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_parse_has_diagnostic(
            &fixture.parsed,
            invalid[index].diagnostic
        ));
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            invalid[index].kind
        ) != 0);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL
        ) >= 2);
        ruda_parse_fixture_dispose(&fixture);
    }
}

static void ruda_test_parser_expression_recovery(void) {
    static const struct {
        const char *source;
        RudaSyntaxKind kind;
        const char *diagnostic;
    } cases[] = {
        {
            "fn broken(): void { const x = ; } fn after(): void {}",
            RUDA_SYNTAX_EXPRESSION,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = (1; } fn after(): void {}",
            RUDA_SYNTAX_PAREN_GROUP,
            RUDA_DIAGNOSTIC_UNTERMINATED_DELIMITER
        },
        {
            "fn broken(): void { const x = values[0)]; } fn after(): void {}",
            RUDA_SYNTAX_BRACKET_GROUP,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = $\"{}\"; } fn after(): void {}",
            RUDA_SYNTAX_INTERPOLATED_EXPRESSION,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = first second; } fn after(): void {}",
            RUDA_SYNTAX_NAME_EXPRESSION,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = 1 2; } fn after(): void {}",
            RUDA_SYNTAX_LITERAL_EXPRESSION,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = !; } fn after(): void {}",
            RUDA_SYNTAX_UNARY_EXPRESSION,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = 1 +; } fn after(): void {}",
            RUDA_SYNTAX_BINARY_EXPRESSION,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { let x = 1; x =; } fn after(): void {}",
            RUDA_SYNTAX_ASSIGNMENT_EXPRESSION,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = value.; } fn after(): void {}",
            RUDA_SYNTAX_MEMBER_EXPRESSION,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = call(, value); } fn after(): void {}",
            RUDA_SYNTAX_CALL_EXPRESSION,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = values[0)]; } fn after(): void {}",
            RUDA_SYNTAX_INDEX_EXPRESSION,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = value? next; } fn after(): void {}",
            RUDA_SYNTAX_TRY_EXPRESSION,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = spawn; } fn after(): void {}",
            RUDA_SYNTAX_SPAWN_EXPRESSION,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = [1; } fn after(): void {}",
            RUDA_SYNTAX_ARRAY_EXPRESSION,
            RUDA_DIAGNOSTIC_UNTERMINATED_DELIMITER
        },
        {
            "fn broken(): void { const x = Box { value: 1] }; } "
            "fn after(): void {}",
            RUDA_SYNTAX_RECORD_EXPRESSION,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = (value: int): => value; } "
            "fn after(): void {}",
            RUDA_SYNTAX_CLOSURE_EXPRESSION,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = match { case _ => 1; }; } "
            "fn after(): void {}",
            RUDA_SYNTAX_MATCH_EXPRESSION,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = match value { _ => 1; }; } "
            "fn after(): void {}",
            RUDA_SYNTAX_MATCH_ARM,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = match value { case => 1; }; } "
            "fn after(): void {}",
            RUDA_SYNTAX_PATTERN,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = call(, value); } fn after(): void {}",
            RUDA_SYNTAX_ARGUMENTS,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        }
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        RudaParseFixture fixture;
        bool initialized = ruda_parse_fixture_init(&fixture, cases[index].source);
        RUDA_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_parse_has_diagnostic(
            &fixture.parsed,
            cases[index].diagnostic
        ));
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            cases[index].kind
        ) != 0);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL
        ) >= 2);
        ruda_parse_fixture_dispose(&fixture);
    }
}

static void ruda_test_parser_pattern_grammar(void) {
    static const char valid[] =
        "fn patterns(value: Value): int {\n"
        "  return match value {\n"
        "    case _ => 0;\n"
        "    case 1 => 1;\n"
        "    case binding => 2;\n"
        "    case pkg.Some(inner, [left, right]) => inner;\n"
        "    case User { id, name: alias } => id;\n"
        "    case None | Gone => 3;\n"
        "  };\n"
        "}\n";
    static const struct {
        const char *pattern;
        const char *diagnostic;
    } invalid[] = {
        {"", RUDA_DIAGNOSTIC_EXPECTED_TOKEN},
        {"Some(value]", RUDA_DIAGNOSTIC_UNTERMINATED_DELIMITER},
        {"Some { : value }", RUDA_DIAGNOSTIC_EXPECTED_TOKEN},
        {"[left right]", RUDA_DIAGNOSTIC_UNTERMINATED_DELIMITER},
        {"None |", RUDA_DIAGNOSTIC_EXPECTED_TOKEN},
        {"(value)", RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN}
    };
    RudaParseFixture fixture;
    size_t index = 0;
    RUDA_EXPECT(ruda_parse_fixture_init(&fixture, valid));
    if (fixture.parsed.initialized) {
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_PATTERN
        ) == 6);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_OR_PATTERN
        ) == 1);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_WILDCARD_PATTERN
        ) == 1);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_LITERAL_PATTERN
        ) == 1);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_CONSTRUCTOR_PATTERN
        ) == 2);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_POSITIONAL_PATTERN
        ) == 1);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_NAMED_PATTERN
        ) == 1);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_TUPLE_PATTERN
        ) == 1);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_PATTERN_FIELD
        ) == 2);
        ruda_parse_fixture_dispose(&fixture);
    }

    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        char source[512];
        int length = snprintf(
            source,
            sizeof(source),
            "fn broken(value: Value): int { const x = match value { "
            "case %s => 1; case _ => 0; }; return x; } "
            "fn after(): void {}",
            invalid[index].pattern
        );
        bool initialized = length > 0 && (size_t)length < sizeof(source)
            && ruda_parse_fixture_init(&fixture, source);
        RUDA_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_parse_has_diagnostic(
            &fixture.parsed,
            invalid[index].diagnostic
        ));
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_MATCH_ARM
        ) >= 2);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL
        ) >= 2);
        ruda_parse_fixture_dispose(&fixture);
    }
}

static void ruda_test_parser_record_field_grammar(void) {
    static const char valid[] =
        "fn records(value: int): void {\n"
        "  const empty = Item {};\n"
        "  const shorthand = Item { value };\n"
        "  const explicit = pkg.Item { value: value, other: call(value), };\n"
        "}\n";
    static const struct {
        const char *fields;
        const char *diagnostic;
        size_t minimum_fields;
    } invalid[] = {
        {": value", RUDA_DIAGNOSTIC_EXPECTED_TOKEN, 1},
        {"first second", RUDA_DIAGNOSTIC_EXPECTED_TOKEN, 2},
        {"value:", RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN, 1},
        {"value: 1]", RUDA_DIAGNOSTIC_EXPECTED_TOKEN, 2},
        {"value. other", RUDA_DIAGNOSTIC_EXPECTED_TOKEN, 2}
    };
    RudaParseFixture fixture;
    size_t index = 0;
    RUDA_EXPECT(ruda_parse_fixture_init(&fixture, valid));
    if (fixture.parsed.initialized) {
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_RECORD_EXPRESSION
        ) == 3);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_RECORD_FIELDS
        ) == 3);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_FIELD_VALUE
        ) == 3);
        ruda_parse_fixture_dispose(&fixture);
    }

    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        char source[512];
        int length = snprintf(
            source,
            sizeof(source),
            "fn broken(): void { const x = Item { %s }; return; } "
            "fn after(): void {}",
            invalid[index].fields
        );
        bool initialized = length > 0 && (size_t)length < sizeof(source)
            && ruda_parse_fixture_init(&fixture, source);
        RUDA_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_parse_has_diagnostic(
            &fixture.parsed,
            invalid[index].diagnostic
        ));
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_RECORD_FIELDS
        ) != 0);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_FIELD_VALUE
        ) >= invalid[index].minimum_fields);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL
        ) >= 2);
        ruda_parse_fixture_dispose(&fixture);
    }
}

static void ruda_test_parser_exact_delimited_grammar(void) {
    static const char valid[] =
        "@meta(1, call(2), Item { value: 3 },)\n"
        "enum Payload {\n"
        "  Pos(int, pkg.Value),\n"
        "  Named { id: int; value: pkg.Value; },\n"
        "}\n"
        "fn exact(value: int): void {\n"
        "  const item = values[call(value)];\n"
        "  const closure = (item: int): int => item;\n"
        "}\n";
    static const struct {
        const char *source;
        RudaSyntaxKind kind;
        const char *diagnostic;
    } invalid[] = {
        {
            "@meta(, 1) fn broken(): void {} fn after(): void {}",
            RUDA_SYNTAX_ATTRIBUTE,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "enum Bad { Empty(), Good } fn after(): void {}",
            RUDA_SYNTAX_POSITIONAL_PAYLOAD,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "enum Bad { Broken(int string), Good } fn after(): void {}",
            RUDA_SYNTAX_POSITIONAL_PAYLOAD,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "enum Bad { Broken { public id: int; }, Good } "
            "fn after(): void {}",
            RUDA_SYNTAX_PAYLOAD_FIELD,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "enum Bad { Broken { id: int }, Good } fn after(): void {}",
            RUDA_SYNTAX_NAMED_PAYLOAD,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = values[]; } fn after(): void {}",
            RUDA_SYNTAX_BRACKET_GROUP,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = values[0, 1]; } "
            "fn after(): void {}",
            RUDA_SYNTAX_INDEX_EXPRESSION,
            RUDA_DIAGNOSTIC_UNTERMINATED_DELIMITER
        },
        {
            "fn broken(): void { const x = (item) => item; } "
            "fn after(): void {}",
            RUDA_SYNTAX_CLOSURE_EXPRESSION,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        }
    };
    RudaParseFixture fixture;
    size_t index = 0;
    RUDA_EXPECT(ruda_parse_fixture_init(&fixture, valid));
    if (fixture.parsed.initialized) {
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_POSITIONAL_PAYLOAD
        ) == 1);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_NAMED_PAYLOAD
        ) == 1);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_PAYLOAD_FIELD
        ) == 2);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_BRACKET_GROUP
        ) == 1);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_CLOSURE_EXPRESSION
        ) == 1);
        ruda_parse_fixture_dispose(&fixture);
    }

    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        bool initialized = ruda_parse_fixture_init(&fixture, invalid[index].source);
        RUDA_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_parse_has_diagnostic(
            &fixture.parsed,
            invalid[index].diagnostic
        ));
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            invalid[index].kind
        ) != 0);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL
        ) >= 1);
        ruda_parse_fixture_dispose(&fixture);
    }
}

static void ruda_test_parser_statement_forms(void) {
    static const char source[] =
        "fn nested(value: Option<Array<Result<int, Error>>>): Option<Array<int>> {\n"
        "  let delayed: int;\n"
        "  let _ = 0;\n"
        "  const _ = 0;\n"
        "  call();\n"
        "  defer cleanup();\n"
        "  for const item of value {\n"
        "    use(item);\n"
        "  }\n"
        "  for let index = 0; index < 3; index += 1 {\n"
        "    if index == 1 { continue; } else if false { break; } else { use(index); }\n"
        "  }\n"
        "  while false { break; }\n"
        "  scope { const task = spawn service.call(); task.wait(); }\n"
        "  unsafe { panic(\"checked\"); }\n"
        "  { yield value; }\n"
        "  { return; }\n"
        "  return value;\n"
        "}\n";
    RudaParseFixture fixture;
    bool initialized = ruda_parse_fixture_init(&fixture, source);
    RUDA_EXPECT(initialized);
    if (!initialized) {
        return;
    }
    RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
    RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
    RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_FOR_STATEMENT
    ) == 2);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_FOR_HEADER
    ) == 2);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_IF_STATEMENT
    ) == 2);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_WHILE_STATEMENT
    ) == 1);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_UNSAFE_STATEMENT
    ) == 1);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_SCOPE_STATEMENT
    ) == 1);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_LET_DECL
    ) == 2);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_DEFER_STATEMENT
    ) == 1);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_YIELD_STATEMENT
    ) == 1);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_RETURN_STATEMENT
    ) == 2);
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_BLOCK_STATEMENT
    ) == 2);
    ruda_parse_fixture_dispose(&fixture);
}

static void ruda_test_parser_statement_recovery(void) {
    static const struct {
        const char *source;
        RudaSyntaxKind kind;
        const char *diagnostic;
        size_t minimum_function_count;
    } cases[] = {
        {
            "fn broken(): void { call() } fn after(): void {}",
            RUDA_SYNTAX_EXPRESSION_STATEMENT,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { return 1 } fn after(): void {}",
            RUDA_SYNTAX_RETURN_STATEMENT,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { defer; } fn after(): void {}",
            RUDA_SYNTAX_DEFER_STATEMENT,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { if {} } fn after(): void {}",
            RUDA_SYNTAX_IF_STATEMENT,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { while {} } fn after(): void {}",
            RUDA_SYNTAX_WHILE_STATEMENT,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { for const of values {} } fn after(): void {}",
            RUDA_SYNTAX_FOR_STATEMENT,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { for let i = 0 i < 3; i += 1 {} } "
            "fn after(): void {}",
            RUDA_SYNTAX_FOR_HEADER,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { for const i = 0; i < 3; i += 1 {} } "
            "fn after(): void {}",
            RUDA_SYNTAX_FOR_HEADER,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
            2
        },
        {
            "fn broken(values: Values): void { for let item of values {} } "
            "fn after(): void {}",
            RUDA_SYNTAX_FOR_HEADER,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
            2
        },
        {
            "fn broken(values: Values): void { for item of values {} } "
            "fn after(): void {}",
            RUDA_SYNTAX_FOR_HEADER,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
            2
        },
        {
            "fn broken(values: Values): void { for const _ of values {} } "
            "fn after(): void {}",
            RUDA_SYNTAX_FOR_HEADER,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { for let i = ; true; i += 1 {} } "
            "fn after(): void {}",
            RUDA_SYNTAX_FOR_HEADER,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { for let i = 0; ; i += 1 {} } "
            "fn after(): void {}",
            RUDA_SYNTAX_FOR_HEADER,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { for let i = 0; true; {} } "
            "fn after(): void {}",
            RUDA_SYNTAX_FOR_HEADER,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { scope { return;",
            RUDA_SYNTAX_SCOPE_STATEMENT,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN,
            1
        },
        {
            "fn broken(): void { break } fn after(): void {}",
            RUDA_SYNTAX_BREAK_STATEMENT,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { continue } fn after(): void {}",
            RUDA_SYNTAX_CONTINUE_STATEMENT,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { yield; } fn after(): void {}",
            RUDA_SYNTAX_YIELD_STATEMENT,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { unsafe { return;",
            RUDA_SYNTAX_UNSAFE_STATEMENT,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN,
            1
        },
        {
            "fn broken(): void { { return;",
            RUDA_SYNTAX_BLOCK_STATEMENT,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN,
            1
        }
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        RudaParseFixture fixture;
        size_t function_count = 0;
        bool initialized = ruda_parse_fixture_init(&fixture, cases[index].source);
        RUDA_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_parse_has_diagnostic(
            &fixture.parsed,
            cases[index].diagnostic
        ));
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            cases[index].kind
        ) != 0);
        function_count = ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL
        );
        if (function_count < cases[index].minimum_function_count) {
            fprintf(
                stderr,
                "statement recovery case %zu retained %zu function(s), expected %zu\n",
                index,
                function_count,
                cases[index].minimum_function_count
            );
        }
        RUDA_EXPECT(function_count >= cases[index].minimum_function_count);
        ruda_parse_fixture_dispose(&fixture);
    }
}

static void ruda_test_parser_type_grammar(void) {
    static const char valid[] =
        "type Scalar = int;\n"
        "type Qualified = pkg.User;\n"
        "type Nested = Map<string, Array<Result<int, Error>>>;\n"
        "type Tuple = [string, int];\n"
        "type Callback = (string, int) => Result<void, Error>;\n"
        "fn maximum<T: Ordered<T> & Hash<T>>(value: T): T { return value; }\n";
    static const struct {
        const char *source;
        const char *diagnostic;
    } invalid[] = {
        {
            "type Missing = ; fn after(): void {}",
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "type EmptyArguments = Array<>; fn after(): void {}",
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "type Qualified = pkg.; fn after(): void {}",
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "type EmptyTuple = []; fn after(): void {}",
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "type NotFunction = (int); fn after(): void {}",
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "type Extra = int string; fn after(): void {}",
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "type MissingClose = Map<string, Array<int>; fn after(): void {}",
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "type ExtraClose = Array<int>>; fn after(): void {}",
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn emptyParameters<>(value: int): int { return value; } "
            "fn after(): void {}",
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn missingConstraint<T: >(value: T): T { return value; } "
            "fn after(): void {}",
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
        }
    };
    RudaParseFixture fixture;
    size_t index = 0;
    bool initialized = ruda_parse_fixture_init(&fixture, valid);
    RUDA_EXPECT(initialized);
    if (initialized) {
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_TYPE
        ) >= 16);
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_TYPE_ARGUMENTS
        ) >= 6);
        ruda_parse_fixture_dispose(&fixture);
    }
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        initialized = ruda_parse_fixture_init(&fixture, invalid[index].source);
        RUDA_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_parse_has_diagnostic(
            &fixture.parsed,
            invalid[index].diagnostic
        ));
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
        if (strcmp(
                invalid[index].diagnostic,
                RUDA_DIAGNOSTIC_EXPECTED_TOKEN
            ) == 0) {
            RUDA_EXPECT(ruda_syntax_count_synthetic_tokens(
                fixture.parsed.tree.root
            ) != 0);
        }
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL
        ) >= 1);
        ruda_parse_fixture_dispose(&fixture);
    }
}

static void ruda_test_parser_incremental_reuse(void) {
    static const char initial[] =
        "fn first(): int { return 1; }\n"
        "fn middle(): int { return 2; }\n"
        "fn last(): int { return 3; }\n";
    static const char same_length[] =
        "fn first(): int { return 1; }\n"
        "fn middle(): int { return 4; }\n"
        "fn last(): int { return 3; }\n";
    static const char shifted[] =
        "fn first(): int { return 1; }\n"
        "fn middle(): int { return 200; }\n"
        "fn last(): int { return 3; }\n";
    size_t edit_start = (size_t)(strstr(initial, "return 2") - initial)
        + strlen("return ");
    size_t failure_index = 0;

    {
        RudaSourceManager sources;
        const RudaSourceSnapshot *initial_snapshot = NULL;
        const RudaSourceSnapshot *overlay = NULL;
        RudaLexResult initial_lexed;
        RudaLexResult overlay_lexed;
        RudaParseResult initial_parsed;
        RudaParseResult overlay_parsed;
        RudaIncrementalParseStats stats;
        const RudaGreenNode *first = NULL;
        const RudaGreenNode *middle = NULL;
        const RudaGreenNode *last = NULL;
        RUDA_EXPECT(ruda_source_manager_init(
            &sources,
            ruda_system_allocator()
        ));
        RUDA_EXPECT(ruda_source_manager_add(
            &sources,
            "incremental.ruda",
            strlen("incremental.ruda"),
            initial,
            strlen(initial),
            &initial_snapshot
        ) == RUDA_SOURCE_OK);
        RUDA_EXPECT(ruda_lex_snapshot(
            initial_snapshot,
            ruda_system_allocator(),
            ruda_lex_default_options(),
            &initial_lexed
        ) == RUDA_LEX_OK);
        RUDA_EXPECT(ruda_parse_file(
            &initial_lexed,
            ruda_system_allocator(),
            ruda_parse_default_options(),
            &initial_parsed
        ) == RUDA_PARSE_OK);
        first = ruda_syntax_direct_nth_kind(
            initial_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            0
        );
        middle = ruda_syntax_direct_nth_kind(
            initial_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            1
        );
        last = ruda_syntax_direct_nth_kind(
            initial_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            2
        );
        RUDA_EXPECT(first != NULL && middle != NULL && last != NULL);
        RUDA_EXPECT(ruda_source_manager_overlay(
            &sources,
            ruda_source_file_id(initial_snapshot),
            same_length,
            strlen(same_length),
            &overlay
        ) == RUDA_SOURCE_OK);
        RUDA_EXPECT(ruda_lex_snapshot(
            overlay,
            ruda_system_allocator(),
            ruda_lex_default_options(),
            &overlay_lexed
        ) == RUDA_LEX_OK);
        RUDA_EXPECT(ruda_parse_file_incremental(
            &initial_parsed,
            &overlay_lexed,
            edit_start,
            edit_start + 1,
            edit_start + 1,
            ruda_system_allocator(),
            ruda_parse_default_options(),
            &overlay_parsed,
            &stats
        ) == RUDA_PARSE_OK);
        RUDA_EXPECT(stats.reused_top_level_nodes == 2);
        RUDA_EXPECT(ruda_syntax_direct_nth_kind(
            overlay_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            0
        ) == first);
        RUDA_EXPECT(ruda_syntax_direct_nth_kind(
            overlay_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            1
        ) != middle);
        RUDA_EXPECT(ruda_syntax_direct_nth_kind(
            overlay_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            2
        ) == last);
        ruda_parse_result_dispose(&initial_parsed);
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&overlay_parsed.tree));
        ruda_parse_result_dispose(&overlay_parsed);
        ruda_lex_result_dispose(&overlay_lexed);
        ruda_lex_result_dispose(&initial_lexed);
        ruda_source_manager_dispose(&sources);
    }

    {
        RudaSourceManager sources;
        const RudaSourceSnapshot *initial_snapshot = NULL;
        const RudaSourceSnapshot *overlay = NULL;
        RudaLexResult initial_lexed;
        RudaLexResult overlay_lexed;
        RudaParseResult initial_parsed;
        RudaParseResult overlay_parsed;
        RudaIncrementalParseStats stats;
        const RudaGreenNode *first = NULL;
        const RudaGreenNode *last = NULL;
        bool reached_success = false;
        RUDA_EXPECT(ruda_source_manager_init(
            &sources,
            ruda_system_allocator()
        ));
        RUDA_EXPECT(ruda_source_manager_add(
            &sources,
            "incremental-shift.ruda",
            strlen("incremental-shift.ruda"),
            initial,
            strlen(initial),
            &initial_snapshot
        ) == RUDA_SOURCE_OK);
        RUDA_EXPECT(ruda_lex_snapshot(
            initial_snapshot,
            ruda_system_allocator(),
            ruda_lex_default_options(),
            &initial_lexed
        ) == RUDA_LEX_OK);
        RUDA_EXPECT(ruda_parse_file(
            &initial_lexed,
            ruda_system_allocator(),
            ruda_parse_default_options(),
            &initial_parsed
        ) == RUDA_PARSE_OK);
        first = ruda_syntax_direct_nth_kind(
            initial_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            0
        );
        last = ruda_syntax_direct_nth_kind(
            initial_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            2
        );
        RUDA_EXPECT(ruda_source_manager_overlay(
            &sources,
            ruda_source_file_id(initial_snapshot),
            shifted,
            strlen(shifted),
            &overlay
        ) == RUDA_SOURCE_OK);
        RUDA_EXPECT(ruda_lex_snapshot(
            overlay,
            ruda_system_allocator(),
            ruda_lex_default_options(),
            &overlay_lexed
        ) == RUDA_LEX_OK);
        RUDA_EXPECT(ruda_parse_file_incremental(
            &initial_parsed,
            &overlay_lexed,
            edit_start,
            edit_start + 1,
            edit_start + 3,
            ruda_system_allocator(),
            ruda_parse_default_options(),
            &overlay_parsed,
            &stats
        ) == RUDA_PARSE_OK);
        RUDA_EXPECT(stats.reused_top_level_nodes == 1);
        RUDA_EXPECT(ruda_syntax_direct_nth_kind(
            overlay_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            0
        ) == first);
        RUDA_EXPECT(ruda_syntax_direct_nth_kind(
            overlay_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            2
        ) != last);
        ruda_parse_result_dispose(&overlay_parsed);

        memset(&overlay_parsed, 0, sizeof(overlay_parsed));
        RUDA_EXPECT(ruda_parse_file_incremental(
            &initial_parsed,
            &overlay_lexed,
            edit_start,
            edit_start + 1,
            edit_start + 1,
            ruda_system_allocator(),
            ruda_parse_default_options(),
            &overlay_parsed,
            &stats
        ) == RUDA_PARSE_INVALID_ARGUMENT);
        RUDA_EXPECT(!overlay_parsed.initialized);

        for (failure_index = 0; failure_index < 256; failure_index += 1) {
            RudaFailAllocator state;
            RudaAllocator allocator;
            RudaParseStatus status;
            ruda_fail_allocator_init(
                &state,
                ruda_system_allocator(),
                failure_index
            );
            allocator = ruda_fail_allocator(&state);
            status = ruda_parse_file_incremental(
                &initial_parsed,
                &overlay_lexed,
                edit_start,
                edit_start + 1,
                edit_start + 3,
                allocator,
                ruda_parse_default_options(),
                &overlay_parsed,
                &stats
            );
            if (status == RUDA_PARSE_OK) {
                reached_success = true;
                RUDA_EXPECT(stats.reused_top_level_nodes == 1);
                ruda_parse_result_dispose(&overlay_parsed);
            } else {
                RUDA_EXPECT(status == RUDA_PARSE_ALLOCATION_FAILED);
                RUDA_EXPECT(!overlay_parsed.initialized);
                RUDA_EXPECT(stats.reused_top_level_nodes == 0);
                RUDA_EXPECT(stats.reparsed_bytes == 0);
            }
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
            if (reached_success) {
                break;
            }
        }
        RUDA_EXPECT(reached_success);
        ruda_parse_result_dispose(&initial_parsed);
        ruda_lex_result_dispose(&overlay_lexed);
        ruda_lex_result_dispose(&initial_lexed);
        ruda_source_manager_dispose(&sources);
    }
}

static void ruda_test_parser_limits_and_allocation(void) {
    static const char source[] =
        "fn main(): void { const value = (((((((((((1))))))))))); }";
    RudaSourceManager sources;
    const RudaSourceSnapshot *snapshot = NULL;
    RudaLexResult lexed;
    RudaParseResult parsed;
    RudaParseOptions options = ruda_parse_default_options();
    size_t failure_index = 0;
    bool reached_success = false;
    RUDA_EXPECT(ruda_source_manager_init(&sources, ruda_system_allocator()));
    RUDA_EXPECT(ruda_source_manager_add(
        &sources,
        "limits.ruda",
        strlen("limits.ruda"),
        source,
        strlen(source),
        &snapshot
    ) == RUDA_SOURCE_OK);
    RUDA_EXPECT(ruda_lex_snapshot(
        snapshot,
        ruda_system_allocator(),
        ruda_lex_default_options(),
        &lexed
    ) == RUDA_LEX_OK);

    options.max_events = 8;
    RUDA_EXPECT(ruda_parse_file(
        &lexed,
        ruda_system_allocator(),
        options,
        &parsed
    ) == RUDA_PARSE_LIMIT_REACHED);
    RUDA_EXPECT(!parsed.initialized);

    options = ruda_parse_default_options();
    options.max_nesting = 8;
    RUDA_EXPECT(ruda_parse_file(
        &lexed,
        ruda_system_allocator(),
        options,
        &parsed
    ) == RUDA_PARSE_LIMIT_REACHED);
    RUDA_EXPECT(!parsed.initialized);

    options = ruda_parse_default_options();
    options.max_diagnostics = 1;
    {
        static const char diagnostic_source[] = "a; b; c;";
        RudaSourceManager diagnostic_sources;
        const RudaSourceSnapshot *diagnostic_snapshot = NULL;
        RudaLexResult diagnostic_lexed;
        RUDA_EXPECT(ruda_source_manager_init(
            &diagnostic_sources,
            ruda_system_allocator()
        ));
        RUDA_EXPECT(ruda_source_manager_add(
            &diagnostic_sources,
            "diagnostic-limit.ruda",
            strlen("diagnostic-limit.ruda"),
            diagnostic_source,
            strlen(diagnostic_source),
            &diagnostic_snapshot
        ) == RUDA_SOURCE_OK);
        RUDA_EXPECT(ruda_lex_snapshot(
            diagnostic_snapshot,
            ruda_system_allocator(),
            ruda_lex_default_options(),
            &diagnostic_lexed
        ) == RUDA_LEX_OK);
        RUDA_EXPECT(ruda_parse_file(
            &diagnostic_lexed,
            ruda_system_allocator(),
            options,
            &parsed
        ) == RUDA_PARSE_LIMIT_REACHED);
        RUDA_EXPECT(!parsed.initialized);
        ruda_lex_result_dispose(&diagnostic_lexed);
        ruda_source_manager_dispose(&diagnostic_sources);
    }

    for (failure_index = 0; failure_index < 128; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaParseStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        status = ruda_parse_file(
            &lexed,
            allocator,
            ruda_parse_default_options(),
            &parsed
        );
        if (status == RUDA_PARSE_OK) {
            reached_success = true;
            RUDA_EXPECT(ruda_syntax_tree_is_lossless(&parsed.tree));
            ruda_parse_result_dispose(&parsed);
        } else {
            RUDA_EXPECT(status == RUDA_PARSE_ALLOCATION_FAILED);
            RUDA_EXPECT(!parsed.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
    ruda_lex_result_dispose(&lexed);
    ruda_source_manager_dispose(&sources);
}

static void ruda_test_parser_all_single_bytes(void) {
    unsigned int value = 0;
    for (value = 0; value <= 255; value += 1) {
        unsigned char byte = (unsigned char)value;
        RudaParseFixture fixture;
        bool initialized = ruda_parse_fixture_init_bytes(&fixture, &byte, 1);
        RUDA_EXPECT(initialized);
        if (initialized) {
            RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
            RUDA_EXPECT(fixture.parsed.tree.root->text_length == 1);
            ruda_parse_fixture_dispose(&fixture);
        }
    }
}

void ruda_test_parser(void) {
    ruda_test_parser_snapshot();
    ruda_test_parser_empty_source();
    ruda_test_parser_examples();
    ruda_test_parser_all_productions();
    ruda_test_parser_recovery();
    ruda_test_parser_delimiter_recovery();
    ruda_test_parser_declarations();
    ruda_test_parser_declaration_grammar();
    ruda_test_parser_keyword_boundaries();
    ruda_test_parser_declaration_recovery();
    ruda_test_parser_pratt_expressions();
    ruda_test_parser_expression_grammar();
    ruda_test_parser_expression_recovery();
    ruda_test_parser_pattern_grammar();
    ruda_test_parser_record_field_grammar();
    ruda_test_parser_exact_delimited_grammar();
    ruda_test_parser_statement_forms();
    ruda_test_parser_statement_recovery();
    ruda_test_parser_type_grammar();
    ruda_test_parser_incremental_reuse();
    ruda_test_parser_limits_and_allocation();
    ruda_test_parser_all_single_bytes();
}
