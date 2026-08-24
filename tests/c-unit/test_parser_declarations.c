#include "test_parser_support.h"

void mmay_test_parser_declarations(void) {
    static const char source[] =
        "import { Item as Renamed, other, } from \"pkg\";\n"
        "type Id = i64;\n"
        "type Box<T> { public mutable value: T, }\n"
        "enum Maybe<T> { None, Some(T), Named { value: T, }, }\n"
        "interface Reader<T> { read(buffer: T): Result<int, Error>; Close; }\n"
        "@link(\"c\") extern \"c\" fn call(value: i32): i32;\n"
        "const answer: int = 42;\n"
        "@fuzz fn fuzzBytes(input: []byte): void { const _ = input; }\n"
        "@test fn ok(): void { expect.equal(answer, 42); }\n";
    MarimayParseFixture fixture;
    MMAY_EXPECT(mmay_parse_fixture_init(&fixture, source));
    if (!fixture.parsed.initialized) {
        return;
    }
    MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
    MMAY_EXPECT(fixture.parsed.diagnostics.length == 0);
    MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
    mmay_parse_fixture_dispose(&fixture);
}

void mmay_test_parser_declaration_grammar(void) {
    static const char valid[] =
        "import * as http from \"std/http\";\n"
        "import { Item, Other as Alias, } from \"pkg\";\n"
        "@tag(1) public type Record<T> { public mutable value: T, }\n"
        "type Alias<T> = Result<T, Error>;\n"
        "enum Choice<T> { None, One(T), Named { value: T, }, }\n"
        "interface Reader<T> { read(value: T): Result<T, Error>; Close; }\n"
        "public unsafe fn local(value: int): int { return value; }\n"
        "extern \"c\" unsafe fn foreign(value: int): int;\n"
        "const answer: int = 42;\n"
        "@test fn emptyTest(): void {}\n"
        "@fuzz fn emptyFuzz(input: bytes): void {}\n";
    static const struct {
        const char *source;
        MarimaySyntaxKind kind;
        const char *diagnostic;
    } invalid[] = {
        {
            "public import * as x from \"pkg\"; fn after(): void {}",
            MMAY_SYNTAX_IMPORT_DECL,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "@tag import * as x from \"pkg\"; fn after(): void {}",
            MMAY_SYNTAX_IMPORT_DECL,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "import * from \"pkg\"; fn after(): void {}",
            MMAY_SYNTAX_IMPORT_CLAUSE,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "import { First Second } from \"pkg\"; fn after(): void {}",
            MMAY_SYNTAX_IMPORT_DECL,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "type Bad { value: int other: int } fn after(): void {}",
            MMAY_SYNTAX_FIELD_DECL,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "type Bad { value: int; } fn after(): void {}",
            MMAY_SYNTAX_FIELD_DECL,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "enum Bad { First Second } fn after(): void {}",
            MMAY_SYNTAX_ENUM_VARIANT,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "enum Bad { Named { value: int; }, } fn after(): void {}",
            MMAY_SYNTAX_NAMED_PAYLOAD,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "interface Bad { call(value: int) int; } fn after(): void {}",
            MMAY_SYNTAX_INTERFACE_ITEM,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "const missing: int; fn after(): void {}",
            MMAY_SYNTAX_CONST_DECL,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "extern \"c\" fn bad(): void {} fn after(): void {}",
            MMAY_SYNTAX_EXTERN_DECL,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        }
    };
    MarimayParseFixture fixture;
    size_t index = 0;
    MMAY_EXPECT(mmay_parse_fixture_init(&fixture, valid));
    if (fixture.parsed.initialized) {
        MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
        MMAY_EXPECT(fixture.parsed.diagnostics.length == 0);
        MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_IMPORT_DECL
        ) == 2);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_ENUM_VARIANT
        ) == 3);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL
        ) == 3);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_EXTERN_DECL
        ) == 1);
        mmay_parse_fixture_dispose(&fixture);
    }
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        bool initialized = mmay_parse_fixture_init(&fixture, invalid[index].source);
        MMAY_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
        MMAY_EXPECT(mmay_parse_has_diagnostic(
            &fixture.parsed,
            invalid[index].diagnostic
        ));
        MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            invalid[index].kind
        ) != 0);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL
        ) >= 1);
        mmay_parse_fixture_dispose(&fixture);
    }
}

