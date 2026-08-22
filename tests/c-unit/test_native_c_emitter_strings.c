#include "test_native_c_emitter_support.h"

#include <string.h>

static bool mmay_test_native_build_string_return(
    MarimayMirFunction *function,
    MarimaySymbolId symbol
) {
    static const unsigned char value[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirStringId constant = MMAY_MIR_NO_STRING;
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(function, mmay_system_allocator(), symbol, NULL,
            0, MMAY_TYPE_STRING, mmay_mir_function_default_options())
        || !mmay_mir_add_register(function, MMAY_TYPE_STRING, &result)
        || !mmay_mir_add_block(function, &block)
        || !mmay_mir_add_string_constant(function, value, sizeof(value),
            &constant)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_native_instruction(MMAY_MIR_CONSTANT_STRING, result,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0, false, 400);
    instruction.string_constant = constant;
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(function, block,
            mmay_test_native_terminator(MMAY_MIR_RETURN, result,
                MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 401))) {
        mmay_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool mmay_test_native_build_string_equal(
    MarimayMirFunction *function,
    MarimaySymbolId symbol
) {
    static const unsigned char value[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    MarimayMirRegister left = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister right = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirStringId first = MMAY_MIR_NO_STRING;
    MarimayMirStringId second = MMAY_MIR_NO_STRING;
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(function, mmay_system_allocator(), symbol, NULL,
            0, MMAY_TYPE_BOOL, mmay_mir_function_default_options())
        || !mmay_mir_add_register(function, MMAY_TYPE_STRING, &left)
        || !mmay_mir_add_register(function, MMAY_TYPE_STRING, &right)
        || !mmay_mir_add_register(function, MMAY_TYPE_BOOL, &result)
        || !mmay_mir_add_block(function, &block)
        || !mmay_mir_add_string_constant(function, value, sizeof(value), &first)
        || !mmay_mir_add_string_constant(function, value, sizeof(value), &second)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_native_instruction(MMAY_MIR_CONSTANT_STRING, left,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0, false, 410);
    instruction.string_constant = first;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(MMAY_MIR_CONSTANT_STRING, right,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0, false, 411);
    instruction.string_constant = second;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(MMAY_MIR_EQUAL_STRING, result,
        left, right, 0, false, 412);
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(function, block,
            mmay_test_native_terminator(MMAY_MIR_RETURN, result,
                MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 413))) {
        goto failure;
    }
    return true;

failure:
    mmay_mir_function_dispose(function);
    return false;
}

bool mmay_test_native_build_panic_leaf(
    MarimayMirFunction *function,
    MarimaySymbolId symbol
) {
    static const unsigned char message[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirStringId constant = MMAY_MIR_NO_STRING;
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(function, mmay_system_allocator(), symbol, NULL,
            0, MMAY_TYPE_NEVER, mmay_mir_function_default_options())
        || !mmay_mir_add_register(function, MMAY_TYPE_STRING, &value)
        || !mmay_mir_add_block(function, &block)
        || !mmay_mir_add_string_constant(function, message, sizeof(message),
            &constant)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_native_instruction(MMAY_MIR_CONSTANT_STRING, value,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0, false, 200);
    instruction.string_constant = constant;
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(function, block,
            mmay_test_native_terminator(MMAY_MIR_PANIC, value,
                MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 201))) {
        mmay_mir_function_dispose(function);
        return false;
    }
    return true;
}


static bool mmay_test_native_build_never_entry(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    MarimaySymbolId callee
) {
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirCallId call = MMAY_MIR_NO_CALL;
    MarimayMirTerminator terminator;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(function, mmay_system_allocator(), symbol, NULL,
            0, MMAY_TYPE_NEVER, mmay_mir_function_default_options())
        || !mmay_mir_add_block(function, &block)
        || !mmay_mir_add_call(function, callee, MMAY_TYPE_NEVER, NULL, 0,
            &call)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    terminator = mmay_test_native_terminator(MMAY_MIR_CALL_NEVER,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 210);
    terminator.call = call;
    if (!mmay_mir_set_terminator(function, block, terminator)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    return true;
}

static void mmay_test_native_strings_and_panic(void) {
    static const unsigned char value[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    const MarimaySymbolId return_symbol = {{0x40}};
    const MarimaySymbolId equal_symbol = {{0x41}};
    const MarimaySymbolId leaf_symbol = {{0x10}};
    const MarimaySymbolId entry_symbol = {{0x20}};
    MarimayMirFunction returned;
    MarimayMirFunction equal;
    MarimayMirFunction leaf;
    MarimayMirFunction entry;
    const MarimayMirFunction *one_function[1];
    const MarimayMirFunction *ordered[2];
    const MarimayMirFunction *permuted[2];
    MarimayNativeCProgram native_program;
    MarimayOwnedBytes first;
    MarimayOwnedBytes second;
    MarimayBytecodeFunction bytecode;
    MarimayBytecodeProgram program;
    MarimayBytecodeExecution execution;
    MarimayBytecodePanic panic;
    MarimayBytecodeRunOptions run_options;
    MarimayBytecodeValue result;
    const unsigned char *bytes = NULL;
    size_t length = 0;
    size_t failure_index = 0;
    bool reached_success = false;

    memset(&returned, 0, sizeof(returned));
    memset(&equal, 0, sizeof(equal));
    memset(&leaf, 0, sizeof(leaf));
    memset(&entry, 0, sizeof(entry));
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    memset(&bytecode, 0, sizeof(bytecode));
    memset(&program, 0, sizeof(program));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    MMAY_EXPECT(mmay_test_native_build_string_return(&returned, return_symbol));
    MMAY_EXPECT(mmay_test_native_build_string_equal(&equal, equal_symbol));
    MMAY_EXPECT(mmay_test_native_build_panic_leaf(&leaf, leaf_symbol));
    MMAY_EXPECT(mmay_test_native_build_never_entry(&entry, entry_symbol,
        leaf_symbol));
    MMAY_EXPECT(mmay_mir_verify(&returned) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_verify(&equal) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_verify(&leaf) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_verify(&entry) == MMAY_MIR_VERIFY_OK);

    one_function[0] = &returned;
    native_program.functions = one_function;
    native_program.function_count = 1;
    native_program.entry_symbol = return_symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(&native_program,
        mmay_system_allocator(), mmay_native_c_emit_default_options(), &first)
        == MMAY_NATIVE_C_EMIT_OK);
    MMAY_EXPECT(mmay_native_c_emit_program(&native_program,
        mmay_system_allocator(), mmay_native_c_emit_default_options(), &second)
        == MMAY_NATIVE_C_EMIT_OK);
    MMAY_EXPECT(first.length != 0 && first.length == second.length
        && memcmp(first.data, second.data, first.length) == 0);
    MMAY_EXPECT(mmay_test_native_contains(&first, "mmay_g_string"));
#if !defined(_WIN32)
    mmay_test_native_compile_and_run(&first, "ok:string:6:700021e29883\n", 0);
#endif
    mmay_owned_bytes_dispose(&second);
    mmay_owned_bytes_dispose(&first);

    MMAY_EXPECT(mmay_mir_lower_bytecode_function(&returned,
        mmay_system_allocator(), mmay_mir_bytecode_lower_default_options(),
        &bytecode) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(&execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()));
    result.type = MMAY_BYTECODE_TYPE_I64;
    result.as.i64 = INT64_MIN;
    run_options = mmay_bytecode_run_default_options();
    run_options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run(&bytecode, NULL, 0, mmay_system_allocator(),
        run_options, &result) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_STRING
        && mmay_bytecode_execution_string_bytes(&execution, result, &bytes, &length)
        && length == sizeof(value) && memcmp(bytes, value, sizeof(value)) == 0);
    mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_function_dispose(&bytecode);

    one_function[0] = &equal;
    native_program.entry_symbol = equal_symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(&native_program,
        mmay_system_allocator(), mmay_native_c_emit_default_options(), &first)
        == MMAY_NATIVE_C_EMIT_OK);
    MMAY_EXPECT(mmay_test_native_contains(&first, "mmay_g_string_equal"));
#if !defined(_WIN32)
    mmay_test_native_compile_and_run(&first, "ok:bool:true\n", 0);
#endif
    mmay_owned_bytes_dispose(&first);
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(&equal, mmay_system_allocator(),
        mmay_mir_bytecode_lower_default_options(), &bytecode)
        == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(&execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()));
    result.type = MMAY_BYTECODE_TYPE_I64;
    result.as.i64 = INT64_MIN;
    run_options = mmay_bytecode_run_default_options();
    run_options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run(&bytecode, NULL, 0, mmay_system_allocator(),
        run_options, &result) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_BOOL && result.as.boolean);
    mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_function_dispose(&bytecode);

    ordered[0] = &leaf;
    ordered[1] = &entry;
    permuted[0] = &entry;
    permuted[1] = &leaf;
    native_program.functions = ordered;
    native_program.function_count = 2;
    native_program.entry_symbol = entry_symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(&native_program,
        mmay_system_allocator(), mmay_native_c_emit_default_options(), &first)
        == MMAY_NATIVE_C_EMIT_OK);
    native_program.functions = permuted;
    MMAY_EXPECT(mmay_native_c_emit_program(&native_program,
        mmay_system_allocator(), mmay_native_c_emit_default_options(), &second)
        == MMAY_NATIVE_C_EMIT_OK);
    MMAY_EXPECT(first.length == second.length
        && memcmp(first.data, second.data, first.length) == 0);
