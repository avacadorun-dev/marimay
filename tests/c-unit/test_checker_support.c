#include "test_checker_support.h"


bool ruda_checker_fixture_init_path(
    RudaCheckerFixture *fixture,
    const char *path,
    const char *source
) {
    const RudaSourceSnapshot *snapshot = NULL;
    memset(fixture, 0, sizeof(*fixture));
    if (!ruda_source_manager_init(
            &fixture->sources,
            ruda_system_allocator()
        )) {
        return false;
    }
    fixture->sources_initialized = true;
    if (ruda_source_manager_add(
            &fixture->sources,
            path,
            strlen(path),
            source,
            strlen(source),
            &snapshot
        ) != RUDA_SOURCE_OK) {
        return false;
    }
    if (ruda_lex_snapshot(
            snapshot,
            ruda_system_allocator(),
            ruda_lex_default_options(),
            &fixture->lexed
        ) != RUDA_LEX_OK) {
        return false;
    }
    fixture->lexed_initialized = true;
    if (fixture->lexed.diagnostics.length != 0
        || ruda_parse_file(
            &fixture->lexed,
            ruda_system_allocator(),
            ruda_parse_default_options(),
            &fixture->parsed
        ) != RUDA_PARSE_OK) {
        return false;
    }
    fixture->parsed_initialized = true;
    if (fixture->parsed.diagnostics.length != 0
        || ruda_ast_project(
            &fixture->parsed.tree,
            ruda_system_allocator(),
            ruda_ast_default_options(),
            &fixture->ast
        ) != RUDA_AST_OK) {
        return false;
    }
    fixture->ast_initialized = true;
    return true;
}

bool ruda_checker_fixture_init(
    RudaCheckerFixture *fixture,
    const char *source
) {
    return ruda_checker_fixture_init_path(fixture, "checker.ruda", source);
}

void ruda_checker_fixture_dispose(RudaCheckerFixture *fixture) {
    if (fixture->ast_initialized) {
        ruda_ast_tree_dispose(&fixture->ast);
    }
    if (fixture->parsed_initialized) {
        ruda_parse_result_dispose(&fixture->parsed);
    }
    if (fixture->lexed_initialized) {
        ruda_lex_result_dispose(&fixture->lexed);
    }
    if (fixture->sources_initialized) {
        ruda_source_manager_dispose(&fixture->sources);
    }
    memset(fixture, 0, sizeof(*fixture));
}

void ruda_checker_expect_one_diagnostic(
    const char *source,
    const char *code,
    const char *marked_text,
    size_t occurrence
) {
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckStatus status;
    const RudaCheckDiagnostic *diagnostic = NULL;
    const char *marked = source;
    size_t index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    for (index = 0; index <= occurrence && marked != NULL; index += 1) {
        marked = strstr(marked, marked_text);
        if (index != occurrence && marked != NULL) {
            marked += strlen(marked_text);
        }
    }
    RUDA_EXPECT(marked != NULL);
    status = ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    );
    if (status != RUDA_CHECK_OK) {
        fprintf(
            stderr,
            "checker diagnostic fixture for %s returned status %d\n",
            code,
            (int)status
        );
    }
    RUDA_EXPECT(status == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(checked.functions.length == 0);
        RUDA_EXPECT(checked.constants.length == 0);
        RUDA_EXPECT(checked.type_aliases.length == 0);
        RUDA_EXPECT(checked.records.length == 0);
        RUDA_EXPECT(checked.enums.length == 0);
        RUDA_EXPECT(checked.function_index.length == 0);
        diagnostic = ruda_check_diagnostic(&checked, 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL && marked != NULL) {
            if (diagnostic->start != (size_t)(marked - source)
                || diagnostic->end
                    != (size_t)(marked - source) + strlen(marked_text)) {
                fprintf(
                    stderr,
                    "checker diagnostic span mismatch for %s: expected %zu..%zu, got %zu..%zu\n",
                    code,
                    (size_t)(marked - source),
                    (size_t)(marked - source) + strlen(marked_text),
                    diagnostic->start,
                    diagnostic->end
                );
            }
            RUDA_EXPECT(diagnostic->snapshot == fixture.ast.snapshot);
            RUDA_EXPECT(strcmp(diagnostic->code, code) == 0);
            RUDA_EXPECT(diagnostic->start == (size_t)(marked - source));
            RUDA_EXPECT(diagnostic->end
                == (size_t)(marked - source) + strlen(marked_text));
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

void ruda_checker_expect_one_diagnostic_path(
    const char *path,
    const char *source,
    const char *code,
    const char *marked_text
) {
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckDiagnostic *diagnostic = NULL;
    const char *marked = strstr(source, marked_text);
    RUDA_EXPECT(ruda_checker_fixture_init_path(&fixture, path, source));
    RUDA_EXPECT(marked != NULL);
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker-path",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(checked.functions.length == 0);
        diagnostic = ruda_check_diagnostic(&checked, 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL && marked != NULL) {
            RUDA_EXPECT(strcmp(diagnostic->code, code) == 0);
            RUDA_EXPECT(diagnostic->start == (size_t)(marked - source));
            RUDA_EXPECT(diagnostic->end
                == (size_t)(marked - source) + strlen(marked_text));
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

void ruda_checker_expect_diagnostic_message(
    const char *source,
    const char *expected_message
) {
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckDiagnostic *diagnostic = NULL;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker-message",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 1);
        diagnostic = ruda_check_diagnostic(&checked, 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            RUDA_EXPECT(strcmp(
                diagnostic->code,
                RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH
            ) == 0);
            RUDA_EXPECT(diagnostic->message != NULL);
            if (diagnostic->message != NULL) {
                RUDA_EXPECT(strcmp(
                    diagnostic->message,
                    expected_message
                ) == 0);
            }
            RUDA_EXPECT(diagnostic->owned_message.data != NULL);
            RUDA_EXPECT(diagnostic->owned_message.length
                == strlen(expected_message));
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

size_t ruda_test_mir_instruction_kind_count(
    const RudaMirFunction *function,
    RudaMirInstructionKind kind
) {
    size_t block_index = 0;
    size_t count = 0;
    if (function == NULL) {
        return 0;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks,
            block_index
        );
        size_t instruction_index = 0;
        if (block == NULL) {
            continue;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions,
                instruction_index
            );
            if (instruction != NULL && instruction->kind == kind) {
                count += 1;
            }
        }
    }
    return count;
}

const RudaMirInstruction *ruda_test_mir_instruction_kind_at(
    const RudaMirFunction *function,
    RudaMirInstructionKind kind,
    size_t occurrence
) {
    size_t block_index = 0;
    size_t found = 0;
    if (function == NULL) {
        return NULL;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks,
            block_index
        );
        size_t instruction_index = 0;
        if (block == NULL) {
            continue;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions,
                instruction_index
            );
            if (instruction != NULL && instruction->kind == kind) {
                if (found == occurrence) {
                    return instruction;
                }
                found += 1;
            }
        }
    }
    return NULL;
}
