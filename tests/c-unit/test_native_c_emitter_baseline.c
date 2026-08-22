#include "test_native_c_emitter_support.h"

#include <string.h>

static bool mmay_test_native_build_leaf(
    MarimayMirFunction *function,
    MarimaySymbolId symbol
) {
    MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(function, mmay_system_allocator(), symbol, NULL,
            0, MMAY_TYPE_I64, mmay_mir_function_default_options())
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &value)
        || !mmay_mir_add_block(function, &block)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_native_instruction(MMAY_MIR_CONSTANT_I64, value,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 42, false, 10);
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(function, block,
            mmay_test_native_terminator(MMAY_MIR_RETURN, value,
                MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 11))) {
        mmay_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool mmay_test_native_build_entry(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    MarimaySymbolId callee,
    bool branch_value
) {
    MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId entry = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId yes = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId no = MMAY_MIR_NO_BLOCK;
    MarimayMirCallId call = MMAY_MIR_NO_CALL;
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(function, mmay_system_allocator(), symbol, NULL,
            0, MMAY_TYPE_I64, mmay_mir_function_default_options())
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &value)
        || !mmay_mir_add_register(function, MMAY_TYPE_BOOL, &value)
        || !mmay_mir_add_block(function, &entry)
        || !mmay_mir_add_block(function, &yes)
        || !mmay_mir_add_block(function, &no)
        || !mmay_mir_add_call(function, callee, MMAY_TYPE_I64, NULL, 0, &call)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_native_instruction(MMAY_MIR_CONSTANT_BOOL, 1,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0, branch_value, 20);
    if (!mmay_mir_append_instruction(function, entry, &instruction)
        || !mmay_mir_set_terminator(function, entry,
            mmay_test_native_terminator(MMAY_MIR_BRANCH, 1, yes, no, 21))) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(MMAY_MIR_CALL, 0,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0, false, 22);
    instruction.call = call;
    if (!mmay_mir_append_instruction(function, yes, &instruction)
        || !mmay_mir_set_terminator(function, yes,
            mmay_test_native_terminator(MMAY_MIR_RETURN, 0,
                MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 23))) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(MMAY_MIR_CONSTANT_I64, 0,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0, false, 24);
    if (!mmay_mir_append_instruction(function, no, &instruction)
        || !mmay_mir_set_terminator(function, no,
            mmay_test_native_terminator(MMAY_MIR_RETURN, 0,
                MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 25))) {
        goto failure;
    }
    return true;

failure:
    mmay_mir_function_dispose(function);
    return false;
}

static bool mmay_test_native_build_overflow(
    MarimayMirFunction *function,
    MarimaySymbolId symbol
) {
    MarimayMirRegister left = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister right = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(function, mmay_system_allocator(), symbol, NULL,
            0, MMAY_TYPE_I64, mmay_mir_function_default_options())
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &left)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &right)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &result)
        || !mmay_mir_add_block(function, &block)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_native_instruction(MMAY_MIR_CONSTANT_I64, left,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, INT64_MAX, false, 30);
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(MMAY_MIR_CONSTANT_I64, right,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 1, false, 31);
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(MMAY_MIR_ADD_CHECKED_I64, result,
        left, right, 0, false, 32);
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(function, block,
            mmay_test_native_terminator(MMAY_MIR_RETURN, result,
                MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 33))) {
        goto failure;
    }
    return true;

failure:
    mmay_mir_function_dispose(function);
    return false;
}


void mmay_test_native_baseline(void) {
    const MarimaySymbolId leaf_symbol = {{0x10}};
    const MarimaySymbolId entry_symbol = {{0x20}};
    MarimayMirFunction leaf;
    MarimayMirFunction entry;
    const MarimayMirFunction *ordered[2];
    const MarimayMirFunction *permuted[2];
    MarimayNativeCProgram program;
    MarimayNativeCEmitOptions options = mmay_native_c_emit_default_options();
    MarimayOwnedBytes first;
    MarimayOwnedBytes second;
    size_t failure_index = 0;
    bool reached_success = false;

    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    MMAY_EXPECT(mmay_test_native_build_leaf(&leaf, leaf_symbol));
    MMAY_EXPECT(mmay_test_native_build_entry(&entry, entry_symbol, leaf_symbol,
        true));
    MMAY_EXPECT(mmay_mir_verify(&leaf) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_verify(&entry) == MMAY_MIR_VERIFY_OK);
    ordered[0] = &leaf;
    ordered[1] = &entry;
    permuted[0] = &entry;
    permuted[1] = &leaf;
    mmay_test_native_vm_program(permuted, entry_symbol, 42);
    memset(&program, 0, sizeof(program));
    program.functions = ordered;
    program.function_count = 2;
    program.entry_symbol = entry_symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(&program, mmay_system_allocator(),
        options, &first) == MMAY_NATIVE_C_EMIT_OK);
    program.functions = permuted;
    MMAY_EXPECT(mmay_native_c_emit_program(&program, mmay_system_allocator(),
        options, &second) == MMAY_NATIVE_C_EMIT_OK);
    MMAY_EXPECT(first.length != 0 && first.length == second.length
        && memcmp(first.data, second.data, first.length) == 0);
    MMAY_EXPECT(mmay_test_native_contains(&first, "#include <stdint.h>"));
    MMAY_EXPECT(mmay_test_native_contains(&first, "mmay_g_b_"));
    MMAY_EXPECT(mmay_test_native_contains(&first, "mmay_g_f_"));
    MMAY_EXPECT(mmay_test_native_contains(
        &first, "(void)mmay_g_cleanup_p;"
    ));
