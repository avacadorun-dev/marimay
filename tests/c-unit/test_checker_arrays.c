#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ruda/ast/ast.h"
#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/bytecode/mir_lower.h"
#include "ruda/bytecode/vm.h"
#include "ruda/check/checker.h"
#include "ruda/generated/diagnostic_codes.h"
#include "ruda/lex/lexer.h"
#include "ruda/mir/range.h"
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
            if (store == NULL || (store->kind != RUDA_MIR_ARRAY_SET
                    && store->kind != RUDA_MIR_ARRAY_SET_IN_BOUNDS)) {
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
                    && (previous->kind == RUDA_MIR_ARRAY_GET
                        || previous->kind == RUDA_MIR_ARRAY_GET_IN_BOUNDS)
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

static size_t ruda_checker_arrays_instruction_count(
    const RudaMirFunction *function,
    RudaMirInstructionKind kind
) {
    size_t block_index = 0;
    size_t count = 0;
    for (block_index = 0; function != NULL
        && block_index < function->blocks.length; block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0; block != NULL
            && instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction != NULL && instruction->kind == kind) {
                count += 1;
            }
        }
    }
    return count;
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
    {
        RudaCheckStatus actual = ruda_check_ast(
        &fixture.ast, "test/checker-array-status", ruda_system_allocator(),
        ruda_check_default_options(), &checked
        );
        RUDA_EXPECT(actual == expected);
    }
    if (expected == RUDA_CHECK_OK) {
        RUDA_EXPECT(checked.initialized);
        if (checked.initialized) {
            RUDA_EXPECT(checked.diagnostics.length == 0);
            ruda_check_result_dispose(&checked);
        }
    } else {
        RUDA_EXPECT(!checked.initialized);
    }
    ruda_checker_arrays_fixture_dispose(&fixture);
}

