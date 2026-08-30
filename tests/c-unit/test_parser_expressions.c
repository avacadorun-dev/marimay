#include "test_parser_support.h"

void mmay_test_parser_pratt_expressions(void) {
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
        "  const matched = match a { 0 => b, _ if c > 0 => c, };\n"
        "  expect.true(true);\n"
        "  expect.false(false);\n"
        "}\n";
    MarimayParseFixture fixture;
    const MarimayGreenNode *plus = NULL;
    const MarimayGreenNode *assignment = NULL;
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
        MMAY_SYNTAX_BINARY_EXPRESSION
    ) >= 7);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_ASSIGNMENT_EXPRESSION
    ) == 1);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_MEMBER_EXPRESSION
    ) >= 5);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_CALL_EXPRESSION
    ) >= 3);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_TYPE_ARGUMENTS
    ) >= 1);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_INDEX_EXPRESSION
    ) == 1);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_TRY_EXPRESSION
    ) == 1);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_UNARY_EXPRESSION
    ) == 2);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_ARRAY_EXPRESSION
    ) == 1);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_RECORD_EXPRESSION
    ) == 1);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_CLOSURE_EXPRESSION
    ) == 1);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_SPAWN_EXPRESSION
    ) == 1);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_MATCH_EXPRESSION
    ) == 1);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_MATCH_ARM
    ) == 2);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_PATTERN
    ) == 2);

    plus = mmay_syntax_find_kind_with_direct_token(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_BINARY_EXPRESSION,
        MMAY_TOKEN_PLUS
    );
    MMAY_EXPECT(plus != NULL);
    MMAY_EXPECT(mmay_syntax_has_direct_node(plus, MMAY_SYNTAX_BINARY_EXPRESSION));
    assignment = mmay_syntax_find_kind_with_direct_token(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_ASSIGNMENT_EXPRESSION,
        MMAY_TOKEN_EQUAL
    );
    MMAY_EXPECT(assignment != NULL);
    MMAY_EXPECT(mmay_syntax_has_direct_node(
        assignment,
        MMAY_SYNTAX_ASSIGNMENT_EXPRESSION
    ) == false);
    mmay_parse_fixture_dispose(&fixture);
}

void mmay_test_parser_expression_grammar(void) {
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
        "  const tuple = (a, b);\n"
        "  const converted = a as u64;\n"
        "  const empty = [];\n"
        "  const record = pkg.Item {};\n"
        "  const closure = (value: int): int => { return value; };\n"
        "  const inferredClosure = (value) => value;\n"
        "  const matched = match a { 0 => b, _ => { c }, };\n"
        "  const task = spawn service.call(a);\n"
        "}\n";
    static const struct {
        const char *source;
        MarimaySyntaxKind kind;
        const char *diagnostic;
    } invalid[] = {
        {
            "fn broken(a: int, b: int, c: int): void { a = b = c; } "
            "fn after(): void {}",
            MMAY_SYNTAX_ASSIGNMENT_EXPRESSION,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(a: int, b: int, c: int): void { const x = a < b < c; } "
            "fn after(): void {}",
            MMAY_SYNTAX_BINARY_EXPRESSION,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(value: int): void { const x = value as; } "
            "fn after(): void {}",
            MMAY_SYNTAX_CONVERSION_EXPRESSION,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = (); } fn after(): void {}",
            MMAY_SYNTAX_PAREN_GROUP,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = _; } fn after(): void {}",
            MMAY_SYNTAX_EXPRESSION,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(value: Value): void { const x = spawn value; } "
            "fn after(): void {}",
            MMAY_SYNTAX_SPAWN_EXPRESSION,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(value: Value): void { const x = (value) { field: value }; } "
            "fn after(): void {}",
            MMAY_SYNTAX_PAREN_GROUP,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(value: Value): void { const x = match value { _ => return, }; } fn after(): void {}",
            MMAY_SYNTAX_MATCH_ARM,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
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
            MMAY_SYNTAX_ASSIGNMENT_EXPRESSION
        ) == 11);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_UNARY_EXPRESSION
        ) >= 3);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_LITERAL_EXPRESSION
        ) >= 8);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_CLOSURE_EXPRESSION
        ) == 2);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_CONVERSION_EXPRESSION
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
        ) >= 2);
        mmay_parse_fixture_dispose(&fixture);
    }
}

