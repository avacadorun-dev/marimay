#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mmay/ast/ast.h"
#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/bytecode/mir_lower.h"
#include "mmay/bytecode/vm.h"
#include "mmay/check/checker.h"
#include "mmay/generated/diagnostic_codes.h"
#include "mmay/lex/lexer.h"
#include "mmay/mir/range.h"
#include "mmay/parse/parser.h"
#include "mmay/source/source_manager.h"

typedef struct MarimayCheckerArraysFixture {
    MarimaySourceManager sources;
    MarimayLexResult lexed;
    MarimayParseResult parsed;
    MarimayAstTree ast;
    bool sources_initialized;
    bool lexed_initialized;
    bool parsed_initialized;
    bool ast_initialized;
} MarimayCheckerArraysFixture;

static bool mmay_checker_arrays_fixture_init(
    MarimayCheckerArraysFixture *fixture,
    const char *source
) {
    const MarimaySourceSnapshot *snapshot = NULL;
    memset(fixture, 0, sizeof(*fixture));
    if (!mmay_source_manager_init(&fixture->sources, mmay_system_allocator())) {
        return false;
    }
    fixture->sources_initialized = true;
    if (mmay_source_manager_add(
            &fixture->sources, "checker-arrays.my",
            sizeof("checker-arrays.my") - 1, source, strlen(source),
            &snapshot
        ) != MMAY_SOURCE_OK
        || mmay_lex_snapshot(
            snapshot, mmay_system_allocator(), mmay_lex_default_options(),
            &fixture->lexed
        ) != MMAY_LEX_OK) {
        return false;
    }
    fixture->lexed_initialized = true;
    if (fixture->lexed.diagnostics.length != 0
        || mmay_parse_file(
            &fixture->lexed, mmay_system_allocator(), mmay_parse_default_options(),
            &fixture->parsed
        ) != MMAY_PARSE_OK) {
        return false;
    }
    fixture->parsed_initialized = true;
    if (fixture->parsed.diagnostics.length != 0
        || mmay_ast_project(
            &fixture->parsed.tree, mmay_system_allocator(), mmay_ast_default_options(),
            &fixture->ast
        ) != MMAY_AST_OK) {
        return false;
    }
    fixture->ast_initialized = true;
    return true;
}

