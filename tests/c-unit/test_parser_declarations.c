#include "test_parser_support.h"

void ruda_test_parser_declarations(void) {
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

void ruda_test_parser_declaration_grammar(void) {
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

void ruda_test_parser_keyword_boundaries(void) {
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

void ruda_test_parser_declaration_recovery(void) {
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

void ruda_test_parser_type_grammar(void) {
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