void mmay_test_parser_keyword_boundaries(void) {
    static const char source[] =
        "type Ordinary {\n"
        "  case: int, of: int, yield: int, test: int, fuzz: int, panic: int,\n"
        "  any: int, async: int, await: int, class: int, extends: int,\n"
        "  impl: int, throws: int,\n"
        "}\n"
        "fn yield(): int { return 1; }\n"
        "fn of(case: int): int { return case + yield(); }\n"
        "fn case(): int { return of(yield()); }\n"
        "fn test(): void {} fn fuzz(): void {} fn panic(): void {}\n"
        "fn any(): void {} fn async(): void {}\n"
        "fn await(): void {} fn class(): void {} fn extends(): void {}\n"
        "fn impl(): void {} fn throws(): void {}\n";
    MarimayParseFixture fixture;
    bool initialized = mmay_parse_fixture_init(&fixture, source);
    MMAY_EXPECT(initialized);
    if (!initialized) {
        return;
    }
    MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
    MMAY_EXPECT(fixture.parsed.diagnostics.length == 0);
    MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_FUNCTION_DECL
    ) == 13);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_CALL_EXPRESSION
    ) == 3);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_RECORD_EXPRESSION
    ) == 0);
    mmay_parse_fixture_dispose(&fixture);

    {
        static const char *const hard_names[] = {
            "as", "break", "const", "continue", "defer", "else", "enum",
            "extern", "false", "fn", "for", "from", "if", "import", "in",
            "interface", "let", "match", "mutable", "public", "return",
            "scope", "spawn", "true", "type", "unsafe", "while"
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
            MMAY_EXPECT(written > 0
                && (size_t)written < sizeof(invalid_source));
            initialized = mmay_parse_fixture_init(&fixture, invalid_source);
            MMAY_EXPECT(initialized);
            if (!initialized) {
                continue;
            }
            MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
            MMAY_EXPECT(mmay_parse_has_diagnostic(
                &fixture.parsed,
                MMAY_DIAGNOSTIC_EXPECTED_TOKEN
            ));
            MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
            mmay_parse_fixture_dispose(&fixture);
        }
    }
}

void mmay_test_parser_declaration_recovery(void) {
    static const struct {
        const char *source;
        MarimaySyntaxKind kinds[4];
        size_t kind_count;
        size_t minimum_function_count;
    } cases[] = {
        {
            "@() fn decorated(): void {} fn after(): void {}",
            {MMAY_SYNTAX_ATTRIBUTE},
            1,
            2
        },
        {
            "import { , Good } from \"pkg\"; fn after(): void {}",
            {
                MMAY_SYNTAX_IMPORT_DECL,
                MMAY_SYNTAX_IMPORT_CLAUSE,
                MMAY_SYNTAX_IMPORT_ITEM
            },
            3,
            1
        },
        {
            "type = int; fn after(): void {}",
            {MMAY_SYNTAX_TYPE_ALIAS_DECL},
            1,
            1
        },
        {
            "type Bad { : int, } fn after(): void {}",
            {MMAY_SYNTAX_RECORD_DECL, MMAY_SYNTAX_FIELD_DECL},
            2,
            1
        },
        {
            "enum Bad { , Good } fn after(): void {}",
            {MMAY_SYNTAX_ENUM_DECL, MMAY_SYNTAX_ENUM_VARIANT},
            2,
            1
        },
        {
            "enum Bad { Broken { : int, }, } fn after(): void {}",
            {MMAY_SYNTAX_ENUM_DECL, MMAY_SYNTAX_NAMED_PAYLOAD},
            2,
            1
        },
        {
            "interface Bad { (): int; } fn after(): void {}",
            {MMAY_SYNTAX_INTERFACE_DECL, MMAY_SYNTAX_INTERFACE_ITEM},
            2,
            1
        },
        {
            "fn (value int): void {} fn after(): void {}",
            {
                MMAY_SYNTAX_FUNCTION_DECL,
                MMAY_SYNTAX_PARAMETERS,
                MMAY_SYNTAX_PARAMETER,
                MMAY_SYNTAX_BLOCK
            },
            4,
            2
        },
        {
            "fn broken(value: int: void {} fn after(): void {}",
            {MMAY_SYNTAX_FUNCTION_DECL, MMAY_SYNTAX_PARAMETERS},
            2,
            2
        },
        {
            "fn broken(): void { const value = 1;",
            {MMAY_SYNTAX_FUNCTION_DECL, MMAY_SYNTAX_BLOCK},
            2,
            1
        },
        {
            "extern fn foreign(): void; fn after(): void {}",
            {MMAY_SYNTAX_EXTERN_DECL},
            1,
            1
        },
        {
            "const : int = 1; fn after(): void {}",
            {MMAY_SYNTAX_CONST_DECL},
            1,
            1
        },
        {
            "fn broken(): void { let : int = 1; } fn after(): void {}",
            {MMAY_SYNTAX_LET_DECL},
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
        MarimayParseFixture fixture;
        size_t kind_index = 0;
        bool initialized = mmay_parse_fixture_init(&fixture, cases[case_index].source);
        MMAY_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
        MMAY_EXPECT(fixture.parsed.diagnostics.length != 0);
        MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
        MMAY_EXPECT(mmay_syntax_count_synthetic_tokens(
            fixture.parsed.tree.root
        ) != 0);
        for (kind_index = 0; kind_index < cases[case_index].kind_count; kind_index += 1) {
            MMAY_EXPECT(mmay_syntax_count_kind(
                fixture.parsed.tree.root,
                cases[case_index].kinds[kind_index]
            ) != 0);
        }
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL
        ) >= cases[case_index].minimum_function_count);
        mmay_parse_fixture_dispose(&fixture);
    }

    {
        MarimayParseFixture fixture;
        bool initialized = mmay_parse_fixture_init(&fixture, declaration_starters);
        MMAY_EXPECT(initialized);
        if (!initialized) {
            return;
        }
        MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
        MMAY_EXPECT(fixture.parsed.diagnostics.length == 7);
        MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_ERROR
        ) == 7);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_EXTERN_DECL
        ) == 1);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_CONST_DECL
        ) == 1);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_ATTRIBUTE
        ) == 3);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL
        ) == 5);
        mmay_parse_fixture_dispose(&fixture);
    }
}