#if !defined(_WIN32)
    mmay_test_native_compile_and_run(&first,
        "panic:G4000:10000000000000000000000000000000:201:202:6:700021e29883:2:"
        "10000000000000000000000000000000:201:202:"
        "20000000000000000000000000000000:210:211\n", 1);
#endif
    mmay_owned_bytes_dispose(&second);
    mmay_owned_bytes_dispose(&first);

    MMAY_EXPECT(mmay_mir_lower_bytecode_program(permuted, 2, entry_symbol,
        mmay_system_allocator(), mmay_mir_bytecode_program_lower_default_options(),
        &program) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(&execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()));
    MMAY_EXPECT(mmay_bytecode_panic_init(&panic, mmay_system_allocator()));
    run_options = mmay_bytecode_run_default_options();
    run_options.execution = &execution;
    run_options.panic = &panic;
    result.type = MMAY_BYTECODE_TYPE_I64;
    result.as.i64 = 1234;
    MMAY_EXPECT(mmay_bytecode_run_program(&program, NULL, 0,
        mmay_system_allocator(), run_options, &result) == MMAY_BYTECODE_RUN_PANIC);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
    MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4000") == 0
        && panic.message.length == sizeof(value)
        && memcmp(panic.message.data, value, sizeof(value)) == 0
        && memcmp(&panic.symbol, &leaf_symbol, sizeof(leaf_symbol)) == 0
        && panic.span.start == 201 && panic.span.end == 202
        && panic.frames.length == 2);
    {
        const MarimayBytecodePanicFrame *leaf_frame = mmay_bytecode_panic_frame(
            &panic, 0
        );
        const MarimayBytecodePanicFrame *entry_frame = mmay_bytecode_panic_frame(
            &panic, 1
        );
        MMAY_EXPECT(leaf_frame != NULL && entry_frame != NULL
            && memcmp(&leaf_frame->symbol, &leaf_symbol, sizeof(leaf_symbol)) == 0
            && leaf_frame->call_span.start == 201 && leaf_frame->call_span.end == 202
            && memcmp(&entry_frame->symbol, &entry_symbol, sizeof(entry_symbol)) == 0
            && entry_frame->call_span.start == 210
            && entry_frame->call_span.end == 211);
    }
    mmay_bytecode_panic_dispose(&panic);
    mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_program_dispose(&program);

    {
        MarimayNativeCEmitOptions options = mmay_native_c_emit_default_options();
        options.max_functions = 1;
        mmay_test_native_emit(ordered, 2, entry_symbol, options,
            MMAY_NATIVE_C_EMIT_LIMIT_REACHED);
        options = mmay_native_c_emit_default_options();
        options.max_generated_bytes = 1;
        mmay_test_native_emit(ordered, 2, entry_symbol, options,
            MMAY_NATIVE_C_EMIT_LIMIT_REACHED);
    }
    for (failure_index = 0; failure_index < 128 && !reached_success;
        failure_index += 1) {
        MarimayFailAllocator state;
        MarimayOwnedBytes output;
        MarimayNativeCEmitStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        memset(&output, 0, sizeof(output));
        status = mmay_native_c_emit_program(&native_program,
            mmay_fail_allocator(&state), mmay_native_c_emit_default_options(),
            &output);
        MMAY_EXPECT(status == MMAY_NATIVE_C_EMIT_ALLOCATION_FAILED
            || status == MMAY_NATIVE_C_EMIT_OK);
        reached_success = reached_success || status == MMAY_NATIVE_C_EMIT_OK;
        mmay_owned_bytes_dispose(&output);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
    mmay_mir_function_dispose(&entry);
    mmay_mir_function_dispose(&leaf);
    mmay_mir_function_dispose(&equal);
    mmay_mir_function_dispose(&returned);
}

void mmay_test_native_strings_panic(void) {
    mmay_test_native_strings_and_panic();
}
