#include "test_bytecode_vm_support.h"

void mmay_test_vm_f64(void) {
    typedef struct MarimayTestVmF64Case {
        MarimayOpcode opcode;
        uint64_t left;
        uint64_t right;
        MarimayBytecodeType type;
        uint64_t expected_bits;
        bool expected_boolean;
    } MarimayTestVmF64Case;
    static const MarimayTestVmF64Case cases[] = {
        {MMAY_OPCODE_F64_ADD, UINT64_C(0x3ff8000000000000),
            UINT64_C(0x4002000000000000), MMAY_BYTECODE_TYPE_F64,
            UINT64_C(0x400e000000000000), false},
        {MMAY_OPCODE_F64_SUBTRACT, UINT64_C(0x4014000000000000),
            UINT64_C(0x4000000000000000), MMAY_BYTECODE_TYPE_F64,
            UINT64_C(0x4008000000000000), false},
        {MMAY_OPCODE_F64_MULTIPLY, UINT64_C(0xc000000000000000),
            UINT64_C(0x4008000000000000), MMAY_BYTECODE_TYPE_F64,
            UINT64_C(0xc018000000000000), false},
        {MMAY_OPCODE_F64_DIVIDE, UINT64_C(0xc018000000000000),
            UINT64_C(0x4008000000000000), MMAY_BYTECODE_TYPE_F64,
            UINT64_C(0xc000000000000000), false},
        {MMAY_OPCODE_F64_DIVIDE, UINT64_C(0x3ff0000000000000),
            UINT64_C(0x0000000000000000), MMAY_BYTECODE_TYPE_F64,
            UINT64_C(0x7ff0000000000000), false},
        {MMAY_OPCODE_F64_DIVIDE, UINT64_C(0xbff0000000000000),
            UINT64_C(0x0000000000000000), MMAY_BYTECODE_TYPE_F64,
            UINT64_C(0xfff0000000000000), false},
        {MMAY_OPCODE_F64_DIVIDE, UINT64_C(0x0000000000000000),
            UINT64_C(0x0000000000000000), MMAY_BYTECODE_TYPE_F64,
            UINT64_C(0x7ff8000000000000), false},
        {MMAY_OPCODE_F64_NEGATE, UINT64_C(0x0000000000000000), 0,
            MMAY_BYTECODE_TYPE_F64, UINT64_C(0x8000000000000000), false},
        {MMAY_OPCODE_F64_NEGATE, UINT64_C(0x7ff8000000000001), 0,
            MMAY_BYTECODE_TYPE_F64, UINT64_C(0xfff8000000000001), false},
        {MMAY_OPCODE_F64_EQUAL, UINT64_C(0x8000000000000000),
            UINT64_C(0x0000000000000000), MMAY_BYTECODE_TYPE_BOOL, 0, true},
        {MMAY_OPCODE_F64_EQUAL, UINT64_C(0x7ff8000000000001),
            UINT64_C(0x7ff8000000000001), MMAY_BYTECODE_TYPE_BOOL, 0, false},
        {MMAY_OPCODE_F64_LESS, UINT64_C(0xbff0000000000000),
            UINT64_C(0x0000000000000000), MMAY_BYTECODE_TYPE_BOOL, 0, true},
        {MMAY_OPCODE_F64_LESS, UINT64_C(0x7ff8000000000001),
            UINT64_C(0x3ff0000000000000), MMAY_BYTECODE_TYPE_BOOL, 0, false},
        {MMAY_OPCODE_F64_LESS_EQUAL, UINT64_C(0x8000000000000000),
            UINT64_C(0x0000000000000000), MMAY_BYTECODE_TYPE_BOOL, 0, true}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        MarimayBytecodeFunction function;
        MarimayBytecodeValue result;
        memset(&function, 0, sizeof(function));
        result = mmay_test_vm_i64(1234);
        MMAY_EXPECT(mmay_test_vm_build_f64(&function, cases[index].opcode,
            cases[index].left, cases[index].right, cases[index].type));
        MMAY_EXPECT(mmay_bytecode_run(&function, NULL, 0,
            mmay_system_allocator(), mmay_bytecode_run_default_options(), &result)
            == MMAY_BYTECODE_RUN_OK);
        MMAY_EXPECT(result.type == cases[index].type);
        if (cases[index].type == MMAY_BYTECODE_TYPE_F64) {
            MMAY_EXPECT(result.as.f64_bits == cases[index].expected_bits);
        } else {
            MMAY_EXPECT(result.as.boolean == cases[index].expected_boolean);
        }
        mmay_bytecode_function_dispose(&function);
    }
}

