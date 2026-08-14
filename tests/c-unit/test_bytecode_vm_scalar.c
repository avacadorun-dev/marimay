#include "test_bytecode_vm_support.h"

void ruda_test_vm_f64(void) {
    typedef struct RudaTestVmF64Case {
        RudaOpcode opcode;
        uint64_t left;
        uint64_t right;
        RudaBytecodeType type;
        uint64_t expected_bits;
        bool expected_boolean;
    } RudaTestVmF64Case;
    static const RudaTestVmF64Case cases[] = {
        {RUDA_OPCODE_F64_ADD, UINT64_C(0x3ff8000000000000),
            UINT64_C(0x4002000000000000), RUDA_BYTECODE_TYPE_F64,
            UINT64_C(0x400e000000000000), false},
        {RUDA_OPCODE_F64_SUBTRACT, UINT64_C(0x4014000000000000),
            UINT64_C(0x4000000000000000), RUDA_BYTECODE_TYPE_F64,
            UINT64_C(0x4008000000000000), false},
        {RUDA_OPCODE_F64_MULTIPLY, UINT64_C(0xc000000000000000),
            UINT64_C(0x4008000000000000), RUDA_BYTECODE_TYPE_F64,
            UINT64_C(0xc018000000000000), false},
        {RUDA_OPCODE_F64_DIVIDE, UINT64_C(0xc018000000000000),
            UINT64_C(0x4008000000000000), RUDA_BYTECODE_TYPE_F64,
            UINT64_C(0xc000000000000000), false},
        {RUDA_OPCODE_F64_DIVIDE, UINT64_C(0x3ff0000000000000),
            UINT64_C(0x0000000000000000), RUDA_BYTECODE_TYPE_F64,
            UINT64_C(0x7ff0000000000000), false},
        {RUDA_OPCODE_F64_DIVIDE, UINT64_C(0xbff0000000000000),
            UINT64_C(0x0000000000000000), RUDA_BYTECODE_TYPE_F64,
            UINT64_C(0xfff0000000000000), false},
        {RUDA_OPCODE_F64_DIVIDE, UINT64_C(0x0000000000000000),
            UINT64_C(0x0000000000000000), RUDA_BYTECODE_TYPE_F64,
            UINT64_C(0x7ff8000000000000), false},
        {RUDA_OPCODE_F64_NEGATE, UINT64_C(0x0000000000000000), 0,
            RUDA_BYTECODE_TYPE_F64, UINT64_C(0x8000000000000000), false},
        {RUDA_OPCODE_F64_NEGATE, UINT64_C(0x7ff8000000000001), 0,
            RUDA_BYTECODE_TYPE_F64, UINT64_C(0xfff8000000000001), false},
        {RUDA_OPCODE_F64_EQUAL, UINT64_C(0x8000000000000000),
            UINT64_C(0x0000000000000000), RUDA_BYTECODE_TYPE_BOOL, 0, true},
        {RUDA_OPCODE_F64_EQUAL, UINT64_C(0x7ff8000000000001),
            UINT64_C(0x7ff8000000000001), RUDA_BYTECODE_TYPE_BOOL, 0, false},
        {RUDA_OPCODE_F64_LESS, UINT64_C(0xbff0000000000000),
            UINT64_C(0x0000000000000000), RUDA_BYTECODE_TYPE_BOOL, 0, true},
        {RUDA_OPCODE_F64_LESS, UINT64_C(0x7ff8000000000001),
            UINT64_C(0x3ff0000000000000), RUDA_BYTECODE_TYPE_BOOL, 0, false},
        {RUDA_OPCODE_F64_LESS_EQUAL, UINT64_C(0x8000000000000000),
            UINT64_C(0x0000000000000000), RUDA_BYTECODE_TYPE_BOOL, 0, true}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        RudaBytecodeFunction function;
        RudaBytecodeValue result;
        memset(&function, 0, sizeof(function));
        result = ruda_test_vm_i64(1234);
        RUDA_EXPECT(ruda_test_vm_build_f64(&function, cases[index].opcode,
            cases[index].left, cases[index].right, cases[index].type));
        RUDA_EXPECT(ruda_bytecode_run(&function, NULL, 0,
            ruda_system_allocator(), ruda_bytecode_run_default_options(), &result)
            == RUDA_BYTECODE_RUN_OK);
        RUDA_EXPECT(result.type == cases[index].type);
        if (cases[index].type == RUDA_BYTECODE_TYPE_F64) {
            RUDA_EXPECT(result.as.f64_bits == cases[index].expected_bits);
        } else {
            RUDA_EXPECT(result.as.boolean == cases[index].expected_boolean);
        }
        ruda_bytecode_function_dispose(&function);
    }
}

