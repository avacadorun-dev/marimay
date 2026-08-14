#include "test_mir_bytecode_support.h"

#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"

static bool ruda_test_mir_bytecode_build_direct_leaf(
    RudaMirFunction *function,
    RudaSymbolId symbol
) {
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_I64, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &result)
        || !ruda_mir_add_block(function, &block)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_mir_bytecode_instruction(RUDA_MIR_CONSTANT_I64,
        result, RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 42, false, 20);
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_mir_bytecode_terminator(RUDA_MIR_RETURN, result,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 21))) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_mir_bytecode_build_direct_entry(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    RudaSymbolId callee
) {
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_I64, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &result)
        || !ruda_mir_add_block(function, &block)
        || !ruda_mir_add_call(function, callee, RUDA_TYPE_I64, NULL, 0, &call)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_mir_bytecode_instruction(RUDA_MIR_CALL, result,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 10);
    instruction.call = call;
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_mir_bytecode_terminator(RUDA_MIR_RETURN, result,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 11))) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_mir_bytecode_build_void_leaf(
    RudaMirFunction *function,
    RudaSymbolId symbol
) {
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(
            function,
            ruda_system_allocator(),
            symbol,
            NULL,
            0,
            RUDA_TYPE_VOID,
            ruda_mir_function_default_options()
        ) || !ruda_mir_add_block(function, &block)
        || !ruda_mir_set_terminator(
            function,
            block,
            ruda_test_mir_bytecode_terminator(
                RUDA_MIR_RETURN,
                RUDA_MIR_NO_REGISTER,
                RUDA_MIR_NO_BLOCK,
                RUDA_MIR_NO_BLOCK,
                31
            )
        )) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_mir_bytecode_build_void_entry(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    RudaSymbolId callee
) {
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(
            function,
            ruda_system_allocator(),
            symbol,
            NULL,
            0,
            RUDA_TYPE_VOID,
            ruda_mir_function_default_options()
        ) || !ruda_mir_add_register(function, RUDA_TYPE_VOID, &result)
        || !ruda_mir_add_block(function, &block)
        || !ruda_mir_add_call(
            function,
            callee,
            RUDA_TYPE_VOID,
            NULL,
            0,
            &call
        )) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_CALL,
        result,
        RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER,
        0,
        false,
        30
    );
    instruction.call = call;
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(
            function,
            block,
            ruda_test_mir_bytecode_terminator(
                RUDA_MIR_RETURN,
                RUDA_MIR_NO_REGISTER,
                RUDA_MIR_NO_BLOCK,
                RUDA_MIR_NO_BLOCK,
                31
            )
        )) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_mir_bytecode_build_string_panic_leaf(
    RudaMirFunction *function,
    RudaSymbolId symbol
) {
    static const unsigned char message[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirStringId string_id = RUDA_MIR_NO_STRING;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_NEVER, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_STRING, &value)
        || !ruda_mir_add_block(function, &block)
        || !ruda_mir_add_string_constant(function, message, sizeof(message),
            &string_id)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_mir_bytecode_instruction(RUDA_MIR_CONSTANT_STRING,
        value, RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 200);
    instruction.string_constant = string_id;
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_mir_bytecode_terminator(RUDA_MIR_PANIC, value,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 201))) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_mir_bytecode_build_never_entry(
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
    terminator = ruda_test_mir_bytecode_terminator(RUDA_MIR_CALL_NEVER,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 210);
    terminator.call = call;
    if (!ruda_mir_set_terminator(function, block, terminator)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static void ruda_test_mir_bytecode_string_panic_program(void) {
    static const unsigned char message[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    const RudaSymbolId leaf_symbol = {{0x10}};
    const RudaSymbolId entry_symbol = {{0x20}};
    RudaMirFunction leaf;
    RudaMirFunction entry;
    const RudaMirFunction *functions[2];
    RudaMirExecution mir_execution;
    RudaMirPanic mir_panic;
    RudaMirInterpretOptions mir_options;
    RudaMirValue mir_result;
    RudaBytecodeProgram program;
    RudaBytecodeProgram decoded;
    RudaBytecodeExecution execution;
    RudaBytecodePanic panic;
    RudaBytecodeRunOptions run_options;
    RudaBytecodeValue result;
    RudaOwnedBytes encoded;
    const RudaBytecodeProgramFunction *lowered_leaf;
    const RudaBytecodeProgramFunction *lowered_entry;
    const RudaBytecodeInstruction *instruction;
    const RudaBytecodePanicFrame *frame;

    memset(&mir_execution, 0, sizeof(mir_execution));
    memset(&mir_panic, 0, sizeof(mir_panic));
    memset(&mir_result, 0, sizeof(mir_result));
    memset(&program, 0, sizeof(program));
    memset(&decoded, 0, sizeof(decoded));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    memset(&encoded, 0, sizeof(encoded));
    RUDA_EXPECT(ruda_test_mir_bytecode_build_string_panic_leaf(&leaf,
        leaf_symbol));
    RUDA_EXPECT(ruda_test_mir_bytecode_build_never_entry(&entry, entry_symbol,
        leaf_symbol));
    RUDA_EXPECT(ruda_mir_verify(&leaf) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_verify(&entry) == RUDA_MIR_VERIFY_OK);

    RUDA_EXPECT(ruda_mir_execution_init(&mir_execution, ruda_system_allocator(),
        ruda_mir_execution_default_options()));
    mir_options = ruda_mir_interpret_default_options();
    mir_options.execution = &mir_execution;
    mir_options.panic = &mir_panic;
    RUDA_EXPECT(ruda_mir_interpret(&leaf, NULL, 0, ruda_system_allocator(),
        mir_options, &mir_result) == RUDA_MIR_INTERPRET_PANIC);
    RUDA_EXPECT(mir_panic.code != NULL && strcmp(mir_panic.code, "G4000") == 0);
    RUDA_EXPECT(mir_panic.message.length == sizeof(message)
        && memcmp(mir_panic.message.data, message, sizeof(message)) == 0);
    RUDA_EXPECT(memcmp(&mir_panic.symbol, &leaf_symbol, sizeof(leaf_symbol)) == 0
        && mir_panic.span.start == 201 && mir_panic.span.end == 202);
    ruda_mir_panic_dispose(&mir_panic);
    ruda_mir_execution_dispose(&mir_execution);

    functions[0] = &entry;
    functions[1] = &leaf;
    RUDA_EXPECT(ruda_mir_lower_bytecode_program(functions, 2, entry_symbol,
        ruda_system_allocator(), ruda_mir_bytecode_program_lower_default_options(),
        &program) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(program.functions.length == 2 && program.entry_function == 1);
    lowered_leaf = ruda_bytecode_program_function(&program, 0);
    lowered_entry = ruda_bytecode_program_function(&program, 1);
    instruction = lowered_leaf != NULL
        ? ruda_test_mir_bytecode_at(&lowered_leaf->body, 0, 0) : NULL;
    RUDA_EXPECT(instruction != NULL && instruction->opcode == RUDA_OPCODE_CONST_STRING);
    instruction = lowered_leaf != NULL
        ? ruda_test_mir_bytecode_at(&lowered_leaf->body, 0, 1) : NULL;
    RUDA_EXPECT(instruction != NULL && instruction->opcode == RUDA_OPCODE_PANIC_STRING
        && instruction->span.start == 201 && instruction->span.end == 202);
    instruction = lowered_entry != NULL
        ? ruda_test_mir_bytecode_at(&lowered_entry->body, 0, 0) : NULL;
    RUDA_EXPECT(instruction != NULL
        && instruction->opcode == RUDA_OPCODE_CALL_DIRECT_NEVER
        && instruction->operands[0].unsigned_value == 0
        && instruction->span.start == 210 && instruction->span.end == 211);
    RUDA_EXPECT(ruda_bytecode_encode_program(&program, ruda_system_allocator(),
        &encoded) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_decode_program(encoded.data, encoded.length,
        ruda_system_allocator(), ruda_bytecode_program_default_options(),
        &decoded) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(&execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()));
    RUDA_EXPECT(ruda_bytecode_panic_init(&panic, ruda_system_allocator()));
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    run_options.panic = &panic;
    result.type = RUDA_BYTECODE_TYPE_I64;
    result.as.i64 = 1234;
    RUDA_EXPECT(ruda_bytecode_run_program(&decoded, NULL, 0,
        ruda_system_allocator(), run_options, &result) == RUDA_BYTECODE_RUN_PANIC);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
    RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4000") == 0);
    RUDA_EXPECT(panic.message.length == sizeof(message)
        && memcmp(panic.message.data, message, sizeof(message)) == 0);
    RUDA_EXPECT(memcmp(&panic.symbol, &leaf_symbol, sizeof(leaf_symbol)) == 0
        && panic.span.source == 0 && panic.span.start == 201
        && panic.span.end == 202 && panic.frames.length == 2);
    frame = ruda_bytecode_panic_frame(&panic, 0);
    RUDA_EXPECT(frame != NULL && memcmp(&frame->symbol, &leaf_symbol,
        sizeof(leaf_symbol)) == 0 && frame->call_span.start == 201
        && frame->call_span.end == 202);
    frame = ruda_bytecode_panic_frame(&panic, 1);
    RUDA_EXPECT(frame != NULL && memcmp(&frame->symbol, &entry_symbol,
        sizeof(entry_symbol)) == 0 && frame->call_span.start == 210
        && frame->call_span.end == 211);
    ruda_bytecode_panic_dispose(&panic);
    ruda_bytecode_execution_dispose(&execution);
    ruda_bytecode_program_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_program_dispose(&program);
    ruda_mir_function_dispose(&entry);
    ruda_mir_function_dispose(&leaf);
}

static void ruda_test_mir_bytecode_direct_program(void) {
    const RudaSymbolId leaf_symbol = {{0x10}};
    const RudaSymbolId entry_symbol = {{0x20}};
    RudaMirFunction leaf;
    RudaMirFunction entry;
    const RudaMirFunction *functions[2];
    RudaBytecodeProgram program;
    RudaBytecodeProgram decoded;
    RudaOwnedBytes encoded;
    RudaBytecodeValue result;
    const RudaBytecodeProgramFunction *lowered_entry;
    const RudaBytecodeInstruction *instruction;

    memset(&program, 0, sizeof(program));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    RUDA_EXPECT(ruda_test_mir_bytecode_build_direct_leaf(&leaf, leaf_symbol));
    RUDA_EXPECT(ruda_test_mir_bytecode_build_direct_entry(&entry, entry_symbol,
        leaf_symbol));
    RUDA_EXPECT(ruda_mir_verify(&leaf) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_verify(&entry) == RUDA_MIR_VERIFY_OK);
    /* Deliberately unordered: program lowering owns stable symbol sorting. */
    functions[0] = &entry;
    functions[1] = &leaf;
    RUDA_EXPECT(ruda_mir_lower_bytecode_program(functions, 2, entry_symbol,
        ruda_system_allocator(), ruda_mir_bytecode_program_lower_default_options(),
        &program) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(program.entry_function == 1 && program.functions.length == 2);
    lowered_entry = ruda_bytecode_program_function(&program, 1);
    instruction = lowered_entry != NULL
        ? ruda_test_mir_bytecode_at(&lowered_entry->body, 0, 0) : NULL;
    RUDA_EXPECT(instruction != NULL && instruction->opcode == RUDA_OPCODE_CALL_DIRECT);
    if (instruction != NULL) {
        RUDA_EXPECT(instruction->operands[0].unsigned_value == 0);
        RUDA_EXPECT(instruction->operands[1].unsigned_value == 0);
        RUDA_EXPECT(instruction->span.source == 0 && instruction->span.start == 10);
    }
    RUDA_EXPECT(ruda_bytecode_encode_program(&program, ruda_system_allocator(),
        &encoded) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_decode_program(encoded.data, encoded.length,
        ruda_system_allocator(), ruda_bytecode_program_default_options(),
        &decoded) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_run_program(&decoded, NULL, 0,
        ruda_system_allocator(), ruda_bytecode_run_default_options(), &result)
        == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 42);
    ruda_bytecode_program_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_program_dispose(&program);
    ruda_mir_function_dispose(&entry);
    ruda_mir_function_dispose(&leaf);

    memset(&program, 0, sizeof(program));
    RUDA_EXPECT(ruda_test_mir_bytecode_build_void_leaf(&leaf, leaf_symbol));
    RUDA_EXPECT(ruda_test_mir_bytecode_build_void_entry(
        &entry,
        entry_symbol,
        leaf_symbol
    ));
    functions[0] = &entry;
    functions[1] = &leaf;
    RUDA_EXPECT(ruda_mir_lower_bytecode_program(
        functions,
        2,
        entry_symbol,
        ruda_system_allocator(),
        ruda_mir_bytecode_program_lower_default_options(),
        &program
    ) == RUDA_MIR_BYTECODE_LOWER_OK);
    lowered_entry = ruda_bytecode_program_function(&program, 1);
    instruction = lowered_entry != NULL
        ? ruda_test_mir_bytecode_at(&lowered_entry->body, 0, 0)
        : NULL;
    RUDA_EXPECT(instruction != NULL
        && instruction->opcode == RUDA_OPCODE_CALL_DIRECT_VOID);
    if (instruction != NULL) {
        RUDA_EXPECT(instruction->operands[0].unsigned_value == 0);
        RUDA_EXPECT(instruction->span.start == 30);
    }
    RUDA_EXPECT(ruda_bytecode_run_program(
        &program,
        NULL,
        0,
        ruda_system_allocator(),
        ruda_bytecode_run_default_options(),
        &result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_VOID);
    ruda_bytecode_program_dispose(&program);
    ruda_mir_function_dispose(&entry);
    ruda_mir_function_dispose(&leaf);
}

static bool ruda_test_mir_bytecode_build_f64_leaf(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    uint64_t bits
) {
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;

    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(
            function, ruda_system_allocator(), symbol, NULL, 0, RUDA_TYPE_F64,
            ruda_mir_function_default_options()
        ) || !ruda_mir_add_register(function, RUDA_TYPE_F64, &result)
        || !ruda_mir_add_block(function, &block)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_CONSTANT_F64, result, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 620
    );
    instruction.f64_bits = bits;
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(
            function, block, ruda_test_mir_bytecode_terminator(
                RUDA_MIR_RETURN, result, RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK,
                621
            )
        )) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_mir_bytecode_build_f64_entry(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    RudaSymbolId callee
) {
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    RudaMirInstruction instruction;

    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(
            function, ruda_system_allocator(), symbol, NULL, 0, RUDA_TYPE_F64,
            ruda_mir_function_default_options()
        ) || !ruda_mir_add_register(function, RUDA_TYPE_F64, &result)
        || !ruda_mir_add_block(function, &block)
        || !ruda_mir_add_call(function, callee, RUDA_TYPE_F64, NULL, 0, &call)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_CALL, result, RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0,
        false, 630
    );
    instruction.call = call;
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(
            function, block, ruda_test_mir_bytecode_terminator(
                RUDA_MIR_RETURN, result, RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK,
                631
            )
        )) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static void ruda_test_mir_bytecode_f64(void) {
    static const uint64_t left_bits = UINT64_C(0x8000000000000000);
    static const uint64_t right_bits = UINT64_C(0x0000000000000000);
    static const uint64_t nan_bits = UINT64_C(0x7ff8000000000042);
    RudaMirFunction input;
    RudaBytecodeFunction lowered;
    RudaBytecodeFunction decoded;
    RudaOwnedBytes encoded;
    RudaMirRegister left = RUDA_MIR_NO_REGISTER;
    RudaMirRegister right = RUDA_MIR_NO_REGISTER;
    RudaMirRegister copied = RUDA_MIR_NO_REGISTER;
    RudaMirRegister sum = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaMirValue mir_result;
    RudaBytecodeValue vm_result;
    const RudaBytecodeInstruction *lowered_instruction;

    memset(&input, 0, sizeof(input));
    memset(&lowered, 0, sizeof(lowered));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    RUDA_EXPECT(ruda_mir_function_init(
        &input, ruda_system_allocator(), (RudaSymbolId){{0x64}}, NULL, 0,
        RUDA_TYPE_F64, ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&input, RUDA_TYPE_F64, &left));
    RUDA_EXPECT(ruda_mir_add_register(&input, RUDA_TYPE_F64, &right));
    RUDA_EXPECT(ruda_mir_add_register(&input, RUDA_TYPE_F64, &copied));
    RUDA_EXPECT(ruda_mir_add_register(&input, RUDA_TYPE_F64, &sum));
    RUDA_EXPECT(ruda_mir_add_block(&input, &block));
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_CONSTANT_F64, left, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 600
    );
    instruction.f64_bits = left_bits;
    RUDA_EXPECT(ruda_mir_append_instruction(&input, block, &instruction));
    instruction.target = right;
    instruction.f64_bits = right_bits;
    RUDA_EXPECT(ruda_mir_append_instruction(&input, block, &instruction));
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_COPY, copied, left, RUDA_MIR_NO_REGISTER, 0, false, 602
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&input, block, &instruction));
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_ADD_F64, sum, copied, right, 0, false, 603
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&input, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &input, block, ruda_test_mir_bytecode_terminator(
            RUDA_MIR_RETURN, copied, RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 604
        )
    ));
    RUDA_EXPECT(ruda_mir_verify(&input) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(
        &input, ruda_system_allocator(), ruda_test_mir_bytecode_options(),
        &lowered
    ) == RUDA_MIR_BYTECODE_LOWER_OK);
    lowered_instruction = ruda_test_mir_bytecode_at(&lowered, 0, 0);
    RUDA_EXPECT(lowered_instruction != NULL
        && lowered_instruction->opcode == RUDA_OPCODE_CONST_F64
        && lowered_instruction->operands[1].unsigned_value == left_bits
        && lowered_instruction->span.source == 2
        && lowered_instruction->span.start == 600);
    lowered_instruction = ruda_test_mir_bytecode_at(&lowered, 0, 3);
    RUDA_EXPECT(lowered_instruction != NULL
        && lowered_instruction->opcode == RUDA_OPCODE_F64_ADD);
    RUDA_EXPECT(ruda_bytecode_encode_function(
        &lowered, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_decode_function(
        encoded.data, encoded.length, ruda_system_allocator(),
        ruda_bytecode_function_default_options(), &decoded
    ) == RUDA_BYTECODE_CODEC_OK);
    memset(&mir_result, 0, sizeof(mir_result));
    memset(&vm_result, 0, sizeof(vm_result));
    RUDA_EXPECT(ruda_mir_interpret(
        &input, NULL, 0, ruda_system_allocator(),
        ruda_mir_interpret_default_options(), &mir_result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(ruda_bytecode_run(
        &decoded, NULL, 0, ruda_system_allocator(),
        ruda_bytecode_run_default_options(), &vm_result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(mir_result.type == RUDA_TYPE_F64
        && vm_result.type == RUDA_BYTECODE_TYPE_F64
        && mir_result.as.f64_bits == left_bits
        && vm_result.as.f64_bits == left_bits);
    ruda_bytecode_function_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_function_dispose(&lowered);
    ruda_mir_function_dispose(&input);

    /* A NaN constant survives lowering, codec, and return without rewriting. */
    memset(&input, 0, sizeof(input));
    memset(&lowered, 0, sizeof(lowered));
    RUDA_EXPECT(ruda_mir_function_init(
        &input, ruda_system_allocator(), (RudaSymbolId){{0x65}}, NULL, 0,
        RUDA_TYPE_F64, ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&input, RUDA_TYPE_F64, &left));
    RUDA_EXPECT(ruda_mir_add_block(&input, &block));
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_CONSTANT_F64, left, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 610
    );
    instruction.f64_bits = nan_bits;
    RUDA_EXPECT(ruda_mir_append_instruction(&input, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &input, block, ruda_test_mir_bytecode_terminator(
            RUDA_MIR_RETURN, left, RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 611
        )
    ));
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(
        &input, ruda_system_allocator(), ruda_test_mir_bytecode_options(),
        &lowered
    ) == RUDA_MIR_BYTECODE_LOWER_OK);
    memset(&vm_result, 0, sizeof(vm_result));
    RUDA_EXPECT(ruda_bytecode_run(
        &lowered, NULL, 0, ruda_system_allocator(),
        ruda_bytecode_run_default_options(), &vm_result
    ) == RUDA_BYTECODE_RUN_OK && vm_result.as.f64_bits == nan_bits);
    ruda_bytecode_function_dispose(&lowered);
    ruda_mir_function_dispose(&input);

    {
        const RudaSymbolId leaf_symbol = {{0x66}};
        const RudaSymbolId entry_symbol = {{0x67}};
        const RudaMirFunction *functions[2];
        RudaMirFunction leaf;
        RudaMirFunction entry;
        RudaBytecodeProgram program;

        memset(&leaf, 0, sizeof(leaf));
        memset(&entry, 0, sizeof(entry));
        memset(&program, 0, sizeof(program));
        RUDA_EXPECT(ruda_test_mir_bytecode_build_f64_leaf(
            &leaf, leaf_symbol, nan_bits
        ));
        RUDA_EXPECT(ruda_test_mir_bytecode_build_f64_entry(
            &entry, entry_symbol, leaf_symbol
        ));
        functions[0] = &entry;
        functions[1] = &leaf;
        RUDA_EXPECT(ruda_mir_lower_bytecode_program(
            functions, 2, entry_symbol, ruda_system_allocator(),
            ruda_mir_bytecode_program_lower_default_options(), &program
        ) == RUDA_MIR_BYTECODE_LOWER_OK);
        memset(&vm_result, 0, sizeof(vm_result));
        RUDA_EXPECT(ruda_bytecode_run_program(
            &program, NULL, 0, ruda_system_allocator(),
            ruda_bytecode_run_default_options(), &vm_result
        ) == RUDA_BYTECODE_RUN_OK
            && vm_result.type == RUDA_BYTECODE_TYPE_F64
            && vm_result.as.f64_bits == nan_bits);
        ruda_bytecode_program_dispose(&program);
        ruda_mir_function_dispose(&entry);
        ruda_mir_function_dispose(&leaf);
    }
}

static bool ruda_test_mir_bytecode_build_array(
    RudaMirFunction *function,
    int64_t access_index
) {
    const RudaTypeId array_type = RUDA_TYPE_FIRST_DYNAMIC + 100;
    const RudaSymbolId symbol = {{0x71}};
    RudaMirRegister first = RUDA_MIR_NO_REGISTER;
    RudaMirRegister second = RUDA_MIR_NO_REGISTER;
    RudaMirRegister array_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister length = RUDA_MIR_NO_REGISTER;
    RudaMirRegister index = RUDA_MIR_NO_REGISTER;
    RudaMirRegister replacement = RUDA_MIR_NO_REGISTER;
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirRegister checked = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirArrayId array = RUDA_MIR_NO_ARRAY;
    RudaMirArrayConstructionId construction =
        RUDA_MIR_NO_ARRAY_CONSTRUCTION;
    RudaMirArrayArgument arguments[2];
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(
            function, ruda_system_allocator(), symbol, NULL, 0,
            RUDA_TYPE_F64, ruda_mir_function_default_options()
        ) || !ruda_mir_add_register(function, RUDA_TYPE_F64, &first)
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &second)
        || !ruda_mir_add_register(function, array_type, &array_value)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &length)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &index)
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &replacement)
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &result)
        || !ruda_mir_add_register(function, array_type, &checked)
        || !ruda_mir_add_array(function, array_type, RUDA_TYPE_F64, &array)
        || !ruda_mir_add_block(function, &block)) {
        goto failure;
    }
    arguments[0].register_id = first;
    arguments[0].type = RUDA_TYPE_F64;
    arguments[1].register_id = second;
    arguments[1].type = RUDA_TYPE_F64;
    if (!ruda_mir_add_array_construction(
            function, array, arguments, 2, &construction
        )) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_CONSTANT_F64, first, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 800
    );
    instruction.f64_bits = UINT64_C(0x3ff8000000000000);
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_CONSTANT_F64, second, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 801
    );
    instruction.f64_bits = UINT64_C(0x4004000000000000);
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_NEW;
    instruction.target = array_value;
    instruction.array = array;
    instruction.array_construction = construction;
    instruction.span = (RudaMirSpan){802, 803};
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_LENGTH;
    instruction.target = length;
    instruction.first = array_value;
    instruction.array = array;
    instruction.span = (RudaMirSpan){803, 804};
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_CONSTANT_I64, index, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, access_index, false, 804
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_REQUIRE_INDEX;
    instruction.target = checked;
    instruction.first = array_value;
    instruction.second = index;
    instruction.array = array;
    instruction.span = (RudaMirSpan){805, 806};
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_CONSTANT_F64, replacement, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 806
    );
    instruction.f64_bits = UINT64_C(0x4022000000000000);
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_SET;
    instruction.target = result;
    instruction.first = checked;
    instruction.second = index;
    instruction.third = replacement;
    instruction.array = array;
    instruction.span = (RudaMirSpan){805, 806};
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_GET;
    instruction.target = result;
    instruction.first = array_value;
    instruction.second = index;
    instruction.array = array;
    instruction.span = (RudaMirSpan){807, 808};
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(
            function, block, ruda_test_mir_bytecode_terminator(
                RUDA_MIR_RETURN, result, RUDA_MIR_NO_BLOCK,
                RUDA_MIR_NO_BLOCK, 808
            )
        )) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}

