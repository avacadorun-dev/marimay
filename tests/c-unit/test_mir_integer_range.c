#include "test_mir_support.h"
#include "test_native_c_emitter_support.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/bytecode/codec.h"
#include "ruda/bytecode/mir_lower.h"
#include "ruda/bytecode/vm.h"
#include "ruda/mir/range.h"
#include "ruda/native/c_emitter.h"

static RudaMirInstruction *ruda_test_integer_mir_at(
    RudaMirFunction *function,
    size_t index
) {
    RudaMirBlock *block = function != NULL
        ? ruda_vector_at(&function->blocks, 0) : NULL;
    return block != NULL ? ruda_vector_at(&block->instructions, index) : NULL;
}

static RudaBytecodeInstruction *ruda_test_integer_bytecode_at(
    RudaBytecodeFunction *function,
    size_t index
) {
    RudaBytecodeBlock *block = function != NULL
        ? ruda_vector_at(&function->blocks, 0) : NULL;
    return block != NULL ? ruda_vector_at(&block->instructions, index) : NULL;
}

static bool ruda_test_integer_build_safe(
    RudaMirFunction *function,
    RudaAllocator allocator,
    RudaSymbolId symbol
) {
    RudaMirRegister registers[5];
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    size_t index = 0;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(
            function, allocator, symbol, NULL, 0, RUDA_TYPE_I64,
            ruda_mir_function_default_options()
        )) {
        return false;
    }
    for (index = 0; index < 5; index += 1) {
        registers[index] = RUDA_MIR_NO_REGISTER;
        if (!ruda_mir_add_register(
                function, RUDA_TYPE_I64, &registers[index]
            ) || registers[index] != index) {
            goto failure;
        }
    }
    if (!ruda_mir_add_block(function, &block)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_I64, 0, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 7, false, 7000
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_I64, 1, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 3, false, 7001
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ADD_CHECKED_I64, 2, 0, 1, 0, false, 7002
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_SUBTRACT_CHECKED_I64, 3, 2, 1, 0, false, 7003
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_MULTIPLY_CHECKED_I64, 4, 3, 1, 0, false, 7004
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(
            function, block,
            ruda_test_native_terminator(
                RUDA_MIR_RETURN, 4, RUDA_MIR_NO_BLOCK,
                RUDA_MIR_NO_BLOCK, 7005
            )
        )) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}

static bool ruda_test_integer_build_overflow(
    RudaMirFunction *function,
    RudaAllocator allocator,
    RudaSymbolId symbol
) {
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    size_t index = 0;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(
            function, allocator, symbol, NULL, 0, RUDA_TYPE_I64,
            ruda_mir_function_default_options()
        )) {
        return false;
    }
    for (index = 0; index < 3; index += 1) {
        if (!ruda_mir_add_register(function, RUDA_TYPE_I64, &value)
            || value != index) {
            goto failure;
        }
    }
    if (!ruda_mir_add_block(function, &block)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_I64, 0, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, INT64_MAX, false, 30
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_I64, 1, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 1, false, 31
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ADD_CHECKED_I64, 2, 0, 1, 0, false, 32
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(
            function, block,
            ruda_test_native_terminator(
                RUDA_MIR_RETURN, 2, RUDA_MIR_NO_BLOCK,
                RUDA_MIR_NO_BLOCK, 33
            )
        )) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}

