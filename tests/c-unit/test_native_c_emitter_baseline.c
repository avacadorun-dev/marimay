#include "test_native_c_emitter_support.h"

#include <string.h>

static bool ruda_test_native_build_leaf(
    RudaMirFunction *function,
    RudaSymbolId symbol
) {
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_I64, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &value)
        || !ruda_mir_add_block(function, &block)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_I64, value,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 42, false, 10);
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_native_terminator(RUDA_MIR_RETURN, value,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 11))) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_native_build_entry(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    RudaSymbolId callee,
    bool branch_value
) {
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId entry = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId yes = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId no = RUDA_MIR_NO_BLOCK;
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_I64, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &value)
        || !ruda_mir_add_register(function, RUDA_TYPE_BOOL, &value)
        || !ruda_mir_add_block(function, &entry)
        || !ruda_mir_add_block(function, &yes)
        || !ruda_mir_add_block(function, &no)
        || !ruda_mir_add_call(function, callee, RUDA_TYPE_I64, NULL, 0, &call)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_BOOL, 1,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, branch_value, 20);
    if (!ruda_mir_append_instruction(function, entry, &instruction)
        || !ruda_mir_set_terminator(function, entry,
            ruda_test_native_terminator(RUDA_MIR_BRANCH, 1, yes, no, 21))) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CALL, 0,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 22);
    instruction.call = call;
    if (!ruda_mir_append_instruction(function, yes, &instruction)
        || !ruda_mir_set_terminator(function, yes,
            ruda_test_native_terminator(RUDA_MIR_RETURN, 0,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 23))) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_I64, 0,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 24);
    if (!ruda_mir_append_instruction(function, no, &instruction)
        || !ruda_mir_set_terminator(function, no,
            ruda_test_native_terminator(RUDA_MIR_RETURN, 0,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 25))) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}

static bool ruda_test_native_build_overflow(
    RudaMirFunction *function,
    RudaSymbolId symbol
) {
    RudaMirRegister left = RUDA_MIR_NO_REGISTER;
    RudaMirRegister right = RUDA_MIR_NO_REGISTER;
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_I64, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &left)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &right)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &result)
        || !ruda_mir_add_block(function, &block)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_I64, left,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, INT64_MAX, false, 30);
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_I64, right,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 1, false, 31);
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_ADD_CHECKED_I64, result,
        left, right, 0, false, 32);
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_native_terminator(RUDA_MIR_RETURN, result,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 33))) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}


void ruda_test_native_baseline(void) {
    const RudaSymbolId leaf_symbol = {{0x10}};
    const RudaSymbolId entry_symbol = {{0x20}};
    RudaMirFunction leaf;
    RudaMirFunction entry;
    const RudaMirFunction *ordered[2];
    const RudaMirFunction *permuted[2];
    RudaNativeCProgram program;
    RudaNativeCEmitOptions options = ruda_native_c_emit_default_options();
    RudaOwnedBytes first;
    RudaOwnedBytes second;
    size_t failure_index = 0;
    bool reached_success = false;

    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    RUDA_EXPECT(ruda_test_native_build_leaf(&leaf, leaf_symbol));
    RUDA_EXPECT(ruda_test_native_build_entry(&entry, entry_symbol, leaf_symbol,
        true));
    RUDA_EXPECT(ruda_mir_verify(&leaf) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_verify(&entry) == RUDA_MIR_VERIFY_OK);
    ordered[0] = &leaf;
    ordered[1] = &entry;
    permuted[0] = &entry;
    permuted[1] = &leaf;
    ruda_test_native_vm_program(permuted, entry_symbol, 42);
    memset(&program, 0, sizeof(program));
    program.functions = ordered;
    program.function_count = 2;
    program.entry_symbol = entry_symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(&program, ruda_system_allocator(),
        options, &first) == RUDA_NATIVE_C_EMIT_OK);
    program.functions = permuted;
    RUDA_EXPECT(ruda_native_c_emit_program(&program, ruda_system_allocator(),
        options, &second) == RUDA_NATIVE_C_EMIT_OK);
    RUDA_EXPECT(first.length != 0 && first.length == second.length
        && memcmp(first.data, second.data, first.length) == 0);
    RUDA_EXPECT(ruda_test_native_contains(&first, "#include <stdint.h>"));
    RUDA_EXPECT(ruda_test_native_contains(&first, "ruda_g_b_"));
    RUDA_EXPECT(ruda_test_native_contains(&first, "ruda_g_f_"));
    RUDA_EXPECT(ruda_test_native_contains(
        &first, "(void)ruda_g_cleanup_p;"
    ));
#if !defined(_WIN32)
    ruda_test_native_compile_and_run(&first, "ok:i64:42\n", 0);
