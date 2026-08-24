#include "test_formatter_support.h"

void mmay_test_formatter_round_trip(void) {
    static const char source[] =
        "@derive(Debug)\r\n"
        "type  Box<T>{public mutable value:T,}\r\n"
        "fn  compute( a:int,b:int,c:int):Result<int,Error>{\r\n"
        "/*keep\r\n"
        "  shape*/const values=[a,b+c];\r\n"
        "for i in 0..3 {const _ = values.push(i);}\r\n"
        "return service.load<Result<int,Error>>(values[0],b*c)?.value;\r\n"
        "}\r\n";
    MarimayFormatterFixture original;
    MarimayFormatterFixture formatted;
    MarimayFormatResult first;
    MarimayFormatResult second;
    MarimayAstTree original_ast;
    MarimayAstTree formatted_ast;
    bool original_ready = mmay_formatter_fixture_init(
        &original,
        "unformatted.my",
        (const unsigned char *)source,
        strlen(source)
    );
    MMAY_EXPECT(original_ready);
    if (!original_ready) {
        return;
    }
    MMAY_EXPECT(original.lexed.diagnostics.length == 0);
    MMAY_EXPECT(original.parsed.diagnostics.length == 0);
    MMAY_EXPECT(mmay_format_source(
        &original.lexed,
        &original.parsed,
        mmay_system_allocator(),
        mmay_format_default_options(),
        &first
    ) == MMAY_FORMAT_OK);
    if (!first.initialized) {
        mmay_formatter_fixture_dispose(&original);
        return;
    }
    MMAY_EXPECT(first.changed);
    MMAY_EXPECT_SNAPSHOT("formatter-canonical.my", (const char *)first.output.data);

    MMAY_EXPECT(mmay_formatter_fixture_init(
        &formatted,
        "formatted.my",
        first.output.data,
        first.output.length
    ));
    if (!formatted.parsed.initialized) {
        mmay_format_result_dispose(&first);
        mmay_formatter_fixture_dispose(&original);
        return;
    }
    MMAY_EXPECT(formatted.lexed.diagnostics.length == 0);
    MMAY_EXPECT(formatted.parsed.diagnostics.length == 0);
    MMAY_EXPECT(mmay_formatter_comments_equal(&original.lexed, &formatted.lexed));
    MMAY_EXPECT(mmay_format_source(
        &formatted.lexed,
        &formatted.parsed,
        mmay_system_allocator(),
        mmay_format_default_options(),
        &second
    ) == MMAY_FORMAT_OK);
    if (second.initialized) {
        MMAY_EXPECT(!second.changed);
        MMAY_EXPECT(second.output.length == first.output.length);
        MMAY_EXPECT(memcmp(
            second.output.data,
            first.output.data,
            first.output.length
        ) == 0);
        mmay_format_result_dispose(&second);
    }

    MMAY_EXPECT(mmay_ast_project(
        &original.parsed.tree,
        mmay_system_allocator(),
        mmay_ast_default_options(),
        &original_ast
    ) == MMAY_AST_OK);
    MMAY_EXPECT(mmay_ast_project(
        &formatted.parsed.tree,
        mmay_system_allocator(),
        mmay_ast_default_options(),
        &formatted_ast
    ) == MMAY_AST_OK);
    if (original_ast.initialized && formatted_ast.initialized) {
        MMAY_EXPECT(mmay_formatter_ast_semantically_equal(
            original_ast.root,
            original.snapshot,
            formatted_ast.root,
            formatted.snapshot
        ));
        MMAY_EXPECT(mmay_fuzz_ast_semantically_equal(
            &original_ast,
            original.snapshot,
            &formatted_ast,
            formatted.snapshot
        ));
        mmay_ast_tree_dispose(&formatted_ast);
        mmay_ast_tree_dispose(&original_ast);
    }
    mmay_formatter_fixture_dispose(&formatted);
    mmay_format_result_dispose(&first);
    mmay_formatter_fixture_dispose(&original);
}