void mmay_test_parser_expression_recovery(void) {
    static const struct {
        const char *source;
        MarimaySyntaxKind kind;
        const char *diagnostic;
    } cases[] = {
        {
            "fn broken(): void { const x = ; } fn after(): void {}",
            MMAY_SYNTAX_EXPRESSION,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = (1; } fn after(): void {}",
            MMAY_SYNTAX_PAREN_GROUP,
            MMAY_DIAGNOSTIC_UNTERMINATED_DELIMITER
        },
        {
            "fn broken(): void { const x = values[0)]; } fn after(): void {}",
            MMAY_SYNTAX_BRACKET_GROUP,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = $\"{}\"; } fn after(): void {}",
            MMAY_SYNTAX_INTERPOLATED_EXPRESSION,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = first second; } fn after(): void {}",
            MMAY_SYNTAX_NAME_EXPRESSION,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = 1 2; } fn after(): void {}",
            MMAY_SYNTAX_LITERAL_EXPRESSION,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = !; } fn after(): void {}",
            MMAY_SYNTAX_UNARY_EXPRESSION,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = 1 +; } fn after(): void {}",
            MMAY_SYNTAX_BINARY_EXPRESSION,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { let x = 1; x =; } fn after(): void {}",
            MMAY_SYNTAX_ASSIGNMENT_EXPRESSION,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = value.; } fn after(): void {}",
            MMAY_SYNTAX_MEMBER_EXPRESSION,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = call(, value); } fn after(): void {}",
            MMAY_SYNTAX_CALL_EXPRESSION,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = values[0)]; } fn after(): void {}",
            MMAY_SYNTAX_INDEX_EXPRESSION,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = value? next; } fn after(): void {}",
            MMAY_SYNTAX_TRY_EXPRESSION,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = spawn; } fn after(): void {}",
            MMAY_SYNTAX_SPAWN_EXPRESSION,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = [1; } fn after(): void {}",
            MMAY_SYNTAX_ARRAY_EXPRESSION,
            MMAY_DIAGNOSTIC_UNTERMINATED_DELIMITER
        },
        {
            "fn broken(): void { const x = Box { value: 1] }; } "
            "fn after(): void {}",
            MMAY_SYNTAX_RECORD_EXPRESSION,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = (value: int): => value; } "
            "fn after(): void {}",
            MMAY_SYNTAX_CLOSURE_EXPRESSION,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = match { _ => 1, }; } fn after(): void {}",
            MMAY_SYNTAX_MATCH_EXPRESSION,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = match value { _ => 1 _ => 2, }; } fn after(): void {}",
            MMAY_SYNTAX_MATCH_ARM,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = match value { => 1, }; } fn after(): void {}",
            MMAY_SYNTAX_PATTERN,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "fn broken(value: Value): int { return match value { case Ready => 1, _ => 0, }; } fn after(): void {}",
            MMAY_SYNTAX_MATCH_ARM,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = call(, value); } fn after(): void {}",
            MMAY_SYNTAX_ARGUMENTS,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        }
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        MarimayParseFixture fixture;
        bool initialized = mmay_parse_fixture_init(&fixture, cases[index].source);
        MMAY_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
        MMAY_EXPECT(mmay_parse_has_diagnostic(
            &fixture.parsed,
            cases[index].diagnostic
        ));
        MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            cases[index].kind
        ) != 0);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL
        ) >= 2);
        mmay_parse_fixture_dispose(&fixture);
    }
}

