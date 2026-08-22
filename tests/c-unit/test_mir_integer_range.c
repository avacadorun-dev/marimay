#include "test_mir_support.h"
#include "test_native_c_emitter_support.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/bytecode/codec.h"
#include "mmay/bytecode/mir_lower.h"
#include "mmay/bytecode/vm.h"
#include "mmay/mir/range.h"
#include "mmay/native/c_emitter.h"

static MarimayMirInstruction *mmay_test_integer_mir_at(
    MarimayMirFunction *function,
    size_t index
) {
    MarimayMirBlock *block = function != NULL
        ? mmay_vector_at(&function->blocks, 0) : NULL;
    return block != NULL ? mmay_vector_at(&block->instructions, index) : NULL;
}

static MarimayBytecodeInstruction *mmay_test_integer_bytecode_at(
    MarimayBytecodeFunction *function,
    size_t index
) {
    MarimayBytecodeBlock *block = function != NULL
        ? mmay_vector_at(&function->blocks, 0) : NULL;
    return block != NULL ? mmay_vector_at(&block->instructions, index) : NULL;
}

static bool mmay_test_integer_build_safe(
    MarimayMirFunction *function,
    MarimayAllocator allocator,
    MarimaySymbolId symbol
) {
    MarimayMirRegister registers[5];
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    size_t index = 0;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(
            function, allocator, symbol, NULL, 0, MMAY_TYPE_I64,
            mmay_mir_function_default_options()
        )) {
        return false;
    }
    for (index = 0; index < 5; index += 1) {
        registers[index] = MMAY_MIR_NO_REGISTER;
        if (!mmay_mir_add_register(
                function, MMAY_TYPE_I64, &registers[index]
            ) || registers[index] != index) {
            goto failure;
        }
    }
    if (!mmay_mir_add_block(function, &block)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_I64, 0, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 7, false, 7000
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_I64, 1, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 3, false, 7001
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ADD_CHECKED_I64, 2, 0, 1, 0, false, 7002
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_SUBTRACT_CHECKED_I64, 3, 2, 1, 0, false, 7003
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_MULTIPLY_CHECKED_I64, 4, 3, 1, 0, false, 7004
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(
            function, block,
            mmay_test_native_terminator(
                MMAY_MIR_RETURN, 4, MMAY_MIR_NO_BLOCK,
                MMAY_MIR_NO_BLOCK, 7005
            )
        )) {
        goto failure;
    }
    return true;

failure:
    mmay_mir_function_dispose(function);
    return false;
}

static bool mmay_test_integer_build_overflow(
    MarimayMirFunction *function,
    MarimayAllocator allocator,
    MarimaySymbolId symbol
) {
    MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    size_t index = 0;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(
            function, allocator, symbol, NULL, 0, MMAY_TYPE_I64,
            mmay_mir_function_default_options()
        )) {
        return false;
    }
    for (index = 0; index < 3; index += 1) {
        if (!mmay_mir_add_register(function, MMAY_TYPE_I64, &value)
            || value != index) {
            goto failure;
        }
    }
    if (!mmay_mir_add_block(function, &block)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_I64, 0, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, INT64_MAX, false, 30
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_I64, 1, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 1, false, 31
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ADD_CHECKED_I64, 2, 0, 1, 0, false, 32
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(
            function, block,
            mmay_test_native_terminator(
                MMAY_MIR_RETURN, 2, MMAY_MIR_NO_BLOCK,
                MMAY_MIR_NO_BLOCK, 33
            )
        )) {
        goto failure;
    }
    return true;

failure:
    mmay_mir_function_dispose(function);
    return false;
}

