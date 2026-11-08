#include "test_mir_bytecode_support.h"

#include <string.h>

static bool preparation_mir_function(MarimayMirFunction *function,
    MarimayAllocator allocator, MarimaySymbolId symbol) {
    MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    if (!mmay_mir_function_init(function, allocator, symbol, NULL, 0,
            MMAY_TYPE_I64, mmay_mir_function_default_options())
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &value)
        || !mmay_mir_add_block(function, &block)) return false;
    MarimayMirInstruction instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_CONSTANT_I64, value, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 42, false, 10);
    return mmay_mir_append_instruction(function, block, &instruction)
        && mmay_mir_set_terminator(function, block,
            mmay_test_mir_bytecode_terminator(MMAY_MIR_RETURN, value,
                MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 11));
}

static void preparation_compare_lowering(const MarimayMirFunction *input,
    const MarimayBytecodeProgram *program) {
    MarimayBytecodeFunction standalone = {0};
    MarimayOwnedBytes expected = {0}, actual = {0};
    MarimayAllocator allocator = mmay_system_allocator();
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(input, allocator,
        mmay_mir_bytecode_lower_default_options(), &standalone)
        == MMAY_MIR_BYTECODE_LOWER_OK);
    const MarimayBytecodeProgramFunction *lowered =
        mmay_bytecode_program_function_by_symbol(program, input->symbol);
    MMAY_EXPECT(lowered != NULL);
    if (lowered != NULL) {
        MMAY_EXPECT(mmay_bytecode_encode_function(&standalone, allocator,
            &expected) == MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(mmay_bytecode_encode_function(&lowered->body, allocator,
            &actual) == MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(actual.length == expected.length);
        if (actual.length == expected.length) {
            MMAY_EXPECT(memcmp(actual.data, expected.data, actual.length) == 0);
        }
    }
    mmay_owned_bytes_dispose(&actual);
    mmay_owned_bytes_dispose(&expected);
    mmay_bytecode_function_dispose(&standalone);
}

static void preparation_lowering_limits(void) {
    const MarimaySymbolId symbol = {{0x10}};
    MarimayFailAllocator state;
    MarimayMirFunction input = {0};
    MarimayBytecodeProgram program = {0};
    const MarimayMirFunction *functions[] = {&input};
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MarimayAllocator allocator = mmay_fail_allocator(&state);
    MMAY_EXPECT(preparation_mir_function(&input, allocator, symbol));
    bool saw_limit = false, saw_success = false;
    for (unsigned kind = 0; kind < 2; kind += 1) {
        for (size_t limit = 1; limit < 96; limit += 1) {
            MarimayMirBytecodeProgramLowerOptions options =
                mmay_mir_bytecode_program_lower_default_options();
            if (kind == 0) options.program_options.max_verify_work = limit;
            else options.program_options.function_options.max_verify_work = limit;
            MarimayMirBytecodeLowerStatus status = mmay_mir_lower_bytecode_program(
                functions, 1, symbol, allocator, options, &program);
            MMAY_EXPECT(status == MMAY_MIR_BYTECODE_LOWER_OK
                || status == MMAY_MIR_BYTECODE_LOWER_LIMIT_REACHED);
            if (status == MMAY_MIR_BYTECODE_LOWER_OK) {
                saw_success = true;
                MMAY_EXPECT(mmay_bytecode_program_verify(&program)
                    == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
                preparation_compare_lowering(&input, &program);
            } else {
                saw_limit = true;
                MMAY_EXPECT(!program.initialized);
            }
            mmay_bytecode_program_dispose(&program);
        }
    }
    MMAY_EXPECT(saw_limit && saw_success);
    MarimayMirBlock *block = mmay_vector_at(&input.blocks, 0);
    MMAY_EXPECT(block != NULL);
    if (block != NULL) {
        block->terminator.value = MMAY_MIR_NO_REGISTER;
        MMAY_EXPECT(mmay_mir_lower_bytecode_program(functions, 1, symbol,
            allocator, mmay_mir_bytecode_program_lower_default_options(),
            &program) == MMAY_MIR_BYTECODE_LOWER_INVALID_MIR);
        MMAY_EXPECT(!program.initialized);
    }
    mmay_bytecode_program_dispose(&program);
    mmay_mir_function_dispose(&input);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

static void preparation_lowering_failures(void) {
    const MarimaySymbolId symbols[] = {{{0x20}}, {{0x10}}};
    bool complete = false;
    for (size_t failure = 0; failure < 512 && !complete; failure += 1) {
        MarimayFailAllocator state;
        MarimayMirFunction input[2] = {0};
        MarimayBytecodeProgram program = {0};
        const MarimayMirFunction *functions[] = {&input[0], &input[1]};
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MarimayAllocator allocator = mmay_fail_allocator(&state);
        for (size_t index = 0; index < 2; index += 1) {
            MMAY_EXPECT(preparation_mir_function(&input[index], allocator,
                symbols[index]));
        }
        state.failure_index = state.attempts + failure;
        MarimayMirBytecodeLowerStatus status = mmay_mir_lower_bytecode_program(
            functions, 2, symbols[0], allocator,
            mmay_mir_bytecode_program_lower_default_options(), &program);
        MMAY_EXPECT(status == MMAY_MIR_BYTECODE_LOWER_OK
            || status == MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED);
        if (status == MMAY_MIR_BYTECODE_LOWER_OK) {
            complete = true;
            state.failure_index = SIZE_MAX;
            MMAY_EXPECT(program.entry_function == 1);
            preparation_compare_lowering(&input[0], &program);
            preparation_compare_lowering(&input[1], &program);
        } else {
            MMAY_EXPECT(!program.initialized);
        }
        mmay_bytecode_program_dispose(&program);
        mmay_mir_function_dispose(&input[0]);
        mmay_mir_function_dispose(&input[1]);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(complete);
}

void mmay_test_mir_program_preparation(void) {
    preparation_lowering_limits();
    preparation_lowering_failures();
}