void mmay_test_parser_type_grammar(void) {
    static const char valid[] =
        "type Scalar = int;\n"
        "type Qualified = pkg.User;\n"
        "type Nested = Map<string, []Result<int, Error>>;\n"
        "type Tuple = (string, int);\n"
        "type Callback = (string, int) => Result<void, Error>;\n"
        "fn maximum<T: Ordered<T> & Hash<T>>(value: T): T { return value; }\n";
    static const struct {
        const char *source;
        const char *diagnostic;
    } invalid[] = {
        {
            "type Missing = ; fn after(): void {}",
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "type EmptyArguments = Array<>; fn after(): void {}",
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "type Qualified = pkg.; fn after(): void {}",
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "type EmptyTuple = []; fn after(): void {}",
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "type NotFunction = (int); fn after(): void {}",
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "type Extra = int string; fn after(): void {}",
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "type MissingClose = Map<string, []int; fn after(): void {}",
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "type ExtraClose = []int>; fn after(): void {}",
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn emptyParameters<>(value: int): int { return value; } "
            "fn after(): void {}",
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn missingConstraint<T: >(value: T): T { return value; } "
            "fn after(): void {}",
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        }
    };
    MarimayParseFixture fixture;
    size_t index = 0;
    bool initialized = mmay_parse_fixture_init(&fixture, valid);
    MMAY_EXPECT(initialized);
    if (initialized) {
        MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
        MMAY_EXPECT(fixture.parsed.diagnostics.length == 0);
        MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_TYPE
        ) >= 16);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_TYPE_ARGUMENTS
        ) >= 5);
        mmay_parse_fixture_dispose(&fixture);
    }
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        initialized = mmay_parse_fixture_init(&fixture, invalid[index].source);
        MMAY_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
        MMAY_EXPECT(mmay_parse_has_diagnostic(
            &fixture.parsed,
            invalid[index].diagnostic
        ));
        MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
        if (strcmp(
                invalid[index].diagnostic,
                MMAY_DIAGNOSTIC_EXPECTED_TOKEN
            ) == 0 && index != 4) {
            MMAY_EXPECT(mmay_syntax_count_synthetic_tokens(
                fixture.parsed.tree.root
            ) != 0);
        }
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL
        ) >= 1);
        mmay_parse_fixture_dispose(&fixture);
    }
}
