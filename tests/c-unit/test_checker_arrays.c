#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ruda/ast/ast.h"
#include "ruda/base/allocator.h"
#include "ruda/check/checker.h"
#include "ruda/generated/diagnostic_codes.h"
#include "ruda/lex/lexer.h"
#include "ruda/parse/parser.h"
#include "ruda/source/source_manager.h"

typedef struct RudaCheckerArraysFixture {
    RudaSourceManager sources;
    RudaLexResult lexed;
    RudaParseResult parsed;
    RudaAstTree ast;
    bool sources_initialized;
    bool lexed_initialized;
    bool parsed_initialized;
    bool ast_initialized;
} RudaCheckerArraysFixture;

static bool ruda_checker_arrays_fixture_init(
    RudaCheckerArraysFixture *fixture,
    const char *source
) {
    const RudaSourceSnapshot *snapshot = NULL;
    memset(fixture, 0, sizeof(*fixture));
    if (!ruda_source_manager_init(&fixture->sources, ruda_system_allocator())) {
        return false;
    }
    fixture->sources_initialized = true;
    if (ruda_source_manager_add(
            &fixture->sources, "checker-arrays.ruda", 19, source, strlen(source),
            &snapshot
        ) != RUDA_SOURCE_OK
        || ruda_lex_snapshot(
            snapshot, ruda_system_allocator(), ruda_lex_default_options(),
            &fixture->lexed
        ) != RUDA_LEX_OK) {
        return false;
    }
    fixture->lexed_initialized = true;
    if (fixture->lexed.diagnostics.length != 0
        || ruda_parse_file(
            &fixture->lexed, ruda_system_allocator(), ruda_parse_default_options(),
            &fixture->parsed
        ) != RUDA_PARSE_OK) {
        return false;
    }
    fixture->parsed_initialized = true;
    if (fixture->parsed.diagnostics.length != 0
        || ruda_ast_project(
            &fixture->parsed.tree, ruda_system_allocator(), ruda_ast_default_options(),
            &fixture->ast
        ) != RUDA_AST_OK) {
        return false;
    }
    fixture->ast_initialized = true;
    return true;
}