void mmay_test_formatter_fixture_file(void) {
    static const struct {
        const char *input_path;
        const char *expected_path;
    } fixtures[] = {
        {
            "syntax/parser/all-productions.my",
            "syntax/parser/all-productions.my"
        },
        {
            "format/types.input.my",
            "format/types.expected.my"
        },
        {
            "format/imports.input.my",
            "format/imports.expected.my"
        },
        {
            "format/import-names.input.my",
            "format/import-names.expected.my"
        },
        {
            "format/braced-groups.input.my",
            "format/braced-groups.expected.my"
        },
        {
            "format/named-enum-payload.input.my",
            "format/named-enum-payload.expected.my"
        },
        {
            "format/nominal-cp1.input.my",
            "format/nominal-cp1.expected.my"
        },
        {
            "format/grammar.input.my",
            "format/grammar.expected.my"
        },
        {
            "format/comments.input.my",
            "format/comments.expected.my"
        },
        {
            "format/strings.input.my",
            "format/strings.expected.my"
        },
        {
            "format/matrix-arrays.input.my",
            "format/matrix-arrays.expected.my"
        },
        {
            "format/array-checkpoint.input.my",
            "format/array-checkpoint.expected.my"
        },
        {
            "format/array-mutation.input.my",
            "format/array-mutation.expected.my"
        },
        {
            "format/stdout-preview.input.my",
            "format/stdout-preview.expected.my"
        },
        {
            "format/generics.input.my",
            "format/generics.expected.my"
        },
        {
            "format/nested-generics.input.my",
            "format/nested-generics.expected.my"
        },
        {
            "format/numeric-widths.input.my",
            "format/numeric-widths.expected.my"
        },
        {
            "format/defer.input.my",
            "format/defer.expected.my"
        },
        {
            "format/test-output.input.my",
            "format/test-output.expected.my"
        },
        {
            "format/fuzz-regression.input.my",
            "format/fuzz-regression.expected.my"
        },
        {
            "format/fuzz-parenthesized-regression.input.my",
            "format/fuzz-parenthesized-regression.expected.my"
        },
        {
            "format/fuzz-index-regression.input.my",
            "format/fuzz-index-regression.expected.my"
        },
        {
            "format/fuzz-match-trailing-comma.input.my",
            "format/fuzz-match-trailing-comma.expected.my"
        },
        {
            "format/cp30-interpolation.input.my",
            "format/cp30-interpolation.expected.my"
        },
        {
            "format/revision-surface.input.my",
            "format/revision-surface.expected.my"
        }
    };
    unsigned char input[32768];
    unsigned char expected[32768];
    size_t fixture_index = 0;
    for (fixture_index = 0;
        fixture_index < sizeof(fixtures) / sizeof(fixtures[0]);
        fixture_index += 1) {
        size_t input_length = 0;
        size_t expected_length = 0;
        MarimayFormatterFixture fixture;
        MarimayFormatterFixture formatted;
        MarimayFormatResult result;
        MarimayFormatResult second;
        MarimayAstTree original_ast;
        MarimayAstTree formatted_ast;
        bool fixture_ready;
        bool formatted_ready = false;
        memset(&original_ast, 0, sizeof(original_ast));
        memset(&formatted_ast, 0, sizeof(formatted_ast));
        MMAY_EXPECT(mmay_formatter_read_file(
            fixtures[fixture_index].input_path,
            input,
            sizeof(input),
            &input_length
        ));
        MMAY_EXPECT(mmay_formatter_read_file(
            fixtures[fixture_index].expected_path,
            expected,
            sizeof(expected),
            &expected_length
        ));
        fixture_ready = mmay_formatter_fixture_init(
            &fixture,
            fixtures[fixture_index].input_path,
            input,
            input_length
        );
        MMAY_EXPECT(fixture_ready);
        if (!fixture_ready) {
            continue;
        }
        MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
        if (fixture.parsed.diagnostics.length != 0) {
            size_t diagnostic_index = 0;
            for (diagnostic_index = 0;
                diagnostic_index < fixture.parsed.diagnostics.length;
                diagnostic_index += 1) {
                const MarimayParseDiagnostic *diagnostic = mmay_parse_diagnostic(
                    &fixture.parsed,
                    diagnostic_index
                );
                if (diagnostic != NULL) {
                    fprintf(
                        stderr,
                        "%s:%zu..%zu: %s %s\n",
                        fixtures[fixture_index].input_path,
                        diagnostic->start,
                        diagnostic->end,
                        diagnostic->code,
                        diagnostic->message
                    );
                }
            }
        }
        MMAY_EXPECT(fixture.parsed.diagnostics.length == 0);
        MMAY_EXPECT(mmay_format_source(
            &fixture.lexed,
            &fixture.parsed,
            mmay_system_allocator(),
            mmay_format_default_options(),
            &result
        ) == MMAY_FORMAT_OK);
        if (result.initialized) {
            if (result.output.length != expected_length
                || memcmp(
                    result.output.data,
                    expected,
                    result.output.length < expected_length
                        ? result.output.length
                        : expected_length
                ) != 0) {
                fprintf(
                    stderr,
                    "--- formatter actual for %s ---\n",
                    fixtures[fixture_index].input_path
                );
                (void)fwrite(
                    result.output.data,
                    1,
                    result.output.length,
                    stderr
                );
            }
            MMAY_EXPECT(result.output.length == expected_length);
            MMAY_EXPECT(result.output.length != expected_length
                || memcmp(result.output.data, expected, expected_length) == 0);
            formatted_ready = mmay_formatter_fixture_init(
                &formatted,
                fixtures[fixture_index].expected_path,
                result.output.data,
                result.output.length
            );
            MMAY_EXPECT(formatted_ready);
            if (formatted_ready) {
                MMAY_EXPECT(formatted.lexed.diagnostics.length == 0);
                MMAY_EXPECT(formatted.parsed.diagnostics.length == 0);
                MMAY_EXPECT(mmay_formatter_comments_equal(
                    &fixture.lexed,
                    &formatted.lexed
                ));
                MMAY_EXPECT(mmay_format_source(
                    &formatted.lexed,
                    &formatted.parsed,
                    mmay_system_allocator(),
                    mmay_format_default_options(),
                    &second
                ) == MMAY_FORMAT_OK);
                if (second.initialized) {
                    MMAY_EXPECT(!second.changed);
                    MMAY_EXPECT(second.output.length == result.output.length);
                    MMAY_EXPECT(second.output.length != result.output.length
                        || memcmp(
                            second.output.data,
                            result.output.data,
                            result.output.length
                        ) == 0);
                    mmay_format_result_dispose(&second);
                }
                MMAY_EXPECT(mmay_ast_project(
                    &fixture.parsed.tree,
                    mmay_system_allocator(),
                    mmay_ast_default_options(),
                    &original_ast
                ) == MMAY_AST_OK);
                MMAY_EXPECT(mmay_ast_project(
                    &formatted.parsed.tree,
                    mmay_system_allocator(),
                    mmay_ast_default_options(),
                    &formatted_ast
                ) == MMAY_AST_OK);
                if (original_ast.initialized && formatted_ast.initialized) {
                    MMAY_EXPECT(mmay_formatter_ast_semantically_equal(
                        original_ast.root,
                        fixture.snapshot,
                        formatted_ast.root,
                        formatted.snapshot
                    ));
                    MMAY_EXPECT(mmay_fuzz_ast_semantically_equal(
                        &original_ast,
                        fixture.snapshot,
                        &formatted_ast,
                        formatted.snapshot
                    ));
                }
                mmay_ast_tree_dispose(&formatted_ast);
                mmay_ast_tree_dispose(&original_ast);
                mmay_formatter_fixture_dispose(&formatted);
            }
            mmay_format_result_dispose(&result);
        }
        mmay_formatter_fixture_dispose(&fixture);
    }
}

