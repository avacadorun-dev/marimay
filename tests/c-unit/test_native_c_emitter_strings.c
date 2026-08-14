#include "test_native_c_emitter_support.h"

#include <string.h>

static bool ruda_test_native_build_string_return(
    RudaMirFunction *function,
    RudaSymbolId symbol
) {
    static const unsigned char value[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirStringId constant = RUDA_MIR_NO_STRING;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_STRING, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_STRING, &result)
        || !ruda_mir_add_block(function, &block)
        || !ruda_mir_add_string_constant(function, value, sizeof(value),
            &constant)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_STRING, result,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 400);
    instruction.string_constant = constant;
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_native_terminator(RUDA_MIR_RETURN, result,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 401))) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_native_build_string_equal(
    RudaMirFunction *function,
    RudaSymbolId symbol
) {
    static const unsigned char value[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    RudaMirRegister left = RUDA_MIR_NO_REGISTER;
    RudaMirRegister right = RUDA_MIR_NO_REGISTER;
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirStringId first = RUDA_MIR_NO_STRING;
    RudaMirStringId second = RUDA_MIR_NO_STRING;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_BOOL, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_STRING, &left)
        || !ruda_mir_add_register(function, RUDA_TYPE_STRING, &right)
        || !ruda_mir_add_register(function, RUDA_TYPE_BOOL, &result)
        || !ruda_mir_add_block(function, &block)
        || !ruda_mir_add_string_constant(function, value, sizeof(value), &first)
        || !ruda_mir_add_string_constant(function, value, sizeof(value), &second)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_STRING, left,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 410);
    instruction.string_constant = first;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_STRING, right,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 411);
    instruction.string_constant = second;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_EQUAL_STRING, result,
        left, right, 0, false, 412);
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_native_terminator(RUDA_MIR_RETURN, result,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 413))) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}

bool ruda_test_native_build_panic_leaf(
    RudaMirFunction *function,
    RudaSymbolId symbol
) {
    static const unsigned char message[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirStringId constant = RUDA_MIR_NO_STRING;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_NEVER, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_STRING, &value)
        || !ruda_mir_add_block(function, &block)
        || !ruda_mir_add_string_constant(function, message, sizeof(message),
            &constant)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_STRING, value,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 200);
    instruction.string_constant = constant;
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_native_terminator(RUDA_MIR_PANIC, value,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 201))) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}


