#include "test_mir_support.h"
#include "test_native_c_emitter_support.h"

#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/bytecode/codec.h"
#include "mmay/bytecode/mir_lower.h"
#include "mmay/bytecode/vm.h"
#include "mmay/mir/range.h"
#include "mmay/native/c_emitter.h"

enum {
    MMAY_TEST_RANGE_ARRAY_TYPE = MMAY_TYPE_FIRST_DYNAMIC + 91,
    MMAY_TEST_RANGE_RECORD_TYPE,
    MMAY_TEST_RANGE_INTERFACE_TYPE
};

static bool mmay_test_mir_range_build(
    MarimayMirFunction *function,
    MarimayAllocator allocator,
    MarimaySymbolId symbol,
    bool call_between_proof_and_access
) {
    MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister array_value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister index = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister proof = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister projected = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister stored = MMAY_MIR_NO_REGISTER;
    MarimayMirArrayId array = MMAY_MIR_NO_ARRAY;
    MarimayMirArrayConstructionId construction = MMAY_MIR_NO_ARRAY_CONSTRUCTION;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    MarimayMirArrayArgument argument;
    MarimayMirCallId call = MMAY_MIR_NO_CALL;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(
            function, allocator, symbol, NULL, 0, MMAY_TYPE_F64,
            mmay_mir_function_default_options()
        ) || !mmay_mir_add_register(function, MMAY_TYPE_F64, &value)
        || !mmay_mir_add_register(
            function, MMAY_TEST_RANGE_ARRAY_TYPE, &array_value
        ) || !mmay_mir_add_register(function, MMAY_TYPE_I64, &index)
        || !mmay_mir_add_register(
            function, MMAY_TEST_RANGE_ARRAY_TYPE, &proof
        ) || !mmay_mir_add_register(function, MMAY_TYPE_F64, &projected)
        || !mmay_mir_add_register(function, MMAY_TYPE_F64, &stored)
        || !mmay_mir_add_array(
            function, MMAY_TEST_RANGE_ARRAY_TYPE, MMAY_TYPE_F64, &array
        )) {
        mmay_mir_function_dispose(function);
        return false;
    }
    argument.register_id = value;
    argument.type = MMAY_TYPE_F64;
    if (!mmay_mir_add_array_construction(
            function, array, &argument, 1, &construction
        ) || !mmay_mir_add_block(function, &block)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_F64;
    instruction.target = value;
    instruction.f64_bits = UINT64_C(0x4014000000000000);
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ARRAY_NEW;
    instruction.target = array_value;
    instruction.array = array;
    instruction.array_construction = construction;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_I64;
    instruction.target = index;
    instruction.integer = 0;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ARRAY_REQUIRE_INDEX;
    instruction.target = proof;
    instruction.first = array_value;
    instruction.second = index;
    instruction.array = array;
    instruction.span.start = 17;
    instruction.span.end = 23;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    if (call_between_proof_and_access) {
        if (!mmay_mir_add_call(
                function, symbol, MMAY_TYPE_F64, NULL, 0, &call
            )) {
            mmay_mir_function_dispose(function);
            return false;
        }
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_CALL;
        instruction.target = projected;
        instruction.call = call;
        if (!mmay_mir_append_instruction(function, block, &instruction)) {
            mmay_mir_function_dispose(function);
            return false;
        }
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = call_between_proof_and_access
        ? MMAY_MIR_ARRAY_GET_IN_BOUNDS : MMAY_MIR_ARRAY_GET;
    instruction.target = projected;
    instruction.first = proof;
    instruction.second = index;
    instruction.array = array;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    if (!call_between_proof_and_access) {
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_ARRAY_SET;
        instruction.target = stored;
        instruction.first = proof;
        instruction.second = index;
        instruction.third = projected;
        instruction.array = array;
        if (!mmay_mir_append_instruction(function, block, &instruction)) {
            mmay_mir_function_dispose(function);
            return false;
        }
    } else {
        stored = projected;
    }
    if (!mmay_mir_set_terminator(
            function, block, mmay_test_mir_return(stored)
        )) {
        mmay_mir_function_dispose(function);
        return false;
    }
    return true;
}

static MarimayMirInstruction *mmay_test_mir_range_instruction(
    MarimayMirFunction *function,
    size_t index
) {
    MarimayMirBlock *block = function != NULL
        ? mmay_vector_at(&function->blocks, 0) : NULL;
    return block != NULL ? mmay_vector_at(&block->instructions, index) : NULL;
}

static void mmay_test_mir_range_optimizer(
    MarimaySymbolId symbol
) {
    MarimayMirFunction checked;
    MarimayMirFunction function;
    MarimayMirInstruction *proof = NULL;
    MarimayMirInstruction *get = NULL;
    MarimayMirInstruction *set = NULL;
    MarimayMirExecution execution;
    MarimayMirExecution checked_execution;
    MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
    MarimayMirValue result;
    MarimayMirValue checked_result;
    MarimayBytecodeFunction lowered;
    MarimayBytecodeFunction checked_lowered;
    MarimayBytecodeFunction decoded;
    MarimayBytecodeExecution bytecode_execution;
    MarimayBytecodeExecution checked_bytecode_execution;
    MarimayBytecodeRunOptions bytecode_options;
    MarimayBytecodeRunOptions checked_bytecode_options;
    MarimayBytecodeValue bytecode_result;
    MarimayBytecodeValue checked_bytecode_result;
    MarimayOwnedBytes encoded;
    MarimayBytecodeBlock *block = NULL;
    MarimayBytecodeInstruction *instruction = NULL;
    MarimayNativeCProgram native_program;
    MarimayNativeCProgram checked_native_program;
    MarimayOwnedBytes generated;
    MarimayOwnedBytes checked_generated;
    const MarimayMirFunction *functions[1];
    const MarimayMirFunction *checked_functions[1];
    size_t instruction_index = 0;
    bool corrupt_rejected = false;
    memset(&execution, 0, sizeof(execution));
    memset(&checked_execution, 0, sizeof(checked_execution));
    memset(&lowered, 0, sizeof(lowered));
    memset(&checked_lowered, 0, sizeof(checked_lowered));
    memset(&decoded, 0, sizeof(decoded));
    memset(&bytecode_execution, 0, sizeof(bytecode_execution));
    memset(&checked_bytecode_execution, 0, sizeof(checked_bytecode_execution));
    memset(&encoded, 0, sizeof(encoded));
    memset(&native_program, 0, sizeof(native_program));
    memset(&checked_native_program, 0, sizeof(checked_native_program));
    memset(&generated, 0, sizeof(generated));
    memset(&checked_generated, 0, sizeof(checked_generated));
    MMAY_EXPECT(mmay_test_mir_range_build(
        &checked, mmay_system_allocator(), symbol, false
    ));
    MMAY_EXPECT(mmay_mir_verify(&checked) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_execution_init(
        &checked_execution, mmay_system_allocator(),
        mmay_mir_execution_default_options()
    ));
    options.execution = &checked_execution;
    MMAY_EXPECT(mmay_mir_interpret(
        &checked, NULL, 0, mmay_system_allocator(), options, &checked_result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(checked_result.type == MMAY_TYPE_F64
        && checked_result.as.f64_bits == UINT64_C(0x4014000000000000));
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(
        &checked, mmay_system_allocator(),
        mmay_mir_bytecode_lower_default_options(), &checked_lowered
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_verify(&checked_lowered)
        == MMAY_BYTECODE_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &checked_bytecode_execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    checked_bytecode_options = mmay_bytecode_run_default_options();
    checked_bytecode_options.execution = &checked_bytecode_execution;
    memset(&checked_bytecode_result, 0, sizeof(checked_bytecode_result));
    MMAY_EXPECT(mmay_bytecode_run(
        &checked_lowered, NULL, 0, mmay_system_allocator(),
        checked_bytecode_options, &checked_bytecode_result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(checked_bytecode_result.type == MMAY_BYTECODE_TYPE_F64
        && checked_bytecode_result.as.f64_bits == UINT64_C(0x4014000000000000));
    MMAY_EXPECT(checked_bytecode_execution.managed_heap.roots == NULL);
    checked_functions[0] = &checked;
    checked_native_program.functions = checked_functions;
    checked_native_program.function_count = 1;
    checked_native_program.entry_symbol = symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &checked_native_program, mmay_system_allocator(),
        mmay_native_c_emit_default_options(), &checked_generated
    ) == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    mmay_test_native_compile_and_run(
        &checked_generated, "ok:f64:4014000000000000\n", 0
    );
#endif
    MMAY_EXPECT(mmay_test_mir_range_build(
        &function, mmay_system_allocator(), symbol, false
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_optimize_ranges(&function)
        == MMAY_MIR_RANGE_OPTIMIZE_OK);
    proof = mmay_test_mir_range_instruction(&function, 3);
    get = mmay_test_mir_range_instruction(&function, 4);
    set = mmay_test_mir_range_instruction(&function, 5);
    MMAY_EXPECT(proof != NULL
        && proof->kind == MMAY_MIR_ARRAY_ASSUME_INDEX);
    MMAY_EXPECT(get != NULL && get->kind == MMAY_MIR_ARRAY_GET_IN_BOUNDS);
    MMAY_EXPECT(set != NULL && set->kind == MMAY_MIR_ARRAY_SET_IN_BOUNDS);
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_execution_init(
        &execution, mmay_system_allocator(), mmay_mir_execution_default_options()
    ));
    options.execution = &execution;
    MMAY_EXPECT(mmay_mir_interpret(
        &function, NULL, 0, mmay_system_allocator(), options, &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.type == MMAY_TYPE_F64
        && result.as.f64_bits == UINT64_C(0x4014000000000000));
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(
        &function, mmay_system_allocator(),
        mmay_mir_bytecode_lower_default_options(), &lowered
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_verify(&lowered) == MMAY_BYTECODE_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &bytecode_execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    bytecode_options = mmay_bytecode_run_default_options();
    bytecode_options.execution = &bytecode_execution;
    memset(&bytecode_result, 0, sizeof(bytecode_result));
    MMAY_EXPECT(mmay_bytecode_run(
        &lowered, NULL, 0, mmay_system_allocator(), bytecode_options,
        &bytecode_result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(bytecode_result.type == MMAY_BYTECODE_TYPE_F64
        && bytecode_result.as.f64_bits == UINT64_C(0x4014000000000000));
    MMAY_EXPECT(bytecode_execution.managed_heap.roots == NULL);
    MMAY_EXPECT(mmay_bytecode_encode_function(
        &lowered, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_function(
        encoded.data, encoded.length, mmay_system_allocator(),
        mmay_bytecode_function_default_options(), &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_verify(&decoded) == MMAY_BYTECODE_VERIFY_OK);
    memset(&bytecode_result, 0, sizeof(bytecode_result));
    MMAY_EXPECT(mmay_bytecode_run(
        &decoded, NULL, 0, mmay_system_allocator(), bytecode_options,
        &bytecode_result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(bytecode_result.type == MMAY_BYTECODE_TYPE_F64
        && bytecode_result.as.f64_bits == UINT64_C(0x4014000000000000));
    MMAY_EXPECT(bytecode_execution.managed_heap.roots == NULL);
    functions[0] = &function;
    native_program.functions = functions;
    native_program.function_count = 1;
    native_program.entry_symbol = symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &native_program, mmay_system_allocator(),
        mmay_native_c_emit_default_options(), &generated
    ) == MMAY_NATIVE_C_EMIT_OK);
    MMAY_EXPECT(generated.data != NULL && strstr(
        (const char *)generated.data, "mmay_managed_array_generated_bind"
    ) != NULL);
    MMAY_EXPECT(generated.data != NULL && strstr(
        (const char *)generated.data,
        "mmay_managed_array_generated_get_scalar"
    ) != NULL);
    MMAY_EXPECT(generated.data != NULL && strstr(
        (const char *)generated.data,
        "mmay_managed_array_generated_set_scalar"
    ) != NULL);
    MMAY_EXPECT(generated.data != NULL && strstr(
        (const char *)generated.data, "mmay_managed_array_access_get"
    ) == NULL);
    MMAY_EXPECT(generated.data != NULL && strstr(
        (const char *)generated.data, "mmay_g_array_check"
    ) == NULL);
#if !defined(_WIN32)
    mmay_test_native_compile_and_run(
        &generated, "ok:f64:4014000000000000\n", 0
    );
#endif
    block = mmay_vector_at(&lowered.blocks, 0);
    for (instruction_index = 0;
        block != NULL && instruction_index < block->instructions.length;
        instruction_index += 1) {
        instruction = mmay_vector_at(&block->instructions, instruction_index);
        if (instruction != NULL
            && instruction->opcode == MMAY_OPCODE_ARRAY_REQUIRE_INDEX) {
            instruction->opcode = MMAY_OPCODE_COPY;
            corrupt_rejected = true;
            break;
        }
    }
    MMAY_EXPECT(corrupt_rejected);
    MMAY_EXPECT(mmay_bytecode_verify(&lowered)
        == MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT);
    mmay_bytecode_function_dispose(&lowered);
    mmay_bytecode_function_dispose(&checked_lowered);
    mmay_bytecode_function_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_owned_bytes_dispose(&generated);
    mmay_owned_bytes_dispose(&checked_generated);
    mmay_bytecode_execution_dispose(&bytecode_execution);
    mmay_bytecode_execution_dispose(&checked_bytecode_execution);
    mmay_mir_execution_dispose(&execution);
    mmay_mir_execution_dispose(&checked_execution);
    mmay_mir_function_dispose(&function);
    mmay_mir_function_dispose(&checked);
}

static void mmay_test_mir_range_invalidations(MarimaySymbolId symbol) {
    MarimayMirFunction function;
    MMAY_EXPECT(mmay_test_mir_range_build(
        &function, mmay_system_allocator(), symbol, true
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
    mmay_mir_function_dispose(&function);
}

static void mmay_test_mir_range_interface_call_invalidates(
    MarimaySymbolId symbol
) {
    MarimayMirFunction function;
    MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister array_value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister index = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister proof = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister interface_value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister record_value = MMAY_MIR_NO_REGISTER;
    MarimayMirArrayId array = MMAY_MIR_NO_ARRAY;
    MarimayMirArrayConstructionId array_construction =
        MMAY_MIR_NO_ARRAY_CONSTRUCTION;
    MarimayMirRecordId record = MMAY_MIR_NO_RECORD;
    MarimayMirRecordConstructionId record_construction =
        MMAY_MIR_NO_RECORD_CONSTRUCTION;
    MarimayMirInterfaceId interface_id = MMAY_MIR_NO_INTERFACE;
    MarimayMirInterfaceConstructionId interface_construction =
        MMAY_MIR_NO_INTERFACE_CONSTRUCTION;
    MarimayMirCallId call = MMAY_MIR_NO_CALL;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirArrayArgument array_argument;
    MarimayMirCallArgument call_argument;
    MarimayMirInstruction instruction;
    MarimaySymbolId method = {{0x22, 0x59}};
    memset(&function, 0, sizeof(function));
    MMAY_EXPECT(mmay_mir_function_init(
        &function, mmay_system_allocator(), symbol, NULL, 0, MMAY_TYPE_F64,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_F64, &value)
        && mmay_mir_add_register(
            &function, MMAY_TEST_RANGE_ARRAY_TYPE, &array_value
        ) && mmay_mir_add_register(&function, MMAY_TYPE_I64, &index)
        && mmay_mir_add_register(
            &function, MMAY_TEST_RANGE_ARRAY_TYPE, &proof
        ) && mmay_mir_add_register(&function, MMAY_TYPE_F64, &result)
        && mmay_mir_add_register(
            &function, MMAY_TEST_RANGE_INTERFACE_TYPE, &interface_value
        ) && mmay_mir_add_register(
            &function, MMAY_TEST_RANGE_RECORD_TYPE, &record_value
        ) && mmay_mir_add_array(
            &function, MMAY_TEST_RANGE_ARRAY_TYPE, MMAY_TYPE_F64, &array
        ) && mmay_mir_add_record(
            &function, MMAY_TEST_RANGE_RECORD_TYPE, NULL, 0, &record
        ) && mmay_mir_add_interface(
            &function, MMAY_TEST_RANGE_INTERFACE_TYPE, 1, &interface_id
        ) && mmay_mir_add_record_construction(
            &function, record, NULL, 0, &record_construction
        ) && mmay_mir_add_block(&function, &block));
    array_argument.register_id = value;
    array_argument.type = MMAY_TYPE_F64;
    call_argument.register_id = interface_value;
    call_argument.type = MMAY_TEST_RANGE_INTERFACE_TYPE;
    MMAY_EXPECT(mmay_mir_add_array_construction(
        &function, array, &array_argument, 1, &array_construction
    ) && mmay_mir_add_interface_construction(
        &function, interface_id, record_value, MMAY_TEST_RANGE_RECORD_TYPE,
        &method, 1, &interface_construction
    ) && mmay_mir_add_call(
        &function, method, MMAY_TYPE_F64, &call_argument, 1, &call
    ) && mmay_mir_set_call_interface_slot(&function, call, 0));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_F64;
    instruction.target = value;
    instruction.f64_bits = UINT64_C(0x4014000000000000);
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction.kind = MMAY_MIR_ARRAY_NEW;
    instruction.target = array_value;
    instruction.array = array;
    instruction.array_construction = array_construction;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction.kind = MMAY_MIR_CONSTANT_I64;
    instruction.target = index;
    instruction.integer = 0;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction.kind = MMAY_MIR_ARRAY_REQUIRE_INDEX;
    instruction.target = proof;
    instruction.first = array_value;
    instruction.second = index;
    instruction.array = array;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction.kind = MMAY_MIR_RECORD_NEW;
    instruction.target = record_value;
    instruction.record = record;
    instruction.record_construction = record_construction;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction.kind = MMAY_MIR_INTERFACE_NEW;
    instruction.target = interface_value;
    instruction.interface_construction = interface_construction;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction.kind = MMAY_MIR_CALL_INTERFACE;
    instruction.target = result;
    instruction.call = call;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction.kind = MMAY_MIR_ARRAY_GET_IN_BOUNDS;
    instruction.target = result;
    instruction.first = proof;
    instruction.second = index;
    instruction.array = array;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction)
        && mmay_mir_set_terminator(
            &function, block, mmay_test_mir_return(result)
        ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
    mmay_mir_function_dispose(&function);
}

static void mmay_test_mir_range_mutations_invalidate(
    MarimaySymbolId symbol
) {
    static const MarimayMirInstructionKind mutations[] = {
        MMAY_MIR_ARRAY_SET,
        MMAY_MIR_ARRAY_SET_IN_BOUNDS,
        MMAY_MIR_ARRAY_PUSH,
        MMAY_MIR_ARRAY_POP,
        MMAY_MIR_ARRAY_TRUNCATE,
        MMAY_MIR_ARRAY_CLEAR,
        MMAY_MIR_ARRAY_RESERVE
    };
    size_t case_index = 0;
    for (case_index = 0;
        case_index < sizeof(mutations) / sizeof(mutations[0]);
        case_index += 1) {
        MarimayMirFunction function;
        MarimayMirInstruction *proof = NULL;
        MarimayMirInstruction *mutation = NULL;
        MarimayMirInstruction *access = NULL;
        MMAY_EXPECT(mmay_test_mir_range_build(
            &function, mmay_system_allocator(), symbol, false
        ));
        proof = mmay_test_mir_range_instruction(&function, 3);
        mutation = mmay_test_mir_range_instruction(&function, 4);
        access = mmay_test_mir_range_instruction(&function, 5);
        MMAY_EXPECT(proof != NULL && mutation != NULL && access != NULL);
        if (proof != NULL && mutation != NULL && access != NULL) {
            mutation->kind = mutations[case_index];
            mutation->first = proof->target;
            mutation->second = 2;
            mutation->third = 0;
            mutation->array = proof->array;
            switch (mutation->kind) {
                case MMAY_MIR_ARRAY_SET:
                case MMAY_MIR_ARRAY_SET_IN_BOUNDS:
                    mutation->target = 4;
                    mutation->third = 0;
                    break;
                case MMAY_MIR_ARRAY_PUSH:
                    mutation->target = proof->target;
                    mutation->second = 0;
                    break;
                case MMAY_MIR_ARRAY_POP:
                    mutation->target = 4;
                    break;
                case MMAY_MIR_ARRAY_TRUNCATE:
                case MMAY_MIR_ARRAY_RESERVE:
                    mutation->target = proof->target;
                    break;
                case MMAY_MIR_ARRAY_CLEAR:
                    mutation->target = proof->target;
                    break;
                default:
                    MMAY_EXPECT(false);
                    break;
            }
            access->kind = MMAY_MIR_ARRAY_GET_IN_BOUNDS;
            access->target = 5;
            access->first = proof->target;
            access->second = 2;
            access->array = proof->array;
            MMAY_EXPECT(mmay_mir_verify(&function)
                == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
        }
        mmay_mir_function_dispose(&function);
    }
}

static void mmay_test_mir_range_block_boundary_invalidates(
    MarimaySymbolId symbol
) {
    MarimayMirFunction function;
    MarimayMirBlockId successor = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    MarimayMirInstruction *proof = NULL;
    MarimayMirInstruction *first = NULL;
    MarimayMirInstruction *second = NULL;
    MarimayMirBlock *entry = NULL;
    MMAY_EXPECT(mmay_test_mir_range_build(
        &function, mmay_system_allocator(), symbol, false
    ));
    proof = mmay_test_mir_range_instruction(&function, 3);
    first = mmay_test_mir_range_instruction(&function, 4);
    second = mmay_test_mir_range_instruction(&function, 5);
    entry = mmay_vector_at(&function.blocks, 0);
    MMAY_EXPECT(proof != NULL && first != NULL && second != NULL
        && entry != NULL && mmay_mir_add_block(&function, &successor));
    if (proof != NULL && first != NULL && second != NULL
        && entry != NULL && successor != MMAY_MIR_NO_BLOCK) {
        first->kind = MMAY_MIR_COPY;
        first->target = 4;
        first->first = 0;
        second->kind = MMAY_MIR_COPY;
        second->target = 5;
        second->first = 0;
        entry->terminator = mmay_test_mir_goto(successor);
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_ARRAY_GET_IN_BOUNDS;
        instruction.target = 4;
        instruction.first = proof->target;
        instruction.second = 2;
        instruction.array = proof->array;
        MMAY_EXPECT(mmay_mir_append_instruction(
            &function, successor, &instruction
        ));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function, successor, mmay_test_mir_return(4)
        ));
        MMAY_EXPECT(mmay_mir_verify(&function)
            == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
    }
    mmay_mir_function_dispose(&function);
}

static void mmay_test_mir_range_keeps_unproven_checks(
    MarimaySymbolId symbol
) {
    static const int64_t indices[] = {-1, INT64_MAX};
    size_t case_index = 0;
    for (case_index = 0;
        case_index < sizeof(indices) / sizeof(indices[0]);
        case_index += 1) {
        MarimayMirFunction function;
        MarimayMirInstruction *index = NULL;
        MarimayMirInstruction *proof = NULL;
        MarimayMirInstruction *access = NULL;
        MMAY_EXPECT(mmay_test_mir_range_build(
            &function, mmay_system_allocator(), symbol, false
        ));
        index = mmay_test_mir_range_instruction(&function, 2);
        proof = mmay_test_mir_range_instruction(&function, 3);
        access = mmay_test_mir_range_instruction(&function, 4);
        MMAY_EXPECT(index != NULL && proof != NULL && access != NULL);
        if (index != NULL && proof != NULL && access != NULL) {
            index->integer = indices[case_index];
            MMAY_EXPECT(mmay_mir_optimize_ranges(&function)
                == MMAY_MIR_RANGE_OPTIMIZE_OK);
            MMAY_EXPECT(proof->kind == MMAY_MIR_ARRAY_REQUIRE_INDEX);
            MMAY_EXPECT(access->kind == MMAY_MIR_ARRAY_GET_IN_BOUNDS);
            MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
        }
        mmay_mir_function_dispose(&function);
    }
}

static void mmay_test_mir_range_rejects_forged_assumption(
    MarimaySymbolId symbol
) {
    MarimayMirFunction function;
    MarimayMirInstruction *constant = NULL;
    MarimayMirInstruction *proof = NULL;
    MarimayMirInstruction *get = NULL;
    MMAY_EXPECT(mmay_test_mir_range_build(
        &function, mmay_system_allocator(), symbol, false
    ));
    constant = mmay_test_mir_range_instruction(&function, 2);
    proof = mmay_test_mir_range_instruction(&function, 3);
    get = mmay_test_mir_range_instruction(&function, 4);
    MMAY_EXPECT(constant != NULL && proof != NULL && get != NULL);
    if (constant != NULL && proof != NULL && get != NULL) {
        constant->integer = 1;
        proof->kind = MMAY_MIR_ARRAY_ASSUME_INDEX;
        get->kind = MMAY_MIR_ARRAY_GET_IN_BOUNDS;
        MMAY_EXPECT(mmay_mir_verify(&function)
            == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
    }
    mmay_mir_function_dispose(&function);
}

static void mmay_test_mir_range_rejects_negative_true_edge(
    MarimaySymbolId symbol
) {
    MarimayMirFunction function;
    MarimayMirBlock *entry = NULL;
    MarimayMirBlockId body = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId exit = MMAY_MIR_NO_BLOCK;
    MarimayMirRegister length = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister condition = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    MarimayMirInstruction *index = NULL;
    MarimayMirInstruction *carrier = NULL;
    MarimayMirInstruction *discard = NULL;
    MarimayMirInstruction *stored = NULL;
    MMAY_EXPECT(mmay_test_mir_range_build(
        &function, mmay_system_allocator(), symbol, false
    ));
    index = mmay_test_mir_range_instruction(&function, 2);
    carrier = mmay_test_mir_range_instruction(&function, 3);
    discard = mmay_test_mir_range_instruction(&function, 4);
    stored = mmay_test_mir_range_instruction(&function, 5);
    MMAY_EXPECT(index != NULL && carrier != NULL && discard != NULL
        && stored != NULL
        && mmay_mir_add_register(&function, MMAY_TYPE_I64, &length)
        && mmay_mir_add_register(&function, MMAY_TYPE_BOOL, &condition)
        && mmay_mir_add_block(&function, &body)
        && mmay_mir_add_block(&function, &exit));
    entry = mmay_vector_at(&function.blocks, 0);
    if (entry != NULL && index != NULL && carrier != NULL
        && discard != NULL && stored != NULL && body != MMAY_MIR_NO_BLOCK
        && exit != MMAY_MIR_NO_BLOCK) {
        index->integer = -1;
        carrier->kind = MMAY_MIR_COPY;
        carrier->first = 1;
        discard->kind = MMAY_MIR_COPY;
        discard->first = 0;
        stored->kind = MMAY_MIR_COPY;
        stored->first = 0;
        entry->terminator.kind = MMAY_MIR_TERMINATOR_UNSET;
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_ARRAY_LENGTH;
        instruction.target = length;
        instruction.first = 1;
        instruction.array = carrier->array;
        MMAY_EXPECT(mmay_mir_append_instruction(&function, 0, &instruction));
        instruction.kind = MMAY_MIR_LESS_I64;
        instruction.target = condition;
        instruction.first = 2;
        instruction.second = length;
        MMAY_EXPECT(mmay_mir_append_instruction(&function, 0, &instruction));
        entry->terminator = mmay_test_mir_branch(condition, body, exit);
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_ARRAY_ASSUME_INDEX;
        instruction.target = 3;
        instruction.first = 1;
        instruction.second = 2;
        instruction.array = carrier->array;
        MMAY_EXPECT(mmay_mir_append_instruction(&function, body, &instruction));
        instruction.kind = MMAY_MIR_ARRAY_GET_IN_BOUNDS;
        instruction.target = 4;
        instruction.first = 3;
        instruction.second = 2;
        MMAY_EXPECT(mmay_mir_append_instruction(&function, body, &instruction));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function, body, mmay_test_mir_return(4)
        ));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function, exit, mmay_test_mir_return(0)
        ));
        MMAY_EXPECT(mmay_mir_verify(&function)
            == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
        index->integer = 0;
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    }
    mmay_mir_function_dispose(&function);
}

static void mmay_test_mir_range_allocation_fallback(MarimaySymbolId symbol) {
    MarimayFailAllocator state;
    MarimayMirFunction function;
    size_t baseline_allocations = 0, baseline_bytes = 0, failure_index = 0;
    bool reached_success = false;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_test_mir_range_build(
        &function, mmay_fail_allocator(&state), symbol, false
    ));
    baseline_allocations = state.live_allocations;
    baseline_bytes = state.live_bytes;
    for (failure_index = 0; failure_index < 80 && !reached_success;
        failure_index += 1) {
        MarimayMirRangeOptimizeStatus status;
        MarimayMirInstruction *get = mmay_test_mir_range_instruction(&function, 4);
        state.failure_index = state.attempts + failure_index;
        status = mmay_mir_optimize_ranges(&function);
        if (status == MMAY_MIR_RANGE_OPTIMIZE_OK) {
            reached_success = true;
            MMAY_EXPECT(get != NULL
                && get->kind == MMAY_MIR_ARRAY_GET_IN_BOUNDS);
        } else {
            MMAY_EXPECT(status == MMAY_MIR_RANGE_OPTIMIZE_ALLOCATION_FAILED);
            MMAY_EXPECT(get != NULL && get->kind == MMAY_MIR_ARRAY_GET);
        }
        MMAY_EXPECT(state.live_allocations == baseline_allocations);
        MMAY_EXPECT(state.live_bytes == baseline_bytes);
    }
    MMAY_EXPECT(reached_success);
    mmay_mir_function_dispose(&function);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
}

static void mmay_test_mir_range_verifier_and_codec_allocations(
    MarimaySymbolId symbol
) {
    MarimayMirFunction checked;
    MarimayBytecodeFunction lowered;
    MarimayOwnedBytes encoded;
    size_t failure_index = 0;
    bool reached_success = false;
    memset(&lowered, 0, sizeof(lowered));
    memset(&encoded, 0, sizeof(encoded));
    MMAY_EXPECT(mmay_test_mir_range_build(
        &checked, mmay_system_allocator(), symbol, false
    ));
    MMAY_EXPECT(mmay_mir_optimize_ranges(&checked)
        == MMAY_MIR_RANGE_OPTIMIZE_OK);
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(
        &checked, mmay_system_allocator(),
        mmay_mir_bytecode_lower_default_options(), &lowered
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_encode_function(
        &lowered, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);

    for (failure_index = 0; failure_index < 128 && !reached_success;
        failure_index += 1) {
        MarimayFailAllocator state;
        MarimayMirFunction function;
        MarimayMirVerifyStatus status;
        size_t baseline_allocations = 0, baseline_bytes = 0;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_test_mir_range_build(
            &function, mmay_fail_allocator(&state), symbol, false
        ));
        MMAY_EXPECT(mmay_mir_optimize_ranges(&function)
            == MMAY_MIR_RANGE_OPTIMIZE_OK);
        baseline_allocations = state.live_allocations;
        baseline_bytes = state.live_bytes;
        state.failure_index = state.attempts + failure_index;
        status = mmay_mir_verify(&function);
        if (status == MMAY_MIR_VERIFY_OK) {
            reached_success = true;
        } else {
            MMAY_EXPECT(status == MMAY_MIR_VERIFY_ALLOCATION_FAILED);
        }
        MMAY_EXPECT(state.live_allocations == baseline_allocations);
        MMAY_EXPECT(state.live_bytes == baseline_bytes);
        mmay_mir_function_dispose(&function);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);

    for (failure_index = 0, reached_success = false;
        failure_index < 128 && !reached_success; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayOwnedBytes output;
        MarimayBytecodeCodecStatus status;
        memset(&output, 0, sizeof(output));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_bytecode_encode_function(
            &lowered, mmay_fail_allocator(&state), &output
        );
        if (status == MMAY_BYTECODE_CODEC_OK) {
            reached_success = true;
        } else {
            MMAY_EXPECT(status == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED);
            MMAY_EXPECT(output.data == NULL && output.length == 0);
        }
        mmay_owned_bytes_dispose(&output);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);

    for (failure_index = 0, reached_success = false;
        failure_index < 128 && !reached_success; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeFunction output;
        MarimayBytecodeCodecStatus status;
        memset(&output, 0, sizeof(output));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_bytecode_decode_function(
            encoded.data, encoded.length, mmay_fail_allocator(&state),
            mmay_bytecode_function_default_options(), &output
        );
        if (status == MMAY_BYTECODE_CODEC_OK) {
            reached_success = true;
        } else {
            MMAY_EXPECT(status == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED);
            MMAY_EXPECT(!output.initialized);
        }
        mmay_bytecode_function_dispose(&output);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);

    for (failure_index = 0, reached_success = false;
        failure_index < 128 && !reached_success; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeFunction function;
        MarimayBytecodeVerifyStatus status;
        size_t baseline_allocations = 0, baseline_bytes = 0;
        memset(&function, 0, sizeof(function));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_mir_lower_bytecode_function(
            &checked, mmay_fail_allocator(&state),
            mmay_mir_bytecode_lower_default_options(), &function
        ) == MMAY_MIR_BYTECODE_LOWER_OK);
        baseline_allocations = state.live_allocations;
        baseline_bytes = state.live_bytes;
        state.failure_index = state.attempts + failure_index;
        status = mmay_bytecode_verify(&function);
        if (status == MMAY_BYTECODE_VERIFY_OK) {
            reached_success = true;
        } else {
            MMAY_EXPECT(status == MMAY_BYTECODE_VERIFY_ALLOCATION_FAILED);
        }
        MMAY_EXPECT(state.live_allocations == baseline_allocations);
        MMAY_EXPECT(state.live_bytes == baseline_bytes);
        mmay_bytecode_function_dispose(&function);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_function_dispose(&lowered);
    mmay_mir_function_dispose(&checked);
}

void mmay_test_mir_range(void) {
    MarimaySymbolId symbol = {{0x22, 0x58}};
    mmay_test_mir_range_optimizer(symbol);
    mmay_test_mir_range_invalidations(symbol);
    mmay_test_mir_range_interface_call_invalidates(symbol);
    mmay_test_mir_range_mutations_invalidate(symbol);
    mmay_test_mir_range_block_boundary_invalidates(symbol);
    mmay_test_mir_range_keeps_unproven_checks(symbol);
    mmay_test_mir_range_rejects_forged_assumption(symbol);
    mmay_test_mir_range_rejects_negative_true_edge(symbol);
    mmay_test_mir_range_allocation_fallback(symbol);
    mmay_test_mir_range_verifier_and_codec_allocations(symbol);
}