static void mmay_checker_arrays_fixture_dispose(MarimayCheckerArraysFixture *fixture) {
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

static const MarimayCheckedFunction *mmay_checker_arrays_function(
    const MarimayCheckResult *checked,
    const char *name
) {
    size_t index = 0;
    size_t length = strlen(name);
    for (index = 0; checked != NULL && index < checked->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(checked, index);
        if (function != NULL && function->name.length == length
            && memcmp(function->name.data, name, length) == 0) {
            return function;
        }
    }
    return NULL;
}

static size_t mmay_checker_arrays_function_index(
    const MarimayCheckResult *checked,
    const char *name
) {
    size_t index = 0;
    size_t length = strlen(name);
    for (index = 0; checked != NULL && index < checked->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(checked, index);
        if (function != NULL && function->name.length == length
            && memcmp(function->name.data, name, length) == 0) {
            return index;
        }
    }
    return SIZE_MAX;
}

static bool mmay_checker_arrays_is_array_of(
    const MarimayCheckResult *checked,
    MarimayTypeId type,
    MarimayTypeId expected_element
) {
    MarimayTypeId element = MMAY_TYPE_INVALID;
    return mmay_type_array_element(&checked->types, type, &element)
        && element == expected_element;
}

static bool mmay_checker_arrays_has_checked_store(
    const MarimayMirFunction *function,
    bool *compound
) {
    size_t block_index = 0;
    bool found = false;
    if (compound != NULL) {
        *compound = false;
    }
    for (block_index = 0; function != NULL && block_index < function->blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(&function->blocks, block_index);
        size_t instruction_index = 0;
        if (block == NULL) {
            return false;
        }
        for (instruction_index = 0; instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *store = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            size_t previous_index = 0;
            bool checked = false;
            if (store == NULL || (store->kind != MMAY_MIR_ARRAY_SET
                    && store->kind != MMAY_MIR_ARRAY_SET_IN_BOUNDS)) {
                continue;
            }
            for (previous_index = 0; previous_index < instruction_index;
                previous_index += 1) {
                const MarimayMirInstruction *previous = mmay_vector_at_const(
                    &block->instructions, previous_index
                );
                if (previous != NULL && previous->kind == MMAY_MIR_ARRAY_REQUIRE_INDEX
                    && previous->target == store->first
                    && previous->second == store->second) {
                    checked = true;
                }
                if (compound != NULL && previous != NULL
                    && (previous->kind == MMAY_MIR_ARRAY_GET
                        || previous->kind == MMAY_MIR_ARRAY_GET_IN_BOUNDS)
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

static size_t mmay_checker_arrays_instruction_count(
    const MarimayMirFunction *function,
    MarimayMirInstructionKind kind
) {
    size_t block_index = 0;
    size_t count = 0;
    for (block_index = 0; function != NULL
        && block_index < function->blocks.length; block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0; block != NULL
            && instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction != NULL && instruction->kind == kind) {
                count += 1;
            }
        }
    }
    return count;
}

static void mmay_checker_arrays_expect_type_diagnostic(const char *source) {
    MarimayCheckerArraysFixture fixture;
    MarimayCheckResult checked;
    size_t index = 0;
    bool found = false;
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_checker_arrays_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/checker-arrays", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        for (index = 0; index < checked.diagnostics.length; index += 1) {
            const MarimayCheckDiagnostic *diagnostic = mmay_check_diagnostic(&checked, index);
            if (diagnostic != NULL
                && strcmp(diagnostic->code, MMAY_DIAGNOSTIC_TYPE_MISMATCH) == 0) {
                found = true;
            }
        }
        MMAY_EXPECT(found);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_arrays_fixture_dispose(&fixture);
}

static void mmay_checker_arrays_expect_status(
    const char *source,
    MarimayCheckStatus expected
) {
    MarimayCheckerArraysFixture fixture;
    MarimayCheckResult checked;
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_checker_arrays_fixture_init(&fixture, source));
    {
        MarimayCheckStatus actual = mmay_check_ast(
        &fixture.ast, "test/checker-array-status", mmay_system_allocator(),
        mmay_check_default_options(), &checked
        );
        MMAY_EXPECT(actual == expected);
    }
    if (expected == MMAY_CHECK_OK) {
        MMAY_EXPECT(checked.initialized);
        if (checked.initialized) {
            if (checked.diagnostics.length != 0) {
                size_t diagnostic_index = 0;
                fprintf(stderr, "unexpected array fixture diagnostics for:\n%s\n", source);
                for (diagnostic_index = 0;
                    diagnostic_index < checked.diagnostics.length;
                    diagnostic_index += 1) {
                    const MarimayCheckDiagnostic *diagnostic =
                        mmay_check_diagnostic(&checked, diagnostic_index);
                    if (diagnostic != NULL) {
                        fprintf(
                            stderr,
                            "%s: %s\n",
                            diagnostic->code,
                            diagnostic->message
                        );
                    }
                }
            }
            MMAY_EXPECT(checked.diagnostics.length == 0);
            mmay_check_result_dispose(&checked);
        }
    } else {
        MMAY_EXPECT(!checked.initialized);
    }
    mmay_checker_arrays_fixture_dispose(&fixture);
}

static void mmay_test_checker_arrays_surface_and_matrix(void) {
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
        "  for row in 0..2 {\n"
        "    for column in 0..2 {\n"
        "      for inner in 0..3 {\n"
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
        "  const _ = empty.push(9);\n"
        "  let nested: [][]int = [[7, 8]];\n"
        "  const _ = nested[0].push(9);\n"
        "  return values[2] + values.length() + empty[0] + nested[0][2];\n"
        "}\n"
        "fn sorting(): int {\n"
        "  let values: []int = [3, 1, 2];\n"
        "  for pass in 0..3 {\n"
        "    for index in 0..2 {\n"
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
        "  for outer in 0..values.length() {\n"
        "    let index = 0;\n"
        "    while index < values.length() {\n"
        "      total += values[index];\n"
        "      index += 2;\n"
        "    }\n"
        "  }\n"
        "  return total;\n"
        "}\n";
    MarimayCheckerArraysFixture fixture;
    MarimayCheckResult checked;
    MarimayMirValue result;
    MarimayBytecodeFunction lowered;
    MarimayBytecodeFunction decoded;
    MarimayBytecodeExecution execution;
    MarimayOwnedBytes encoded;
    MarimayBytecodeValue vm_result;
    MarimayBytecodeRunOptions run_options;
    const MarimayCheckedFunction *surface = NULL;
    const MarimayCheckedFunction *matrix = NULL;
    const MarimayCheckedFunction *generic = NULL;
    const MarimayCheckedFunction *sorting = NULL;
    const MarimayCheckedFunction *proven_loop = NULL;
    MarimayCheckedFunction *mutable_proven_loop = NULL;
    const MarimayTypeId *first_parameter = NULL;
    const MarimayTypeId *second_parameter = NULL;
    MarimayTypeId inner = MMAY_TYPE_INVALID;
    MarimayTypeId empty_element = MMAY_TYPE_INVALID;
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
    MMAY_EXPECT(mmay_checker_arrays_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/checker-arrays", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        surface = mmay_checker_arrays_function(&checked, "surface");
        matrix = mmay_checker_arrays_function(&checked, "matrix");
        generic = mmay_checker_arrays_function(&checked, "generic");
        sorting = mmay_checker_arrays_function(&checked, "sorting");
        proven_loop = mmay_checker_arrays_function(&checked, "provenLoop");
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(surface != NULL && matrix != NULL && generic != NULL
            && sorting != NULL && proven_loop != NULL);
        if (surface != NULL && matrix != NULL && generic != NULL
            && sorting != NULL && proven_loop != NULL) {
            first_parameter = mmay_vector_at_const(&surface->mir.register_types, 0);
            second_parameter = mmay_vector_at_const(&surface->mir.register_types, 1);
            MMAY_EXPECT(first_parameter != NULL && second_parameter != NULL);
            if (first_parameter != NULL && second_parameter != NULL) {
                MMAY_EXPECT(mmay_checker_arrays_is_array_of(
                    &checked, *first_parameter, MMAY_TYPE_F64
                ));
                MMAY_EXPECT(mmay_type_array_element(
                    &checked.types, *second_parameter, &inner
                ) && mmay_checker_arrays_is_array_of(&checked, inner, MMAY_TYPE_F64));
            }
            for (index = 0; index < surface->mir.array_constructions.length; index += 1) {
                const MarimayMirArrayConstruction *construction = mmay_vector_at_const(
                    &surface->mir.array_constructions, index
                );
                const MarimayMirArray *array = construction == NULL ? NULL : mmay_vector_at_const(
                    &surface->mir.arrays, construction->array
                );
                if (construction != NULL && array != NULL && construction->argument_count == 0) {
                    empty_constructions += 1;
                    empty_element = array->element_type;
                }
                if (construction != NULL && array != NULL
                    && construction->argument_count == 2
                    && array->element_type == MMAY_TYPE_F64) {
                    inferred_f64 = true;
                }
            }
            for (index = 0; index < surface->mir.blocks.length; index += 1) {
                const MarimayMirBlock *block = mmay_vector_at_const(&surface->mir.blocks, index);
                size_t instruction_index = 0;
                for (instruction_index = 0; block != NULL
                    && instruction_index < block->instructions.length; instruction_index += 1) {
                    const MarimayMirInstruction *instruction = mmay_vector_at_const(
                        &block->instructions, instruction_index
                    );
                    if (instruction != NULL && instruction->kind == MMAY_MIR_ARRAY_LENGTH) {
                        length_instructions += 1;
                    }
                    if (instruction != NULL
                        && instruction->kind == MMAY_MIR_ARRAY_GET) {
                        size_t previous_index = 0;
                        for (previous_index = 0;
                            previous_index < instruction_index;
                            previous_index += 1) {
                            const MarimayMirInstruction *previous =
                                mmay_vector_at_const(
                                    &block->instructions, previous_index
                                );
                            if (previous != NULL
                                && previous->kind
                                    == MMAY_MIR_ARRAY_REQUIRE_INDEX
                                && previous->target == instruction->first
                                && previous->second == instruction->second) {
                                checked_gets += 1;
                                break;
                            }
                        }
                    }
                }
            }
            MMAY_EXPECT(empty_constructions == 1 && empty_element == MMAY_TYPE_F64);
            MMAY_EXPECT(inferred_f64);
            MMAY_EXPECT(length_instructions == 2);
            MMAY_EXPECT(checked_gets > 0);
            MMAY_EXPECT(mmay_checker_arrays_has_checked_store(&surface->mir, &compound));
            MMAY_EXPECT(compound);
            MMAY_EXPECT(mmay_checker_arrays_has_checked_store(&matrix->mir, NULL));
            MMAY_EXPECT(mmay_mir_verify(&surface->mir) == MMAY_MIR_VERIFY_OK);
            MMAY_EXPECT(mmay_mir_verify(&matrix->mir) == MMAY_MIR_VERIFY_OK);
            MMAY_EXPECT(mmay_mir_verify(&generic->mir) == MMAY_MIR_VERIFY_OK);
            MMAY_EXPECT(mmay_mir_verify(&sorting->mir) == MMAY_MIR_VERIFY_OK);
            index = mmay_checker_arrays_function_index(&checked, "provenLoop");
            mutable_proven_loop = index == SIZE_MAX ? NULL : mmay_vector_at(
                &checked.functions, index
            );
            MMAY_EXPECT(mutable_proven_loop != NULL);
            if (mutable_proven_loop != NULL) {
                size_t block_index = 0;
                MMAY_EXPECT(mmay_mir_optimize_ranges(&mutable_proven_loop->mir)
                    == MMAY_MIR_RANGE_OPTIMIZE_OK);
                for (block_index = 0;
                    block_index < mutable_proven_loop->mir.blocks.length;
                    block_index += 1) {
                    const MarimayMirBlock *block = mmay_vector_at_const(
                        &mutable_proven_loop->mir.blocks, block_index
                    );
                    size_t instruction_index = 0;
                    for (instruction_index = 0; block != NULL
                        && instruction_index < block->instructions.length;
                        instruction_index += 1) {
                        const MarimayMirInstruction *instruction =
                            mmay_vector_at_const(
                                &block->instructions, instruction_index
                            );
                        if (instruction != NULL && instruction->kind
                            == MMAY_MIR_ARRAY_ASSUME_INDEX) {
                            assumed_ranges += 1;
                        }
                    }
                }
                MMAY_EXPECT(assumed_ranges > 0);
                MMAY_EXPECT(mmay_mir_verify(&mutable_proven_loop->mir)
                    == MMAY_MIR_VERIFY_OK);
            }
            MMAY_EXPECT(mmay_mir_lower_bytecode_function(
                &generic->mir, mmay_system_allocator(),
                mmay_mir_bytecode_lower_default_options(), &lowered
            ) == MMAY_MIR_BYTECODE_LOWER_OK);
            MMAY_EXPECT(lowered.array_types.length == 2);
            MMAY_EXPECT(mmay_bytecode_verify(&lowered) == MMAY_BYTECODE_VERIFY_OK);
            MMAY_EXPECT(mmay_bytecode_encode_function(
                &lowered, mmay_system_allocator(), &encoded
            ) == MMAY_BYTECODE_CODEC_OK);
            MMAY_EXPECT(mmay_bytecode_decode_function(
                encoded.data, encoded.length, mmay_system_allocator(),
                mmay_bytecode_function_default_options(), &decoded
            ) == MMAY_BYTECODE_CODEC_OK);
            MMAY_EXPECT(mmay_bytecode_verify(&decoded) == MMAY_BYTECODE_VERIFY_OK);
            {
                MarimayBytecodeArrayConstruction *construction = mmay_vector_at(
                    &decoded.array_constructions, 0
                );
                MarimayBytecodeArrayType *array_type = mmay_vector_at(
                    &decoded.array_types, 0
                );
                MarimayBytecodeInstruction *push = NULL;
                size_t block_index = 0;
                size_t instruction_index = 0;
                for (block_index = 0; block_index < decoded.blocks.length;
                    block_index += 1) {
                    MarimayBytecodeBlock *block = mmay_vector_at(
                        &decoded.blocks, block_index
                    );
                    for (instruction_index = 0; block != NULL
                        && instruction_index < block->instructions.length;
                        instruction_index += 1) {
                        MarimayBytecodeInstruction *candidate = mmay_vector_at(
                            &block->instructions, instruction_index
                        );
                        if (candidate != NULL
                            && candidate->opcode == MMAY_OPCODE_ARRAY_PUSH) {
                            push = candidate;
                        }
                    }
                }
                if (construction != NULL) {
                    uint32_t saved_layout = construction->array_layout;
                    construction->array_layout = UINT32_MAX;
                    MMAY_EXPECT(mmay_bytecode_verify(&decoded)
                        != MMAY_BYTECODE_VERIFY_OK);
                    construction->array_layout = saved_layout;
                }
                if (array_type != NULL) {
                    uint32_t saved_layout = array_type->element_layout;
                    array_type->element_layout = 0;
                    MMAY_EXPECT(mmay_bytecode_verify(&decoded)
                        != MMAY_BYTECODE_VERIFY_OK);
                    array_type->element_layout = saved_layout;
                }
                if (push != NULL) {
                    uint64_t saved_value = push->operands[1].unsigned_value;
                    push->operands[1].unsigned_value =
                        push->operands[0].unsigned_value;
                    MMAY_EXPECT(mmay_bytecode_verify(&decoded)
                        != MMAY_BYTECODE_VERIFY_OK);
                    push->operands[1].unsigned_value = saved_value;
                }
                MMAY_EXPECT(mmay_bytecode_verify(&decoded)
                    == MMAY_BYTECODE_VERIFY_OK);
            }
            MMAY_EXPECT(mmay_bytecode_execution_init(
                &execution, mmay_system_allocator(),
                mmay_bytecode_execution_default_options()
            ));
            run_options = mmay_bytecode_run_default_options();
            run_options.execution = &execution;
            MMAY_EXPECT(mmay_bytecode_run(
                &decoded, NULL, 0, mmay_system_allocator(),
                run_options, &vm_result
            ) == MMAY_BYTECODE_RUN_OK);
            MMAY_EXPECT(vm_result.type == MMAY_BYTECODE_TYPE_I64
                && vm_result.as.i64 == 24);
            index = mmay_checker_arrays_function_index(&checked, "matrix");
            MMAY_EXPECT(index != SIZE_MAX);
            if (index != SIZE_MAX) {
                MMAY_EXPECT(mmay_check_interpret_function(
                    &checked, index, NULL, 0, mmay_system_allocator(),
                    mmay_check_interpret_default_options(), &result
                ) == MMAY_MIR_INTERPRET_OK);
                MMAY_EXPECT(result.type == MMAY_TYPE_F64
                    && result.as.f64_bits == UINT64_C(0x4079f00000000000));
            }
            index = mmay_checker_arrays_function_index(&checked, "generic");
            MMAY_EXPECT(index != SIZE_MAX);
            if (index != SIZE_MAX) {
                memset(&result, 0, sizeof(result));
                MMAY_EXPECT(mmay_check_interpret_function(
                    &checked, index, NULL, 0, mmay_system_allocator(),
                    mmay_check_interpret_default_options(), &result
                ) == MMAY_MIR_INTERPRET_OK);
                MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 24);
            }
            index = mmay_checker_arrays_function_index(&checked, "sorting");
            MMAY_EXPECT(index != SIZE_MAX);
            if (index != SIZE_MAX) {
                memset(&result, 0, sizeof(result));
                MMAY_EXPECT(mmay_check_interpret_function(
                    &checked, index, NULL, 0, mmay_system_allocator(),
                    mmay_check_interpret_default_options(), &result
                ) == MMAY_MIR_INTERPRET_OK);
                MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 2);
            }
            index = mmay_checker_arrays_function_index(&checked, "provenLoop");
            MMAY_EXPECT(index != SIZE_MAX);
            if (index != SIZE_MAX) {
                memset(&result, 0, sizeof(result));
                MMAY_EXPECT(mmay_check_interpret_function(
                    &checked, index, NULL, 0, mmay_system_allocator(),
                    mmay_check_interpret_default_options(), &result
                ) == MMAY_MIR_INTERPRET_OK);
                MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 12);
            }
        }
        if (execution.initialized) {
            mmay_bytecode_execution_dispose(&execution);
        }
        if (decoded.initialized) {
            mmay_bytecode_function_dispose(&decoded);
        }
        if (lowered.initialized) {
            mmay_bytecode_function_dispose(&lowered);
        }
        if (encoded.data != NULL) {
            mmay_owned_bytes_dispose(&encoded);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_arrays_fixture_dispose(&fixture);
}

static void mmay_test_checker_arrays_d045_and_diagnostics(void) {
    static const char bounds_source[] =
        "fn bounds(): int { let values = [1]; values[3] = panic(\"rhs\"); }\n";
    static const unsigned char bounds_message[] =
        "array index 3 is outside length 1";
    MarimayCheckerArraysFixture fixture;
    MarimayCheckResult checked;
    MarimayCheckInterpretOptions options = mmay_check_interpret_default_options();
    MarimayMirPanic panic;
    MarimayMirValue result;
    const MarimayCheckedFunction *bounds = NULL;
    const char *access = strstr(bounds_source, "values[3]");
    size_t index = 0;
    memset(&checked, 0, sizeof(checked));
    memset(&panic, 0, sizeof(panic));
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_checker_arrays_fixture_init(&fixture, bounds_source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/checker-arrays", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        bounds = mmay_checker_arrays_function(&checked, "bounds");
        index = mmay_checker_arrays_function_index(&checked, "bounds");
        MMAY_EXPECT(bounds != NULL && index != SIZE_MAX && access != NULL);
        options.panic = &panic;
        if (index != SIZE_MAX) {
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked, index, NULL, 0, mmay_system_allocator(), options, &result
            ) == MMAY_MIR_INTERPRET_PANIC);
            MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4002") == 0);
            MMAY_EXPECT(panic.message.length == sizeof(bounds_message) - 1);
            MMAY_EXPECT(panic.message.data != NULL && memcmp(
                panic.message.data,
                bounds_message,
                sizeof(bounds_message) - 1
            ) == 0);
            if (bounds != NULL && access != NULL) {
                MMAY_EXPECT(mmay_symbol_id_equal(
                    panic.symbol,
                    bounds->mir.symbol
                ));
                MMAY_EXPECT(panic.span.start == (size_t)(access - bounds_source));
                MMAY_EXPECT(panic.span.end
                    == (size_t)(access - bounds_source) + strlen("values[3]"));
            }
        }
        mmay_mir_panic_dispose(&panic);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_arrays_fixture_dispose(&fixture);

    mmay_checker_arrays_expect_type_diagnostic(
        "fn bad(): void { let values = [1, true]; }\n"
    );
    mmay_checker_arrays_expect_type_diagnostic(
        "fn bad(): void { let values = [1]; values[true] = 2; }\n"
    );
    mmay_checker_arrays_expect_type_diagnostic(
        "fn bad(): void { let values = [1]; values[0] = true; }\n"
    );
    mmay_checker_arrays_expect_type_diagnostic(
        "fn bad(): void { let values = []; }\n"
    );
    mmay_checker_arrays_expect_status(
        "fn deferred(): void { let values = arrayOf(1); }\n",
        MMAY_CHECK_OK
    );
    mmay_checker_arrays_expect_status(
        "fn deferred(): void { let values = arrayOf<int>(); }\n",
        MMAY_CHECK_OK
    );
    mmay_checker_arrays_expect_status(
        "fn deferred(): void { let values = [1]; const _ = push(values, 2); }\n",
        MMAY_CHECK_OK
    );
    mmay_checker_arrays_expect_status(
        "fn deferred(): void { let values = [1]; const _ = values.push(2); }\n",
        MMAY_CHECK_OK
    );
}

