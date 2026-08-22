#include "test_mir_support.h"
#include "test_native_c_emitter_support.h"

#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/bytecode/codec.h"
#include "ruda/bytecode/mir_lower.h"
#include "ruda/bytecode/vm.h"
#include "ruda/mir/range.h"
#include "ruda/native/c_emitter.h"

enum {
    RUDA_TEST_RANGE_ARRAY_TYPE = RUDA_TYPE_FIRST_DYNAMIC + 91,
    RUDA_TEST_RANGE_RECORD_TYPE,
    RUDA_TEST_RANGE_INTERFACE_TYPE
};

static bool ruda_test_mir_range_build(
    RudaMirFunction *function,
    RudaAllocator allocator,
    RudaSymbolId symbol,
    bool call_between_proof_and_access
) {
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister array_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister index = RUDA_MIR_NO_REGISTER;
    RudaMirRegister proof = RUDA_MIR_NO_REGISTER;
    RudaMirRegister projected = RUDA_MIR_NO_REGISTER;
    RudaMirRegister stored = RUDA_MIR_NO_REGISTER;
    RudaMirArrayId array = RUDA_MIR_NO_ARRAY;
    RudaMirArrayConstructionId construction = RUDA_MIR_NO_ARRAY_CONSTRUCTION;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaMirArrayArgument argument;
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(
            function, allocator, symbol, NULL, 0, RUDA_TYPE_F64,
            ruda_mir_function_default_options()
        ) || !ruda_mir_add_register(function, RUDA_TYPE_F64, &value)
        || !ruda_mir_add_register(
            function, RUDA_TEST_RANGE_ARRAY_TYPE, &array_value
        ) || !ruda_mir_add_register(function, RUDA_TYPE_I64, &index)
        || !ruda_mir_add_register(
            function, RUDA_TEST_RANGE_ARRAY_TYPE, &proof
        ) || !ruda_mir_add_register(function, RUDA_TYPE_F64, &projected)
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &stored)
        || !ruda_mir_add_array(
            function, RUDA_TEST_RANGE_ARRAY_TYPE, RUDA_TYPE_F64, &array
        )) {
        ruda_mir_function_dispose(function);
        return false;
    }
    argument.register_id = value;
    argument.type = RUDA_TYPE_F64;
    if (!ruda_mir_add_array_construction(
            function, array, &argument, 1, &construction
        ) || !ruda_mir_add_block(function, &block)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_F64;
    instruction.target = value;
    instruction.f64_bits = UINT64_C(0x4014000000000000);
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_NEW;
    instruction.target = array_value;
    instruction.array = array;
    instruction.array_construction = construction;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_I64;
    instruction.target = index;
    instruction.integer = 0;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_REQUIRE_INDEX;
    instruction.target = proof;
    instruction.first = array_value;
    instruction.second = index;
    instruction.array = array;
    instruction.span.start = 17;
    instruction.span.end = 23;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    if (call_between_proof_and_access) {
        if (!ruda_mir_add_call(
                function, symbol, RUDA_TYPE_F64, NULL, 0, &call
            )) {
            ruda_mir_function_dispose(function);
            return false;
        }
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_CALL;
        instruction.target = projected;
        instruction.call = call;
        if (!ruda_mir_append_instruction(function, block, &instruction)) {
            ruda_mir_function_dispose(function);
            return false;
        }
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = call_between_proof_and_access
        ? RUDA_MIR_ARRAY_GET_IN_BOUNDS : RUDA_MIR_ARRAY_GET;
    instruction.target = projected;
    instruction.first = proof;
    instruction.second = index;
    instruction.array = array;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    if (!call_between_proof_and_access) {
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_ARRAY_SET;
        instruction.target = stored;
        instruction.first = proof;
        instruction.second = index;
        instruction.third = projected;
        instruction.array = array;
        if (!ruda_mir_append_instruction(function, block, &instruction)) {
            ruda_mir_function_dispose(function);
            return false;
        }
    } else {
        stored = projected;
    }
    if (!ruda_mir_set_terminator(
            function, block, ruda_test_mir_return(stored)
        )) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static RudaMirInstruction *ruda_test_mir_range_instruction(
    RudaMirFunction *function,
    size_t index
) {
    RudaMirBlock *block = function != NULL
        ? ruda_vector_at(&function->blocks, 0) : NULL;
    return block != NULL ? ruda_vector_at(&block->instructions, index) : NULL;
}

static void ruda_test_mir_range_optimizer(
    RudaSymbolId symbol
) {
    RudaMirFunction checked;
    RudaMirFunction function;
    RudaMirInstruction *proof = NULL;
    RudaMirInstruction *get = NULL;
    RudaMirInstruction *set = NULL;
    RudaMirExecution execution;
    RudaMirExecution checked_execution;
    RudaMirInterpretOptions options = ruda_mir_interpret_default_options();
    RudaMirValue result;
    RudaMirValue checked_result;
    RudaBytecodeFunction lowered;
    RudaBytecodeFunction checked_lowered;
    RudaBytecodeFunction decoded;
    RudaBytecodeExecution bytecode_execution;
    RudaBytecodeExecution checked_bytecode_execution;
    RudaBytecodeRunOptions bytecode_options;
    RudaBytecodeRunOptions checked_bytecode_options;
    RudaBytecodeValue bytecode_result;
    RudaBytecodeValue checked_bytecode_result;
    RudaOwnedBytes encoded;
    RudaBytecodeBlock *block = NULL;
    RudaBytecodeInstruction *instruction = NULL;
    RudaNativeCProgram native_program;
    RudaNativeCProgram checked_native_program;
    RudaOwnedBytes generated;
    RudaOwnedBytes checked_generated;
    const RudaMirFunction *functions[1];
    const RudaMirFunction *checked_functions[1];
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
    RUDA_EXPECT(ruda_test_mir_range_build(
        &checked, ruda_system_allocator(), symbol, false
    ));
    RUDA_EXPECT(ruda_mir_verify(&checked) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_execution_init(
        &checked_execution, ruda_system_allocator(),
        ruda_mir_execution_default_options()
    ));
    options.execution = &checked_execution;
    RUDA_EXPECT(ruda_mir_interpret(
        &checked, NULL, 0, ruda_system_allocator(), options, &checked_result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(checked_result.type == RUDA_TYPE_F64
        && checked_result.as.f64_bits == UINT64_C(0x4014000000000000));
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(
        &checked, ruda_system_allocator(),
        ruda_mir_bytecode_lower_default_options(), &checked_lowered
    ) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_verify(&checked_lowered)
        == RUDA_BYTECODE_VERIFY_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &checked_bytecode_execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()
    ));
    checked_bytecode_options = ruda_bytecode_run_default_options();
    checked_bytecode_options.execution = &checked_bytecode_execution;
    memset(&checked_bytecode_result, 0, sizeof(checked_bytecode_result));
    RUDA_EXPECT(ruda_bytecode_run(
        &checked_lowered, NULL, 0, ruda_system_allocator(),
        checked_bytecode_options, &checked_bytecode_result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(checked_bytecode_result.type == RUDA_BYTECODE_TYPE_F64
        && checked_bytecode_result.as.f64_bits == UINT64_C(0x4014000000000000));
    RUDA_EXPECT(checked_bytecode_execution.managed_heap.roots == NULL);
    checked_functions[0] = &checked;
    checked_native_program.functions = checked_functions;
    checked_native_program.function_count = 1;
    checked_native_program.entry_symbol = symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(
        &checked_native_program, ruda_system_allocator(),
        ruda_native_c_emit_default_options(), &checked_generated
    ) == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    ruda_test_native_compile_and_run(
        &checked_generated, "ok:f64:4014000000000000\n", 0
    );
#endif
    RUDA_EXPECT(ruda_test_mir_range_build(
        &function, ruda_system_allocator(), symbol, false
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_optimize_ranges(&function)
        == RUDA_MIR_RANGE_OPTIMIZE_OK);
    proof = ruda_test_mir_range_instruction(&function, 3);
    get = ruda_test_mir_range_instruction(&function, 4);
    set = ruda_test_mir_range_instruction(&function, 5);
    RUDA_EXPECT(proof != NULL
        && proof->kind == RUDA_MIR_ARRAY_ASSUME_INDEX);
    RUDA_EXPECT(get != NULL && get->kind == RUDA_MIR_ARRAY_GET_IN_BOUNDS);
    RUDA_EXPECT(set != NULL && set->kind == RUDA_MIR_ARRAY_SET_IN_BOUNDS);
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_execution_init(
        &execution, ruda_system_allocator(), ruda_mir_execution_default_options()
    ));
    options.execution = &execution;
    RUDA_EXPECT(ruda_mir_interpret(
        &function, NULL, 0, ruda_system_allocator(), options, &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.type == RUDA_TYPE_F64
        && result.as.f64_bits == UINT64_C(0x4014000000000000));
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(
        &function, ruda_system_allocator(),
        ruda_mir_bytecode_lower_default_options(), &lowered
    ) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_verify(&lowered) == RUDA_BYTECODE_VERIFY_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &bytecode_execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()
    ));
    bytecode_options = ruda_bytecode_run_default_options();
    bytecode_options.execution = &bytecode_execution;
    memset(&bytecode_result, 0, sizeof(bytecode_result));
    RUDA_EXPECT(ruda_bytecode_run(
        &lowered, NULL, 0, ruda_system_allocator(), bytecode_options,
        &bytecode_result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(bytecode_result.type == RUDA_BYTECODE_TYPE_F64
        && bytecode_result.as.f64_bits == UINT64_C(0x4014000000000000));
    RUDA_EXPECT(bytecode_execution.managed_heap.roots == NULL);
    RUDA_EXPECT(ruda_bytecode_encode_function(
        &lowered, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_decode_function(
        encoded.data, encoded.length, ruda_system_allocator(),
        ruda_bytecode_function_default_options(), &decoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_verify(&decoded) == RUDA_BYTECODE_VERIFY_OK);
    memset(&bytecode_result, 0, sizeof(bytecode_result));
    RUDA_EXPECT(ruda_bytecode_run(
        &decoded, NULL, 0, ruda_system_allocator(), bytecode_options,
        &bytecode_result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(bytecode_result.type == RUDA_BYTECODE_TYPE_F64
        && bytecode_result.as.f64_bits == UINT64_C(0x4014000000000000));
    RUDA_EXPECT(bytecode_execution.managed_heap.roots == NULL);
    functions[0] = &function;
    native_program.functions = functions;
    native_program.function_count = 1;
    native_program.entry_symbol = symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(
        &native_program, ruda_system_allocator(),
        ruda_native_c_emit_default_options(), &generated
    ) == RUDA_NATIVE_C_EMIT_OK);
    RUDA_EXPECT(generated.data != NULL && strstr(
        (const char *)generated.data, "ruda_managed_array_get_in_bounds"
    ) != NULL);
    RUDA_EXPECT(generated.data != NULL && strstr(
        (const char *)generated.data, "ruda_managed_array_set_in_bounds"
    ) != NULL);
#if !defined(_WIN32)
    ruda_test_native_compile_and_run(
        &generated, "ok:f64:4014000000000000\n", 0
    );
#endif
    block = ruda_vector_at(&lowered.blocks, 0);
    for (instruction_index = 0;
        block != NULL && instruction_index < block->instructions.length;
        instruction_index += 1) {
        instruction = ruda_vector_at(&block->instructions, instruction_index);
        if (instruction != NULL
            && instruction->opcode == RUDA_OPCODE_ARRAY_REQUIRE_INDEX) {
            instruction->opcode = RUDA_OPCODE_COPY;
            corrupt_rejected = true;
            break;
        }
    }
    RUDA_EXPECT(corrupt_rejected);
    RUDA_EXPECT(ruda_bytecode_verify(&lowered)
        == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
    ruda_bytecode_function_dispose(&lowered);
    ruda_bytecode_function_dispose(&checked_lowered);
    ruda_bytecode_function_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_owned_bytes_dispose(&generated);
    ruda_owned_bytes_dispose(&checked_generated);
    ruda_bytecode_execution_dispose(&bytecode_execution);
    ruda_bytecode_execution_dispose(&checked_bytecode_execution);
    ruda_mir_execution_dispose(&execution);
    ruda_mir_execution_dispose(&checked_execution);
    ruda_mir_function_dispose(&function);
    ruda_mir_function_dispose(&checked);
}

static void ruda_test_mir_range_invalidations(RudaSymbolId symbol) {
    RudaMirFunction function;
    RUDA_EXPECT(ruda_test_mir_range_build(
        &function, ruda_system_allocator(), symbol, true
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_INVALID_ARGUMENT);
    ruda_mir_function_dispose(&function);
}

static void ruda_test_mir_range_interface_call_invalidates(
    RudaSymbolId symbol
) {
    RudaMirFunction function;
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister array_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister index = RUDA_MIR_NO_REGISTER;
    RudaMirRegister proof = RUDA_MIR_NO_REGISTER;
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirRegister interface_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister record_value = RUDA_MIR_NO_REGISTER;
    RudaMirArrayId array = RUDA_MIR_NO_ARRAY;
    RudaMirArrayConstructionId array_construction =
        RUDA_MIR_NO_ARRAY_CONSTRUCTION;
    RudaMirRecordId record = RUDA_MIR_NO_RECORD;
    RudaMirRecordConstructionId record_construction =
        RUDA_MIR_NO_RECORD_CONSTRUCTION;
    RudaMirInterfaceId interface_id = RUDA_MIR_NO_INTERFACE;
    RudaMirInterfaceConstructionId interface_construction =
        RUDA_MIR_NO_INTERFACE_CONSTRUCTION;
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirArrayArgument array_argument;
    RudaMirCallArgument call_argument;
    RudaMirInstruction instruction;
    RudaSymbolId method = {{0x22, 0x59}};
    memset(&function, 0, sizeof(function));
    RUDA_EXPECT(ruda_mir_function_init(
        &function, ruda_system_allocator(), symbol, NULL, 0, RUDA_TYPE_F64,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_F64, &value)
        && ruda_mir_add_register(
            &function, RUDA_TEST_RANGE_ARRAY_TYPE, &array_value
        ) && ruda_mir_add_register(&function, RUDA_TYPE_I64, &index)
        && ruda_mir_add_register(
            &function, RUDA_TEST_RANGE_ARRAY_TYPE, &proof
        ) && ruda_mir_add_register(&function, RUDA_TYPE_F64, &result)
        && ruda_mir_add_register(
            &function, RUDA_TEST_RANGE_INTERFACE_TYPE, &interface_value
        ) && ruda_mir_add_register(
            &function, RUDA_TEST_RANGE_RECORD_TYPE, &record_value
        ) && ruda_mir_add_array(
            &function, RUDA_TEST_RANGE_ARRAY_TYPE, RUDA_TYPE_F64, &array
        ) && ruda_mir_add_record(
            &function, RUDA_TEST_RANGE_RECORD_TYPE, NULL, 0, &record
        ) && ruda_mir_add_interface(
            &function, RUDA_TEST_RANGE_INTERFACE_TYPE, 1, &interface_id
        ) && ruda_mir_add_record_construction(
            &function, record, NULL, 0, &record_construction
        ) && ruda_mir_add_block(&function, &block));
    array_argument.register_id = value;
    array_argument.type = RUDA_TYPE_F64;
    call_argument.register_id = interface_value;
    call_argument.type = RUDA_TEST_RANGE_INTERFACE_TYPE;
    RUDA_EXPECT(ruda_mir_add_array_construction(
        &function, array, &array_argument, 1, &array_construction
    ) && ruda_mir_add_interface_construction(
        &function, interface_id, record_value, RUDA_TEST_RANGE_RECORD_TYPE,
        &method, 1, &interface_construction
    ) && ruda_mir_add_call(
        &function, method, RUDA_TYPE_F64, &call_argument, 1, &call
    ) && ruda_mir_set_call_interface_slot(&function, call, 0));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_F64;
    instruction.target = value;
    instruction.f64_bits = UINT64_C(0x4014000000000000);
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction.kind = RUDA_MIR_ARRAY_NEW;
    instruction.target = array_value;
    instruction.array = array;
    instruction.array_construction = array_construction;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction.kind = RUDA_MIR_CONSTANT_I64;
    instruction.target = index;
    instruction.integer = 0;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction.kind = RUDA_MIR_ARRAY_REQUIRE_INDEX;
    instruction.target = proof;
    instruction.first = array_value;
    instruction.second = index;
    instruction.array = array;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction.kind = RUDA_MIR_RECORD_NEW;
    instruction.target = record_value;
    instruction.record = record;
    instruction.record_construction = record_construction;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction.kind = RUDA_MIR_INTERFACE_NEW;
    instruction.target = interface_value;
    instruction.interface_construction = interface_construction;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction.kind = RUDA_MIR_CALL_INTERFACE;
    instruction.target = result;
    instruction.call = call;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction.kind = RUDA_MIR_ARRAY_GET_IN_BOUNDS;
    instruction.target = result;
    instruction.first = proof;
    instruction.second = index;
    instruction.array = array;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction)
        && ruda_mir_set_terminator(
            &function, block, ruda_test_mir_return(result)
        ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_INVALID_ARGUMENT);
    ruda_mir_function_dispose(&function);
}

static void ruda_test_mir_range_mutations_invalidate(
    RudaSymbolId symbol
) {
    static const RudaMirInstructionKind mutations[] = {
        RUDA_MIR_ARRAY_SET,
        RUDA_MIR_ARRAY_SET_IN_BOUNDS,
        RUDA_MIR_ARRAY_PUSH,
        RUDA_MIR_ARRAY_POP,
        RUDA_MIR_ARRAY_TRUNCATE,
        RUDA_MIR_ARRAY_CLEAR,
        RUDA_MIR_ARRAY_RESERVE
    };
    size_t case_index = 0;
    for (case_index = 0;
        case_index < sizeof(mutations) / sizeof(mutations[0]);
        case_index += 1) {
        RudaMirFunction function;
        RudaMirInstruction *proof = NULL;
        RudaMirInstruction *mutation = NULL;
        RudaMirInstruction *access = NULL;
        RUDA_EXPECT(ruda_test_mir_range_build(
            &function, ruda_system_allocator(), symbol, false
        ));
        proof = ruda_test_mir_range_instruction(&function, 3);
        mutation = ruda_test_mir_range_instruction(&function, 4);
        access = ruda_test_mir_range_instruction(&function, 5);
        RUDA_EXPECT(proof != NULL && mutation != NULL && access != NULL);
        if (proof != NULL && mutation != NULL && access != NULL) {
            mutation->kind = mutations[case_index];
            mutation->first = proof->target;
            mutation->second = 2;
            mutation->third = 0;
            mutation->array = proof->array;
            switch (mutation->kind) {
                case RUDA_MIR_ARRAY_SET:
                case RUDA_MIR_ARRAY_SET_IN_BOUNDS:
                    mutation->target = 4;
                    mutation->third = 0;
                    break;
                case RUDA_MIR_ARRAY_PUSH:
                    mutation->target = proof->target;
                    mutation->second = 0;
                    break;
                case RUDA_MIR_ARRAY_POP:
                    mutation->target = 4;
                    break;
                case RUDA_MIR_ARRAY_TRUNCATE:
                case RUDA_MIR_ARRAY_RESERVE:
                    mutation->target = proof->target;
                    break;
                case RUDA_MIR_ARRAY_CLEAR:
                    mutation->target = proof->target;
                    break;
                default:
                    RUDA_EXPECT(false);
                    break;
            }
            access->kind = RUDA_MIR_ARRAY_GET_IN_BOUNDS;
            access->target = 5;
            access->first = proof->target;
            access->second = 2;
            access->array = proof->array;
            RUDA_EXPECT(ruda_mir_verify(&function)
                == RUDA_MIR_VERIFY_INVALID_ARGUMENT);
        }
        ruda_mir_function_dispose(&function);
    }
}

static void ruda_test_mir_range_block_boundary_invalidates(
    RudaSymbolId symbol
) {
    RudaMirFunction function;
    RudaMirBlockId successor = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaMirInstruction *proof = NULL;
    RudaMirInstruction *first = NULL;
    RudaMirInstruction *second = NULL;
    RudaMirBlock *entry = NULL;
    RUDA_EXPECT(ruda_test_mir_range_build(
        &function, ruda_system_allocator(), symbol, false
    ));
    proof = ruda_test_mir_range_instruction(&function, 3);
    first = ruda_test_mir_range_instruction(&function, 4);
    second = ruda_test_mir_range_instruction(&function, 5);
    entry = ruda_vector_at(&function.blocks, 0);
    RUDA_EXPECT(proof != NULL && first != NULL && second != NULL
        && entry != NULL && ruda_mir_add_block(&function, &successor));
    if (proof != NULL && first != NULL && second != NULL
        && entry != NULL && successor != RUDA_MIR_NO_BLOCK) {
        first->kind = RUDA_MIR_COPY;
        first->target = 4;
        first->first = 0;
        second->kind = RUDA_MIR_COPY;
        second->target = 5;
        second->first = 0;
        entry->terminator = ruda_test_mir_goto(successor);
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_ARRAY_GET_IN_BOUNDS;
        instruction.target = 4;
        instruction.first = proof->target;
        instruction.second = 2;
        instruction.array = proof->array;
        RUDA_EXPECT(ruda_mir_append_instruction(
            &function, successor, &instruction
        ));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function, successor, ruda_test_mir_return(4)
        ));
        RUDA_EXPECT(ruda_mir_verify(&function)
            == RUDA_MIR_VERIFY_INVALID_ARGUMENT);
    }
    ruda_mir_function_dispose(&function);
}

static void ruda_test_mir_range_keeps_unproven_checks(
    RudaSymbolId symbol
) {
    static const int64_t indices[] = {-1, INT64_MAX};
    size_t case_index = 0;
    for (case_index = 0;
        case_index < sizeof(indices) / sizeof(indices[0]);
        case_index += 1) {
        RudaMirFunction function;
        RudaMirInstruction *index = NULL;
        RudaMirInstruction *proof = NULL;
        RudaMirInstruction *access = NULL;
        RUDA_EXPECT(ruda_test_mir_range_build(
            &function, ruda_system_allocator(), symbol, false
        ));
        index = ruda_test_mir_range_instruction(&function, 2);
        proof = ruda_test_mir_range_instruction(&function, 3);
        access = ruda_test_mir_range_instruction(&function, 4);
        RUDA_EXPECT(index != NULL && proof != NULL && access != NULL);
        if (index != NULL && proof != NULL && access != NULL) {
            index->integer = indices[case_index];
            RUDA_EXPECT(ruda_mir_optimize_ranges(&function)
                == RUDA_MIR_RANGE_OPTIMIZE_OK);
            RUDA_EXPECT(proof->kind == RUDA_MIR_ARRAY_REQUIRE_INDEX);
            RUDA_EXPECT(access->kind == RUDA_MIR_ARRAY_GET_IN_BOUNDS);
            RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
        }
        ruda_mir_function_dispose(&function);
    }
}

static void ruda_test_mir_range_rejects_forged_assumption(
    RudaSymbolId symbol
) {
    RudaMirFunction function;
    RudaMirInstruction *constant = NULL;
    RudaMirInstruction *proof = NULL;
    RudaMirInstruction *get = NULL;
    RUDA_EXPECT(ruda_test_mir_range_build(
        &function, ruda_system_allocator(), symbol, false
    ));
    constant = ruda_test_mir_range_instruction(&function, 2);
    proof = ruda_test_mir_range_instruction(&function, 3);
    get = ruda_test_mir_range_instruction(&function, 4);
    RUDA_EXPECT(constant != NULL && proof != NULL && get != NULL);
    if (constant != NULL && proof != NULL && get != NULL) {
        constant->integer = 1;
        proof->kind = RUDA_MIR_ARRAY_ASSUME_INDEX;
        get->kind = RUDA_MIR_ARRAY_GET_IN_BOUNDS;
        RUDA_EXPECT(ruda_mir_verify(&function)
            == RUDA_MIR_VERIFY_INVALID_ARGUMENT);
    }
    ruda_mir_function_dispose(&function);
}

static void ruda_test_mir_range_rejects_negative_true_edge(
    RudaSymbolId symbol
) {
    RudaMirFunction function;
    RudaMirBlock *entry = NULL;
    RudaMirBlockId body = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId exit = RUDA_MIR_NO_BLOCK;
    RudaMirRegister length = RUDA_MIR_NO_REGISTER;
    RudaMirRegister condition = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    RudaMirInstruction *index = NULL;
    RudaMirInstruction *carrier = NULL;
    RudaMirInstruction *discard = NULL;
    RudaMirInstruction *stored = NULL;
    RUDA_EXPECT(ruda_test_mir_range_build(
        &function, ruda_system_allocator(), symbol, false
    ));
    index = ruda_test_mir_range_instruction(&function, 2);
    carrier = ruda_test_mir_range_instruction(&function, 3);
    discard = ruda_test_mir_range_instruction(&function, 4);
    stored = ruda_test_mir_range_instruction(&function, 5);
    RUDA_EXPECT(index != NULL && carrier != NULL && discard != NULL
        && stored != NULL
        && ruda_mir_add_register(&function, RUDA_TYPE_I64, &length)
        && ruda_mir_add_register(&function, RUDA_TYPE_BOOL, &condition)
        && ruda_mir_add_block(&function, &body)
        && ruda_mir_add_block(&function, &exit));
    entry = ruda_vector_at(&function.blocks, 0);
    if (entry != NULL && index != NULL && carrier != NULL
        && discard != NULL && stored != NULL && body != RUDA_MIR_NO_BLOCK
        && exit != RUDA_MIR_NO_BLOCK) {
        index->integer = -1;
        carrier->kind = RUDA_MIR_COPY;
        carrier->first = 1;
        discard->kind = RUDA_MIR_COPY;
        discard->first = 0;
        stored->kind = RUDA_MIR_COPY;
        stored->first = 0;
        entry->terminator.kind = RUDA_MIR_TERMINATOR_UNSET;
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_ARRAY_LENGTH;
        instruction.target = length;
        instruction.first = 1;
        instruction.array = carrier->array;
        RUDA_EXPECT(ruda_mir_append_instruction(&function, 0, &instruction));
        instruction.kind = RUDA_MIR_LESS_I64;
        instruction.target = condition;
        instruction.first = 2;
        instruction.second = length;
        RUDA_EXPECT(ruda_mir_append_instruction(&function, 0, &instruction));
        entry->terminator = ruda_test_mir_branch(condition, body, exit);
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_ARRAY_ASSUME_INDEX;
        instruction.target = 3;
        instruction.first = 1;
        instruction.second = 2;
        instruction.array = carrier->array;
        RUDA_EXPECT(ruda_mir_append_instruction(&function, body, &instruction));
        instruction.kind = RUDA_MIR_ARRAY_GET_IN_BOUNDS;
        instruction.target = 4;
        instruction.first = 3;
        instruction.second = 2;
        RUDA_EXPECT(ruda_mir_append_instruction(&function, body, &instruction));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function, body, ruda_test_mir_return(4)
        ));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function, exit, ruda_test_mir_return(0)
        ));
        RUDA_EXPECT(ruda_mir_verify(&function)
            == RUDA_MIR_VERIFY_INVALID_ARGUMENT);
        index->integer = 0;
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    }
    ruda_mir_function_dispose(&function);
}

static void ruda_test_mir_range_allocation_fallback(RudaSymbolId symbol) {
    RudaFailAllocator state;
    RudaMirFunction function;
    size_t baseline_allocations = 0, baseline_bytes = 0, failure_index = 0;
    bool reached_success = false;
    ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
    RUDA_EXPECT(ruda_test_mir_range_build(
        &function, ruda_fail_allocator(&state), symbol, false
    ));
    baseline_allocations = state.live_allocations;
    baseline_bytes = state.live_bytes;
    for (failure_index = 0; failure_index < 80 && !reached_success;
        failure_index += 1) {
        RudaMirRangeOptimizeStatus status;
        RudaMirInstruction *get = ruda_test_mir_range_instruction(&function, 4);
        state.failure_index = state.attempts + failure_index;
        status = ruda_mir_optimize_ranges(&function);
        if (status == RUDA_MIR_RANGE_OPTIMIZE_OK) {
            reached_success = true;
            RUDA_EXPECT(get != NULL
                && get->kind == RUDA_MIR_ARRAY_GET_IN_BOUNDS);
        } else {
            RUDA_EXPECT(status == RUDA_MIR_RANGE_OPTIMIZE_ALLOCATION_FAILED);
            RUDA_EXPECT(get != NULL && get->kind == RUDA_MIR_ARRAY_GET);
        }
        RUDA_EXPECT(state.live_allocations == baseline_allocations);
        RUDA_EXPECT(state.live_bytes == baseline_bytes);
    }
    RUDA_EXPECT(reached_success);
    ruda_mir_function_dispose(&function);
    RUDA_EXPECT(state.live_allocations == 0);
    RUDA_EXPECT(state.live_bytes == 0);
}

static void ruda_test_mir_range_verifier_and_codec_allocations(
    RudaSymbolId symbol
) {
    RudaMirFunction checked;
    RudaBytecodeFunction lowered;
    RudaOwnedBytes encoded;
    size_t failure_index = 0;
    bool reached_success = false;
    memset(&lowered, 0, sizeof(lowered));
    memset(&encoded, 0, sizeof(encoded));
    RUDA_EXPECT(ruda_test_mir_range_build(
        &checked, ruda_system_allocator(), symbol, false
    ));
    RUDA_EXPECT(ruda_mir_optimize_ranges(&checked)
        == RUDA_MIR_RANGE_OPTIMIZE_OK);
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(
        &checked, ruda_system_allocator(),
        ruda_mir_bytecode_lower_default_options(), &lowered
    ) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_encode_function(
        &lowered, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);

    for (failure_index = 0; failure_index < 128 && !reached_success;
        failure_index += 1) {
        RudaFailAllocator state;
        RudaMirFunction function;
        RudaMirVerifyStatus status;
        size_t baseline_allocations = 0, baseline_bytes = 0;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        RUDA_EXPECT(ruda_test_mir_range_build(
            &function, ruda_fail_allocator(&state), symbol, false
        ));
        RUDA_EXPECT(ruda_mir_optimize_ranges(&function)
            == RUDA_MIR_RANGE_OPTIMIZE_OK);
        baseline_allocations = state.live_allocations;
        baseline_bytes = state.live_bytes;
        state.failure_index = state.attempts + failure_index;
        status = ruda_mir_verify(&function);
        if (status == RUDA_MIR_VERIFY_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(status == RUDA_MIR_VERIFY_ALLOCATION_FAILED);
        }
        RUDA_EXPECT(state.live_allocations == baseline_allocations);
        RUDA_EXPECT(state.live_bytes == baseline_bytes);
        ruda_mir_function_dispose(&function);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);

    for (failure_index = 0, reached_success = false;
        failure_index < 128 && !reached_success; failure_index += 1) {
        RudaFailAllocator state;
        RudaOwnedBytes output;
        RudaBytecodeCodecStatus status;
        memset(&output, 0, sizeof(output));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        status = ruda_bytecode_encode_function(
            &lowered, ruda_fail_allocator(&state), &output
        );
        if (status == RUDA_BYTECODE_CODEC_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
            RUDA_EXPECT(output.data == NULL && output.length == 0);
        }
        ruda_owned_bytes_dispose(&output);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);

    for (failure_index = 0, reached_success = false;
        failure_index < 128 && !reached_success; failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeFunction output;
        RudaBytecodeCodecStatus status;
        memset(&output, 0, sizeof(output));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        status = ruda_bytecode_decode_function(
            encoded.data, encoded.length, ruda_fail_allocator(&state),
            ruda_bytecode_function_default_options(), &output
        );
        if (status == RUDA_BYTECODE_CODEC_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
            RUDA_EXPECT(!output.initialized);
        }
        ruda_bytecode_function_dispose(&output);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);

    for (failure_index = 0, reached_success = false;
        failure_index < 128 && !reached_success; failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeFunction function;
        RudaBytecodeVerifyStatus status;
        size_t baseline_allocations = 0, baseline_bytes = 0;
        memset(&function, 0, sizeof(function));
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        RUDA_EXPECT(ruda_mir_lower_bytecode_function(
            &checked, ruda_fail_allocator(&state),
            ruda_mir_bytecode_lower_default_options(), &function
        ) == RUDA_MIR_BYTECODE_LOWER_OK);
        baseline_allocations = state.live_allocations;
        baseline_bytes = state.live_bytes;
        state.failure_index = state.attempts + failure_index;
        status = ruda_bytecode_verify(&function);
        if (status == RUDA_BYTECODE_VERIFY_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(status == RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED);
        }
        RUDA_EXPECT(state.live_allocations == baseline_allocations);
        RUDA_EXPECT(state.live_bytes == baseline_bytes);
        ruda_bytecode_function_dispose(&function);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_function_dispose(&lowered);
    ruda_mir_function_dispose(&checked);
}

void ruda_test_mir_range(void) {
    RudaSymbolId symbol = {{0x22, 0x58}};
    ruda_test_mir_range_optimizer(symbol);
    ruda_test_mir_range_invalidations(symbol);
    ruda_test_mir_range_interface_call_invalidates(symbol);
    ruda_test_mir_range_mutations_invalidate(symbol);
    ruda_test_mir_range_block_boundary_invalidates(symbol);
    ruda_test_mir_range_keeps_unproven_checks(symbol);
    ruda_test_mir_range_rejects_forged_assumption(symbol);
    ruda_test_mir_range_rejects_negative_true_edge(symbol);
    ruda_test_mir_range_allocation_fallback(symbol);
    ruda_test_mir_range_verifier_and_codec_allocations(symbol);
}