void mmay_test_formatter_long_list(void) {
    static const char source[] =
        "fn main(): void {return compute("
        "firstArgumentWithALongName,secondArgumentWithALongName,"
        "thirdArgumentWithALongName,fourthArgumentWithALongName);}\n";
    MarimayFormatterFixture first_fixture;
    MarimayFormatterFixture second_fixture;
    MarimayFormatResult first;
    MarimayFormatResult second;
    MMAY_EXPECT(mmay_formatter_fixture_init(
        &first_fixture,
        "long-list.my",
        (const unsigned char *)source,
        strlen(source)
    ));
    if (!first_fixture.parsed.initialized) {
        return;
    }
    MMAY_EXPECT(first_fixture.lexed.diagnostics.length == 0);
    MMAY_EXPECT(first_fixture.parsed.diagnostics.length == 0);
    MMAY_EXPECT(mmay_format_source(
        &first_fixture.lexed,
        &first_fixture.parsed,
        mmay_system_allocator(),
        mmay_format_default_options(),
        &first
    ) == MMAY_FORMAT_OK);
    if (!first.initialized) {
        mmay_formatter_fixture_dispose(&first_fixture);
        return;
    }
    MMAY_EXPECT_SNAPSHOT("formatter-long-list.my", (const char *)first.output.data);
    MMAY_EXPECT(mmay_formatter_fixture_init(
        &second_fixture,
        "long-list-formatted.my",
        first.output.data,
        first.output.length
    ));
    if (second_fixture.parsed.initialized) {
        MMAY_EXPECT(second_fixture.lexed.diagnostics.length == 0);
        MMAY_EXPECT(second_fixture.parsed.diagnostics.length == 0);
        MMAY_EXPECT(mmay_format_source(
            &second_fixture.lexed,
            &second_fixture.parsed,
            mmay_system_allocator(),
            mmay_format_default_options(),
            &second
        ) == MMAY_FORMAT_OK);
        if (second.initialized) {
            MMAY_EXPECT(!second.changed);
            MMAY_EXPECT(second.output.length == first.output.length);
            MMAY_EXPECT(second.output.length != first.output.length
                || memcmp(
                    second.output.data,
                    first.output.data,
                    first.output.length
                ) == 0);
            mmay_format_result_dispose(&second);
        }
        mmay_formatter_fixture_dispose(&second_fixture);
    }
    mmay_format_result_dispose(&first);
    mmay_formatter_fixture_dispose(&first_fixture);
}