static void mmay_test_checker_arrays_length_shadow(void) {
    static const char source[] =
        "fn length(value: int): int { return value + 1; }\n"
        "fn use(): int { return length(4); }\n";
    MarimayCheckerArraysFixture fixture;
    MarimayCheckResult checked;
    MarimayMirValue result;
    size_t index = SIZE_MAX;
    memset(&checked, 0, sizeof(checked));
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_checker_arrays_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/checker-array-shadow", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        index = mmay_checker_arrays_function_index(&checked, "use");
        MMAY_EXPECT(index != SIZE_MAX);
        if (index != SIZE_MAX) {
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked, index, NULL, 0, mmay_system_allocator(),
                mmay_check_interpret_default_options(), &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 5);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_arrays_fixture_dispose(&fixture);
}

static void mmay_test_checker_arrays_for_of(void) {
    static const char source[] =
        "fn iterate(): int {\n"
        "  let total = 0;\n"
        "  for value in [1, 2, 3, 4] {\n"
        "    if value == 2 { continue; }\n"
        "    total += value;\n"
        "    if value == 3 { break; }\n"
        "  }\n"
        "  return total;\n"
        "}\n";
    MarimayCheckerArraysFixture fixture;
    MarimayCheckResult checked;
    MarimayBytecodeFunction lowered;
    MarimayBytecodeExecution execution;
    MarimayBytecodeRunOptions run_options;
    MarimayBytecodeValue vm_result;
    MarimayMirValue mir_result;
    const MarimayCheckedFunction *function = NULL;
    size_t index = SIZE_MAX;
    memset(&checked, 0, sizeof(checked));
    memset(&lowered, 0, sizeof(lowered));
    memset(&execution, 0, sizeof(execution));
    memset(&vm_result, 0, sizeof(vm_result));
    memset(&mir_result, 0, sizeof(mir_result));
    MMAY_EXPECT(mmay_checker_arrays_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/checker-array-for-of", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        function = mmay_checker_arrays_function(&checked, "iterate");
        index = mmay_checker_arrays_function_index(&checked, "iterate");
        MMAY_EXPECT(function != NULL && index != SIZE_MAX);
        if (function != NULL && index != SIZE_MAX) {
            MMAY_EXPECT(mmay_mir_verify(&function->mir) == MMAY_MIR_VERIFY_OK);
            MMAY_EXPECT(mmay_checker_arrays_instruction_count(
                &function->mir, MMAY_MIR_ARRAY_LENGTH
            ) == 2);
            MMAY_EXPECT(mmay_checker_arrays_instruction_count(
                &function->mir, MMAY_MIR_ARRAY_GET_IN_BOUNDS
            ) == 1);
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked, index, NULL, 0, mmay_system_allocator(),
                mmay_check_interpret_default_options(), &mir_result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(mir_result.type == MMAY_TYPE_I64
                && mir_result.as.i64 == 4);
            MMAY_EXPECT(mmay_mir_lower_bytecode_function(
                &function->mir, mmay_system_allocator(),
                mmay_mir_bytecode_lower_default_options(), &lowered
            ) == MMAY_MIR_BYTECODE_LOWER_OK);
            if (lowered.initialized) {
                MMAY_EXPECT(mmay_bytecode_verify(&lowered)
                    == MMAY_BYTECODE_VERIFY_OK);
                MMAY_EXPECT(mmay_bytecode_execution_init(
                    &execution, mmay_system_allocator(),
                    mmay_bytecode_execution_default_options()
                ));
                if (execution.initialized) {
                    run_options = mmay_bytecode_run_default_options();
                    run_options.execution = &execution;
                    MMAY_EXPECT(mmay_bytecode_run(
                        &lowered, NULL, 0, mmay_system_allocator(),
                        run_options, &vm_result
                    ) == MMAY_BYTECODE_RUN_OK);
                    MMAY_EXPECT(vm_result.type == MMAY_BYTECODE_TYPE_I64
                        && vm_result.as.i64 == 4);
                    mmay_bytecode_execution_dispose(&execution);
                }
                mmay_bytecode_function_dispose(&lowered);
            }
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_arrays_fixture_dispose(&fixture);
}

static void mmay_test_checker_arrays_allocation_failures(void) {
    static const char source[] =
        "fn arrays(): int {\n"
        "  let values = arrayOf(1, 2);\n"
        "  let empty = arrayOf<int>();\n"
        "  const _ = values.push(3);\n"
        "  values[0] += 2;\n"
        "  const _ = empty.push(4);\n"
        "  return values[0] + empty[0];\n"
        "}\n";
    MarimayCheckerArraysFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    MMAY_EXPECT(mmay_checker_arrays_fixture_init(&fixture, source));
    for (failure_index = 0; failure_index < 1024; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        memset(&checked, 0, sizeof(checked));
        mmay_fail_allocator_init(
            &state,
            mmay_system_allocator(),
            failure_index
        );
        allocator = mmay_fail_allocator(&state);
        status = mmay_check_ast(
            &fixture.ast,
            "test/checker-array-allocation",
            allocator,
            mmay_check_default_options(),
            &checked
        );
        if (status == MMAY_CHECK_OK) {
            reached_success = true;
            MMAY_EXPECT(checked.initialized);
            MMAY_EXPECT(checked.diagnostics.length == 0);
            mmay_check_result_dispose(&checked);
        } else {
            MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
            MMAY_EXPECT(!checked.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
    mmay_checker_arrays_fixture_dispose(&fixture);
}

static void mmay_test_checker_arrays_limits(void) {
    static const char source[] =
        "fn arrays(): [][]f64 { return [[1.0, 2.0]]; }\n";
    MarimayCheckerArraysFixture fixture;
    MarimayCheckOptions options;
    MarimayCheckResult checked;
    MMAY_EXPECT(mmay_checker_arrays_fixture_init(&fixture, source));

    memset(&checked, 0, sizeof(checked));
    options = mmay_check_default_options();
    options.mir_options.max_array_layouts = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/checker-array-layout-limit", mmay_system_allocator(),
        options, &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);

    memset(&checked, 0, sizeof(checked));
    options = mmay_check_default_options();
    options.mir_options.max_array_arguments = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/checker-array-argument-limit", mmay_system_allocator(),
        options, &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);

    memset(&checked, 0, sizeof(checked));
    options = mmay_check_default_options();
    options.type_options.max_types = MMAY_TYPE_FIRST_DYNAMIC + 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/checker-array-type-limit", mmay_system_allocator(),
        options, &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);

    mmay_checker_arrays_fixture_dispose(&fixture);
}

void mmay_test_checker_arrays(void) {
    mmay_test_checker_arrays_surface_and_matrix();
    mmay_test_checker_arrays_d045_and_diagnostics();
    mmay_test_checker_arrays_length_shadow();
    mmay_test_checker_arrays_for_of();
    mmay_test_checker_arrays_allocation_failures();
    mmay_test_checker_arrays_limits();
}
