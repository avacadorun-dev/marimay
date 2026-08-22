#include "test_checker_support.h"


bool mmay_checker_fixture_init_path(
    MarimayCheckerFixture *fixture,
    const char *path,
    const char *source
) {
    const MarimaySourceSnapshot *snapshot = NULL;
    memset(fixture, 0, sizeof(*fixture));
    if (!mmay_source_manager_init(
            &fixture->sources,
            mmay_system_allocator()
        )) {
        return false;
    }
    fixture->sources_initialized = true;
    if (mmay_source_manager_add(
            &fixture->sources,
            path,
            strlen(path),
            source,
            strlen(source),
            &snapshot
        ) != MMAY_SOURCE_OK) {
        return false;
    }
    if (mmay_lex_snapshot(
            snapshot,
            mmay_system_allocator(),
            mmay_lex_default_options(),
            &fixture->lexed
        ) != MMAY_LEX_OK) {
        return false;
    }
    fixture->lexed_initialized = true;
    if (fixture->lexed.diagnostics.length != 0
        || mmay_parse_file(
            &fixture->lexed,
            mmay_system_allocator(),
            mmay_parse_default_options(),
            &fixture->parsed
        ) != MMAY_PARSE_OK) {
        return false;
    }
    fixture->parsed_initialized = true;
    if (fixture->parsed.diagnostics.length != 0
        || mmay_ast_project(
            &fixture->parsed.tree,
            mmay_system_allocator(),
            mmay_ast_default_options(),
            &fixture->ast
        ) != MMAY_AST_OK) {
        return false;
    }
    fixture->ast_initialized = true;
    return true;
}

bool mmay_checker_fixture_init(
    MarimayCheckerFixture *fixture,
    const char *source
) {
    return mmay_checker_fixture_init_path(fixture, "checker.my", source);
}

void mmay_checker_fixture_dispose(MarimayCheckerFixture *fixture) {
    if (fixture->ast_initialized) {
        mmay_ast_tree_dispose(&fixture->ast);
    }
    if (fixture->parsed_initialized) {
        mmay_parse_result_dispose(&fixture->parsed);
    }
    if (fixture->lexed_initialized) {
        mmay_lex_result_dispose(&fixture->lexed);
    }
    if (fixture->sources_initialized) {
        mmay_source_manager_dispose(&fixture->sources);
    }
    memset(fixture, 0, sizeof(*fixture));
}

void mmay_checker_expect_one_diagnostic(
    const char *source,
    const char *code,
    const char *marked_text,
    size_t occurrence
) {
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayCheckStatus status;
    const MarimayCheckDiagnostic *diagnostic = NULL;
    const char *marked = source;
    size_t index = 0;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    for (index = 0; index <= occurrence && marked != NULL; index += 1) {
        marked = strstr(marked, marked_text);
        if (index != occurrence && marked != NULL) {
            marked += strlen(marked_text);
        }
    }
    MMAY_EXPECT(marked != NULL);
    status = mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    );
    if (status != MMAY_CHECK_OK) {
        fprintf(
            stderr,
            "checker diagnostic fixture for %s returned status %d\n",
            code,
            (int)status
        );
    }
    MMAY_EXPECT(status == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 1);
        MMAY_EXPECT(checked.functions.length == 0);
        MMAY_EXPECT(checked.constants.length == 0);
        MMAY_EXPECT(checked.type_aliases.length == 0);
        MMAY_EXPECT(checked.records.length == 0);
        MMAY_EXPECT(checked.enums.length == 0);
        MMAY_EXPECT(checked.function_index.length == 0);
        diagnostic = mmay_check_diagnostic(&checked, 0);
        MMAY_EXPECT(diagnostic != NULL);
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
            MMAY_EXPECT(diagnostic->snapshot == fixture.ast.snapshot);
            MMAY_EXPECT(strcmp(diagnostic->code, code) == 0);
            MMAY_EXPECT(diagnostic->start == (size_t)(marked - source));
            MMAY_EXPECT(diagnostic->end
                == (size_t)(marked - source) + strlen(marked_text));
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_checker_expect_one_diagnostic_path(
    const char *path,
    const char *source,
    const char *code,
    const char *marked_text
) {
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckDiagnostic *diagnostic = NULL;
    const char *marked = strstr(source, marked_text);
    MMAY_EXPECT(mmay_checker_fixture_init_path(&fixture, path, source));
    MMAY_EXPECT(marked != NULL);
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker-path",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 1);
        MMAY_EXPECT(checked.functions.length == 0);
        diagnostic = mmay_check_diagnostic(&checked, 0);
        MMAY_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL && marked != NULL) {
            MMAY_EXPECT(strcmp(diagnostic->code, code) == 0);
            MMAY_EXPECT(diagnostic->start == (size_t)(marked - source));
            MMAY_EXPECT(diagnostic->end
                == (size_t)(marked - source) + strlen(marked_text));
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_checker_expect_diagnostic_message(
    const char *source,
    const char *expected_message
) {
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckDiagnostic *diagnostic = NULL;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker-message",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 1);
        diagnostic = mmay_check_diagnostic(&checked, 0);
        MMAY_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            MMAY_EXPECT(strcmp(
                diagnostic->code,
                MMAY_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH
            ) == 0);
            MMAY_EXPECT(diagnostic->message != NULL);
            if (diagnostic->message != NULL) {
                MMAY_EXPECT(strcmp(
                    diagnostic->message,
                    expected_message
                ) == 0);
            }
            MMAY_EXPECT(diagnostic->owned_message.data != NULL);
            MMAY_EXPECT(diagnostic->owned_message.length
                == strlen(expected_message));
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

size_t mmay_test_mir_instruction_kind_count(
    const MarimayMirFunction *function,
    MarimayMirInstructionKind kind
) {
    size_t block_index = 0;
    size_t count = 0;
    if (function == NULL) {
        return 0;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
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
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
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

const MarimayMirInstruction *mmay_test_mir_instruction_kind_at(
    const MarimayMirFunction *function,
    MarimayMirInstructionKind kind,
    size_t occurrence
) {
    size_t block_index = 0;
    size_t found = 0;
    if (function == NULL) {
        return NULL;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
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
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
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