static void ruda_test_checker_arrays_surface_and_matrix(void) {
    static const char source[] =
        "fn surface(values: []f64, matrices: [][]f64): int {\n"
        "  let inferred = [1.0, 2.0];\n"
        "  let contextual: []f64 = [];\n"
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
        "}\n"
        "fn generic(): int {\n"
        "  let values = arrayOf(1, 2);\n"
        "  let pushed: Result<void, ArrayGrowthError> = values.push(3);\n"
        "  values[1] += 4;\n"
        "  let empty = arrayOf<int>();\n"
        "  empty.push(9);\n"
        "  let nested: [][]int = [[7, 8]];\n"
        "  nested[0].push(9);\n"
        "  return values[2] + values.length() + empty[0] + nested[0][2];\n"
        "}\n"
        "fn sorting(): int {\n"
        "  let values: []int = [3, 1, 2];\n"
        "  for let pass = 0; pass < 3; pass += 1 {\n"
        "    for let index = 0; index < 2; index += 1 {\n"
        "      if values[index] > values[index + 1] {\n"
        "        let temporary = values[index];\n"
        "        values[index] = values[index + 1];\n"
        "        values[index + 1] = temporary;\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "  let found = 0;\n"
        "  while found < values.length() && values[found] != 2 { found += 1; }\n"
        "  return values[found];\n"
        "}\n"
        "fn provenLoop(): int {\n"
        "  let values: []int = [1, 2, 3];\n"
        "  let total = 0;\n"
        "  for let outer = 0; outer < values.length(); outer += 1 {\n"
        "    for let index = 0; index < values.length(); index += 2 {\n"
        "      total += values[index];\n"
        "    }\n"
        "  }\n"
        "  return total;\n"
        "}\n";
    RudaCheckerArraysFixture fixture;
    RudaCheckResult checked;
    RudaMirValue result;
    RudaBytecodeFunction lowered;
    RudaBytecodeFunction decoded;
    RudaBytecodeExecution execution;
    RudaOwnedBytes encoded;
    RudaBytecodeValue vm_result;
    RudaBytecodeRunOptions run_options;
    const RudaCheckedFunction *surface = NULL;
    const RudaCheckedFunction *matrix = NULL;
    const RudaCheckedFunction *generic = NULL;
    const RudaCheckedFunction *sorting = NULL;
    const RudaCheckedFunction *proven_loop = NULL;
    RudaCheckedFunction *mutable_proven_loop = NULL;
    const RudaTypeId *first_parameter = NULL;
    const RudaTypeId *second_parameter = NULL;
    RudaTypeId inner = RUDA_TYPE_INVALID;
    RudaTypeId empty_element = RUDA_TYPE_INVALID;
    size_t index = 0;
    size_t empty_constructions = 0;
    size_t length_instructions = 0;
    size_t checked_gets = 0;
    size_t assumed_ranges = 0;
    bool inferred_f64 = false;
    bool compound = false;
    memset(&checked, 0, sizeof(checked));
    memset(&result, 0, sizeof(result));
    memset(&lowered, 0, sizeof(lowered));
    memset(&decoded, 0, sizeof(decoded));
    memset(&execution, 0, sizeof(execution));
    memset(&encoded, 0, sizeof(encoded));
    memset(&vm_result, 0, sizeof(vm_result));
    RUDA_EXPECT(ruda_checker_arrays_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast, "test/checker-arrays", ruda_system_allocator(),
        ruda_check_default_options(), &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        surface = ruda_checker_arrays_function(&checked, "surface");
        matrix = ruda_checker_arrays_function(&checked, "matrix");
        generic = ruda_checker_arrays_function(&checked, "generic");
        sorting = ruda_checker_arrays_function(&checked, "sorting");
        proven_loop = ruda_checker_arrays_function(&checked, "provenLoop");
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(surface != NULL && matrix != NULL && generic != NULL
            && sorting != NULL && proven_loop != NULL);
        if (surface != NULL && matrix != NULL && generic != NULL
            && sorting != NULL && proven_loop != NULL) {
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
                    if (instruction != NULL
                        && instruction->kind == RUDA_MIR_ARRAY_GET) {
                        size_t previous_index = 0;
                        for (previous_index = 0;
                            previous_index < instruction_index;
                            previous_index += 1) {
                            const RudaMirInstruction *previous =
                                ruda_vector_at_const(
                                    &block->instructions, previous_index
                                );
                            if (previous != NULL
                                && previous->kind
                                    == RUDA_MIR_ARRAY_REQUIRE_INDEX
                                && previous->target == instruction->first
                                && previous->second == instruction->second) {
                                checked_gets += 1;
                                break;
                            }
                        }
                    }
                }
            }
            RUDA_EXPECT(empty_constructions == 1 && empty_element == RUDA_TYPE_F64);
            RUDA_EXPECT(inferred_f64);
            RUDA_EXPECT(length_instructions == 2);
            RUDA_EXPECT(checked_gets > 0);
            RUDA_EXPECT(ruda_checker_arrays_has_checked_store(&surface->mir, &compound));
            RUDA_EXPECT(compound);
            RUDA_EXPECT(ruda_checker_arrays_has_checked_store(&matrix->mir, NULL));
            RUDA_EXPECT(ruda_mir_verify(&surface->mir) == RUDA_MIR_VERIFY_OK);
            RUDA_EXPECT(ruda_mir_verify(&matrix->mir) == RUDA_MIR_VERIFY_OK);
            RUDA_EXPECT(ruda_mir_verify(&generic->mir) == RUDA_MIR_VERIFY_OK);
            RUDA_EXPECT(ruda_mir_verify(&sorting->mir) == RUDA_MIR_VERIFY_OK);
            index = ruda_checker_arrays_function_index(&checked, "provenLoop");
            mutable_proven_loop = index == SIZE_MAX ? NULL : ruda_vector_at(
                &checked.functions, index
            );
            RUDA_EXPECT(mutable_proven_loop != NULL);
            if (mutable_proven_loop != NULL) {
                size_t block_index = 0;
                RUDA_EXPECT(ruda_mir_optimize_ranges(&mutable_proven_loop->mir)
                    == RUDA_MIR_RANGE_OPTIMIZE_OK);
                for (block_index = 0;
                    block_index < mutable_proven_loop->mir.blocks.length;
                    block_index += 1) {
                    const RudaMirBlock *block = ruda_vector_at_const(
                        &mutable_proven_loop->mir.blocks, block_index
                    );
                    size_t instruction_index = 0;
                    for (instruction_index = 0; block != NULL
                        && instruction_index < block->instructions.length;
                        instruction_index += 1) {
                        const RudaMirInstruction *instruction =
                            ruda_vector_at_const(
                                &block->instructions, instruction_index
                            );
                        if (instruction != NULL && instruction->kind
                            == RUDA_MIR_ARRAY_ASSUME_INDEX) {
                            assumed_ranges += 1;
                        }
                    }
                }
                RUDA_EXPECT(assumed_ranges > 0);
                RUDA_EXPECT(ruda_mir_verify(&mutable_proven_loop->mir)
                    == RUDA_MIR_VERIFY_OK);
            }
            RUDA_EXPECT(ruda_mir_lower_bytecode_function(
                &generic->mir, ruda_system_allocator(),
                ruda_mir_bytecode_lower_default_options(), &lowered
            ) == RUDA_MIR_BYTECODE_LOWER_OK);
            RUDA_EXPECT(lowered.array_types.length == 2);
            RUDA_EXPECT(ruda_bytecode_verify(&lowered) == RUDA_BYTECODE_VERIFY_OK);
            RUDA_EXPECT(ruda_bytecode_encode_function(
                &lowered, ruda_system_allocator(), &encoded
            ) == RUDA_BYTECODE_CODEC_OK);
            RUDA_EXPECT(ruda_bytecode_decode_function(
                encoded.data, encoded.length, ruda_system_allocator(),
                ruda_bytecode_function_default_options(), &decoded
            ) == RUDA_BYTECODE_CODEC_OK);
            RUDA_EXPECT(ruda_bytecode_verify(&decoded) == RUDA_BYTECODE_VERIFY_OK);
            {
                RudaBytecodeArrayConstruction *construction = ruda_vector_at(
                    &decoded.array_constructions, 0
                );
                RudaBytecodeArrayType *array_type = ruda_vector_at(
                    &decoded.array_types, 0
                );
                RudaBytecodeInstruction *push = NULL;
                size_t block_index = 0;
                size_t instruction_index = 0;
                for (block_index = 0; block_index < decoded.blocks.length;
                    block_index += 1) {
                    RudaBytecodeBlock *block = ruda_vector_at(
                        &decoded.blocks, block_index
                    );
                    for (instruction_index = 0; block != NULL
                        && instruction_index < block->instructions.length;
                        instruction_index += 1) {
                        RudaBytecodeInstruction *candidate = ruda_vector_at(
                            &block->instructions, instruction_index
                        );
                        if (candidate != NULL
                            && candidate->opcode == RUDA_OPCODE_ARRAY_PUSH) {
                            push = candidate;
                        }
                    }
                }
                if (construction != NULL) {
                    uint32_t saved_layout = construction->array_layout;
                    construction->array_layout = UINT32_MAX;
                    RUDA_EXPECT(ruda_bytecode_verify(&decoded)
                        != RUDA_BYTECODE_VERIFY_OK);
                    construction->array_layout = saved_layout;
                }
                if (array_type != NULL) {
                    uint32_t saved_layout = array_type->element_layout;
                    array_type->element_layout = 0;
                    RUDA_EXPECT(ruda_bytecode_verify(&decoded)
                        != RUDA_BYTECODE_VERIFY_OK);
                    array_type->element_layout = saved_layout;
                }
                if (push != NULL) {
                    uint64_t saved_value = push->operands[1].unsigned_value;
                    push->operands[1].unsigned_value =
                        push->operands[0].unsigned_value;
                    RUDA_EXPECT(ruda_bytecode_verify(&decoded)
                        != RUDA_BYTECODE_VERIFY_OK);
                    push->operands[1].unsigned_value = saved_value;
                }
                RUDA_EXPECT(ruda_bytecode_verify(&decoded)
                    == RUDA_BYTECODE_VERIFY_OK);
            }
            RUDA_EXPECT(ruda_bytecode_execution_init(
                &execution, ruda_system_allocator(),
                ruda_bytecode_execution_default_options()
            ));
            run_options = ruda_bytecode_run_default_options();
            run_options.execution = &execution;
            RUDA_EXPECT(ruda_bytecode_run(
                &decoded, NULL, 0, ruda_system_allocator(),
                run_options, &vm_result
            ) == RUDA_BYTECODE_RUN_OK);
            RUDA_EXPECT(vm_result.type == RUDA_BYTECODE_TYPE_I64
                && vm_result.as.i64 == 24);
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
            index = ruda_checker_arrays_function_index(&checked, "generic");
            RUDA_EXPECT(index != SIZE_MAX);
            if (index != SIZE_MAX) {
                memset(&result, 0, sizeof(result));
                RUDA_EXPECT(ruda_check_interpret_function(
                    &checked, index, NULL, 0, ruda_system_allocator(),
                    ruda_check_interpret_default_options(), &result
                ) == RUDA_MIR_INTERPRET_OK);
                RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 24);
            }
            index = ruda_checker_arrays_function_index(&checked, "sorting");
            RUDA_EXPECT(index != SIZE_MAX);
            if (index != SIZE_MAX) {
                memset(&result, 0, sizeof(result));
                RUDA_EXPECT(ruda_check_interpret_function(
                    &checked, index, NULL, 0, ruda_system_allocator(),
                    ruda_check_interpret_default_options(), &result
                ) == RUDA_MIR_INTERPRET_OK);
                RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 2);
            }
            index = ruda_checker_arrays_function_index(&checked, "provenLoop");
            RUDA_EXPECT(index != SIZE_MAX);
            if (index != SIZE_MAX) {
                memset(&result, 0, sizeof(result));
                RUDA_EXPECT(ruda_check_interpret_function(
                    &checked, index, NULL, 0, ruda_system_allocator(),
                    ruda_check_interpret_default_options(), &result
                ) == RUDA_MIR_INTERPRET_OK);
                RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 12);
            }
        }
        if (execution.initialized) {
            ruda_bytecode_execution_dispose(&execution);
        }
        if (decoded.initialized) {
            ruda_bytecode_function_dispose(&decoded);
        }
        if (lowered.initialized) {
            ruda_bytecode_function_dispose(&lowered);
        }
        if (encoded.data != NULL) {
            ruda_owned_bytes_dispose(&encoded);
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
        "fn bad(): void { let values = []; }\n"
    );
    ruda_checker_arrays_expect_status(
        "fn deferred(): void { let values = arrayOf(1); }\n",
        RUDA_CHECK_OK
    );
    ruda_checker_arrays_expect_status(
        "fn deferred(): void { let values = arrayOf<int>(); }\n",
        RUDA_CHECK_OK
    );
    ruda_checker_arrays_expect_status(
        "fn deferred(): void { let values = [1]; push(values, 2); }\n",
        RUDA_CHECK_OK
    );
    ruda_checker_arrays_expect_status(
        "fn deferred(): void { let values = [1]; values.push(2); }\n",
        RUDA_CHECK_OK
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

static void ruda_test_checker_arrays_for_of(void) {
    static const char source[] =
        "fn iterate(): int {\n"
        "  let total = 0;\n"
        "  for const value of [1, 2, 3, 4] {\n"
        "    if value == 2 { continue; }\n"
        "    total += value;\n"
        "    if value == 3 { break; }\n"
        "  }\n"
        "  return total;\n"
        "}\n";
    RudaCheckerArraysFixture fixture;
    RudaCheckResult checked;
    RudaBytecodeFunction lowered;
    RudaBytecodeExecution execution;
    RudaBytecodeRunOptions run_options;
    RudaBytecodeValue vm_result;
    RudaMirValue mir_result;
    const RudaCheckedFunction *function = NULL;
    size_t index = SIZE_MAX;
    memset(&checked, 0, sizeof(checked));
    memset(&lowered, 0, sizeof(lowered));
    memset(&execution, 0, sizeof(execution));
    memset(&vm_result, 0, sizeof(vm_result));
    memset(&mir_result, 0, sizeof(mir_result));
    RUDA_EXPECT(ruda_checker_arrays_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast, "test/checker-array-for-of", ruda_system_allocator(),
        ruda_check_default_options(), &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        function = ruda_checker_arrays_function(&checked, "iterate");
        index = ruda_checker_arrays_function_index(&checked, "iterate");
        RUDA_EXPECT(function != NULL && index != SIZE_MAX);
        if (function != NULL && index != SIZE_MAX) {
            RUDA_EXPECT(ruda_mir_verify(&function->mir) == RUDA_MIR_VERIFY_OK);
            RUDA_EXPECT(ruda_checker_arrays_instruction_count(
                &function->mir, RUDA_MIR_ARRAY_LENGTH
            ) == 1);
            RUDA_EXPECT(ruda_checker_arrays_instruction_count(
                &function->mir, RUDA_MIR_ARRAY_GET
            ) == 1);
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked, index, NULL, 0, ruda_system_allocator(),
                ruda_check_interpret_default_options(), &mir_result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(mir_result.type == RUDA_TYPE_I64
                && mir_result.as.i64 == 4);
            RUDA_EXPECT(ruda_mir_lower_bytecode_function(
                &function->mir, ruda_system_allocator(),
                ruda_mir_bytecode_lower_default_options(), &lowered
            ) == RUDA_MIR_BYTECODE_LOWER_OK);
            if (lowered.initialized) {
                RUDA_EXPECT(ruda_bytecode_verify(&lowered)
                    == RUDA_BYTECODE_VERIFY_OK);
                RUDA_EXPECT(ruda_bytecode_execution_init(
                    &execution, ruda_system_allocator(),
                    ruda_bytecode_execution_default_options()
                ));
                if (execution.initialized) {
                    run_options = ruda_bytecode_run_default_options();
                    run_options.execution = &execution;
                    RUDA_EXPECT(ruda_bytecode_run(
                        &lowered, NULL, 0, ruda_system_allocator(),
                        run_options, &vm_result
                    ) == RUDA_BYTECODE_RUN_OK);
                    RUDA_EXPECT(vm_result.type == RUDA_BYTECODE_TYPE_I64
                        && vm_result.as.i64 == 4);
                    ruda_bytecode_execution_dispose(&execution);
                }
                ruda_bytecode_function_dispose(&lowered);
            }
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_arrays_fixture_dispose(&fixture);
}

static void ruda_test_checker_arrays_allocation_failures(void) {
    static const char source[] =
        "fn arrays(): int {\n"
        "  let values = arrayOf(1, 2);\n"
        "  let empty = arrayOf<int>();\n"
        "  values.push(3);\n"
        "  values[0] += 2;\n"
        "  empty.push(4);\n"
        "  return values[0] + empty[0];\n"
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
        "fn arrays(): [][]f64 { return [[1.0, 2.0]]; }\n";
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
    ruda_test_checker_arrays_for_of();
    ruda_test_checker_arrays_allocation_failures();
    ruda_test_checker_arrays_limits();
}
