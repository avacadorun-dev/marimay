#include "test_formatter_support.h"

void ruda_test_formatter_round_trip(void) {
    static const char source[] =
        "@derive(Debug)\r\n"
        "type  Box<T>{public mutable value:T;}\r\n"
        "fn  compute( a:int,b:int,c:int):Result<int,Error>{\r\n"
        "/*keep\r\n  shape*/const values=[a,b+c];\r\n"
        "for let i=0;i<3;i+=1{values.push(i);}\r\n"
        "return service.load<Result<int,Error>>(values[0],b*c)?.value;\r\n"
        "}\r\n";
    RudaFormatterFixture original;
    RudaFormatterFixture formatted;
    RudaFormatResult first;
    RudaFormatResult second;
    RudaAstTree original_ast;
    RudaAstTree formatted_ast;
    bool original_ready = ruda_formatter_fixture_init(
        &original,
        "unformatted.ruda",
        (const unsigned char *)source,
        strlen(source)
    );
    RUDA_EXPECT(original_ready);
    if (!original_ready) {
        return;
    }
    RUDA_EXPECT(original.lexed.diagnostics.length == 0);
    RUDA_EXPECT(original.parsed.diagnostics.length == 0);
    RUDA_EXPECT(ruda_format_source(
        &original.lexed,
        &original.parsed,
        ruda_system_allocator(),
        ruda_format_default_options(),
        &first
    ) == RUDA_FORMAT_OK);
    if (!first.initialized) {
        ruda_formatter_fixture_dispose(&original);
        return;
    }
    RUDA_EXPECT(first.changed);
    RUDA_EXPECT_SNAPSHOT("formatter-canonical.ruda", (const char *)first.output.data);

    RUDA_EXPECT(ruda_formatter_fixture_init(
        &formatted,
        "formatted.ruda",
        first.output.data,
        first.output.length
    ));
    if (!formatted.parsed.initialized) {
        ruda_format_result_dispose(&first);
        ruda_formatter_fixture_dispose(&original);
        return;
    }
    RUDA_EXPECT(formatted.lexed.diagnostics.length == 0);
    RUDA_EXPECT(formatted.parsed.diagnostics.length == 0);
    RUDA_EXPECT(ruda_formatter_comments_equal(&original.lexed, &formatted.lexed));
    RUDA_EXPECT(ruda_format_source(
        &formatted.lexed,
        &formatted.parsed,
        ruda_system_allocator(),
        ruda_format_default_options(),
        &second
    ) == RUDA_FORMAT_OK);
    if (second.initialized) {
        RUDA_EXPECT(!second.changed);
        RUDA_EXPECT(second.output.length == first.output.length);
        RUDA_EXPECT(memcmp(
            second.output.data,
            first.output.data,
            first.output.length
        ) == 0);
        ruda_format_result_dispose(&second);
    }

    RUDA_EXPECT(ruda_ast_project(
        &original.parsed.tree,
        ruda_system_allocator(),
        ruda_ast_default_options(),
        &original_ast
    ) == RUDA_AST_OK);
    RUDA_EXPECT(ruda_ast_project(
        &formatted.parsed.tree,
        ruda_system_allocator(),
        ruda_ast_default_options(),
        &formatted_ast
    ) == RUDA_AST_OK);
    if (original_ast.initialized && formatted_ast.initialized) {
        RUDA_EXPECT(ruda_formatter_ast_semantically_equal(
            original_ast.root,
            original.snapshot,
            formatted_ast.root,
            formatted.snapshot
        ));
        RUDA_EXPECT(ruda_fuzz_ast_semantically_equal(
            &original_ast,
            original.snapshot,
            &formatted_ast,
            formatted.snapshot
        ));
        ruda_ast_tree_dispose(&formatted_ast);
        ruda_ast_tree_dispose(&original_ast);
    }
    ruda_formatter_fixture_dispose(&formatted);
    ruda_format_result_dispose(&first);
    ruda_formatter_fixture_dispose(&original);
}