static void ruda_test_mir_bytecode_arrays(void) {
    RudaMirFunction function;
    RudaMirExecution mir_execution;
    RudaMirPanic mir_panic;
    RudaMirValue mir_result;
    RudaMirInterpretOptions mir_options;
    RudaBytecodeFunction lowered;
    RudaBytecodeFunction decoded;
    RudaBytecodeExecution vm_execution;
    RudaBytecodePanic vm_panic;
    RudaBytecodeValue vm_result;
    RudaBytecodeRunOptions vm_options;
    RudaOwnedBytes encoded;
    const RudaBytecodeInstruction *instruction;
    memset(&function, 0, sizeof(function));
    memset(&mir_execution, 0, sizeof(mir_execution));
    memset(&mir_panic, 0, sizeof(mir_panic));
    memset(&lowered, 0, sizeof(lowered));
    memset(&decoded, 0, sizeof(decoded));
    memset(&vm_execution, 0, sizeof(vm_execution));
    memset(&vm_panic, 0, sizeof(vm_panic));
    memset(&encoded, 0, sizeof(encoded));
    RUDA_EXPECT(ruda_test_mir_bytecode_build_array(&function, 1));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_execution_init(
        &mir_execution, ruda_system_allocator(),
        ruda_mir_execution_default_options()
    ));
    mir_options = ruda_mir_interpret_default_options();
    mir_options.execution = &mir_execution;
    mir_options.panic = &mir_panic;
    memset(&mir_result, 0, sizeof(mir_result));
    RUDA_EXPECT(ruda_mir_interpret(
        &function, NULL, 0, ruda_system_allocator(), mir_options, &mir_result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(mir_result.type == RUDA_TYPE_F64
        && mir_result.as.f64_bits == UINT64_C(0x4022000000000000));
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(
        &function, ruda_system_allocator(), ruda_test_mir_bytecode_options(),
        &lowered
    ) == RUDA_MIR_BYTECODE_LOWER_OK);
    instruction = ruda_test_mir_bytecode_at(&lowered, 0, 5);
    RUDA_EXPECT(instruction != NULL
        && instruction->opcode == RUDA_OPCODE_ARRAY_REQUIRE_INDEX);
    instruction = ruda_test_mir_bytecode_at(&lowered, 0, 7);
    RUDA_EXPECT(instruction != NULL
        && instruction->opcode == RUDA_OPCODE_ARRAY_SET);
    instruction = ruda_test_mir_bytecode_at(&lowered, 0, 8);
    RUDA_EXPECT(instruction != NULL && instruction->opcode == RUDA_OPCODE_COPY);
    RUDA_EXPECT(ruda_bytecode_encode_function(
        &lowered, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_decode_function(
        encoded.data, encoded.length, ruda_system_allocator(),
        ruda_bytecode_function_default_options(), &decoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &vm_execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()
    ));
    RUDA_EXPECT(ruda_bytecode_panic_init(&vm_panic, ruda_system_allocator()));
    vm_options = ruda_bytecode_run_default_options();
    vm_options.execution = &vm_execution;
    vm_options.panic = &vm_panic;
    memset(&vm_result, 0, sizeof(vm_result));
    RUDA_EXPECT(ruda_bytecode_run(
        &decoded, NULL, 0, ruda_system_allocator(), vm_options, &vm_result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(vm_result.type == RUDA_BYTECODE_TYPE_F64
        && vm_result.as.f64_bits == mir_result.as.f64_bits);
    ruda_bytecode_panic_dispose(&vm_panic);
    ruda_bytecode_execution_dispose(&vm_execution);
    ruda_bytecode_function_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_function_dispose(&lowered);
    ruda_mir_panic_dispose(&mir_panic);
    ruda_mir_execution_dispose(&mir_execution);
    ruda_mir_function_dispose(&function);

    memset(&function, 0, sizeof(function));
    memset(&mir_execution, 0, sizeof(mir_execution));
    memset(&mir_panic, 0, sizeof(mir_panic));
    memset(&lowered, 0, sizeof(lowered));
    memset(&vm_execution, 0, sizeof(vm_execution));
    memset(&vm_panic, 0, sizeof(vm_panic));
    RUDA_EXPECT(ruda_test_mir_bytecode_build_array(&function, -1));
    RUDA_EXPECT(ruda_mir_execution_init(
        &mir_execution, ruda_system_allocator(),
        ruda_mir_execution_default_options()
    ));
    mir_options = ruda_mir_interpret_default_options();
    mir_options.execution = &mir_execution;
    mir_options.panic = &mir_panic;
    RUDA_EXPECT(ruda_mir_interpret(
        &function, NULL, 0, ruda_system_allocator(), mir_options, &mir_result
    ) == RUDA_MIR_INTERPRET_PANIC);
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(
        &function, ruda_system_allocator(), ruda_test_mir_bytecode_options(),
        &lowered
    ) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &vm_execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()
    ));
    RUDA_EXPECT(ruda_bytecode_panic_init(&vm_panic, ruda_system_allocator()));
    vm_options = ruda_bytecode_run_default_options();
    vm_options.execution = &vm_execution;
    vm_options.panic = &vm_panic;
    RUDA_EXPECT(ruda_bytecode_run(
        &lowered, NULL, 0, ruda_system_allocator(), vm_options, &vm_result
    ) == RUDA_BYTECODE_RUN_PANIC);
    RUDA_EXPECT(mir_panic.code != NULL && vm_panic.code != NULL
        && strcmp(mir_panic.code, vm_panic.code) == 0
        && mir_panic.message.length == vm_panic.message.length
        && memcmp(mir_panic.message.data, vm_panic.message.data,
            mir_panic.message.length) == 0
        && mir_panic.span.start == vm_panic.span.start
        && mir_panic.span.end == vm_panic.span.end);
    ruda_bytecode_panic_dispose(&vm_panic);
    ruda_bytecode_execution_dispose(&vm_execution);
    ruda_bytecode_function_dispose(&lowered);
    ruda_mir_panic_dispose(&mir_panic);
    ruda_mir_execution_dispose(&mir_execution);
    ruda_mir_function_dispose(&function);
}


void ruda_test_mir_bytecode_programs(void) {
    ruda_test_mir_bytecode_direct_program();
    ruda_test_mir_bytecode_string_panic_program();
    ruda_test_mir_bytecode_f64();
    ruda_test_mir_bytecode_arrays();
}