static void ruda_checker_arrays_fixture_dispose(RudaCheckerArraysFixture *fixture) {
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

static const RudaCheckedFunction *ruda_checker_arrays_function(
    const RudaCheckResult *checked,
    const char *name
) {
    size_t index = 0;
    size_t length = strlen(name);
    for (index = 0; checked != NULL && index < checked->functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(checked, index);
        if (function != NULL && function->name.length == length
            && memcmp(function->name.data, name, length) == 0) {
            return function;
        }
    }
    return NULL;
}

static size_t ruda_checker_arrays_function_index(
    const RudaCheckResult *checked,
    const char *name
) {
    size_t index = 0;
    size_t length = strlen(name);
    for (index = 0; checked != NULL && index < checked->functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(checked, index);
        if (function != NULL && function->name.length == length
            && memcmp(function->name.data, name, length) == 0) {
            return index;
        }
    }
    return SIZE_MAX;
}

static bool ruda_checker_arrays_is_array_of(
    const RudaCheckResult *checked,
    RudaTypeId type,
    RudaTypeId expected_element
) {
    RudaTypeId element = RUDA_TYPE_INVALID;
    return ruda_type_array_element(&checked->types, type, &element)
        && element == expected_element;
}

static bool ruda_checker_arrays_has_checked_store(
    const RudaMirFunction *function,
    bool *compound
) {
    size_t block_index = 0;
    bool found = false;
    if (compound != NULL) {
        *compound = false;
    }
    for (block_index = 0; function != NULL && block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(&function->blocks, block_index);
        size_t instruction_index = 0;
        if (block == NULL) {
            return false;
        }
        for (instruction_index = 0; instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *store = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            size_t previous_index = 0;
            bool checked = false;
            if (store == NULL || store->kind != RUDA_MIR_ARRAY_SET) {
                continue;
            }
            for (previous_index = 0; previous_index < instruction_index;
                previous_index += 1) {
                const RudaMirInstruction *previous = ruda_vector_at_const(
                    &block->instructions, previous_index
                );
                if (previous != NULL && previous->kind == RUDA_MIR_ARRAY_REQUIRE_INDEX
                    && previous->target == store->first
                    && previous->second == store->second) {
                    checked = true;
                }
                if (compound != NULL && previous != NULL
                    && previous->kind == RUDA_MIR_ARRAY_GET
                    && previous->first == store->first
                    && previous->second == store->second) {
                    *compound = true;
                }
            }
            if (!checked) {
                return false;
            }
            found = true;
        }
    }
    return found;
}

static void ruda_checker_arrays_expect_type_diagnostic(const char *source) {
    RudaCheckerArraysFixture fixture;
    RudaCheckResult checked;
    size_t index = 0;
    bool found = false;
    memset(&checked, 0, sizeof(checked));
    RUDA_EXPECT(ruda_checker_arrays_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast, "test/checker-arrays", ruda_system_allocator(),
        ruda_check_default_options(), &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        for (index = 0; index < checked.diagnostics.length; index += 1) {
            const RudaCheckDiagnostic *diagnostic = ruda_check_diagnostic(&checked, index);
            if (diagnostic != NULL
                && strcmp(diagnostic->code, RUDA_DIAGNOSTIC_TYPE_MISMATCH) == 0) {
                found = true;
            }
        }
        RUDA_EXPECT(found);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_arrays_fixture_dispose(&fixture);
}

static void ruda_checker_arrays_expect_status(
    const char *source,
    RudaCheckStatus expected
) {
    RudaCheckerArraysFixture fixture;
    RudaCheckResult checked;
    memset(&checked, 0, sizeof(checked));
    RUDA_EXPECT(ruda_checker_arrays_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast, "test/checker-array-status", ruda_system_allocator(),
        ruda_check_default_options(), &checked
    ) == expected);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_arrays_fixture_dispose(&fixture);
}

static void ruda_test_checker_arrays_surface_and_matrix(void) {
    static const char source[] =
        "fn surface(values: Array<f64>, matrices: Array<Array<f64>>): int {\n"
        "  let inferred = [1.0, 2.0];\n"
        "  let contextual: Array<f64> = [];\n"
        "  let writable = [0.0, 0.0];\n"
        "  writable[0] = inferred[1];\n"
        "  writable[0] += values[0];\n"
        "  return length(contextual) + matrices.length();\n"
        "}\n"
        "fn matrix(): f64 {\n"
        "  let left = [[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]];\n"
        "  let right = [[7.0, 8.0], [9.0, 10.0], [11.0, 12.0]];\n"
        "  let result = [[0.0, 0.0], [0.0, 0.0]];\n"
        "  for let row = 0; row < 2; row += 1 {\n"
        "    for let column = 0; column < 2; column += 1 {\n"
        "      for let inner = 0; inner < 3; inner += 1 {\n"
        "        result[row][column] += left[row][inner] * right[inner][column];\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "  return result[0][0] + result[0][1] + result[1][0] + result[1][1];\n"
        "}\n";
    RudaCheckerArraysFixture fixture;
    RudaCheckResult checked;
    RudaMirValue result;
    const RudaCheckedFunction *surface = NULL;
    const RudaCheckedFunction *matrix = NULL;
    const RudaTypeId *first_parameter = NULL;
    const RudaTypeId *second_parameter = NULL;
    RudaTypeId inner = RUDA_TYPE_INVALID;
    RudaTypeId empty_element = RUDA_TYPE_INVALID;
    size_t index = 0;
    size_t empty_constructions = 0;
    size_t length_instructions = 0;
    bool inferred_f64 = false;
    bool compound = false;
    memset(&checked, 0, sizeof(checked));
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(ruda_checker_arrays_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast, "test/checker-arrays", ruda_system_allocator(),
        ruda_check_default_options(), &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        surface = ruda_checker_arrays_function(&checked, "surface");
        matrix = ruda_checker_arrays_function(&checked, "matrix");
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(surface != NULL && matrix != NULL);
        if (surface != NULL && matrix != NULL) {
            first_parameter = ruda_vector_at_const(&surface->mir.register_types, 0);
            second_parameter = ruda_vector_at_const(&surface->mir.register_types, 1);
            RUDA_EXPECT(first_parameter != NULL && second_parameter != NULL);
            if (first_parameter != NULL && second_parameter != NULL) {
                RUDA_EXPECT(ruda_checker_arrays_is_array_of(
                    &checked, *first_parameter, RUDA_TYPE_F64
                ));
                RUDA_EXPECT(ruda_type_array_element(
                    &checked.types, *second_parameter, &inner
                ) && ruda_checker_arrays_is_array_of(&checked, inner, RUDA_TYPE_F64));
            }
            for (index = 0; index < surface->mir.array_constructions.length; index += 1) {
                const RudaMirArrayConstruction *construction = ruda_vector_at_const(
                    &surface->mir.array_constructions, index
                );
                const RudaMirArray *array = construction == NULL ? NULL : ruda_vector_at_const(
                    &surface->mir.arrays, construction->array
                );
                if (construction != NULL && array != NULL && construction->argument_count == 0) {
                    empty_constructions += 1;
                    empty_element = array->element_type;
                }
                if (construction != NULL && array != NULL
                    && construction->argument_count == 2
                    && array->element_type == RUDA_TYPE_F64) {
                    inferred_f64 = true;
                }
            }
            for (index = 0; index < surface->mir.blocks.length; index += 1) {
                const RudaMirBlock *block = ruda_vector_at_const(&surface->mir.blocks, index);
                size_t instruction_index = 0;
                for (instruction_index = 0; block != NULL
                    && instruction_index < block->instructions.length; instruction_index += 1) {
                    const RudaMirInstruction *instruction = ruda_vector_at_const(
                        &block->instructions, instruction_index
                    );
                    if (instruction != NULL && instruction->kind == RUDA_MIR_ARRAY_LENGTH) {
                        length_instructions += 1;
                    }
                }
            }
            RUDA_EXPECT(empty_constructions == 1 && empty_element == RUDA_TYPE_F64);
            RUDA_EXPECT(inferred_f64);
            RUDA_EXPECT(length_instructions == 2);
            RUDA_EXPECT(ruda_checker_arrays_has_checked_store(&surface->mir, &compound));
            RUDA_EXPECT(compound);
            RUDA_EXPECT(ruda_checker_arrays_has_checked_store(&matrix->mir, NULL));
            RUDA_EXPECT(ruda_mir_verify(&surface->mir) == RUDA_MIR_VERIFY_OK);
            RUDA_EXPECT(ruda_mir_verify(&matrix->mir) == RUDA_MIR_VERIFY_OK);
            index = ruda_checker_arrays_function_index(&checked, "matrix");
            RUDA_EXPECT(index != SIZE_MAX);
            if (index != SIZE_MAX) {
                RUDA_EXPECT(ruda_check_interpret_function(
                    &checked, index, NULL, 0, ruda_system_allocator(),
                    ruda_check_interpret_default_options(), &result
                ) == RUDA_MIR_INTERPRET_OK);
                RUDA_EXPECT(result.type == RUDA_TYPE_F64
                    && result.as.f64_bits == UINT64_C(0x4079f00000000000));
            }
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_arrays_fixture_dispose(&fixture);
}

static void ruda_test_checker_arrays_d045_and_diagnostics(void) {
    static const char bounds_source[] =
        "fn bounds(): int { let values = [1]; values[3] = panic(\"rhs\"); }\n";
    static const unsigned char bounds_message[] =
        "array index 3 is outside length 1";
    RudaCheckerArraysFixture fixture;
    RudaCheckResult checked;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    RudaMirPanic panic;
    RudaMirValue result;
    const RudaCheckedFunction *bounds = NULL;
    const char *access = strstr(bounds_source, "values[3]");
    size_t index = 0;
    memset(&checked, 0, sizeof(checked));
    memset(&panic, 0, sizeof(panic));
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(ruda_checker_arrays_fixture_init(&fixture, bounds_source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast, "test/checker-arrays", ruda_system_allocator(),
        ruda_check_default_options(), &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        bounds = ruda_checker_arrays_function(&checked, "bounds");
        index = ruda_checker_arrays_function_index(&checked, "bounds");
        RUDA_EXPECT(bounds != NULL && index != SIZE_MAX && access != NULL);
        options.panic = &panic;
        if (index != SIZE_MAX) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked, index, NULL, 0, ruda_system_allocator(), options, &result
            ) == RUDA_MIR_INTERPRET_PANIC);
            RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4002") == 0);
            RUDA_EXPECT(panic.message.length == sizeof(bounds_message) - 1);
            RUDA_EXPECT(panic.message.data != NULL && memcmp(
                panic.message.data,
                bounds_message,
                sizeof(bounds_message) - 1
            ) == 0);
            if (bounds != NULL && access != NULL) {
                RUDA_EXPECT(ruda_symbol_id_equal(
                    panic.symbol,
                    bounds->mir.symbol
                ));
                RUDA_EXPECT(panic.span.start == (size_t)(access - bounds_source));
                RUDA_EXPECT(panic.span.end
                    == (size_t)(access - bounds_source) + strlen("values[3]"));
            }
        }
        ruda_mir_panic_dispose(&panic);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_arrays_fixture_dispose(&fixture);

    ruda_checker_arrays_expect_type_diagnostic(
        "fn bad(): void { let values = [1, true]; }\n"
    );
    ruda_checker_arrays_expect_type_diagnostic(
        "fn bad(): void { let values = [1]; values[true] = 2; }\n"
    );
    ruda_checker_arrays_expect_type_diagnostic(
        "fn bad(): void { let values = [1]; values[0] = true; }\n"
    );
    ruda_checker_arrays_expect_type_diagnostic(
        "fn bad(values: Array<f64, int>): void {}\n"
    );
    ruda_checker_arrays_expect_type_diagnostic(
        "fn bad(): void { let values = []; }\n"
    );
    ruda_checker_arrays_expect_type_diagnostic(
        "fn bad(values: Array): void {}\n"
    );
    ruda_checker_arrays_expect_status(
        "fn deferred(): void { let values = arrayOf(1); }\n",
        RUDA_CHECK_INCOMPLETE_LANGUAGE
    );
    ruda_checker_arrays_expect_status(
        "fn deferred(): void { let values = arrayOf<int>(); }\n",
        RUDA_CHECK_INCOMPLETE_LANGUAGE
    );
    ruda_checker_arrays_expect_status(
        "fn deferred(): void { let values = [1]; push(values, 2); }\n",
        RUDA_CHECK_INCOMPLETE_LANGUAGE
    );
    ruda_checker_arrays_expect_status(
        "fn deferred(): void { let values = [1]; values.push(2); }\n",
        RUDA_CHECK_INCOMPLETE_LANGUAGE
    );
}

static void ruda_test_checker_arrays_length_shadow(void) {
    static const char source[] =
        "fn length(value: int): int { return value + 1; }\n"
        "fn use(): int { return length(4); }\n";
    RudaCheckerArraysFixture fixture;
    RudaCheckResult checked;
    RudaMirValue result;
    size_t index = SIZE_MAX;
    memset(&checked, 0, sizeof(checked));
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(ruda_checker_arrays_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast, "test/checker-array-shadow", ruda_system_allocator(),
        ruda_check_default_options(), &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        index = ruda_checker_arrays_function_index(&checked, "use");
        RUDA_EXPECT(index != SIZE_MAX);
        if (index != SIZE_MAX) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked, index, NULL, 0, ruda_system_allocator(),
                ruda_check_interpret_default_options(), &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 5);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_arrays_fixture_dispose(&fixture);
}

static void ruda_test_checker_arrays_allocation_failures(void) {
    static const char source[] =
        "fn arrays(): f64 {\n"
        "  let values: Array<Array<f64>> = [[1.0, 2.0]];\n"
        "  let empty: Array<f64> = [];\n"
        "  let count = length(empty);\n"
        "  values[0][0] += 2.0;\n"
        "  return values[0][0];\n"
        "}\n";
    RudaCheckerArraysFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    RUDA_EXPECT(ruda_checker_arrays_fixture_init(&fixture, source));
    for (failure_index = 0; failure_index < 1024; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaCheckResult checked;
        RudaCheckStatus status;
        memset(&checked, 0, sizeof(checked));
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            failure_index
        );
        allocator = ruda_fail_allocator(&state);
        status = ruda_check_ast(
            &fixture.ast,
            "test/checker-array-allocation",
            allocator,
            ruda_check_default_options(),
            &checked
        );
        if (status == RUDA_CHECK_OK) {
            reached_success = true;
            RUDA_EXPECT(checked.initialized);
            RUDA_EXPECT(checked.diagnostics.length == 0);
            ruda_check_result_dispose(&checked);
        } else {
            RUDA_EXPECT(status == RUDA_CHECK_ALLOCATION_FAILED);
            RUDA_EXPECT(!checked.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
    ruda_checker_arrays_fixture_dispose(&fixture);
}

static void ruda_test_checker_arrays_limits(void) {
    static const char source[] =
        "fn arrays(): Array<Array<f64>> { return [[1.0, 2.0]]; }\n";
    RudaCheckerArraysFixture fixture;
    RudaCheckOptions options;
    RudaCheckResult checked;
    RUDA_EXPECT(ruda_checker_arrays_fixture_init(&fixture, source));

    memset(&checked, 0, sizeof(checked));
    options = ruda_check_default_options();
    options.mir_options.max_array_layouts = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast, "test/checker-array-layout-limit", ruda_system_allocator(),
        options, &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);

    memset(&checked, 0, sizeof(checked));
    options = ruda_check_default_options();
    options.mir_options.max_array_arguments = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast, "test/checker-array-argument-limit", ruda_system_allocator(),
        options, &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);

    memset(&checked, 0, sizeof(checked));
    options = ruda_check_default_options();
    options.type_options.max_types = RUDA_TYPE_FIRST_DYNAMIC + 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast, "test/checker-array-type-limit", ruda_system_allocator(),
        options, &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);

    ruda_checker_arrays_fixture_dispose(&fixture);
}

void ruda_test_checker_arrays(void) {
    ruda_test_checker_arrays_surface_and_matrix();
    ruda_test_checker_arrays_d045_and_diagnostics();
    ruda_test_checker_arrays_length_shadow();
    ruda_test_checker_arrays_allocation_failures();
    ruda_test_checker_arrays_limits();
}