static bool ruda_test_native_build_never_entry(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    RudaSymbolId callee
) {
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    RudaMirTerminator terminator;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_NEVER, ruda_mir_function_default_options())
        || !ruda_mir_add_block(function, &block)
        || !ruda_mir_add_call(function, callee, RUDA_TYPE_NEVER, NULL, 0,
            &call)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    terminator = ruda_test_native_terminator(RUDA_MIR_CALL_NEVER,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 210);
    terminator.call = call;
    if (!ruda_mir_set_terminator(function, block, terminator)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static void ruda_test_native_strings_and_panic(void) {
    static const unsigned char value[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    const RudaSymbolId return_symbol = {{0x40}};
    const RudaSymbolId equal_symbol = {{0x41}};
    const RudaSymbolId leaf_symbol = {{0x10}};
    const RudaSymbolId entry_symbol = {{0x20}};
    RudaMirFunction returned;
    RudaMirFunction equal;
    RudaMirFunction leaf;
    RudaMirFunction entry;
    const RudaMirFunction *one_function[1];
    const RudaMirFunction *ordered[2];
    const RudaMirFunction *permuted[2];
    RudaNativeCProgram native_program;
    RudaOwnedBytes first;
    RudaOwnedBytes second;
    RudaBytecodeFunction bytecode;
    RudaBytecodeProgram program;
    RudaBytecodeExecution execution;
    RudaBytecodePanic panic;
    RudaBytecodeRunOptions run_options;
    RudaBytecodeValue result;
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
    RUDA_EXPECT(ruda_test_native_build_string_return(&returned, return_symbol));
    RUDA_EXPECT(ruda_test_native_build_string_equal(&equal, equal_symbol));
    RUDA_EXPECT(ruda_test_native_build_panic_leaf(&leaf, leaf_symbol));
    RUDA_EXPECT(ruda_test_native_build_never_entry(&entry, entry_symbol,
        leaf_symbol));
    RUDA_EXPECT(ruda_mir_verify(&returned) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_verify(&equal) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_verify(&leaf) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_verify(&entry) == RUDA_MIR_VERIFY_OK);

    one_function[0] = &returned;
    native_program.functions = one_function;
    native_program.function_count = 1;
    native_program.entry_symbol = return_symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(&native_program,
        ruda_system_allocator(), ruda_native_c_emit_default_options(), &first)
        == RUDA_NATIVE_C_EMIT_OK);
    RUDA_EXPECT(ruda_native_c_emit_program(&native_program,
        ruda_system_allocator(), ruda_native_c_emit_default_options(), &second)
        == RUDA_NATIVE_C_EMIT_OK);
    RUDA_EXPECT(first.length != 0 && first.length == second.length
        && memcmp(first.data, second.data, first.length) == 0);
    RUDA_EXPECT(ruda_test_native_contains(&first, "ruda_g_string"));
#if !defined(_WIN32)
    ruda_test_native_compile_and_run(&first, "ok:string:6:700021e29883\n", 0);
#endif
    ruda_owned_bytes_dispose(&second);
    ruda_owned_bytes_dispose(&first);

    RUDA_EXPECT(ruda_mir_lower_bytecode_function(&returned,
        ruda_system_allocator(), ruda_mir_bytecode_lower_default_options(),
        &bytecode) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(&execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()));
    result.type = RUDA_BYTECODE_TYPE_I64;
    result.as.i64 = INT64_MIN;
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    RUDA_EXPECT(ruda_bytecode_run(&bytecode, NULL, 0, ruda_system_allocator(),
        run_options, &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_STRING
        && ruda_bytecode_execution_string_bytes(&execution, result, &bytes, &length)
        && length == sizeof(value) && memcmp(bytes, value, sizeof(value)) == 0);
    ruda_bytecode_execution_dispose(&execution);
    ruda_bytecode_function_dispose(&bytecode);

    one_function[0] = &equal;
    native_program.entry_symbol = equal_symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(&native_program,
        ruda_system_allocator(), ruda_native_c_emit_default_options(), &first)
        == RUDA_NATIVE_C_EMIT_OK);
    RUDA_EXPECT(ruda_test_native_contains(&first, "ruda_g_string_equal"));
#if !defined(_WIN32)
    ruda_test_native_compile_and_run(&first, "ok:bool:true\n", 0);
#endif
    ruda_owned_bytes_dispose(&first);
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(&equal, ruda_system_allocator(),
        ruda_mir_bytecode_lower_default_options(), &bytecode)
        == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(&execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()));
    result.type = RUDA_BYTECODE_TYPE_I64;
    result.as.i64 = INT64_MIN;
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    RUDA_EXPECT(ruda_bytecode_run(&bytecode, NULL, 0, ruda_system_allocator(),
        run_options, &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_BOOL && result.as.boolean);
    ruda_bytecode_execution_dispose(&execution);
    ruda_bytecode_function_dispose(&bytecode);

    ordered[0] = &leaf;
    ordered[1] = &entry;
    permuted[0] = &entry;
    permuted[1] = &leaf;
    native_program.functions = ordered;
    native_program.function_count = 2;
    native_program.entry_symbol = entry_symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(&native_program,
        ruda_system_allocator(), ruda_native_c_emit_default_options(), &first)
        == RUDA_NATIVE_C_EMIT_OK);
    native_program.functions = permuted;
    RUDA_EXPECT(ruda_native_c_emit_program(&native_program,
        ruda_system_allocator(), ruda_native_c_emit_default_options(), &second)
        == RUDA_NATIVE_C_EMIT_OK);
    RUDA_EXPECT(first.length == second.length
        && memcmp(first.data, second.data, first.length) == 0);
#if !defined(_WIN32)
    ruda_test_native_compile_and_run(&first,
        "panic:G4000:10000000000000000000000000000000:201:202:6:700021e29883:2:"
        "10000000000000000000000000000000:201:202:"
        "20000000000000000000000000000000:210:211\n", 1);
#endif
    ruda_owned_bytes_dispose(&second);
    ruda_owned_bytes_dispose(&first);

    RUDA_EXPECT(ruda_mir_lower_bytecode_program(permuted, 2, entry_symbol,
        ruda_system_allocator(), ruda_mir_bytecode_program_lower_default_options(),
        &program) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(&execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()));
    RUDA_EXPECT(ruda_bytecode_panic_init(&panic, ruda_system_allocator()));
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    run_options.panic = &panic;
    result.type = RUDA_BYTECODE_TYPE_I64;
    result.as.i64 = 1234;
    RUDA_EXPECT(ruda_bytecode_run_program(&program, NULL, 0,
        ruda_system_allocator(), run_options, &result) == RUDA_BYTECODE_RUN_PANIC);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
    RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4000") == 0
        && panic.message.length == sizeof(value)
        && memcmp(panic.message.data, value, sizeof(value)) == 0
        && memcmp(&panic.symbol, &leaf_symbol, sizeof(leaf_symbol)) == 0
        && panic.span.start == 201 && panic.span.end == 202
        && panic.frames.length == 2);
    {
        const RudaBytecodePanicFrame *leaf_frame = ruda_bytecode_panic_frame(
            &panic, 0
        );
        const RudaBytecodePanicFrame *entry_frame = ruda_bytecode_panic_frame(
            &panic, 1
        );
        RUDA_EXPECT(leaf_frame != NULL && entry_frame != NULL
            && memcmp(&leaf_frame->symbol, &leaf_symbol, sizeof(leaf_symbol)) == 0
            && leaf_frame->call_span.start == 201 && leaf_frame->call_span.end == 202
            && memcmp(&entry_frame->symbol, &entry_symbol, sizeof(entry_symbol)) == 0
            && entry_frame->call_span.start == 210
            && entry_frame->call_span.end == 211);
    }
    ruda_bytecode_panic_dispose(&panic);
    ruda_bytecode_execution_dispose(&execution);
    ruda_bytecode_program_dispose(&program);

    {
        RudaNativeCEmitOptions options = ruda_native_c_emit_default_options();
        options.max_functions = 1;
        ruda_test_native_emit(ordered, 2, entry_symbol, options,
            RUDA_NATIVE_C_EMIT_LIMIT_REACHED);
        options = ruda_native_c_emit_default_options();
        options.max_generated_bytes = 1;
        ruda_test_native_emit(ordered, 2, entry_symbol, options,
            RUDA_NATIVE_C_EMIT_LIMIT_REACHED);
    }
    for (failure_index = 0; failure_index < 128 && !reached_success;
        failure_index += 1) {
        RudaFailAllocator state;
        RudaOwnedBytes output;
        RudaNativeCEmitStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        memset(&output, 0, sizeof(output));
        status = ruda_native_c_emit_program(&native_program,
            ruda_fail_allocator(&state), ruda_native_c_emit_default_options(),
            &output);
        RUDA_EXPECT(status == RUDA_NATIVE_C_EMIT_ALLOCATION_FAILED
            || status == RUDA_NATIVE_C_EMIT_OK);
        reached_success = reached_success || status == RUDA_NATIVE_C_EMIT_OK;
        ruda_owned_bytes_dispose(&output);
        RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
    ruda_mir_function_dispose(&entry);
    ruda_mir_function_dispose(&leaf);
    ruda_mir_function_dispose(&equal);
    ruda_mir_function_dispose(&returned);
}

void ruda_test_native_strings_panic(void) {
    ruda_test_native_strings_and_panic();
}
