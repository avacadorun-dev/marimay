#include "test_parser_support.h"

void ruda_test_parser_pratt_expressions(void) {
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

void ruda_test_parser_expression_grammar(void) {
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
        "  const converted = a as u64;\n"
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
            "fn broken(value: int): void { const x = value as; } "
            "fn after(): void {}",
            RUDA_SYNTAX_CONVERSION_EXPRESSION,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN
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
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_CONVERSION_EXPRESSION
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

void ruda_test_parser_expression_recovery(void) {
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

void ruda_test_parser_pattern_grammar(void) {
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

void ruda_test_parser_record_field_grammar(void) {
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

void ruda_test_parser_exact_delimited_grammar(void) {
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

void ruda_test_parser_statement_forms(void) {
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

void ruda_test_parser_statement_recovery(void) {
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