void mmay_test_parser_pattern_grammar(void) {
    static const char valid[] =
        "fn patterns(value: Value): int {\n"
        "  return match value {\n"
        "    _ => 0,\n"
        "    1 => 1,\n"
        "    _ as binding => 2,\n"
        "    pkg.Some(inner, (left, right)) => inner,\n"
        "    User { id, name: alias } => id,\n"
        "    None | Gone => 3,\n"
        "  };\n"
        "}\n";
    static const struct {
        const char *pattern;
        const char *diagnostic;
    } invalid[] = {
        {"", MMAY_DIAGNOSTIC_EXPECTED_TOKEN},
        {"Some(value]", MMAY_DIAGNOSTIC_UNTERMINATED_DELIMITER},
        {"Some { : value }", MMAY_DIAGNOSTIC_EXPECTED_TOKEN},
        {"[left, right]", MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN},
        {"None |", MMAY_DIAGNOSTIC_EXPECTED_TOKEN},
        {"(value)", MMAY_DIAGNOSTIC_EXPECTED_TOKEN}
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
            MMAY_SYNTAX_PATTERN
        ) == 6);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_OR_PATTERN
        ) == 1);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_WILDCARD_PATTERN
        ) == 2);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_LITERAL_PATTERN
        ) == 1);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_CONSTRUCTOR_PATTERN
        ) == 2);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_POSITIONAL_PATTERN
        ) == 1);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_NAMED_PATTERN
        ) == 1);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_TUPLE_PATTERN
        ) == 1);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_PATTERN_FIELD
        ) == 2);
        mmay_parse_fixture_dispose(&fixture);
    }

    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        char source[512];
        int length = snprintf(
            source,
            sizeof(source),
            "fn broken(value: Value): int { const x = match value { %s => 1, _ => 0, }; return x; } fn after(): void {}",
            invalid[index].pattern
        );
        bool initialized = length > 0 && (size_t)length < sizeof(source)
            && mmay_parse_fixture_init(&fixture, source);
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
            MMAY_SYNTAX_MATCH_ARM
        ) >= 2);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL
        ) >= 2);
        mmay_parse_fixture_dispose(&fixture);
    }
}

void mmay_test_parser_record_field_grammar(void) {
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
        {": value", MMAY_DIAGNOSTIC_EXPECTED_TOKEN, 1},
        {"first second", MMAY_DIAGNOSTIC_EXPECTED_TOKEN, 2},
        {"value:", MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN, 1},
        {"value: 1]", MMAY_DIAGNOSTIC_EXPECTED_TOKEN, 2},
        {"value. other", MMAY_DIAGNOSTIC_EXPECTED_TOKEN, 2}
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
            MMAY_SYNTAX_RECORD_EXPRESSION
        ) == 3);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_RECORD_FIELDS
        ) == 3);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_FIELD_VALUE
        ) == 3);
        mmay_parse_fixture_dispose(&fixture);
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
            && mmay_parse_fixture_init(&fixture, source);
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
            MMAY_SYNTAX_RECORD_FIELDS
        ) != 0);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_FIELD_VALUE
        ) >= invalid[index].minimum_fields);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL
        ) >= 2);
        mmay_parse_fixture_dispose(&fixture);
    }
}

void mmay_test_parser_exact_delimited_grammar(void) {
    static const char valid[] =
        "@meta(1, call(2), Item { value: 3 },)\n"
        "enum Payload {\n"
        "  Pos(int, pkg.Value),\n"
        "  Named { id: int, value: pkg.Value, },\n"
        "}\n"
        "fn exact(value: int): void {\n"
        "  const item = values[call(value)];\n"
        "  const closure = (item: int): int => item;\n"
        "  const inferredClosure = (item) => item;\n"
        "}\n";
    static const struct {
        const char *source;
        MarimaySyntaxKind kind;
        const char *diagnostic;
    } invalid[] = {
        {
            "@meta(, 1) fn broken(): void {} fn after(): void {}",
            MMAY_SYNTAX_ATTRIBUTE,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "enum Bad { Empty(), Good } fn after(): void {}",
            MMAY_SYNTAX_POSITIONAL_PAYLOAD,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "enum Bad { Broken(int string), Good } fn after(): void {}",
            MMAY_SYNTAX_POSITIONAL_PAYLOAD,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "enum Bad { Broken { public id: int, }, Good } fn after(): void {}",
            MMAY_SYNTAX_PAYLOAD_FIELD,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN
        },
        {
            "enum Bad { Broken { id: int value: int }, Good } fn after(): void {}",
            MMAY_SYNTAX_NAMED_PAYLOAD,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = values[]; } fn after(): void {}",
            MMAY_SYNTAX_BRACKET_GROUP,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN
        },
        {
            "fn broken(): void { const x = values[0, 1]; } "
            "fn after(): void {}",
            MMAY_SYNTAX_INDEX_EXPRESSION,
            MMAY_DIAGNOSTIC_UNTERMINATED_DELIMITER
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
            MMAY_SYNTAX_POSITIONAL_PAYLOAD
        ) == 1);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_NAMED_PAYLOAD
        ) == 1);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_PAYLOAD_FIELD
        ) == 2);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_BRACKET_GROUP
        ) == 1);
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_CLOSURE_EXPRESSION
        ) == 2);
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