void mmay_test_formatter_example_corpus(void) {
    static const char *paths[] = {
        "examples/concurrency.my",
        "examples/array_mutation.my",
        "examples/defer_cp7.my",
        "examples/nominal_cp1.my",
        "examples/numeric_cp6.my",
        "examples/service.my",
        "examples/tour.my",
        "examples/tour_test.my"
    };
    unsigned char source[131072];
    size_t path_index = 0;
    for (path_index = 0;
        path_index < sizeof(paths) / sizeof(paths[0]);
        path_index += 1) {
        MarimayFormatterFixture original;
        MarimayFormatterFixture formatted;
        MarimayFormatResult first;
        MarimayFormatResult second;
        MarimayAstTree original_ast;
        MarimayAstTree formatted_ast;
        size_t source_length = 0;
        bool original_ready;
        memset(&original_ast, 0, sizeof(original_ast));
        memset(&formatted_ast, 0, sizeof(formatted_ast));
        MMAY_EXPECT(mmay_formatter_read_file(
            paths[path_index],
            source,
            sizeof(source),
            &source_length
        ));
        original_ready = mmay_formatter_fixture_init(
            &original,
            paths[path_index],
            source,
            source_length
        );
        MMAY_EXPECT(original_ready);
        if (!original_ready) {
            continue;
        }
        MMAY_EXPECT(original.lexed.diagnostics.length == 0);
        MMAY_EXPECT(original.parsed.diagnostics.length == 0);
        MMAY_EXPECT(mmay_format_source(
            &original.lexed,
            &original.parsed,
            mmay_system_allocator(),
            mmay_format_default_options(),
            &first
        ) == MMAY_FORMAT_OK);
        if (!first.initialized) {
            mmay_formatter_fixture_dispose(&original);
            continue;
        }
        MMAY_EXPECT(mmay_formatter_fixture_init(
            &formatted,
            paths[path_index],
            first.output.data,
            first.output.length
        ));
        if (!formatted.parsed.initialized) {
            mmay_format_result_dispose(&first);
            mmay_formatter_fixture_dispose(&original);
            continue;
        }
        MMAY_EXPECT(formatted.lexed.diagnostics.length == 0);
        MMAY_EXPECT(formatted.parsed.diagnostics.length == 0);
        MMAY_EXPECT(mmay_formatter_comments_equal(&original.lexed, &formatted.lexed));
        MMAY_EXPECT(mmay_format_source(
            &formatted.lexed,
            &formatted.parsed,
            mmay_system_allocator(),
            mmay_format_default_options(),
            &second
        ) == MMAY_FORMAT_OK);
        if (second.initialized) {
            MMAY_EXPECT(!second.changed);
            MMAY_EXPECT(second.output.length == first.output.length);
            MMAY_EXPECT(second.output.length != first.output.length
                || memcmp(
                    second.output.data,
                    first.output.data,
                    first.output.length
                ) == 0);
            mmay_format_result_dispose(&second);
        }
        MMAY_EXPECT(mmay_ast_project(
            &original.parsed.tree,
            mmay_system_allocator(),
            mmay_ast_default_options(),
            &original_ast
        ) == MMAY_AST_OK);
        MMAY_EXPECT(mmay_ast_project(
            &formatted.parsed.tree,
            mmay_system_allocator(),
            mmay_ast_default_options(),
            &formatted_ast
        ) == MMAY_AST_OK);
        if (original_ast.initialized && formatted_ast.initialized) {
            bool ast_equal = mmay_formatter_ast_semantically_equal(
                original_ast.root,
                original.snapshot,
                formatted_ast.root,
                formatted.snapshot
            );
            if (!ast_equal) {
                fprintf(
                    stderr,
                    "formatter AST mismatch for %s\n",
                    paths[path_index]
                );
            }
            MMAY_EXPECT(ast_equal);
            MMAY_EXPECT(mmay_fuzz_ast_semantically_equal(
                &original_ast,
                original.snapshot,
                &formatted_ast,
                formatted.snapshot
            ));
        }
        mmay_ast_tree_dispose(&formatted_ast);
        mmay_ast_tree_dispose(&original_ast);
        mmay_formatter_fixture_dispose(&formatted);
        mmay_format_result_dispose(&first);
        mmay_formatter_fixture_dispose(&original);
    }
}