void ruda_test_formatter_fixture_file(void) {
    static const struct {
        const char *input_path;
        const char *expected_path;
    } fixtures[] = {
        {
            "syntax/parser/all-productions.ruda",
            "syntax/parser/all-productions.ruda"
        },
        {
            "format/types.input.ruda",
            "format/types.expected.ruda"
        },
        {
            "format/imports.input.ruda",
            "format/imports.expected.ruda"
        },
        {
            "format/braced-groups.input.ruda",
            "format/braced-groups.expected.ruda"
        },
        {
            "format/named-enum-payload.input.ruda",
            "format/named-enum-payload.expected.ruda"
        },
        {
            "format/nominal-cp1.input.ruda",
            "format/nominal-cp1.expected.ruda"
        },
        {
            "format/grammar.input.ruda",
            "format/grammar.expected.ruda"
        },
        {
            "format/comments.input.ruda",
            "format/comments.expected.ruda"
        },
        {
            "format/strings.input.ruda",
            "format/strings.expected.ruda"
        },
        {
            "format/matrix-arrays.input.ruda",
            "format/matrix-arrays.expected.ruda"
        },
        {
            "format/stdout-preview.input.ruda",
            "format/stdout-preview.expected.ruda"
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
        RudaFormatterFixture fixture;
        RudaFormatterFixture formatted;
        RudaFormatResult result;
        RudaFormatResult second;
        RudaAstTree original_ast;
        RudaAstTree formatted_ast;
        bool fixture_ready;
        bool formatted_ready = false;
        memset(&original_ast, 0, sizeof(original_ast));
        memset(&formatted_ast, 0, sizeof(formatted_ast));
        RUDA_EXPECT(ruda_formatter_read_file(
            fixtures[fixture_index].input_path,
            input,
            sizeof(input),
            &input_length
        ));
        RUDA_EXPECT(ruda_formatter_read_file(
            fixtures[fixture_index].expected_path,
            expected,
            sizeof(expected),
            &expected_length
        ));
        fixture_ready = ruda_formatter_fixture_init(
            &fixture,
            fixtures[fixture_index].input_path,
            input,
            input_length
        );
        RUDA_EXPECT(fixture_ready);
        if (!fixture_ready) {
            continue;
        }
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        if (fixture.parsed.diagnostics.length != 0) {
            size_t diagnostic_index = 0;
            for (diagnostic_index = 0;
                diagnostic_index < fixture.parsed.diagnostics.length;
                diagnostic_index += 1) {
                const RudaParseDiagnostic *diagnostic = ruda_parse_diagnostic(
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
        RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_format_source(
            &fixture.lexed,
            &fixture.parsed,
            ruda_system_allocator(),
            ruda_format_default_options(),
            &result
        ) == RUDA_FORMAT_OK);
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
            RUDA_EXPECT(result.output.length == expected_length);
            RUDA_EXPECT(result.output.length != expected_length
                || memcmp(result.output.data, expected, expected_length) == 0);
            formatted_ready = ruda_formatter_fixture_init(
                &formatted,
                fixtures[fixture_index].expected_path,
                result.output.data,
                result.output.length
            );
            RUDA_EXPECT(formatted_ready);
            if (formatted_ready) {
                RUDA_EXPECT(formatted.lexed.diagnostics.length == 0);
                RUDA_EXPECT(formatted.parsed.diagnostics.length == 0);
                RUDA_EXPECT(ruda_formatter_comments_equal(
                    &fixture.lexed,
                    &formatted.lexed
                ));
                RUDA_EXPECT(ruda_format_source(
                    &formatted.lexed,
                    &formatted.parsed,
                    ruda_system_allocator(),
                    ruda_format_default_options(),
                    &second
                ) == RUDA_FORMAT_OK);
                if (second.initialized) {
                    RUDA_EXPECT(!second.changed);
                    RUDA_EXPECT(second.output.length == result.output.length);
                    RUDA_EXPECT(second.output.length != result.output.length
                        || memcmp(
                            second.output.data,
                            result.output.data,
                            result.output.length
                        ) == 0);
                    ruda_format_result_dispose(&second);
                }
                RUDA_EXPECT(ruda_ast_project(
                    &fixture.parsed.tree,
                    ruda_system_allocator(),
                    ruda_ast_default_options(),
                    &original_ast
                ) == RUDA_AST_OK);
                RUDA_EXPECT(ruda_ast_project(
                    &formatted.parsed.tree,
                    ruda_system_allocator(),
                    ruda_ast_default_options(),
                    &formatted_ast
                ) == RUDA_AST_OK);
                if (original_ast.initialized && formatted_ast.initialized) {
                    RUDA_EXPECT(ruda_formatter_ast_semantically_equal(
                        original_ast.root,
                        fixture.snapshot,
                        formatted_ast.root,
                        formatted.snapshot
                    ));
                    RUDA_EXPECT(ruda_fuzz_ast_semantically_equal(
                        &original_ast,
                        fixture.snapshot,
                        &formatted_ast,
                        formatted.snapshot
                    ));
                }
                ruda_ast_tree_dispose(&formatted_ast);
                ruda_ast_tree_dispose(&original_ast);
                ruda_formatter_fixture_dispose(&formatted);
            }
            ruda_format_result_dispose(&result);
        }
        ruda_formatter_fixture_dispose(&fixture);
    }
}

void ruda_test_formatter_long_list(void) {
    static const char source[] =
        "fn main(): void {return compute("
        "firstArgumentWithALongName,secondArgumentWithALongName,"
        "thirdArgumentWithALongName,fourthArgumentWithALongName);}\n";
    RudaFormatterFixture first_fixture;
    RudaFormatterFixture second_fixture;
    RudaFormatResult first;
    RudaFormatResult second;
    RUDA_EXPECT(ruda_formatter_fixture_init(
        &first_fixture,
        "long-list.ruda",
        (const unsigned char *)source,
        strlen(source)
    ));
    if (!first_fixture.parsed.initialized) {
        return;
    }
    RUDA_EXPECT(first_fixture.lexed.diagnostics.length == 0);
    RUDA_EXPECT(first_fixture.parsed.diagnostics.length == 0);
    RUDA_EXPECT(ruda_format_source(
        &first_fixture.lexed,
        &first_fixture.parsed,
        ruda_system_allocator(),
        ruda_format_default_options(),
        &first
    ) == RUDA_FORMAT_OK);
    if (!first.initialized) {
        ruda_formatter_fixture_dispose(&first_fixture);
        return;
    }
    RUDA_EXPECT_SNAPSHOT("formatter-long-list.ruda", (const char *)first.output.data);
    RUDA_EXPECT(ruda_formatter_fixture_init(
        &second_fixture,
        "long-list-formatted.ruda",
        first.output.data,
        first.output.length
    ));
    if (second_fixture.parsed.initialized) {
        RUDA_EXPECT(second_fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(second_fixture.parsed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_format_source(
            &second_fixture.lexed,
            &second_fixture.parsed,
            ruda_system_allocator(),
            ruda_format_default_options(),
            &second
        ) == RUDA_FORMAT_OK);
        if (second.initialized) {
            RUDA_EXPECT(!second.changed);
            RUDA_EXPECT(second.output.length == first.output.length);
            RUDA_EXPECT(second.output.length != first.output.length
                || memcmp(
                    second.output.data,
                    first.output.data,
                    first.output.length
                ) == 0);
            ruda_format_result_dispose(&second);
        }
        ruda_formatter_fixture_dispose(&second_fixture);
    }
    ruda_format_result_dispose(&first);
    ruda_formatter_fixture_dispose(&first_fixture);
}

void ruda_test_formatter_example_corpus(void) {
    static const char *paths[] = {
        "examples/concurrency.ruda",
        "examples/nominal_cp1.ruda",
        "examples/service.ruda",
        "examples/tour.ruda",
        "examples/tour_test.ruda"
    };
    unsigned char source[131072];
    size_t path_index = 0;
    for (path_index = 0;
        path_index < sizeof(paths) / sizeof(paths[0]);
        path_index += 1) {
        RudaFormatterFixture original;
        RudaFormatterFixture formatted;
        RudaFormatResult first;
        RudaFormatResult second;
        RudaAstTree original_ast;
        RudaAstTree formatted_ast;
        size_t source_length = 0;
        bool original_ready;
        memset(&original_ast, 0, sizeof(original_ast));
        memset(&formatted_ast, 0, sizeof(formatted_ast));
        RUDA_EXPECT(ruda_formatter_read_file(
            paths[path_index],
            source,
            sizeof(source),
            &source_length
        ));
        original_ready = ruda_formatter_fixture_init(
            &original,
            paths[path_index],
            source,
            source_length
        );
        RUDA_EXPECT(original_ready);
        if (!original_ready) {
            continue;
        }
        RUDA_EXPECT(original.lexed.diagnostics.length == 0);
        RUDA_EXPECT(original.parsed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_format_source(
            &original.lexed,
            &original.parsed,
            ruda_system_allocator(),
            ruda_format_default_options(),
            &first
        ) == RUDA_FORMAT_OK);
        if (!first.initialized) {
            ruda_formatter_fixture_dispose(&original);
            continue;
        }
        RUDA_EXPECT(ruda_formatter_fixture_init(
            &formatted,
            paths[path_index],
            first.output.data,
            first.output.length
        ));
        if (!formatted.parsed.initialized) {
            ruda_format_result_dispose(&first);
            ruda_formatter_fixture_dispose(&original);
            continue;
        }
        RUDA_EXPECT(formatted.lexed.diagnostics.length == 0);
        RUDA_EXPECT(formatted.parsed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_formatter_comments_equal(&original.lexed, &formatted.lexed));
        RUDA_EXPECT(ruda_format_source(
            &formatted.lexed,
            &formatted.parsed,
            ruda_system_allocator(),
            ruda_format_default_options(),
            &second
        ) == RUDA_FORMAT_OK);
        if (second.initialized) {
            RUDA_EXPECT(!second.changed);
            RUDA_EXPECT(second.output.length == first.output.length);
            RUDA_EXPECT(second.output.length != first.output.length
                || memcmp(
                    second.output.data,
                    first.output.data,
                    first.output.length
                ) == 0);
            ruda_format_result_dispose(&second);
        }
        RUDA_EXPECT(ruda_ast_project(
            &original.parsed.tree,
            ruda_system_allocator(),
            ruda_ast_default_options(),
            &original_ast
        ) == RUDA_AST_OK);
        RUDA_EXPECT(ruda_ast_project(
            &formatted.parsed.tree,
            ruda_system_allocator(),
            ruda_ast_default_options(),
            &formatted_ast
        ) == RUDA_AST_OK);
        if (original_ast.initialized && formatted_ast.initialized) {
            bool ast_equal = ruda_formatter_ast_semantically_equal(
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
            RUDA_EXPECT(ast_equal);
            RUDA_EXPECT(ruda_fuzz_ast_semantically_equal(
                &original_ast,
                original.snapshot,
                &formatted_ast,
                formatted.snapshot
            ));
        }
        ruda_ast_tree_dispose(&formatted_ast);
        ruda_ast_tree_dispose(&original_ast);
        ruda_formatter_fixture_dispose(&formatted);
        ruda_format_result_dispose(&first);
        ruda_formatter_fixture_dispose(&original);
    }
}