void ruda_test_vm_strings_and_panic(void) {
    static const unsigned char message[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    RudaBytecodeFunction function;
    RudaBytecodeExecution execution;
    RudaBytecodePanic panic;
    RudaBytecodeRunOptions options;
    RudaBytecodeValue result;
    const unsigned char *bytes = NULL;
    size_t length = 0;

    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    RUDA_EXPECT(ruda_bytecode_execution_init(&execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()));
    RUDA_EXPECT(ruda_test_vm_build_string_literal(&function,
        ruda_system_allocator(), message, sizeof(message), false));
    result = ruda_test_vm_i64(1234);
    RUDA_EXPECT(ruda_bytecode_run(&function, NULL, 0, ruda_system_allocator(),
        ruda_bytecode_run_default_options(), &result)
        == RUDA_BYTECODE_RUN_INVALID_ARGUMENT);
    options = ruda_bytecode_run_default_options();
    options.execution = &execution;
    RUDA_EXPECT(ruda_bytecode_run(&function, NULL, 0, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_STRING);
    RUDA_EXPECT(ruda_bytecode_execution_string_bytes(&execution, result,
        &bytes, &length));
    RUDA_EXPECT(length == sizeof(message)
        && memcmp(bytes, message, sizeof(message)) == 0);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_vm_build_string_equal(&function,
        ruda_system_allocator(), message, sizeof(message), message,
        sizeof(message)));
    RUDA_EXPECT(ruda_bytecode_run(&function, NULL, 0, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_BOOL && result.as.boolean);
    ruda_bytecode_function_dispose(&function);
    RUDA_EXPECT(ruda_test_vm_build_string_equal(&function,
        ruda_system_allocator(), (const unsigned char *)"a", 1,
        (const unsigned char *)"b", 1));
    RUDA_EXPECT(ruda_bytecode_run(&function, NULL, 0, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_BOOL && !result.as.boolean);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_bytecode_panic_init(&panic, ruda_system_allocator()));
    RUDA_EXPECT(ruda_test_vm_build_string_literal(&function,
        ruda_system_allocator(), message, sizeof(message), true));
    options.panic = &panic;
    result = ruda_test_vm_i64(1234);
    RUDA_EXPECT(ruda_bytecode_run(&function, NULL, 0, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_PANIC);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
    RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4000") == 0);
    RUDA_EXPECT(panic.message.length == sizeof(message)
        && memcmp(panic.message.data, message, sizeof(message)) == 0);
    RUDA_EXPECT(panic.span.source == 0 && panic.span.start == 141
        && panic.span.end == 142 && panic.frames.length == 1);
    ruda_bytecode_function_dispose(&function);
    ruda_bytecode_panic_dispose(&panic);
    ruda_bytecode_execution_dispose(&execution);

    {
        RudaBytecodeExecution foreign_execution;
        RudaFailAllocator state;
        RudaBytecodeValue argument;
        memset(&foreign_execution, 0, sizeof(foreign_execution));
        RUDA_EXPECT(ruda_bytecode_execution_init(&foreign_execution,
            ruda_system_allocator(), ruda_bytecode_execution_default_options()));
        RUDA_EXPECT(ruda_bytecode_execution_init(&execution,
            ruda_system_allocator(), ruda_bytecode_execution_default_options()));
        RUDA_EXPECT(ruda_test_vm_build_string_literal(&function,
            ruda_system_allocator(), message, sizeof(message), false));
        options = ruda_bytecode_run_default_options();
        options.execution = &execution;
        RUDA_EXPECT(ruda_bytecode_run(&function, NULL, 0,
            ruda_system_allocator(), options, &argument) == RUDA_BYTECODE_RUN_OK);
        ruda_bytecode_function_dispose(&function);
        RUDA_EXPECT(ruda_test_vm_build_string_identity(&function,
            ruda_system_allocator()));
        options.execution = &foreign_execution;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), 0);
        result = ruda_test_vm_i64(1234);
        RUDA_EXPECT(ruda_bytecode_run(&function, &argument, 1,
            ruda_fail_allocator(&state), options, &result)
            == RUDA_BYTECODE_RUN_INVALID_REFERENCE);
        RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
        RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        argument.as.reference = RUDA_BYTECODE_INVALID_REFERENCE;
        result = ruda_test_vm_i64(5678);
        RUDA_EXPECT(ruda_bytecode_run(&function, &argument, 1,
            ruda_fail_allocator(&state), options, &result)
            == RUDA_BYTECODE_RUN_INVALID_REFERENCE);
        RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 5678);
        RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        ruda_bytecode_function_dispose(&function);
        ruda_bytecode_execution_dispose(&execution);
        ruda_bytecode_execution_dispose(&foreign_execution);
    }

    {
        const RudaSymbolId callee_symbol = {{0x10}};
        const RudaSymbolId entry_symbol = {{0x20}};
        RudaBytecodeProgram program;
        RudaBytecodeFunction callee;
        RudaBytecodeFunction entry;
        RudaBytecodeCallId call = RUDA_BYTECODE_NO_CALL;
        uint32_t block = UINT32_MAX;
        uint32_t id = UINT32_MAX;
        uint64_t span = 160;
        const RudaBytecodePanicFrame *callee_frame = NULL;
        const RudaBytecodePanicFrame *caller_frame = NULL;
        memset(&program, 0, sizeof(program));
        memset(&callee, 0, sizeof(callee));
        memset(&entry, 0, sizeof(entry));
        memset(&execution, 0, sizeof(execution));
        memset(&panic, 0, sizeof(panic));
        RUDA_EXPECT(ruda_bytecode_execution_init(&execution,
            ruda_system_allocator(), ruda_bytecode_execution_default_options()));
        RUDA_EXPECT(ruda_bytecode_panic_init(&panic, ruda_system_allocator()));
        RUDA_EXPECT(ruda_test_vm_build_string_literal(&callee,
            ruda_system_allocator(), message, sizeof(message), true));
        RUDA_EXPECT(ruda_bytecode_function_init(&entry,
            ruda_system_allocator(), RUDA_BYTECODE_TYPE_NEVER, 1, NULL, 0,
            ruda_bytecode_function_default_options()));
        RUDA_EXPECT(ruda_bytecode_add_block(&entry, &block));
        RUDA_EXPECT(ruda_bytecode_add_call(&entry, callee_symbol,
            RUDA_BYTECODE_TYPE_NEVER, NULL, 0, &call));
        RUDA_EXPECT(ruda_test_vm_append(&entry, block,
            RUDA_OPCODE_CALL_DIRECT_NEVER, call, 0, 0, &span));
        RUDA_EXPECT(ruda_bytecode_program_init(&program, ruda_system_allocator(),
            1, ruda_bytecode_program_default_options()));
        RUDA_EXPECT(ruda_bytecode_program_take_function(&program, callee_symbol,
            &callee, &id) && id == 0);
        RUDA_EXPECT(ruda_bytecode_program_take_function(&program, entry_symbol,
            &entry, &id) && id == 1);
        RUDA_EXPECT(ruda_bytecode_program_set_entry(&program, entry_symbol));
        options = ruda_bytecode_run_default_options();
        options.execution = &execution;
        options.panic = &panic;
        result = ruda_test_vm_i64(1234);
        RUDA_EXPECT(ruda_bytecode_run_program(&program, NULL, 0,
            ruda_system_allocator(), options, &result) == RUDA_BYTECODE_RUN_PANIC);
        RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
        RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4000") == 0);
        RUDA_EXPECT(panic.message.length == sizeof(message)
            && memcmp(panic.message.data, message, sizeof(message)) == 0);
        RUDA_EXPECT(memcmp(&panic.symbol, &callee_symbol, sizeof(panic.symbol)) == 0
            && panic.span.start == 141 && panic.span.end == 142
            && panic.frames.length == 2);
        callee_frame = ruda_bytecode_panic_frame(&panic, 0);
        caller_frame = ruda_bytecode_panic_frame(&panic, 1);
        RUDA_EXPECT(callee_frame != NULL && caller_frame != NULL);
        if (callee_frame != NULL) {
            RUDA_EXPECT(memcmp(&callee_frame->symbol, &callee_symbol,
                sizeof(callee_symbol)) == 0 && callee_frame->call_span.start == 141
                && callee_frame->call_span.end == 142);
        }
        if (caller_frame != NULL) {
            RUDA_EXPECT(memcmp(&caller_frame->symbol, &entry_symbol,
                sizeof(entry_symbol)) == 0 && caller_frame->call_span.start == 160
                && caller_frame->call_span.end == 161);
        }
        ruda_bytecode_program_dispose(&program);
        ruda_bytecode_panic_dispose(&panic);
        ruda_bytecode_execution_dispose(&execution);
    }

    {
        size_t failure_index = 0;
        bool reached_success = false;
        RUDA_EXPECT(ruda_test_vm_build_string_literal(&function,
            ruda_system_allocator(), message, sizeof(message), false));
        for (failure_index = 0; failure_index < 32 && !reached_success;
            failure_index += 1) {
            RudaFailAllocator state;
            RudaBytecodeExecution failed_execution;
            RudaBytecodeRunStatus status;
            memset(&failed_execution, 0, sizeof(failed_execution));
            ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
            if (!ruda_bytecode_execution_init(&failed_execution,
                    ruda_fail_allocator(&state),
                    ruda_bytecode_execution_default_options())) {
                RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
                continue;
            }
            options = ruda_bytecode_run_default_options();
            options.execution = &failed_execution;
            result = ruda_test_vm_i64(1234);
            status = ruda_bytecode_run(&function, NULL, 0,
                ruda_system_allocator(), options, &result);
            RUDA_EXPECT(status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED
                || status == RUDA_BYTECODE_RUN_OK);
            if (status == RUDA_BYTECODE_RUN_OK) {
                reached_success = true;
            } else {
                RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64
                    && result.as.i64 == 1234);
            }
            ruda_bytecode_execution_dispose(&failed_execution);
            RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_success);
        ruda_bytecode_function_dispose(&function);
    }
}