#if !defined(_WIN32)
    mmay_test_native_compile_and_run(&first, "ok:i64:42\n", 0);
#endif
    mmay_owned_bytes_dispose(&second);
    mmay_owned_bytes_dispose(&first);

    {
        MarimayMirFunction overflow;
        const MarimayMirFunction *overflow_functions[1];
        MarimayOwnedBytes overflow_source;
        memset(&overflow_source, 0, sizeof(overflow_source));
        MMAY_EXPECT(mmay_test_native_build_overflow(&overflow, leaf_symbol));
        mmay_test_native_vm_overflow(&overflow);
        overflow_functions[0] = &overflow;
        program.functions = overflow_functions;
        program.function_count = 1;
        program.entry_symbol = leaf_symbol;
        MMAY_EXPECT(mmay_native_c_emit_program(&program, mmay_system_allocator(),
            mmay_native_c_emit_default_options(), &overflow_source)
            == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
        mmay_test_native_compile_and_run(&overflow_source, "trap:1:32:33\n", 1);
#endif
        mmay_owned_bytes_dispose(&overflow_source);
        mmay_mir_function_dispose(&overflow);
        program.functions = permuted;
        program.function_count = 2;
        program.entry_symbol = entry_symbol;
    }

    {
        MarimayMirFunction false_entry;
        const MarimayMirFunction *false_functions[2];
        MarimayOwnedBytes false_source;
        memset(&false_source, 0, sizeof(false_source));
        MMAY_EXPECT(mmay_test_native_build_entry(&false_entry, entry_symbol,
            leaf_symbol, false));
        false_functions[0] = &false_entry;
        false_functions[1] = &leaf;
        mmay_test_native_vm_program(false_functions, entry_symbol, 0);
        program.functions = false_functions;
        program.function_count = 2;
        program.entry_symbol = entry_symbol;
        MMAY_EXPECT(mmay_native_c_emit_program(&program, mmay_system_allocator(),
            mmay_native_c_emit_default_options(), &false_source)
            == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
        mmay_test_native_compile_and_run(&false_source, "ok:i64:0\n", 0);
#endif
        mmay_owned_bytes_dispose(&false_source);
        mmay_mir_function_dispose(&false_entry);
        program.functions = permuted;
        program.function_count = 2;
        program.entry_symbol = entry_symbol;
    }

    options.max_functions = 1;
    mmay_test_native_emit(ordered, 2, entry_symbol, options,
        MMAY_NATIVE_C_EMIT_LIMIT_REACHED);
    options = mmay_native_c_emit_default_options();
    options.max_generated_bytes = 1;
    mmay_test_native_emit(ordered, 2, entry_symbol, options,
        MMAY_NATIVE_C_EMIT_LIMIT_REACHED);
    options = mmay_native_c_emit_default_options();
    options.max_heap_bytes = 0;
    mmay_test_native_emit(ordered, 2, entry_symbol, options,
        MMAY_NATIVE_C_EMIT_INVALID_ARGUMENT);
    options = mmay_native_c_emit_default_options();
    mmay_test_native_emit(NULL, 0, entry_symbol, options,
        MMAY_NATIVE_C_EMIT_INVALID_ARGUMENT);
    mmay_test_native_emit(permuted, 1, entry_symbol, options,
        MMAY_NATIVE_C_EMIT_UNSUPPORTED_CALL);
    {
        MarimayMirInstruction *malformed = mmay_vector_at(
            &((MarimayMirBlock *)mmay_vector_at(&entry.blocks, 0))->instructions, 0
        );
        MMAY_EXPECT(malformed != NULL);
        if (malformed != NULL) {
            malformed->kind = (MarimayMirInstructionKind)999;
            mmay_test_native_emit(permuted, 2, entry_symbol, options,
                MMAY_NATIVE_C_EMIT_INVALID_MIR);
            malformed->kind = MMAY_MIR_CONSTANT_BOOL;
        }
    }
    {
        MarimayMirCall *call = mmay_vector_at(&entry.calls, 0);
        MMAY_EXPECT(call != NULL);
        if (call != NULL) {
            call->callee = entry_symbol;
            mmay_test_native_emit(permuted, 2, entry_symbol, options,
                MMAY_NATIVE_C_EMIT_UNSUPPORTED_CALL);
            call->callee = leaf_symbol;
        }
    }

    for (failure_index = 0; failure_index < 128 && !reached_success;
        failure_index += 1) {
        MarimayFailAllocator state;
        MarimayOwnedBytes output;
        MarimayNativeCEmitStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        memset(&output, 0, sizeof(output));
        status = mmay_native_c_emit_program(&program, mmay_fail_allocator(&state),
            mmay_native_c_emit_default_options(), &output);
        MMAY_EXPECT(status == MMAY_NATIVE_C_EMIT_ALLOCATION_FAILED
            || status == MMAY_NATIVE_C_EMIT_OK);
        if (status == MMAY_NATIVE_C_EMIT_OK) {
            reached_success = true;
        }
        mmay_owned_bytes_dispose(&output);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
    mmay_mir_function_dispose(&entry);
    mmay_mir_function_dispose(&leaf);
}
