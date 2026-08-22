#include "test_mir_bytecode_support.h"

#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"

static bool mmay_test_mir_bytecode_build_direct_leaf(
    MarimayMirFunction *function,
    MarimaySymbolId symbol
) {
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(function, mmay_system_allocator(), symbol, NULL,
            0, MMAY_TYPE_I64, mmay_mir_function_default_options())
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &result)
        || !mmay_mir_add_block(function, &block)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_mir_bytecode_instruction(MMAY_MIR_CONSTANT_I64,
        result, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 42, false, 20);
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(function, block,
            mmay_test_mir_bytecode_terminator(MMAY_MIR_RETURN, result,
                MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 21))) {
        mmay_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool mmay_test_mir_bytecode_build_direct_entry(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    MarimaySymbolId callee
) {
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirCallId call = MMAY_MIR_NO_CALL;
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(function, mmay_system_allocator(), symbol, NULL,
            0, MMAY_TYPE_I64, mmay_mir_function_default_options())
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &result)
        || !mmay_mir_add_block(function, &block)
        || !mmay_mir_add_call(function, callee, MMAY_TYPE_I64, NULL, 0, &call)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_mir_bytecode_instruction(MMAY_MIR_CALL, result,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0, false, 10);
    instruction.call = call;
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(function, block,
            mmay_test_mir_bytecode_terminator(MMAY_MIR_RETURN, result,
                MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 11))) {
        mmay_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool mmay_test_mir_bytecode_build_void_leaf(
    MarimayMirFunction *function,
    MarimaySymbolId symbol
) {
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(
            function,
            mmay_system_allocator(),
            symbol,
            NULL,
            0,
            MMAY_TYPE_VOID,
            mmay_mir_function_default_options()
        ) || !mmay_mir_add_block(function, &block)
        || !mmay_mir_set_terminator(
            function,
            block,
            mmay_test_mir_bytecode_terminator(
                MMAY_MIR_RETURN,
                MMAY_MIR_NO_REGISTER,
                MMAY_MIR_NO_BLOCK,
                MMAY_MIR_NO_BLOCK,
                31
            )
        )) {
        mmay_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool mmay_test_mir_bytecode_build_void_entry(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    MarimaySymbolId callee
) {
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirCallId call = MMAY_MIR_NO_CALL;
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(
            function,
            mmay_system_allocator(),
            symbol,
            NULL,
            0,
            MMAY_TYPE_VOID,
            mmay_mir_function_default_options()
        ) || !mmay_mir_add_register(function, MMAY_TYPE_VOID, &result)
        || !mmay_mir_add_block(function, &block)
        || !mmay_mir_add_call(
            function,
            callee,
            MMAY_TYPE_VOID,
            NULL,
            0,
            &call
        )) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_CALL,
        result,
        MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER,
        0,
        false,
        30
    );
    instruction.call = call;
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(
            function,
            block,
            mmay_test_mir_bytecode_terminator(
                MMAY_MIR_RETURN,
                MMAY_MIR_NO_REGISTER,
                MMAY_MIR_NO_BLOCK,
                MMAY_MIR_NO_BLOCK,
                31
            )
        )) {
        mmay_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool mmay_test_mir_bytecode_build_string_panic_leaf(
    MarimayMirFunction *function,
    MarimaySymbolId symbol
) {
    static const unsigned char message[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirStringId string_id = MMAY_MIR_NO_STRING;
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(function, mmay_system_allocator(), symbol, NULL,
            0, MMAY_TYPE_NEVER, mmay_mir_function_default_options())
        || !mmay_mir_add_register(function, MMAY_TYPE_STRING, &value)
        || !mmay_mir_add_block(function, &block)
        || !mmay_mir_add_string_constant(function, message, sizeof(message),
            &string_id)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_mir_bytecode_instruction(MMAY_MIR_CONSTANT_STRING,
        value, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0, false, 200);
    instruction.string_constant = string_id;
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(function, block,
            mmay_test_mir_bytecode_terminator(MMAY_MIR_PANIC, value,
                MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 201))) {
        mmay_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool mmay_test_mir_bytecode_build_never_entry(
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
    terminator = mmay_test_mir_bytecode_terminator(MMAY_MIR_CALL_NEVER,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 210);
    terminator.call = call;
    if (!mmay_mir_set_terminator(function, block, terminator)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    return true;
}

static void mmay_test_mir_bytecode_string_panic_program(void) {
    static const unsigned char message[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    const MarimaySymbolId leaf_symbol = {{0x10}};
    const MarimaySymbolId entry_symbol = {{0x20}};
    MarimayMirFunction leaf;
    MarimayMirFunction entry;
    const MarimayMirFunction *functions[2];
    MarimayMirExecution mir_execution;
    MarimayMirPanic mir_panic;
    MarimayMirInterpretOptions mir_options;
    MarimayMirValue mir_result;
    MarimayBytecodeProgram program;
    MarimayBytecodeProgram decoded;
    MarimayBytecodeExecution execution;
    MarimayBytecodePanic panic;
    MarimayBytecodeRunOptions run_options;
    MarimayBytecodeValue result;
    MarimayOwnedBytes encoded;
    const MarimayBytecodeProgramFunction *lowered_leaf;
    const MarimayBytecodeProgramFunction *lowered_entry;
    const MarimayBytecodeInstruction *instruction;
    const MarimayBytecodePanicFrame *frame;

    memset(&mir_execution, 0, sizeof(mir_execution));
    memset(&mir_panic, 0, sizeof(mir_panic));
    memset(&mir_result, 0, sizeof(mir_result));
    memset(&program, 0, sizeof(program));
    memset(&decoded, 0, sizeof(decoded));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    memset(&encoded, 0, sizeof(encoded));
    MMAY_EXPECT(mmay_test_mir_bytecode_build_string_panic_leaf(&leaf,
        leaf_symbol));
    MMAY_EXPECT(mmay_test_mir_bytecode_build_never_entry(&entry, entry_symbol,
        leaf_symbol));
    MMAY_EXPECT(mmay_mir_verify(&leaf) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_verify(&entry) == MMAY_MIR_VERIFY_OK);

    MMAY_EXPECT(mmay_mir_execution_init(&mir_execution, mmay_system_allocator(),
        mmay_mir_execution_default_options()));
    mir_options = mmay_mir_interpret_default_options();
    mir_options.execution = &mir_execution;
    mir_options.panic = &mir_panic;
    MMAY_EXPECT(mmay_mir_interpret(&leaf, NULL, 0, mmay_system_allocator(),
        mir_options, &mir_result) == MMAY_MIR_INTERPRET_PANIC);
    MMAY_EXPECT(mir_panic.code != NULL && strcmp(mir_panic.code, "G4000") == 0);
    MMAY_EXPECT(mir_panic.message.length == sizeof(message)
        && memcmp(mir_panic.message.data, message, sizeof(message)) == 0);
    MMAY_EXPECT(memcmp(&mir_panic.symbol, &leaf_symbol, sizeof(leaf_symbol)) == 0
        && mir_panic.span.start == 201 && mir_panic.span.end == 202);
    mmay_mir_panic_dispose(&mir_panic);
    mmay_mir_execution_dispose(&mir_execution);

    functions[0] = &entry;
    functions[1] = &leaf;
    MMAY_EXPECT(mmay_mir_lower_bytecode_program(functions, 2, entry_symbol,
        mmay_system_allocator(), mmay_mir_bytecode_program_lower_default_options(),
        &program) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(program.functions.length == 2 && program.entry_function == 1);
    lowered_leaf = mmay_bytecode_program_function(&program, 0);
    lowered_entry = mmay_bytecode_program_function(&program, 1);
    instruction = lowered_leaf != NULL
        ? mmay_test_mir_bytecode_at(&lowered_leaf->body, 0, 0) : NULL;
    MMAY_EXPECT(instruction != NULL && instruction->opcode == MMAY_OPCODE_CONST_STRING);
    instruction = lowered_leaf != NULL
        ? mmay_test_mir_bytecode_at(&lowered_leaf->body, 0, 1) : NULL;
    MMAY_EXPECT(instruction != NULL && instruction->opcode == MMAY_OPCODE_PANIC_STRING
        && instruction->span.start == 201 && instruction->span.end == 202);
    instruction = lowered_entry != NULL
        ? mmay_test_mir_bytecode_at(&lowered_entry->body, 0, 0) : NULL;
    MMAY_EXPECT(instruction != NULL
        && instruction->opcode == MMAY_OPCODE_CALL_DIRECT_NEVER
        && instruction->operands[0].unsigned_value == 0
        && instruction->span.start == 210 && instruction->span.end == 211);
    MMAY_EXPECT(mmay_bytecode_encode_program(&program, mmay_system_allocator(),
        &encoded) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_program(encoded.data, encoded.length,
        mmay_system_allocator(), mmay_bytecode_program_default_options(),
        &decoded) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(&execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()));
    MMAY_EXPECT(mmay_bytecode_panic_init(&panic, mmay_system_allocator()));
    run_options = mmay_bytecode_run_default_options();
    run_options.execution = &execution;
    run_options.panic = &panic;
    result.type = MMAY_BYTECODE_TYPE_I64;
    result.as.i64 = 1234;
    MMAY_EXPECT(mmay_bytecode_run_program(&decoded, NULL, 0,
        mmay_system_allocator(), run_options, &result) == MMAY_BYTECODE_RUN_PANIC);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
    MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4000") == 0);
    MMAY_EXPECT(panic.message.length == sizeof(message)
        && memcmp(panic.message.data, message, sizeof(message)) == 0);
    MMAY_EXPECT(memcmp(&panic.symbol, &leaf_symbol, sizeof(leaf_symbol)) == 0
        && panic.span.source == 0 && panic.span.start == 201
        && panic.span.end == 202 && panic.frames.length == 2);
    frame = mmay_bytecode_panic_frame(&panic, 0);
    MMAY_EXPECT(frame != NULL && memcmp(&frame->symbol, &leaf_symbol,
        sizeof(leaf_symbol)) == 0 && frame->call_span.start == 201
        && frame->call_span.end == 202);
    frame = mmay_bytecode_panic_frame(&panic, 1);
    MMAY_EXPECT(frame != NULL && memcmp(&frame->symbol, &entry_symbol,
        sizeof(entry_symbol)) == 0 && frame->call_span.start == 210
        && frame->call_span.end == 211);
    mmay_bytecode_panic_dispose(&panic);
    mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_program_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_program_dispose(&program);
    mmay_mir_function_dispose(&entry);
    mmay_mir_function_dispose(&leaf);
}

static void mmay_test_mir_bytecode_direct_program(void) {
    const MarimaySymbolId leaf_symbol = {{0x10}};
    const MarimaySymbolId entry_symbol = {{0x20}};
    MarimayMirFunction leaf;
    MarimayMirFunction entry;
    const MarimayMirFunction *functions[2];
    MarimayBytecodeProgram program;
    MarimayBytecodeProgram decoded;
    MarimayOwnedBytes encoded;
    MarimayBytecodeValue result;
    const MarimayBytecodeProgramFunction *lowered_entry;
    const MarimayBytecodeInstruction *instruction;

    memset(&program, 0, sizeof(program));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    MMAY_EXPECT(mmay_test_mir_bytecode_build_direct_leaf(&leaf, leaf_symbol));
    MMAY_EXPECT(mmay_test_mir_bytecode_build_direct_entry(&entry, entry_symbol,
        leaf_symbol));
    MMAY_EXPECT(mmay_mir_verify(&leaf) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_verify(&entry) == MMAY_MIR_VERIFY_OK);
    /* Deliberately unordered: program lowering owns stable symbol sorting. */
    functions[0] = &entry;
    functions[1] = &leaf;
    MMAY_EXPECT(mmay_mir_lower_bytecode_program(functions, 2, entry_symbol,
        mmay_system_allocator(), mmay_mir_bytecode_program_lower_default_options(),
        &program) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(program.entry_function == 1 && program.functions.length == 2);
    lowered_entry = mmay_bytecode_program_function(&program, 1);
    instruction = lowered_entry != NULL
        ? mmay_test_mir_bytecode_at(&lowered_entry->body, 0, 0) : NULL;
    MMAY_EXPECT(instruction != NULL && instruction->opcode == MMAY_OPCODE_CALL_DIRECT);
    if (instruction != NULL) {
        MMAY_EXPECT(instruction->operands[0].unsigned_value == 0);
        MMAY_EXPECT(instruction->operands[1].unsigned_value == 0);
        MMAY_EXPECT(instruction->span.source == 0 && instruction->span.start == 10);
    }
    MMAY_EXPECT(mmay_bytecode_encode_program(&program, mmay_system_allocator(),
        &encoded) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_program(encoded.data, encoded.length,
        mmay_system_allocator(), mmay_bytecode_program_default_options(),
        &decoded) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_run_program(&decoded, NULL, 0,
        mmay_system_allocator(), mmay_bytecode_run_default_options(), &result)
        == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 42);
    mmay_bytecode_program_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_program_dispose(&program);
    mmay_mir_function_dispose(&entry);
    mmay_mir_function_dispose(&leaf);

    memset(&program, 0, sizeof(program));
    MMAY_EXPECT(mmay_test_mir_bytecode_build_void_leaf(&leaf, leaf_symbol));
    MMAY_EXPECT(mmay_test_mir_bytecode_build_void_entry(
        &entry,
        entry_symbol,
        leaf_symbol
    ));
    functions[0] = &entry;
    functions[1] = &leaf;
    MMAY_EXPECT(mmay_mir_lower_bytecode_program(
        functions,
        2,
        entry_symbol,
        mmay_system_allocator(),
        mmay_mir_bytecode_program_lower_default_options(),
        &program
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    lowered_entry = mmay_bytecode_program_function(&program, 1);
    instruction = lowered_entry != NULL
        ? mmay_test_mir_bytecode_at(&lowered_entry->body, 0, 0)
        : NULL;
    MMAY_EXPECT(instruction != NULL
        && instruction->opcode == MMAY_OPCODE_CALL_DIRECT_VOID);
    if (instruction != NULL) {
        MMAY_EXPECT(instruction->operands[0].unsigned_value == 0);
        MMAY_EXPECT(instruction->span.start == 30);
    }
    MMAY_EXPECT(mmay_bytecode_run_program(
        &program,
        NULL,
        0,
        mmay_system_allocator(),
        mmay_bytecode_run_default_options(),
        &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_VOID);
    mmay_bytecode_program_dispose(&program);
    mmay_mir_function_dispose(&entry);
    mmay_mir_function_dispose(&leaf);
}

static bool mmay_test_mir_bytecode_build_f64_leaf(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    uint64_t bits
) {
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;

    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(
            function, mmay_system_allocator(), symbol, NULL, 0, MMAY_TYPE_F64,
            mmay_mir_function_default_options()
        ) || !mmay_mir_add_register(function, MMAY_TYPE_F64, &result)
        || !mmay_mir_add_block(function, &block)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_CONSTANT_F64, result, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 620
    );
    instruction.f64_bits = bits;
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(
            function, block, mmay_test_mir_bytecode_terminator(
                MMAY_MIR_RETURN, result, MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK,
                621
            )
        )) {
        mmay_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool mmay_test_mir_bytecode_build_f64_entry(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    MarimaySymbolId callee
) {
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirCallId call = MMAY_MIR_NO_CALL;
    MarimayMirInstruction instruction;

    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(
            function, mmay_system_allocator(), symbol, NULL, 0, MMAY_TYPE_F64,
            mmay_mir_function_default_options()
        ) || !mmay_mir_add_register(function, MMAY_TYPE_F64, &result)
        || !mmay_mir_add_block(function, &block)
        || !mmay_mir_add_call(function, callee, MMAY_TYPE_F64, NULL, 0, &call)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_CALL, result, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0,
        false, 630
    );
    instruction.call = call;
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(
            function, block, mmay_test_mir_bytecode_terminator(
                MMAY_MIR_RETURN, result, MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK,
                631
            )
        )) {
        mmay_mir_function_dispose(function);
        return false;
    }
    return true;
}

static void mmay_test_mir_bytecode_f64(void) {
    static const uint64_t left_bits = UINT64_C(0x8000000000000000);
    static const uint64_t right_bits = UINT64_C(0x0000000000000000);
    static const uint64_t nan_bits = UINT64_C(0x7ff8000000000042);
    MarimayMirFunction input;
    MarimayBytecodeFunction lowered;
    MarimayBytecodeFunction decoded;
    MarimayOwnedBytes encoded;
    MarimayMirRegister left = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister right = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister copied = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister sum = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    MarimayMirValue mir_result;
    MarimayBytecodeValue vm_result;
    const MarimayBytecodeInstruction *lowered_instruction;

    memset(&input, 0, sizeof(input));
    memset(&lowered, 0, sizeof(lowered));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    MMAY_EXPECT(mmay_mir_function_init(
        &input, mmay_system_allocator(), (MarimaySymbolId){{0x64}}, NULL, 0,
        MMAY_TYPE_F64, mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&input, MMAY_TYPE_F64, &left));
    MMAY_EXPECT(mmay_mir_add_register(&input, MMAY_TYPE_F64, &right));
    MMAY_EXPECT(mmay_mir_add_register(&input, MMAY_TYPE_F64, &copied));
    MMAY_EXPECT(mmay_mir_add_register(&input, MMAY_TYPE_F64, &sum));
    MMAY_EXPECT(mmay_mir_add_block(&input, &block));
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_CONSTANT_F64, left, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 600
    );
    instruction.f64_bits = left_bits;
    MMAY_EXPECT(mmay_mir_append_instruction(&input, block, &instruction));
    instruction.target = right;
    instruction.f64_bits = right_bits;
    MMAY_EXPECT(mmay_mir_append_instruction(&input, block, &instruction));
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_COPY, copied, left, MMAY_MIR_NO_REGISTER, 0, false, 602
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&input, block, &instruction));
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_ADD_F64, sum, copied, right, 0, false, 603
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&input, block, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &input, block, mmay_test_mir_bytecode_terminator(
            MMAY_MIR_RETURN, copied, MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 604
        )
    ));
    MMAY_EXPECT(mmay_mir_verify(&input) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(
        &input, mmay_system_allocator(), mmay_test_mir_bytecode_options(),
        &lowered
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    lowered_instruction = mmay_test_mir_bytecode_at(&lowered, 0, 0);
    MMAY_EXPECT(lowered_instruction != NULL
        && lowered_instruction->opcode == MMAY_OPCODE_CONST_F64
        && lowered_instruction->operands[1].unsigned_value == left_bits
        && lowered_instruction->span.source == 2
        && lowered_instruction->span.start == 600);
    lowered_instruction = mmay_test_mir_bytecode_at(&lowered, 0, 3);
    MMAY_EXPECT(lowered_instruction != NULL
        && lowered_instruction->opcode == MMAY_OPCODE_F64_ADD);
    MMAY_EXPECT(mmay_bytecode_encode_function(
        &lowered, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_function(
        encoded.data, encoded.length, mmay_system_allocator(),
        mmay_bytecode_function_default_options(), &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    memset(&mir_result, 0, sizeof(mir_result));
    memset(&vm_result, 0, sizeof(vm_result));
    MMAY_EXPECT(mmay_mir_interpret(
        &input, NULL, 0, mmay_system_allocator(),
        mmay_mir_interpret_default_options(), &mir_result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(mmay_bytecode_run(
        &decoded, NULL, 0, mmay_system_allocator(),
        mmay_bytecode_run_default_options(), &vm_result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(mir_result.type == MMAY_TYPE_F64
        && vm_result.type == MMAY_BYTECODE_TYPE_F64
        && mir_result.as.f64_bits == left_bits
        && vm_result.as.f64_bits == left_bits);
    mmay_bytecode_function_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_function_dispose(&lowered);
    mmay_mir_function_dispose(&input);

    /* A NaN constant survives lowering, codec, and return without rewriting. */
    memset(&input, 0, sizeof(input));
    memset(&lowered, 0, sizeof(lowered));
    MMAY_EXPECT(mmay_mir_function_init(
        &input, mmay_system_allocator(), (MarimaySymbolId){{0x65}}, NULL, 0,
        MMAY_TYPE_F64, mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&input, MMAY_TYPE_F64, &left));
    MMAY_EXPECT(mmay_mir_add_block(&input, &block));
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_CONSTANT_F64, left, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 610
    );
    instruction.f64_bits = nan_bits;
    MMAY_EXPECT(mmay_mir_append_instruction(&input, block, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &input, block, mmay_test_mir_bytecode_terminator(
            MMAY_MIR_RETURN, left, MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 611
        )
    ));
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(
        &input, mmay_system_allocator(), mmay_test_mir_bytecode_options(),
        &lowered
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    memset(&vm_result, 0, sizeof(vm_result));
    MMAY_EXPECT(mmay_bytecode_run(
        &lowered, NULL, 0, mmay_system_allocator(),
        mmay_bytecode_run_default_options(), &vm_result
    ) == MMAY_BYTECODE_RUN_OK && vm_result.as.f64_bits == nan_bits);
    mmay_bytecode_function_dispose(&lowered);
    mmay_mir_function_dispose(&input);

    {
        const MarimaySymbolId leaf_symbol = {{0x66}};
        const MarimaySymbolId entry_symbol = {{0x67}};
        const MarimayMirFunction *functions[2];
        MarimayMirFunction leaf;
        MarimayMirFunction entry;
        MarimayBytecodeProgram program;

        memset(&leaf, 0, sizeof(leaf));
        memset(&entry, 0, sizeof(entry));
        memset(&program, 0, sizeof(program));
        MMAY_EXPECT(mmay_test_mir_bytecode_build_f64_leaf(
            &leaf, leaf_symbol, nan_bits
        ));
        MMAY_EXPECT(mmay_test_mir_bytecode_build_f64_entry(
            &entry, entry_symbol, leaf_symbol
        ));
        functions[0] = &entry;
        functions[1] = &leaf;
        MMAY_EXPECT(mmay_mir_lower_bytecode_program(
            functions, 2, entry_symbol, mmay_system_allocator(),
            mmay_mir_bytecode_program_lower_default_options(), &program
        ) == MMAY_MIR_BYTECODE_LOWER_OK);
        memset(&vm_result, 0, sizeof(vm_result));
        MMAY_EXPECT(mmay_bytecode_run_program(
            &program, NULL, 0, mmay_system_allocator(),
            mmay_bytecode_run_default_options(), &vm_result
        ) == MMAY_BYTECODE_RUN_OK
            && vm_result.type == MMAY_BYTECODE_TYPE_F64
            && vm_result.as.f64_bits == nan_bits);
        mmay_bytecode_program_dispose(&program);
        mmay_mir_function_dispose(&entry);
        mmay_mir_function_dispose(&leaf);
    }
}

static bool mmay_test_mir_bytecode_build_array(
    MarimayMirFunction *function,
    int64_t access_index
) {
    const MarimayTypeId array_type = MMAY_TYPE_FIRST_DYNAMIC + 100;
    const MarimaySymbolId symbol = {{0x71}};
    MarimayMirRegister first = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister second = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister array_value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister length = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister index = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister replacement = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister checked = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirArrayId array = MMAY_MIR_NO_ARRAY;
    MarimayMirArrayConstructionId construction =
        MMAY_MIR_NO_ARRAY_CONSTRUCTION;
    MarimayMirArrayArgument arguments[2];
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(
            function, mmay_system_allocator(), symbol, NULL, 0,
            MMAY_TYPE_F64, mmay_mir_function_default_options()
        ) || !mmay_mir_add_register(function, MMAY_TYPE_F64, &first)
        || !mmay_mir_add_register(function, MMAY_TYPE_F64, &second)
        || !mmay_mir_add_register(function, array_type, &array_value)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &length)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &index)
        || !mmay_mir_add_register(function, MMAY_TYPE_F64, &replacement)
        || !mmay_mir_add_register(function, MMAY_TYPE_F64, &result)
        || !mmay_mir_add_register(function, array_type, &checked)
        || !mmay_mir_add_array(function, array_type, MMAY_TYPE_F64, &array)
        || !mmay_mir_add_block(function, &block)) {
        goto failure;
    }
    arguments[0].register_id = first;
    arguments[0].type = MMAY_TYPE_F64;
    arguments[1].register_id = second;
    arguments[1].type = MMAY_TYPE_F64;
    if (!mmay_mir_add_array_construction(
            function, array, arguments, 2, &construction
        )) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_CONSTANT_F64, first, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 800
    );
    instruction.f64_bits = UINT64_C(0x3ff8000000000000);
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_CONSTANT_F64, second, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 801
    );
    instruction.f64_bits = UINT64_C(0x4004000000000000);
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ARRAY_NEW;
    instruction.target = array_value;
    instruction.array = array;
    instruction.array_construction = construction;
    instruction.span = (MarimayMirSpan){802, 803};
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ARRAY_LENGTH;
    instruction.target = length;
    instruction.first = array_value;
    instruction.array = array;
    instruction.span = (MarimayMirSpan){803, 804};
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_CONSTANT_I64, index, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, access_index, false, 804
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ARRAY_REQUIRE_INDEX;
    instruction.target = checked;
    instruction.first = array_value;
    instruction.second = index;
    instruction.array = array;
    instruction.span = (MarimayMirSpan){805, 806};
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_CONSTANT_F64, replacement, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 806
    );
    instruction.f64_bits = UINT64_C(0x4022000000000000);
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ARRAY_SET;
    instruction.target = result;
    instruction.first = checked;
    instruction.second = index;
    instruction.third = replacement;
    instruction.array = array;
    instruction.span = (MarimayMirSpan){805, 806};
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ARRAY_GET;
    instruction.target = result;
    instruction.first = array_value;
    instruction.second = index;
    instruction.array = array;
    instruction.span = (MarimayMirSpan){807, 808};
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(
            function, block, mmay_test_mir_bytecode_terminator(
                MMAY_MIR_RETURN, result, MMAY_MIR_NO_BLOCK,
                MMAY_MIR_NO_BLOCK, 808
            )
        )) {
        goto failure;
    }
    return true;

failure:
    mmay_mir_function_dispose(function);
    return false;
}

static void mmay_test_mir_bytecode_arrays(void) {
    MarimayMirFunction function;
    MarimayMirExecution mir_execution;
    MarimayMirPanic mir_panic;
    MarimayMirValue mir_result;
    MarimayMirInterpretOptions mir_options;
    MarimayBytecodeFunction lowered;
    MarimayBytecodeFunction decoded;
    MarimayBytecodeExecution vm_execution;
    MarimayBytecodePanic vm_panic;
    MarimayBytecodeValue vm_result;
    MarimayBytecodeRunOptions vm_options;
    MarimayOwnedBytes encoded;
    const MarimayBytecodeInstruction *instruction;
    memset(&function, 0, sizeof(function));
    memset(&mir_execution, 0, sizeof(mir_execution));
    memset(&mir_panic, 0, sizeof(mir_panic));
    memset(&lowered, 0, sizeof(lowered));
    memset(&decoded, 0, sizeof(decoded));
    memset(&vm_execution, 0, sizeof(vm_execution));
    memset(&vm_panic, 0, sizeof(vm_panic));
    memset(&encoded, 0, sizeof(encoded));
    MMAY_EXPECT(mmay_test_mir_bytecode_build_array(&function, 1));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_execution_init(
        &mir_execution, mmay_system_allocator(),
        mmay_mir_execution_default_options()
    ));
    mir_options = mmay_mir_interpret_default_options();
    mir_options.execution = &mir_execution;
    mir_options.panic = &mir_panic;
    memset(&mir_result, 0, sizeof(mir_result));
    MMAY_EXPECT(mmay_mir_interpret(
        &function, NULL, 0, mmay_system_allocator(), mir_options, &mir_result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(mir_result.type == MMAY_TYPE_F64
        && mir_result.as.f64_bits == UINT64_C(0x4022000000000000));
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(
        &function, mmay_system_allocator(), mmay_test_mir_bytecode_options(),
        &lowered
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    instruction = mmay_test_mir_bytecode_at(&lowered, 0, 5);
    MMAY_EXPECT(instruction != NULL
        && instruction->opcode == MMAY_OPCODE_ARRAY_REQUIRE_INDEX);
    instruction = mmay_test_mir_bytecode_at(&lowered, 0, 7);
    MMAY_EXPECT(instruction != NULL
        && instruction->opcode == MMAY_OPCODE_ARRAY_SET);
    instruction = mmay_test_mir_bytecode_at(&lowered, 0, 8);
    MMAY_EXPECT(instruction != NULL && instruction->opcode == MMAY_OPCODE_COPY);
    MMAY_EXPECT(mmay_bytecode_encode_function(
        &lowered, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_function(
        encoded.data, encoded.length, mmay_system_allocator(),
        mmay_bytecode_function_default_options(), &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &vm_execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    MMAY_EXPECT(mmay_bytecode_panic_init(&vm_panic, mmay_system_allocator()));
    vm_options = mmay_bytecode_run_default_options();
    vm_options.execution = &vm_execution;
    vm_options.panic = &vm_panic;
    memset(&vm_result, 0, sizeof(vm_result));
    MMAY_EXPECT(mmay_bytecode_run(
        &decoded, NULL, 0, mmay_system_allocator(), vm_options, &vm_result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(vm_result.type == MMAY_BYTECODE_TYPE_F64
        && vm_result.as.f64_bits == mir_result.as.f64_bits);
    mmay_bytecode_panic_dispose(&vm_panic);
    mmay_bytecode_execution_dispose(&vm_execution);
    mmay_bytecode_function_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_function_dispose(&lowered);
    mmay_mir_panic_dispose(&mir_panic);
    mmay_mir_execution_dispose(&mir_execution);
    mmay_mir_function_dispose(&function);

    memset(&function, 0, sizeof(function));
    memset(&mir_execution, 0, sizeof(mir_execution));
    memset(&mir_panic, 0, sizeof(mir_panic));
    memset(&lowered, 0, sizeof(lowered));
    memset(&vm_execution, 0, sizeof(vm_execution));
    memset(&vm_panic, 0, sizeof(vm_panic));
    MMAY_EXPECT(mmay_test_mir_bytecode_build_array(&function, -1));
    MMAY_EXPECT(mmay_mir_execution_init(
        &mir_execution, mmay_system_allocator(),
        mmay_mir_execution_default_options()
    ));
    mir_options = mmay_mir_interpret_default_options();
    mir_options.execution = &mir_execution;
    mir_options.panic = &mir_panic;
    MMAY_EXPECT(mmay_mir_interpret(
        &function, NULL, 0, mmay_system_allocator(), mir_options, &mir_result
    ) == MMAY_MIR_INTERPRET_PANIC);
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(
        &function, mmay_system_allocator(), mmay_test_mir_bytecode_options(),
        &lowered
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &vm_execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    MMAY_EXPECT(mmay_bytecode_panic_init(&vm_panic, mmay_system_allocator()));
    vm_options = mmay_bytecode_run_default_options();
    vm_options.execution = &vm_execution;
    vm_options.panic = &vm_panic;
    MMAY_EXPECT(mmay_bytecode_run(
        &lowered, NULL, 0, mmay_system_allocator(), vm_options, &vm_result
    ) == MMAY_BYTECODE_RUN_PANIC);
    MMAY_EXPECT(mir_panic.code != NULL && vm_panic.code != NULL
        && strcmp(mir_panic.code, vm_panic.code) == 0
        && mir_panic.message.length == vm_panic.message.length
        && memcmp(mir_panic.message.data, vm_panic.message.data,
            mir_panic.message.length) == 0
        && mir_panic.span.start == vm_panic.span.start
        && mir_panic.span.end == vm_panic.span.end);
    mmay_bytecode_panic_dispose(&vm_panic);
    mmay_bytecode_execution_dispose(&vm_execution);
    mmay_bytecode_function_dispose(&lowered);
    mmay_mir_panic_dispose(&mir_panic);
    mmay_mir_execution_dispose(&mir_execution);
    mmay_mir_function_dispose(&function);
}


void mmay_test_mir_bytecode_programs(void) {
    mmay_test_mir_bytecode_direct_program();
    mmay_test_mir_bytecode_string_panic_program();
    mmay_test_mir_bytecode_f64();
    mmay_test_mir_bytecode_arrays();
}