static void mmay_test_integer_safe_vertical(MarimaySymbolId symbol) {
    MarimayMirFunction function;
    MarimayMirValue mir_result;
    MarimayBytecodeFunction lowered;
    MarimayBytecodeFunction decoded;
    MarimayBytecodeValue bytecode_result;
    MarimayOwnedBytes encoded;
    MarimayOwnedBytes generated;
    MarimayNativeCProgram program;
    const MarimayMirFunction *functions[1];
    MarimayMirInstruction *first_constant = NULL;
    MarimayBytecodeInstruction *bytecode_constant = NULL;
    memset(&lowered, 0, sizeof(lowered));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&generated, 0, sizeof(generated));
    memset(&program, 0, sizeof(program));
    memset(&mir_result, 0, sizeof(mir_result));
    memset(&bytecode_result, 0, sizeof(bytecode_result));
    MMAY_EXPECT(mmay_test_integer_build_safe(
        &function, mmay_system_allocator(), symbol
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_interpret(
        &function, NULL, 0, mmay_system_allocator(),
        mmay_mir_interpret_default_options(), &mir_result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(mir_result.type == MMAY_TYPE_I64
        && mir_result.as.i64 == 21);
    MMAY_EXPECT(mmay_mir_optimize_ranges(&function)
        == MMAY_MIR_RANGE_OPTIMIZE_OK);
    MMAY_EXPECT(mmay_test_integer_mir_at(&function, 2)->kind
        == MMAY_MIR_ADD_PROVEN_I64);
    MMAY_EXPECT(mmay_test_integer_mir_at(&function, 3)->kind
        == MMAY_MIR_SUBTRACT_PROVEN_I64);
    MMAY_EXPECT(mmay_test_integer_mir_at(&function, 4)->kind
        == MMAY_MIR_MULTIPLY_PROVEN_I64);
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_interpret(
        &function, NULL, 0, mmay_system_allocator(),
        mmay_mir_interpret_default_options(), &mir_result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(mir_result.as.i64 == 21);

    MMAY_EXPECT(mmay_mir_lower_bytecode_function(
        &function, mmay_system_allocator(),
        mmay_mir_bytecode_lower_default_options(), &lowered
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_test_integer_bytecode_at(&lowered, 2)->opcode
        == MMAY_OPCODE_I64_ADD_PROVEN);
    MMAY_EXPECT(mmay_test_integer_bytecode_at(&lowered, 3)->opcode
        == MMAY_OPCODE_I64_SUBTRACT_PROVEN);
    MMAY_EXPECT(mmay_test_integer_bytecode_at(&lowered, 4)->opcode
        == MMAY_OPCODE_I64_MULTIPLY_PROVEN);
    MMAY_EXPECT(mmay_bytecode_verify(&lowered) == MMAY_BYTECODE_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_encode_function(
        &lowered, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_function(
        encoded.data, encoded.length, mmay_system_allocator(),
        mmay_bytecode_function_default_options(), &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_verify(&decoded) == MMAY_BYTECODE_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_run(
        &decoded, NULL, 0, mmay_system_allocator(),
        mmay_bytecode_run_default_options(), &bytecode_result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(bytecode_result.type == MMAY_BYTECODE_TYPE_I64
        && bytecode_result.as.i64 == 21);

    functions[0] = &function;
    program.functions = functions;
    program.function_count = 1;
    program.entry_symbol = symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &program, mmay_system_allocator(),
        mmay_native_c_emit_default_options(), &generated
    ) == MMAY_NATIVE_C_EMIT_OK);
    MMAY_EXPECT(mmay_test_native_contains(
        &generated, "mmay_i64_add_proven"
    ));
    MMAY_EXPECT(mmay_test_native_contains(
        &generated, "mmay_i64_subtract_proven"
    ));
    MMAY_EXPECT(mmay_test_native_contains(
        &generated, "mmay_i64_multiply_proven"
    ));
#if !defined(_WIN32)
    mmay_test_native_compile_and_run(&generated, "ok:i64:21\n", 0);
#endif

    first_constant = mmay_test_integer_mir_at(&function, 0);
    MMAY_EXPECT(first_constant != NULL);
    if (first_constant != NULL) {
        first_constant->integer = INT64_MAX;
        MMAY_EXPECT(mmay_mir_verify(&function)
            == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
        first_constant->integer = 7;
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    }
    bytecode_constant = mmay_test_integer_bytecode_at(&decoded, 0);
    MMAY_EXPECT(bytecode_constant != NULL);
    if (bytecode_constant != NULL) {
        bytecode_constant->operands[1].signed_value = INT64_MAX;
        MMAY_EXPECT(mmay_bytecode_verify(&decoded)
            == MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT);
        bytecode_constant->operands[1].signed_value = 7;
        MMAY_EXPECT(mmay_bytecode_verify(&decoded)
            == MMAY_BYTECODE_VERIFY_OK);
    }

    mmay_owned_bytes_dispose(&generated);
    mmay_bytecode_function_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_function_dispose(&lowered);
    mmay_mir_function_dispose(&function);
}

static void mmay_test_integer_overflow_retained(MarimaySymbolId symbol) {
    MarimayMirFunction function;
    MarimayMirValue result;
    MarimayOwnedBytes generated;
    MarimayNativeCProgram program;
    const MarimayMirFunction *functions[1];
    MarimayMirInstruction *addition = NULL;
    memset(&result, 0, sizeof(result));
    memset(&generated, 0, sizeof(generated));
    memset(&program, 0, sizeof(program));
    MMAY_EXPECT(mmay_test_integer_build_overflow(
        &function, mmay_system_allocator(), symbol
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_optimize_ranges(&function)
        == MMAY_MIR_RANGE_OPTIMIZE_OK);
    addition = mmay_test_integer_mir_at(&function, 2);
    MMAY_EXPECT(addition != NULL
        && addition->kind == MMAY_MIR_ADD_CHECKED_I64);
    MMAY_EXPECT(mmay_mir_interpret(
        &function, NULL, 0, mmay_system_allocator(),
        mmay_mir_interpret_default_options(), &result
    ) == MMAY_MIR_INTERPRET_OVERFLOW);
    mmay_test_native_vm_overflow(&function);
    functions[0] = &function;
    program.functions = functions;
    program.function_count = 1;
    program.entry_symbol = symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &program, mmay_system_allocator(),
        mmay_native_c_emit_default_options(), &generated
    ) == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    mmay_test_native_compile_and_run(&generated, "trap:1:32:33\n", 1);
#endif
    if (addition != NULL) {
        addition->kind = MMAY_MIR_ADD_PROVEN_I64;
        MMAY_EXPECT(mmay_mir_verify(&function)
            == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
    }
    mmay_owned_bytes_dispose(&generated);
    mmay_mir_function_dispose(&function);
}

static void mmay_test_integer_unreachable_checked_fallback(
    MarimaySymbolId symbol
) {
    MarimayMirFunction function;
    MarimayMirRegister register_id = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    size_t index = 0;
    MMAY_EXPECT(mmay_test_integer_build_safe(
        &function, mmay_system_allocator(), symbol
    ));
    for (index = 0; index < 3; index += 1) {
        MMAY_EXPECT(mmay_mir_add_register(
            &function, MMAY_TYPE_I64, &register_id
        ));
        MMAY_EXPECT(register_id == 5 + index);
    }
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_I64, 5, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, INT64_MAX, false, 7100
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_I64, 6, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 1, false, 7101
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ADD_CHECKED_I64, 7, 5, 6, 0, false, 7102
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function, block,
        mmay_test_native_terminator(
            MMAY_MIR_RETURN, 7, MMAY_MIR_NO_BLOCK,
            MMAY_MIR_NO_BLOCK, 7103
        )
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_optimize_ranges(&function)
        == MMAY_MIR_RANGE_OPTIMIZE_OK);
    MMAY_EXPECT(mmay_test_integer_mir_at(&function, 2)->kind
        == MMAY_MIR_ADD_PROVEN_I64);
    {
        MarimayMirBlock *unreachable = mmay_vector_at(&function.blocks, block);
        MarimayMirInstruction *addition = unreachable != NULL
            ? mmay_vector_at(&unreachable->instructions, 2) : NULL;
        MMAY_EXPECT(addition != NULL
            && addition->kind == MMAY_MIR_ADD_CHECKED_I64);
        if (addition != NULL) {
            addition->kind = MMAY_MIR_ADD_PROVEN_I64;
            MMAY_EXPECT(mmay_mir_verify(&function)
                == MMAY_MIR_VERIFY_INVALID_CONTROL_FLOW);
            addition->kind = MMAY_MIR_ADD_CHECKED_I64;
        }
    }
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    mmay_mir_function_dispose(&function);
}

static void mmay_test_integer_optimizer_allocation_fallback(
    MarimaySymbolId symbol
) {
    size_t failure_offset = 0;
    bool reached_success = false;
    for (failure_offset = 0; failure_offset < 64 && !reached_success;
        failure_offset += 1) {
        MarimayFailAllocator state;
        MarimayMirFunction function;
        MarimayMirRangeOptimizeStatus status;
        size_t baseline_allocations = 0;
        size_t baseline_bytes = 0;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_test_integer_build_safe(
            &function, mmay_fail_allocator(&state), symbol
        ));
        baseline_allocations = state.live_allocations;
        baseline_bytes = state.live_bytes;
        state.failure_index = state.attempts + failure_offset;
        status = mmay_mir_optimize_ranges(&function);
        MMAY_EXPECT(status == MMAY_MIR_RANGE_OPTIMIZE_OK
            || status == MMAY_MIR_RANGE_OPTIMIZE_ALLOCATION_FAILED);
        reached_success = status == MMAY_MIR_RANGE_OPTIMIZE_OK;
        if (reached_success) {
            MMAY_EXPECT(mmay_test_integer_mir_at(&function, 2)->kind
                == MMAY_MIR_ADD_PROVEN_I64);
        } else {
            MMAY_EXPECT(mmay_test_integer_mir_at(&function, 2)->kind
                == MMAY_MIR_ADD_CHECKED_I64);
        }
        MMAY_EXPECT(state.live_allocations == baseline_allocations);
        MMAY_EXPECT(state.live_bytes == baseline_bytes);
        mmay_mir_function_dispose(&function);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_integer_verifier_allocation_sweeps(
    MarimaySymbolId symbol
) {
    size_t failure_offset = 0;
    bool mir_success = false;
    bool bytecode_success = false;
    for (failure_offset = 0; failure_offset < 64 && !mir_success;
        failure_offset += 1) {
        MarimayFailAllocator state;
        MarimayMirFunction function;
        MarimayMirVerifyStatus status;
        size_t baseline_allocations = 0;
        size_t baseline_bytes = 0;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_test_integer_build_safe(
            &function, mmay_fail_allocator(&state), symbol
        ));
        MMAY_EXPECT(mmay_mir_optimize_ranges(&function)
            == MMAY_MIR_RANGE_OPTIMIZE_OK);
        baseline_allocations = state.live_allocations;
        baseline_bytes = state.live_bytes;
        state.failure_index = state.attempts + failure_offset;
        status = mmay_mir_verify(&function);
        MMAY_EXPECT(status == MMAY_MIR_VERIFY_OK
            || status == MMAY_MIR_VERIFY_ALLOCATION_FAILED);
        mir_success = status == MMAY_MIR_VERIFY_OK;
        MMAY_EXPECT(state.live_allocations == baseline_allocations);
        MMAY_EXPECT(state.live_bytes == baseline_bytes);
        mmay_mir_function_dispose(&function);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(mir_success);

    for (failure_offset = 0; failure_offset < 64 && !bytecode_success;
        failure_offset += 1) {
        MarimayFailAllocator state;
        MarimayMirFunction mir;
        MarimayBytecodeFunction function;
        MarimayBytecodeVerifyStatus status;
        size_t baseline_allocations = 0;
        size_t baseline_bytes = 0;
        memset(&function, 0, sizeof(function));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_test_integer_build_safe(
            &mir, mmay_system_allocator(), symbol
        ));
        MMAY_EXPECT(mmay_mir_optimize_ranges(&mir)
            == MMAY_MIR_RANGE_OPTIMIZE_OK);
        MMAY_EXPECT(mmay_mir_lower_bytecode_function(
            &mir, mmay_fail_allocator(&state),
            mmay_mir_bytecode_lower_default_options(), &function
        ) == MMAY_MIR_BYTECODE_LOWER_OK);
        baseline_allocations = state.live_allocations;
        baseline_bytes = state.live_bytes;
        state.failure_index = state.attempts + failure_offset;
        status = mmay_bytecode_verify(&function);
        MMAY_EXPECT(status == MMAY_BYTECODE_VERIFY_OK
            || status == MMAY_BYTECODE_VERIFY_ALLOCATION_FAILED);
        bytecode_success = status == MMAY_BYTECODE_VERIFY_OK;
        MMAY_EXPECT(state.live_allocations == baseline_allocations);
        MMAY_EXPECT(state.live_bytes == baseline_bytes);
        mmay_bytecode_function_dispose(&function);
        mmay_mir_function_dispose(&mir);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(bytecode_success);
}

void mmay_test_mir_integer_range(void) {
    MarimaySymbolId symbol = {{0x62, 0x27}};
    mmay_test_integer_safe_vertical(symbol);
    mmay_test_integer_overflow_retained(symbol);
    mmay_test_integer_unreachable_checked_fallback(symbol);
    mmay_test_integer_optimizer_allocation_fallback(symbol);
    mmay_test_integer_verifier_allocation_sweeps(symbol);
}