void mmay_test_parser_statement_forms(void) {
    static const char source[] =
        "fn yield(value: int): void {}\n"
        "fn nested(value: Option<[]Result<int, Error>>): Option<[]int> {\n"
        "  let delayed: int;\n"
        "  let _ = 0;\n"
        "  const _ = 0;\n"
        "  call();\n"
        "  defer cleanup();\n"
        "  for item in value {\n"
        "    use(item);\n"
        "  }\n"
        "  for index in 0..3 {\n"
        "    if index == 1 { continue; } else if false { break; } else { use(index); }\n"
        "  }\n"
        "  while false { break; }\n"
        "  scope { const task = spawn service.call(); task.wait(); }\n"
        "  unsafe { panic(\"checked\"); }\n"
        "  { yield(1); }\n"
        "  { return; }\n"
        "  return value;\n"
        "}\n";
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
        MMAY_SYNTAX_FOR_STATEMENT
    ) == 2);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_FOR_HEADER
    ) == 2);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_IF_STATEMENT
    ) == 2);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_WHILE_STATEMENT
    ) == 1);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_UNSAFE_STATEMENT
    ) == 1);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_SCOPE_STATEMENT
    ) == 1);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_LET_DECL
    ) == 2);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_DEFER_STATEMENT
    ) == 1);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_RETURN_STATEMENT
    ) == 2);
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_BLOCK_STATEMENT
    ) == 2);
    mmay_parse_fixture_dispose(&fixture);
}

void mmay_test_parser_statement_recovery(void) {
    static const struct {
        const char *source;
        MarimaySyntaxKind kind;
        const char *diagnostic;
        size_t minimum_function_count;
    } cases[] = {
        {
            "fn broken(): void { call() } fn after(): void {}",
            MMAY_SYNTAX_EXPRESSION_STATEMENT,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { return 1 } fn after(): void {}",
            MMAY_SYNTAX_RETURN_STATEMENT,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { defer; } fn after(): void {}",
            MMAY_SYNTAX_DEFER_STATEMENT,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { if {} } fn after(): void {}",
            MMAY_SYNTAX_IF_STATEMENT,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { while {} } fn after(): void {}",
            MMAY_SYNTAX_WHILE_STATEMENT,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN,
            2
        },
        {
            "fn broken(values: Values): void { for in values {} } "
            "fn after(): void {}",
            MMAY_SYNTAX_FOR_HEADER,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN,
            2
        },
        {
            "fn broken(values: Values): void { for item values {} } "
            "fn after(): void {}",
            MMAY_SYNTAX_FOR_HEADER,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { for item in {} } fn after(): void {}",
            MMAY_SYNTAX_FOR_HEADER,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN,
            2
        },
        {
            "fn broken(values: Values): void { for const item of values {} } "
            "fn after(): void {}",
            MMAY_SYNTAX_FOR_HEADER,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN,
            1
        },
        {
            "fn broken(): void { for let index = 0; index < 3; index += 1 {} } "
            "fn after(): void {}",
            MMAY_SYNTAX_FOR_HEADER,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN,
            1
        },
        {
            "fn broken(): void { yield 1; } fn after(): void {}",
            MMAY_SYNTAX_EXPRESSION_STATEMENT,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { scope { return;",
            MMAY_SYNTAX_SCOPE_STATEMENT,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN,
            1
        },
        {
            "fn broken(): void { break } fn after(): void {}",
            MMAY_SYNTAX_BREAK_STATEMENT,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { continue } fn after(): void {}",
            MMAY_SYNTAX_CONTINUE_STATEMENT,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN,
            2
        },
        {
            "fn broken(): void { unsafe { return;",
            MMAY_SYNTAX_UNSAFE_STATEMENT,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN,
            1
        },
        {
            "fn broken(): void { { return;",
            MMAY_SYNTAX_BLOCK_STATEMENT,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN,
            1
        }
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        MarimayParseFixture fixture;
        size_t function_count = 0;
        bool initialized = mmay_parse_fixture_init(&fixture, cases[index].source);
        MMAY_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
        MMAY_EXPECT(mmay_parse_has_diagnostic(
            &fixture.parsed,
            cases[index].diagnostic
        ));
        MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
        MMAY_EXPECT(mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            cases[index].kind
        ) != 0);
        function_count = mmay_syntax_count_kind(
            fixture.parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL
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
        MMAY_EXPECT(function_count >= cases[index].minimum_function_count);
        mmay_parse_fixture_dispose(&fixture);
    }
}