#endif
    ruda_owned_bytes_dispose(&second);
    ruda_owned_bytes_dispose(&first);

    {
        RudaMirFunction overflow;
        const RudaMirFunction *overflow_functions[1];
        RudaOwnedBytes overflow_source;
        memset(&overflow_source, 0, sizeof(overflow_source));
        RUDA_EXPECT(ruda_test_native_build_overflow(&overflow, leaf_symbol));
        ruda_test_native_vm_overflow(&overflow);
        overflow_functions[0] = &overflow;
        program.functions = overflow_functions;
        program.function_count = 1;
        program.entry_symbol = leaf_symbol;
        RUDA_EXPECT(ruda_native_c_emit_program(&program, ruda_system_allocator(),
            ruda_native_c_emit_default_options(), &overflow_source)
            == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
        ruda_test_native_compile_and_run(&overflow_source, "trap:1:32:33\n", 1);
#endif
        ruda_owned_bytes_dispose(&overflow_source);
        ruda_mir_function_dispose(&overflow);
        program.functions = permuted;
        program.function_count = 2;
        program.entry_symbol = entry_symbol;
    }

    {
        RudaMirFunction false_entry;
        const RudaMirFunction *false_functions[2];
        RudaOwnedBytes false_source;
        memset(&false_source, 0, sizeof(false_source));
        RUDA_EXPECT(ruda_test_native_build_entry(&false_entry, entry_symbol,
            leaf_symbol, false));
        false_functions[0] = &false_entry;
        false_functions[1] = &leaf;
        ruda_test_native_vm_program(false_functions, entry_symbol, 0);
        program.functions = false_functions;
        program.function_count = 2;
        program.entry_symbol = entry_symbol;
        RUDA_EXPECT(ruda_native_c_emit_program(&program, ruda_system_allocator(),
            ruda_native_c_emit_default_options(), &false_source)
            == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
        ruda_test_native_compile_and_run(&false_source, "ok:i64:0\n", 0);
#endif
        ruda_owned_bytes_dispose(&false_source);
        ruda_mir_function_dispose(&false_entry);
        program.functions = permuted;
        program.function_count = 2;
        program.entry_symbol = entry_symbol;
    }

    options.max_functions = 1;
    ruda_test_native_emit(ordered, 2, entry_symbol, options,
        RUDA_NATIVE_C_EMIT_LIMIT_REACHED);
    options = ruda_native_c_emit_default_options();
    options.max_generated_bytes = 1;
    ruda_test_native_emit(ordered, 2, entry_symbol, options,
        RUDA_NATIVE_C_EMIT_LIMIT_REACHED);
    options = ruda_native_c_emit_default_options();
    ruda_test_native_emit(NULL, 0, entry_symbol, options,
        RUDA_NATIVE_C_EMIT_INVALID_ARGUMENT);
    ruda_test_native_emit(permuted, 1, entry_symbol, options,
        RUDA_NATIVE_C_EMIT_UNSUPPORTED_CALL);
    {
        RudaMirInstruction *malformed = ruda_vector_at(
            &((RudaMirBlock *)ruda_vector_at(&entry.blocks, 0))->instructions, 0
        );
        RUDA_EXPECT(malformed != NULL);
        if (malformed != NULL) {
            malformed->kind = (RudaMirInstructionKind)999;
            ruda_test_native_emit(permuted, 2, entry_symbol, options,
                RUDA_NATIVE_C_EMIT_INVALID_MIR);
            malformed->kind = RUDA_MIR_CONSTANT_BOOL;
        }
    }
    {
        RudaMirCall *call = ruda_vector_at(&entry.calls, 0);
        RUDA_EXPECT(call != NULL);
        if (call != NULL) {
            call->callee = entry_symbol;
            ruda_test_native_emit(permuted, 2, entry_symbol, options,
                RUDA_NATIVE_C_EMIT_UNSUPPORTED_CALL);
            call->callee = leaf_symbol;
        }
    }

    for (failure_index = 0; failure_index < 128 && !reached_success;
        failure_index += 1) {
        RudaFailAllocator state;
        RudaOwnedBytes output;
        RudaNativeCEmitStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        memset(&output, 0, sizeof(output));
        status = ruda_native_c_emit_program(&program, ruda_fail_allocator(&state),
            ruda_native_c_emit_default_options(), &output);
        RUDA_EXPECT(status == RUDA_NATIVE_C_EMIT_ALLOCATION_FAILED
            || status == RUDA_NATIVE_C_EMIT_OK);
        if (status == RUDA_NATIVE_C_EMIT_OK) {
            reached_success = true;
        }
        ruda_owned_bytes_dispose(&output);
        RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
    ruda_mir_function_dispose(&entry);
    ruda_mir_function_dispose(&leaf);
}