static void ruda_test_integer_safe_vertical(RudaSymbolId symbol) {
    RudaMirFunction function;
    RudaMirValue mir_result;
    RudaBytecodeFunction lowered;
    RudaBytecodeFunction decoded;
    RudaBytecodeValue bytecode_result;
    RudaOwnedBytes encoded;
    RudaOwnedBytes generated;
    RudaNativeCProgram program;
    const RudaMirFunction *functions[1];
    RudaMirInstruction *first_constant = NULL;
    RudaBytecodeInstruction *bytecode_constant = NULL;
    memset(&lowered, 0, sizeof(lowered));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&generated, 0, sizeof(generated));
    memset(&program, 0, sizeof(program));
    memset(&mir_result, 0, sizeof(mir_result));
    memset(&bytecode_result, 0, sizeof(bytecode_result));
    RUDA_EXPECT(ruda_test_integer_build_safe(
        &function, ruda_system_allocator(), symbol
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_interpret(
        &function, NULL, 0, ruda_system_allocator(),
        ruda_mir_interpret_default_options(), &mir_result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(mir_result.type == RUDA_TYPE_I64
        && mir_result.as.i64 == 21);
    RUDA_EXPECT(ruda_mir_optimize_ranges(&function)
        == RUDA_MIR_RANGE_OPTIMIZE_OK);
    RUDA_EXPECT(ruda_test_integer_mir_at(&function, 2)->kind
        == RUDA_MIR_ADD_PROVEN_I64);
    RUDA_EXPECT(ruda_test_integer_mir_at(&function, 3)->kind
        == RUDA_MIR_SUBTRACT_PROVEN_I64);
    RUDA_EXPECT(ruda_test_integer_mir_at(&function, 4)->kind
        == RUDA_MIR_MULTIPLY_PROVEN_I64);
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_interpret(
        &function, NULL, 0, ruda_system_allocator(),
        ruda_mir_interpret_default_options(), &mir_result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(mir_result.as.i64 == 21);

    RUDA_EXPECT(ruda_mir_lower_bytecode_function(
        &function, ruda_system_allocator(),
        ruda_mir_bytecode_lower_default_options(), &lowered
    ) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_test_integer_bytecode_at(&lowered, 2)->opcode
        == RUDA_OPCODE_I64_ADD_PROVEN);
    RUDA_EXPECT(ruda_test_integer_bytecode_at(&lowered, 3)->opcode
        == RUDA_OPCODE_I64_SUBTRACT_PROVEN);
    RUDA_EXPECT(ruda_test_integer_bytecode_at(&lowered, 4)->opcode
        == RUDA_OPCODE_I64_MULTIPLY_PROVEN);
    RUDA_EXPECT(ruda_bytecode_verify(&lowered) == RUDA_BYTECODE_VERIFY_OK);
    RUDA_EXPECT(ruda_bytecode_encode_function(
        &lowered, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_decode_function(
        encoded.data, encoded.length, ruda_system_allocator(),
        ruda_bytecode_function_default_options(), &decoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_verify(&decoded) == RUDA_BYTECODE_VERIFY_OK);
    RUDA_EXPECT(ruda_bytecode_run(
        &decoded, NULL, 0, ruda_system_allocator(),
        ruda_bytecode_run_default_options(), &bytecode_result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(bytecode_result.type == RUDA_BYTECODE_TYPE_I64
        && bytecode_result.as.i64 == 21);

    functions[0] = &function;
    program.functions = functions;
    program.function_count = 1;
    program.entry_symbol = symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(
        &program, ruda_system_allocator(),
        ruda_native_c_emit_default_options(), &generated
    ) == RUDA_NATIVE_C_EMIT_OK);
    RUDA_EXPECT(ruda_test_native_contains(
        &generated, "ruda_i64_add_proven"
    ));
    RUDA_EXPECT(ruda_test_native_contains(
        &generated, "ruda_i64_subtract_proven"
    ));
    RUDA_EXPECT(ruda_test_native_contains(
        &generated, "ruda_i64_multiply_proven"
    ));
#if !defined(_WIN32)
    ruda_test_native_compile_and_run(&generated, "ok:i64:21\n", 0);
#endif

    first_constant = ruda_test_integer_mir_at(&function, 0);
    RUDA_EXPECT(first_constant != NULL);
    if (first_constant != NULL) {
        first_constant->integer = INT64_MAX;
        RUDA_EXPECT(ruda_mir_verify(&function)
            == RUDA_MIR_VERIFY_INVALID_ARGUMENT);
        first_constant->integer = 7;
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    }
    bytecode_constant = ruda_test_integer_bytecode_at(&decoded, 0);
    RUDA_EXPECT(bytecode_constant != NULL);
    if (bytecode_constant != NULL) {
        bytecode_constant->operands[1].signed_value = INT64_MAX;
        RUDA_EXPECT(ruda_bytecode_verify(&decoded)
            == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
        bytecode_constant->operands[1].signed_value = 7;
        RUDA_EXPECT(ruda_bytecode_verify(&decoded)
            == RUDA_BYTECODE_VERIFY_OK);
    }

    ruda_owned_bytes_dispose(&generated);
    ruda_bytecode_function_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_function_dispose(&lowered);
    ruda_mir_function_dispose(&function);
}

static void ruda_test_integer_overflow_retained(RudaSymbolId symbol) {
    RudaMirFunction function;
    RudaMirValue result;
    RudaOwnedBytes generated;
    RudaNativeCProgram program;
    const RudaMirFunction *functions[1];
    RudaMirInstruction *addition = NULL;
    memset(&result, 0, sizeof(result));
    memset(&generated, 0, sizeof(generated));
    memset(&program, 0, sizeof(program));
    RUDA_EXPECT(ruda_test_integer_build_overflow(
        &function, ruda_system_allocator(), symbol
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_optimize_ranges(&function)
        == RUDA_MIR_RANGE_OPTIMIZE_OK);
    addition = ruda_test_integer_mir_at(&function, 2);
    RUDA_EXPECT(addition != NULL
        && addition->kind == RUDA_MIR_ADD_CHECKED_I64);
    RUDA_EXPECT(ruda_mir_interpret(
        &function, NULL, 0, ruda_system_allocator(),
        ruda_mir_interpret_default_options(), &result
    ) == RUDA_MIR_INTERPRET_OVERFLOW);
    ruda_test_native_vm_overflow(&function);
    functions[0] = &function;
    program.functions = functions;
    program.function_count = 1;
    program.entry_symbol = symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(
        &program, ruda_system_allocator(),
        ruda_native_c_emit_default_options(), &generated
    ) == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    ruda_test_native_compile_and_run(&generated, "trap:1:32:33\n", 1);
#endif
    if (addition != NULL) {
        addition->kind = RUDA_MIR_ADD_PROVEN_I64;
        RUDA_EXPECT(ruda_mir_verify(&function)
            == RUDA_MIR_VERIFY_INVALID_ARGUMENT);
    }
    ruda_owned_bytes_dispose(&generated);
    ruda_mir_function_dispose(&function);
}

static void ruda_test_integer_unreachable_checked_fallback(
    RudaSymbolId symbol
) {
    RudaMirFunction function;
    RudaMirRegister register_id = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    size_t index = 0;
    RUDA_EXPECT(ruda_test_integer_build_safe(
        &function, ruda_system_allocator(), symbol
    ));
    for (index = 0; index < 3; index += 1) {
        RUDA_EXPECT(ruda_mir_add_register(
            &function, RUDA_TYPE_I64, &register_id
        ));
        RUDA_EXPECT(register_id == 5 + index);
    }
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_I64, 5, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, INT64_MAX, false, 7100
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_I64, 6, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 1, false, 7101
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ADD_CHECKED_I64, 7, 5, 6, 0, false, 7102
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function, block,
        ruda_test_native_terminator(
            RUDA_MIR_RETURN, 7, RUDA_MIR_NO_BLOCK,
            RUDA_MIR_NO_BLOCK, 7103
        )
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_optimize_ranges(&function)
        == RUDA_MIR_RANGE_OPTIMIZE_OK);
    RUDA_EXPECT(ruda_test_integer_mir_at(&function, 2)->kind
        == RUDA_MIR_ADD_PROVEN_I64);
    {
        RudaMirBlock *unreachable = ruda_vector_at(&function.blocks, block);
        RudaMirInstruction *addition = unreachable != NULL
            ? ruda_vector_at(&unreachable->instructions, 2) : NULL;
        RUDA_EXPECT(addition != NULL
            && addition->kind == RUDA_MIR_ADD_CHECKED_I64);
        if (addition != NULL) {
            addition->kind = RUDA_MIR_ADD_PROVEN_I64;
            RUDA_EXPECT(ruda_mir_verify(&function)
                == RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW);
            addition->kind = RUDA_MIR_ADD_CHECKED_I64;
        }
    }
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    ruda_mir_function_dispose(&function);
}

static void ruda_test_integer_optimizer_allocation_fallback(
    RudaSymbolId symbol
) {
    size_t failure_offset = 0;
    bool reached_success = false;
    for (failure_offset = 0; failure_offset < 64 && !reached_success;
        failure_offset += 1) {
        RudaFailAllocator state;
        RudaMirFunction function;
        RudaMirRangeOptimizeStatus status;
        size_t baseline_allocations = 0;
        size_t baseline_bytes = 0;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        RUDA_EXPECT(ruda_test_integer_build_safe(
            &function, ruda_fail_allocator(&state), symbol
        ));
        baseline_allocations = state.live_allocations;
        baseline_bytes = state.live_bytes;
        state.failure_index = state.attempts + failure_offset;
        status = ruda_mir_optimize_ranges(&function);
        RUDA_EXPECT(status == RUDA_MIR_RANGE_OPTIMIZE_OK
            || status == RUDA_MIR_RANGE_OPTIMIZE_ALLOCATION_FAILED);
        reached_success = status == RUDA_MIR_RANGE_OPTIMIZE_OK;
        if (reached_success) {
            RUDA_EXPECT(ruda_test_integer_mir_at(&function, 2)->kind
                == RUDA_MIR_ADD_PROVEN_I64);
        } else {
            RUDA_EXPECT(ruda_test_integer_mir_at(&function, 2)->kind
                == RUDA_MIR_ADD_CHECKED_I64);
        }
        RUDA_EXPECT(state.live_allocations == baseline_allocations);
        RUDA_EXPECT(state.live_bytes == baseline_bytes);
        ruda_mir_function_dispose(&function);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_integer_verifier_allocation_sweeps(
    RudaSymbolId symbol
) {
    size_t failure_offset = 0;
    bool mir_success = false;
    bool bytecode_success = false;
    for (failure_offset = 0; failure_offset < 64 && !mir_success;
        failure_offset += 1) {
        RudaFailAllocator state;
        RudaMirFunction function;
        RudaMirVerifyStatus status;
        size_t baseline_allocations = 0;
        size_t baseline_bytes = 0;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        RUDA_EXPECT(ruda_test_integer_build_safe(
            &function, ruda_fail_allocator(&state), symbol
        ));
        RUDA_EXPECT(ruda_mir_optimize_ranges(&function)
            == RUDA_MIR_RANGE_OPTIMIZE_OK);
        baseline_allocations = state.live_allocations;
        baseline_bytes = state.live_bytes;
        state.failure_index = state.attempts + failure_offset;
        status = ruda_mir_verify(&function);
        RUDA_EXPECT(status == RUDA_MIR_VERIFY_OK
            || status == RUDA_MIR_VERIFY_ALLOCATION_FAILED);
        mir_success = status == RUDA_MIR_VERIFY_OK;
        RUDA_EXPECT(state.live_allocations == baseline_allocations);
        RUDA_EXPECT(state.live_bytes == baseline_bytes);
        ruda_mir_function_dispose(&function);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(mir_success);

    for (failure_offset = 0; failure_offset < 64 && !bytecode_success;
        failure_offset += 1) {
        RudaFailAllocator state;
        RudaMirFunction mir;
        RudaBytecodeFunction function;
        RudaBytecodeVerifyStatus status;
        size_t baseline_allocations = 0;
        size_t baseline_bytes = 0;
        memset(&function, 0, sizeof(function));
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        RUDA_EXPECT(ruda_test_integer_build_safe(
            &mir, ruda_system_allocator(), symbol
        ));
        RUDA_EXPECT(ruda_mir_optimize_ranges(&mir)
            == RUDA_MIR_RANGE_OPTIMIZE_OK);
        RUDA_EXPECT(ruda_mir_lower_bytecode_function(
            &mir, ruda_fail_allocator(&state),
            ruda_mir_bytecode_lower_default_options(), &function
        ) == RUDA_MIR_BYTECODE_LOWER_OK);
        baseline_allocations = state.live_allocations;
        baseline_bytes = state.live_bytes;
        state.failure_index = state.attempts + failure_offset;
        status = ruda_bytecode_verify(&function);
        RUDA_EXPECT(status == RUDA_BYTECODE_VERIFY_OK
            || status == RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED);
        bytecode_success = status == RUDA_BYTECODE_VERIFY_OK;
        RUDA_EXPECT(state.live_allocations == baseline_allocations);
        RUDA_EXPECT(state.live_bytes == baseline_bytes);
        ruda_bytecode_function_dispose(&function);
        ruda_mir_function_dispose(&mir);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(bytecode_success);
}

void ruda_test_mir_integer_range(void) {
    RudaSymbolId symbol = {{0x62, 0x27}};
    ruda_test_integer_safe_vertical(symbol);
    ruda_test_integer_overflow_retained(symbol);
    ruda_test_integer_unreachable_checked_fallback(symbol);
    ruda_test_integer_optimizer_allocation_fallback(symbol);
    ruda_test_integer_verifier_allocation_sweeps(symbol);
}
