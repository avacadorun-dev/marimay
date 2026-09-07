#include "test_checker_support.h"
#include "test_mir_bytecode_support.h"

static const char mmay_rune_equality_source[] =
    "@derive(Eq) type Box { value: rune, }\n"
    "fn equal(left: rune, right: rune): bool { return left == right; }\n"
    "fn different(left: rune, right: rune): bool { return left != right; }\n"
    "fn nested(left: rune, right: rune): bool {\n"
    "  return Some(left) == Some(right)\n"
    "    && Box { value: left } == Box { value: right };\n"
    "}\n";

static void mmay_test_rune_equality_verifiers(
    MarimayMirFunction *mir,
    MarimayBytecodeFunction *bytecode
) {
    MarimayMirBlock *mir_block = mmay_vector_at(&mir->blocks, 0);
    MarimayBytecodeBlock *wire_block = mmay_vector_at(&bytecode->blocks, 0);
    MarimayMirInstruction *mir_equal = mir_block != NULL
        ? mmay_vector_at(&mir_block->instructions, 0) : NULL;
    MarimayBytecodeInstruction *wire_equal = wire_block != NULL
        ? mmay_vector_at(&wire_block->instructions, 0) : NULL;
    MarimayTypeId *mir_right = mmay_vector_at(&mir->register_types, 1);
    MarimayBytecodeType *wire_right = mmay_vector_at(
        &bytecode->register_types, 1
    );
    MMAY_EXPECT(mir_equal != NULL && wire_equal != NULL
        && mir_right != NULL && wire_right != NULL);
    if (mir_equal == NULL || wire_equal == NULL
        || mir_right == NULL || wire_right == NULL) return;
    MMAY_EXPECT(mir_equal->kind == MMAY_MIR_EQUAL_I64);
    MMAY_EXPECT(wire_equal->opcode == MMAY_OPCODE_I64_EQUAL);
    mir_equal->kind = MMAY_MIR_LESS_I64;
    wire_equal->opcode = MMAY_OPCODE_I64_LESS;
    MMAY_EXPECT(mmay_mir_verify(mir) == MMAY_MIR_VERIFY_TYPE_MISMATCH);
    MMAY_EXPECT(mmay_bytecode_verify(bytecode)
        == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
    mir_equal->kind = MMAY_MIR_LESS_EQUAL_I64;
    wire_equal->opcode = MMAY_OPCODE_I64_LESS_EQUAL;
    MMAY_EXPECT(mmay_mir_verify(mir) == MMAY_MIR_VERIFY_TYPE_MISMATCH);
    MMAY_EXPECT(mmay_bytecode_verify(bytecode)
        == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
    mir_equal->kind = MMAY_MIR_EQUAL_I64;
    wire_equal->opcode = MMAY_OPCODE_I64_EQUAL;
    *mir_right = MMAY_TYPE_U32;
    *wire_right = MMAY_BYTECODE_TYPE_U32;
    MMAY_EXPECT(mmay_mir_verify(mir) == MMAY_MIR_VERIFY_TYPE_MISMATCH);
    MMAY_EXPECT(mmay_bytecode_verify(bytecode)
        == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
    *mir_right = MMAY_TYPE_RUNE;
    *wire_right = MMAY_BYTECODE_TYPE_RUNE;
    MMAY_EXPECT(mmay_mir_verify(mir) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_verify(bytecode) == MMAY_BYTECODE_VERIFY_OK);
}

static void mmay_test_rune_equality_execution(void) {
    static const uint64_t scalars[] = {
        0, 0x41, 0x3bb, 0xd7ff, 0xe000, 0xffff, 0x10041, 0x1f680, 0x10ffff
    };
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked = {0};
    size_t index = 0;
    if (!mmay_checker_fixture_init(&fixture, mmay_rune_equality_source)) {
        MMAY_EXPECT(false);
        mmay_checker_fixture_dispose(&fixture);
        return;
    }
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/rune-equality", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized && checked.diagnostics.length == 0);
    MMAY_EXPECT(checked.functions.length == 3);
    for (index = 0; index < checked.functions.length; index += 1) {
        MarimayCheckedFunction *function = mmay_vector_at(
            &checked.functions, index
        );
        MarimayBytecodeFunction lowered = {0};
        MarimayBytecodeFunction decoded = {0};
        MarimayOwnedBytes encoded = {0};
        MarimayBytecodeExecution execution = {0};
        MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
        size_t left = 0;
        size_t right = 0;
        if (function == NULL || mmay_mir_lower_bytecode_function(
                &function->mir, mmay_system_allocator(),
                mmay_mir_bytecode_lower_default_options(), &lowered
            ) != MMAY_MIR_BYTECODE_LOWER_OK
            || mmay_bytecode_encode_function(
                &lowered, mmay_system_allocator(), &encoded
            ) != MMAY_BYTECODE_CODEC_OK
            || mmay_bytecode_decode_function(
                encoded.data, encoded.length, mmay_system_allocator(),
                mmay_bytecode_function_default_options(), &decoded
            ) != MMAY_BYTECODE_CODEC_OK
            || !mmay_bytecode_execution_init(
                &execution, mmay_system_allocator(),
                mmay_bytecode_execution_default_options()
            )) {
            MMAY_EXPECT(false);
            goto function_cleanup;
        }
        options.execution = &execution;
        if (index == 0) {
            MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                &function->mir, MMAY_MIR_EQUAL_I64
            ) == 1);
            MMAY_EXPECT(function->mir.blocks.length == 1);
            mmay_test_rune_equality_verifiers(&function->mir, &decoded);
        }
        for (left = 0; left < sizeof(scalars) / sizeof(scalars[0]); left += 1) {
            for (right = 0; right < sizeof(scalars) / sizeof(scalars[0]);
                right += 1) {
                MarimayMirValue arguments[2] = {{0}, {0}};
                MarimayBytecodeValue wire_arguments[2] = {{0}, {0}};
                MarimayMirValue result = {0};
                MarimayBytecodeValue wire_result = {0};
                bool expected = index == 1 ? left != right : left == right;
                arguments[0].type = arguments[1].type = MMAY_TYPE_RUNE;
                arguments[0].as.integer_bits = scalars[left];
                arguments[1].as.integer_bits = scalars[right];
                wire_arguments[0].type = wire_arguments[1].type =
                    MMAY_BYTECODE_TYPE_RUNE;
                wire_arguments[0].as.integer_bits = scalars[left];
                wire_arguments[1].as.integer_bits = scalars[right];
                MMAY_EXPECT(mmay_check_interpret_function(
                    &checked, index, arguments, 2, mmay_system_allocator(),
                    mmay_check_interpret_default_options(), &result
                ) == MMAY_MIR_INTERPRET_OK);
                MMAY_EXPECT(result.type == MMAY_TYPE_BOOL
                    && result.as.boolean == expected);
                MMAY_EXPECT(mmay_bytecode_run(
                    &decoded, wire_arguments, 2, mmay_system_allocator(),
                    options, &wire_result
                ) == MMAY_BYTECODE_RUN_OK);
                MMAY_EXPECT(wire_result.type == MMAY_BYTECODE_TYPE_BOOL
                    && wire_result.as.boolean == expected);
            }
        }
function_cleanup:
        mmay_bytecode_execution_dispose(&execution);
        mmay_bytecode_function_dispose(&decoded);
        mmay_owned_bytes_dispose(&encoded);
        mmay_bytecode_function_dispose(&lowered);
    }
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
}

static void mmay_test_rune_equality_allocations(void) {
    static const char invalid[] =
        "@derive(Eq) type Box { value: []rune, }\n"
        "fn invalid(left: Box, right: Box): bool { return left == right; }\n";
    const char *sources[] = {mmay_rune_equality_source, invalid};
    size_t source_index = 0;
    for (source_index = 0; source_index < 2; source_index += 1) {
        MarimayCheckerFixture fixture;
        bool reached_success = false;
        size_t failure_index = 0;
        if (!mmay_checker_fixture_init(&fixture, sources[source_index])) {
            MMAY_EXPECT(false);
            mmay_checker_fixture_dispose(&fixture);
            continue;
        }
        for (failure_index = 0; failure_index < 4096; failure_index += 1) {
            MarimayFailAllocator state;
            MarimayCheckResult checked = {0};
            MarimayCheckStatus status;
            mmay_fail_allocator_init(
                &state, mmay_system_allocator(), failure_index
            );
            status = mmay_check_ast(
                &fixture.ast, "test/rune-equality-allocation",
                mmay_fail_allocator(&state), mmay_check_default_options(),
                &checked
            );
            if (status == MMAY_CHECK_OK) {
                reached_success = true;
                MMAY_EXPECT(checked.initialized);
                MMAY_EXPECT(checked.diagnostics.length == source_index);
            } else {
                MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
                MMAY_EXPECT(!checked.initialized);
            }
            mmay_check_result_dispose(&checked);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
            if (reached_success) break;
        }
        MMAY_EXPECT(reached_success);
        mmay_checker_fixture_dispose(&fixture);
    }
}

static void mmay_test_rune_equality_diagnostics(void) {
    const char *sources[] = {
        "fn bad(left: rune, right: int): bool { return left == right; }\n",
        "fn bad(left: rune, right: string): bool { return left == right; }\n",
        "fn bad(left: rune, right: bool): bool { return left == right; }\n",
        "fn bad(left: int, right: rune): bool { return left == right; }\n"
    };
    size_t index = 0;
    for (index = 0; index < sizeof(sources) / sizeof(sources[0]); index += 1) {
        mmay_checker_expect_one_diagnostic(
            sources[index], MMAY_DIAGNOSTIC_INVALID_OPERATOR, "==", 0
        );
    }
    mmay_checker_expect_one_diagnostic(
        "fn bad(left: rune, right: rune): bool { return left < right; }\n",
        MMAY_DIAGNOSTIC_INVALID_OPERATOR, "<", 0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(left: rune, right: rune): rune { return left + right; }\n",
        MMAY_DIAGNOSTIC_INVALID_OPERATOR, "+", 0
    );
}

void mmay_test_checker_equality(void) {
    static const char source[] =
        "enum Theme { Work, Play, Named(string), }\n"
        "@derive(Eq) type Pair { left: int, right: string, }\n"
        "fn noValue(): Option<int> { return None; }\n"
        "fn strings(): bool {\n"
        "  return \"a\\0b\" == \"a\\0b\" && \"é\" != \"e\";\n"
        "}\n"
        "fn enums(): bool {\n"
        "  return Theme.Work == Theme.Work\n"
        "    && Theme.Work != Theme.Play\n"
        "    && Theme.Named(\"x\") == Theme.Named(\"x\")\n"
        "    && Theme.Named(\"x\") != Theme.Named(\"y\");\n"
        "}\n"
        "fn options(): bool {\n"
        "  return Some(1) == Some(1) && Some(1) != Some(2)\n"
        "    && noValue() == noValue() && Some(1) != noValue();\n"
        "}\n"
        "fn records(): bool {\n"
        "  return Pair { left: 1, right: \"x\" }\n"
        "      == Pair { left: 1, right: \"x\" }\n"
        "    && Pair { left: 1, right: \"x\" }\n"
        "      != Pair { left: 2, right: \"x\" };\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    size_t index = 0;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/equality", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 5);
        for (index = 0; index < checked.functions.length; index += 1) {
            const MarimayCheckedFunction *function = mmay_checked_function(
                &checked, index
            );
            MarimayMirValue result;
            if (function == NULL || function->mir.result_type != MMAY_TYPE_BOOL) {
                continue;
            }
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked, index, NULL, 0, mmay_system_allocator(),
                mmay_check_interpret_default_options(), &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_BOOL);
            MMAY_EXPECT(result.as.boolean);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    mmay_checker_expect_one_diagnostic(
        "type Pair { value: int, }\n"
        "fn bad(left: Pair, right: Pair): bool { return left == right; }\n",
        MMAY_DIAGNOSTIC_INVALID_OPERATOR, "left == right", 0
    );
    mmay_checker_expect_one_diagnostic(
        "@derive(Hash) type Pair { value: int, }\n",
        MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "@derive(Hash) type Pair { value: int, }", 0
    );
    mmay_test_rune_equality_execution();
    mmay_test_rune_equality_allocations();
    mmay_test_rune_equality_diagnostics();
}