void mmay_test_vm_strings_and_panic(void) {
    static const unsigned char message[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    MarimayBytecodeFunction function;
    MarimayBytecodeExecution execution;
    MarimayBytecodePanic panic;
    MarimayBytecodeRunOptions options;
    MarimayBytecodeValue result;
    const unsigned char *bytes = NULL;
    size_t length = 0;

    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    MMAY_EXPECT(mmay_bytecode_execution_init(&execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()));
    MMAY_EXPECT(mmay_test_vm_build_string_literal(&function,
        mmay_system_allocator(), message, sizeof(message), false));
    result = mmay_test_vm_i64(1234);
    MMAY_EXPECT(mmay_bytecode_run(&function, NULL, 0, mmay_system_allocator(),
        mmay_bytecode_run_default_options(), &result)
        == MMAY_BYTECODE_RUN_INVALID_ARGUMENT);
    options = mmay_bytecode_run_default_options();
    options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run(&function, NULL, 0, mmay_system_allocator(),
        options, &result) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_STRING);
    MMAY_EXPECT(mmay_bytecode_execution_string_bytes(&execution, result,
        &bytes, &length));
    MMAY_EXPECT(length == sizeof(message)
        && memcmp(bytes, message, sizeof(message)) == 0);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_vm_build_string_equal(&function,
        mmay_system_allocator(), message, sizeof(message), message,
        sizeof(message)));
    MMAY_EXPECT(mmay_bytecode_run(&function, NULL, 0, mmay_system_allocator(),
        options, &result) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_BOOL && result.as.boolean);
    mmay_bytecode_function_dispose(&function);
    MMAY_EXPECT(mmay_test_vm_build_string_equal(&function,
        mmay_system_allocator(), (const unsigned char *)"a", 1,
        (const unsigned char *)"b", 1));
    MMAY_EXPECT(mmay_bytecode_run(&function, NULL, 0, mmay_system_allocator(),
        options, &result) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_BOOL && !result.as.boolean);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_bytecode_panic_init(&panic, mmay_system_allocator()));
    MMAY_EXPECT(mmay_test_vm_build_string_literal(&function,
        mmay_system_allocator(), message, sizeof(message), true));
    options.panic = &panic;
    result = mmay_test_vm_i64(1234);
    MMAY_EXPECT(mmay_bytecode_run(&function, NULL, 0, mmay_system_allocator(),
        options, &result) == MMAY_BYTECODE_RUN_PANIC);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
    MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4000") == 0);
    MMAY_EXPECT(panic.message.length == sizeof(message)
        && memcmp(panic.message.data, message, sizeof(message)) == 0);
    MMAY_EXPECT(panic.span.source == 0 && panic.span.start == 141
        && panic.span.end == 142 && panic.frames.length == 1);
    mmay_bytecode_function_dispose(&function);
    mmay_bytecode_panic_dispose(&panic);
    mmay_bytecode_execution_dispose(&execution);

    {
        MarimayBytecodeExecution foreign_execution;
        MarimayFailAllocator state;
        MarimayBytecodeValue argument;
        memset(&foreign_execution, 0, sizeof(foreign_execution));
        MMAY_EXPECT(mmay_bytecode_execution_init(&foreign_execution,
            mmay_system_allocator(), mmay_bytecode_execution_default_options()));
        MMAY_EXPECT(mmay_bytecode_execution_init(&execution,
            mmay_system_allocator(), mmay_bytecode_execution_default_options()));
        MMAY_EXPECT(mmay_test_vm_build_string_literal(&function,
            mmay_system_allocator(), message, sizeof(message), false));
        options = mmay_bytecode_run_default_options();
        options.execution = &execution;
        MMAY_EXPECT(mmay_bytecode_run(&function, NULL, 0,
            mmay_system_allocator(), options, &argument) == MMAY_BYTECODE_RUN_OK);
        mmay_bytecode_function_dispose(&function);
        MMAY_EXPECT(mmay_test_vm_build_string_identity(&function,
            mmay_system_allocator()));
        options.execution = &foreign_execution;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), 0);
        result = mmay_test_vm_i64(1234);
        MMAY_EXPECT(mmay_bytecode_run(&function, &argument, 1,
            mmay_fail_allocator(&state), options, &result)
            == MMAY_BYTECODE_RUN_INVALID_REFERENCE);
        MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        argument.as.reference = MMAY_BYTECODE_INVALID_REFERENCE;
        result = mmay_test_vm_i64(5678);
        MMAY_EXPECT(mmay_bytecode_run(&function, &argument, 1,
            mmay_fail_allocator(&state), options, &result)
            == MMAY_BYTECODE_RUN_INVALID_REFERENCE);
        MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 5678);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        mmay_bytecode_function_dispose(&function);
        mmay_bytecode_execution_dispose(&execution);
        mmay_bytecode_execution_dispose(&foreign_execution);
    }

    {
        const MarimaySymbolId callee_symbol = {{0x10}};
        const MarimaySymbolId entry_symbol = {{0x20}};
        MarimayBytecodeProgram program;
        MarimayBytecodeFunction callee;
        MarimayBytecodeFunction entry;
        MarimayBytecodeCallId call = MMAY_BYTECODE_NO_CALL;
        uint32_t block = UINT32_MAX;
        uint32_t id = UINT32_MAX;
        uint64_t span = 160;
        const MarimayBytecodePanicFrame *callee_frame = NULL;
        const MarimayBytecodePanicFrame *caller_frame = NULL;
        memset(&program, 0, sizeof(program));
        memset(&callee, 0, sizeof(callee));
        memset(&entry, 0, sizeof(entry));
        memset(&execution, 0, sizeof(execution));
        memset(&panic, 0, sizeof(panic));
        MMAY_EXPECT(mmay_bytecode_execution_init(&execution,
            mmay_system_allocator(), mmay_bytecode_execution_default_options()));
        MMAY_EXPECT(mmay_bytecode_panic_init(&panic, mmay_system_allocator()));
        MMAY_EXPECT(mmay_test_vm_build_string_literal(&callee,
            mmay_system_allocator(), message, sizeof(message), true));
        MMAY_EXPECT(mmay_bytecode_function_init(&entry,
            mmay_system_allocator(), MMAY_BYTECODE_TYPE_NEVER, 1, NULL, 0,
            mmay_bytecode_function_default_options()));
        MMAY_EXPECT(mmay_bytecode_add_block(&entry, &block));
        MMAY_EXPECT(mmay_bytecode_add_call(&entry, callee_symbol,
            MMAY_BYTECODE_TYPE_NEVER, NULL, 0, &call));
        MMAY_EXPECT(mmay_test_vm_append(&entry, block,
            MMAY_OPCODE_CALL_DIRECT_NEVER, call, 0, 0, &span));
        MMAY_EXPECT(mmay_bytecode_program_init(&program, mmay_system_allocator(),
            1, mmay_bytecode_program_default_options()));
        MMAY_EXPECT(mmay_bytecode_program_take_function(&program, callee_symbol,
            &callee, &id) && id == 0);
        MMAY_EXPECT(mmay_bytecode_program_take_function(&program, entry_symbol,
            &entry, &id) && id == 1);
        MMAY_EXPECT(mmay_bytecode_program_set_entry(&program, entry_symbol));
        options = mmay_bytecode_run_default_options();
        options.execution = &execution;
        options.panic = &panic;
        result = mmay_test_vm_i64(1234);
        MMAY_EXPECT(mmay_bytecode_run_program(&program, NULL, 0,
            mmay_system_allocator(), options, &result) == MMAY_BYTECODE_RUN_PANIC);
        MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
        MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4000") == 0);
        MMAY_EXPECT(panic.message.length == sizeof(message)
            && memcmp(panic.message.data, message, sizeof(message)) == 0);
        MMAY_EXPECT(memcmp(&panic.symbol, &callee_symbol, sizeof(panic.symbol)) == 0
            && panic.span.start == 141 && panic.span.end == 142
            && panic.frames.length == 2);
        callee_frame = mmay_bytecode_panic_frame(&panic, 0);
        caller_frame = mmay_bytecode_panic_frame(&panic, 1);
        MMAY_EXPECT(callee_frame != NULL && caller_frame != NULL);
        if (callee_frame != NULL) {
            MMAY_EXPECT(memcmp(&callee_frame->symbol, &callee_symbol,
                sizeof(callee_symbol)) == 0 && callee_frame->call_span.start == 141
                && callee_frame->call_span.end == 142);
        }
        if (caller_frame != NULL) {
            MMAY_EXPECT(memcmp(&caller_frame->symbol, &entry_symbol,
                sizeof(entry_symbol)) == 0 && caller_frame->call_span.start == 160
                && caller_frame->call_span.end == 161);
        }
        mmay_bytecode_program_dispose(&program);
        mmay_bytecode_panic_dispose(&panic);
        mmay_bytecode_execution_dispose(&execution);
    }

    {
        size_t failure_index = 0;
        bool reached_success = false;
        MMAY_EXPECT(mmay_test_vm_build_string_literal(&function,
            mmay_system_allocator(), message, sizeof(message), false));
        for (failure_index = 0; failure_index < 32 && !reached_success;
            failure_index += 1) {
            MarimayFailAllocator state;
            MarimayBytecodeExecution failed_execution;
            MarimayBytecodeRunStatus status;
            memset(&failed_execution, 0, sizeof(failed_execution));
            mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
            if (!mmay_bytecode_execution_init(&failed_execution,
                    mmay_fail_allocator(&state),
                    mmay_bytecode_execution_default_options())) {
                MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
                continue;
            }
            options = mmay_bytecode_run_default_options();
            options.execution = &failed_execution;
            result = mmay_test_vm_i64(1234);
            status = mmay_bytecode_run(&function, NULL, 0,
                mmay_system_allocator(), options, &result);
            MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED
                || status == MMAY_BYTECODE_RUN_OK);
            if (status == MMAY_BYTECODE_RUN_OK) {
                reached_success = true;
            } else {
                MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64
                    && result.as.i64 == 1234);
            }
            mmay_bytecode_execution_dispose(&failed_execution);
            MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        }
        MMAY_EXPECT(reached_success);
        mmay_bytecode_function_dispose(&function);
    }
}
