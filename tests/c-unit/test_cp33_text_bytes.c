#include "test_bytecode_support.h"
#include "test_mir_support.h"
#include "test_native_c_emitter_support.h"

#include <stdint.h>
#include <string.h>

#include "mmay/base/text.h"
#include "mmay/base/utf8.h"
#include "mmay/bytecode/mir_lower.h"
#include "mmay/bytecode/vm.h"

typedef struct MarimayCp33TextFixture {
    MarimayMirFunction function;
    MarimayMirArrayId strings_array;
    MarimayMirArrayId runes_array;
    MarimayMirArrayId bytes_array;
    MarimayMirRegister decoded;
    bool empty;
} MarimayCp33TextFixture;

void mmay_test_native_text_failures(
    const MarimayOwnedBytes *source, MarimaySymbolId entry, size_t expected_length
);

typedef struct MarimayCp33Token {
    const unsigned char *bytes;
    size_t width;
    bool zero_width;
    bool carriage_return;
    bool line_feed;
} MarimayCp33Token;

static uint32_t mmay_cp33_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

static size_t mmay_cp33_reference_graphemes(
    const MarimayCp33Token *tokens,
    size_t count
) {
    size_t index = 0;
    size_t graphemes = 0;
    while (index < count) {
        bool cr = tokens[index].carriage_return;
        index += 1U;
        graphemes += 1U;
        if (cr && index < count && tokens[index].line_feed) {
            index += 1U;
            continue;
        }
        while (index < count && tokens[index].zero_width) index += 1U;
    }
    return graphemes;
}

static void mmay_cp33_text_properties(void) {
    static const unsigned char ascii[] = {'A'};
    static const unsigned char nul[] = {0};
    static const unsigned char e_acute[] = {0xc3U, 0xa9U};
    static const unsigned char combining[] = {0xccU, 0x81U};
    static const unsigned char emoji[] = {0xf0U, 0x9fU, 0x98U, 0x80U};
    static const unsigned char cr[] = {'\r'};
    static const unsigned char lf[] = {'\n'};
    static const MarimayCp33Token choices[] = {
        {ascii, sizeof(ascii), false, false, false},
        {nul, sizeof(nul), false, false, false},
        {e_acute, sizeof(e_acute), false, false, false},
        {combining, sizeof(combining), true, false, false},
        {emoji, sizeof(emoji), false, false, false},
        {cr, sizeof(cr), false, true, false},
        {lf, sizeof(lf), false, false, true}
    };
    static const unsigned char invalid[][4] = {
        {0x80U, 0, 0, 0},
        {0xc0U, 0x80U, 0, 0},
        {0xe0U, 0x80U, 0x80U, 0},
        {0xedU, 0xa0U, 0x80U, 0},
        {0xf4U, 0x90U, 0x80U, 0x80U}
    };
    static const size_t invalid_lengths[] = {1, 2, 3, 3, 4};
    uint32_t random = UINT32_C(0x76c033);
    size_t iteration = 0;
    int64_t measured = -1;

    for (iteration = 0; iteration < 512; iteration += 1U) {
        unsigned char bytes[160];
        bool boundaries[sizeof(bytes) + 1U] = {false};
        MarimayCp33Token tokens[32];
        size_t token_count = mmay_cp33_random(&random) % 33U;
        size_t length = 0;
        size_t index = 0;
        size_t grapheme_offset = 0;
        size_t grapheme_count = 0;
        boundaries[0] = true;
        for (index = 0; index < token_count; index += 1U) {
            const MarimayCp33Token token = choices[
                mmay_cp33_random(&random)
                    % (sizeof(choices) / sizeof(choices[0]))
            ];
            tokens[index] = token;
            memcpy(bytes + length, token.bytes, token.width);
            length += token.width;
            boundaries[length] = true;
        }
        MMAY_EXPECT(mmay_utf8_validate((const char *)bytes, length, NULL));
        MMAY_EXPECT(mmay_text_byte_length(length, &measured));
        MMAY_EXPECT(measured == (int64_t)length);
        MMAY_EXPECT(mmay_text_rune_count(bytes, length, &measured));
        MMAY_EXPECT(measured == (int64_t)token_count);
        MMAY_EXPECT(mmay_text_grapheme_count(bytes, length, &measured));
        MMAY_EXPECT(measured == (int64_t)mmay_cp33_reference_graphemes(
            tokens, token_count
        ));
        for (index = 0; index <= length; index += 1U) {
            MMAY_EXPECT(mmay_text_is_rune_boundary(
                bytes, length, (int64_t)index
            ) == boundaries[index]);
        }
        while (grapheme_offset < length) {
            size_t end = SIZE_MAX;
            MMAY_EXPECT(mmay_text_next_grapheme(
                bytes, length, grapheme_offset, &end
            ));
            MMAY_EXPECT(end > grapheme_offset && end <= length);
            grapheme_offset = end;
            grapheme_count += 1U;
        }
        MMAY_EXPECT(grapheme_count
            == mmay_cp33_reference_graphemes(tokens, token_count));
    }

    for (iteration = 0; iteration < sizeof(invalid_lengths)
        / sizeof(invalid_lengths[0]); iteration += 1U) {
        size_t invalid_offset = SIZE_MAX;
        MMAY_EXPECT(!mmay_utf8_validate(
            (const char *)invalid[iteration], invalid_lengths[iteration],
            &invalid_offset
        ));
        MMAY_EXPECT(invalid_offset == 0);
        MMAY_EXPECT(!mmay_text_rune_count(
            invalid[iteration], invalid_lengths[iteration], &measured
        ));
        MMAY_EXPECT(!mmay_text_grapheme_count(
            invalid[iteration], invalid_lengths[iteration], &measured
        ));
    }
}

static void mmay_cp33_text_base_edges(void) {
    static const unsigned char text[] = "\t alpha\r\n";
    static const unsigned char alpha[] = "alpha";
    static const unsigned char crlf[] = "\r\n";
    size_t start = SIZE_MAX;
    size_t end = SIZE_MAX;
    int64_t count = -1;
    MMAY_EXPECT(mmay_text_range_start(NULL, 0) == NULL);
    MMAY_EXPECT(mmay_text_range_start(text, 0) == text);
    MMAY_EXPECT(mmay_text_range_start(text, 2) == text + 2);
    MMAY_EXPECT(mmay_text_range_start(text, sizeof(text) - 1U)
        == text + sizeof(text) - 1U);
    MMAY_EXPECT(!mmay_text_byte_length(0, NULL));
    MMAY_EXPECT(!mmay_text_rune_count(NULL, 1, &count));
    MMAY_EXPECT(!mmay_text_rune_count(text, sizeof(text) - 1U, NULL));
    MMAY_EXPECT(!mmay_text_grapheme_count(NULL, 1, &count));
    MMAY_EXPECT(!mmay_text_next_grapheme(
        text, sizeof(text) - 1U, sizeof(text) - 1U, &end
    ));
    MMAY_EXPECT(!mmay_text_is_rune_boundary(text, sizeof(text) - 1U, -1));
    MMAY_EXPECT(!mmay_text_is_rune_boundary(
        text, sizeof(text) - 1U, (int64_t)sizeof(text)
    ));
    MMAY_EXPECT(mmay_text_contains(
        text, sizeof(text) - 1U, alpha, sizeof(alpha) - 1U
    ));
    MMAY_EXPECT(mmay_text_starts_with(text, sizeof(text) - 1U, NULL, 0));
    MMAY_EXPECT(mmay_text_ends_with(
        text, sizeof(text) - 1U, crlf, sizeof(crlf) - 1U
    ));
    MMAY_EXPECT(!mmay_text_contains(NULL, 1, alpha, sizeof(alpha) - 1U));
    mmay_text_trim_ascii_range(text, sizeof(text) - 1U, &start, &end);
    MMAY_EXPECT(start == 2 && end == 7);
    MMAY_EXPECT(mmay_text_split_count(
        text, sizeof(text) - 1U, crlf, sizeof(crlf) - 1U, &count
    ));
    MMAY_EXPECT(count == 2);
    MMAY_EXPECT(!mmay_text_split_count(text, sizeof(text) - 1U, NULL, 0,
        &count));
}

static bool mmay_cp33_add_registers(
    MarimayCp33TextFixture *fixture,
    MarimayMirRegister registers[22]
) {
    static const MarimayTypeId types[22] = {
        MMAY_TYPE_STRING, MMAY_TYPE_STRING, MMAY_TYPE_STRING, MMAY_TYPE_I64,
        MMAY_TYPE_I64, MMAY_TYPE_I64, MMAY_TYPE_I64, MMAY_TYPE_BOOL,
        MMAY_TYPE_STRING, MMAY_TYPE_STRING, MMAY_TYPE_BOOL, MMAY_TYPE_BOOL,
        MMAY_TYPE_BOOL, MMAY_TYPE_STRING, MMAY_TYPE_I64,
        MMAY_TYPE_FIRST_DYNAMIC + 1, MMAY_TYPE_STRING,
        MMAY_TYPE_FIRST_DYNAMIC + 2, MMAY_TYPE_FIRST_DYNAMIC + 1,
        MMAY_TYPE_FIRST_DYNAMIC + 3, MMAY_TYPE_I64, MMAY_TYPE_STRING
    };
    size_t index = 0;
    for (index = 0; index < 22; index += 1U) {
        if (!mmay_mir_add_register(
                &fixture->function, types[index], &registers[index]
            )) return false;
    }
    return true;
}

static bool mmay_cp33_append_constant_string(
    MarimayMirFunction *function,
    MarimayMirBlockId block,
    MarimayMirRegister target,
    const unsigned char *bytes,
    size_t length
) {
    MarimayMirStringId string = MMAY_MIR_NO_STRING;
    MarimayMirInstruction instruction;
    if (!mmay_mir_add_string_constant(function, bytes, length, &string)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_STRING;
    instruction.target = target;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.string_constant = string;
    return mmay_mir_append_instruction(function, block, &instruction);
}

static bool mmay_cp33_append_text(
    MarimayCp33TextFixture *fixture,
    MarimayMirBlockId block,
    MarimayMirInstructionKind kind,
    MarimayMirTextOperation operation,
    MarimayMirRegister target,
    MarimayMirRegister first,
    MarimayMirRegister second,
    MarimayMirArrayId array
) {
    MarimayMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.text_operation = operation;
    instruction.target = target;
    instruction.first = first;
    instruction.second = second;
    instruction.array = array;
    return mmay_mir_append_instruction(&fixture->function, block, &instruction);
}

static bool mmay_cp33_append_substring(
    MarimayCp33TextFixture *fixture, MarimayMirBlockId block
) {
    MarimayMirInstruction instruction = {0};
    MarimayMirRegister start, end, target;
    if (!mmay_mir_add_register(&fixture->function, MMAY_TYPE_I64, &start)
        || !mmay_mir_add_register(&fixture->function, MMAY_TYPE_I64, &end)
        || !mmay_mir_add_register(&fixture->function, MMAY_TYPE_STRING, &target)) return false;
    instruction.kind = MMAY_MIR_CONSTANT_I64;
    instruction.target = start;
    instruction.integer = fixture->empty ? 0 : 1;
    if (!mmay_mir_append_instruction(&fixture->function, block, &instruction)) return false;
    instruction.target = end;
    instruction.integer = fixture->empty ? 0 : 6;
    if (!mmay_mir_append_instruction(&fixture->function, block, &instruction)) return false;
    instruction.kind = MMAY_MIR_TEXT_TERNARY;
    instruction.text_operation = MMAY_MIR_TEXT_SUBSTRING;
    instruction.target = target;
    instruction.first = 0;
    instruction.second = start;
    instruction.third = end;
    instruction.array = MMAY_MIR_NO_ARRAY;
    return mmay_mir_append_instruction(&fixture->function, block, &instruction);
}

static bool mmay_cp33_text_fixture_init(MarimayCp33TextFixture *fixture, bool empty) {
    static const unsigned char text[] = {
        '\t', 'A', 0xc3U, 0xa9U, 0xccU, 0x81U, '\r', '\n', 'Z', ' '
    };
    static const unsigned char separator[] = {'\r', '\n'};
    static const unsigned char needle[] = {'A', 0xc3U, 0xa9U};
    const MarimaySymbolId symbol = {{0x33U, 0x76U}};
    MarimayMirRegister r[22];
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction integer;
    memset(fixture, 0, sizeof(*fixture));
    fixture->empty = empty;
    fixture->strings_array = MMAY_MIR_NO_ARRAY;
    fixture->runes_array = MMAY_MIR_NO_ARRAY;
    fixture->bytes_array = MMAY_MIR_NO_ARRAY;
    if (!mmay_mir_function_init(
            &fixture->function, mmay_system_allocator(), symbol, NULL, 0,
            MMAY_TYPE_STRING, mmay_mir_function_default_options()
        ) || !mmay_mir_add_array(
            &fixture->function, MMAY_TYPE_FIRST_DYNAMIC + 1,
            MMAY_TYPE_STRING, &fixture->strings_array
        ) || !mmay_mir_add_array(
            &fixture->function, MMAY_TYPE_FIRST_DYNAMIC + 2,
            MMAY_TYPE_RUNE, &fixture->runes_array
        ) || !mmay_mir_add_array(
            &fixture->function, MMAY_TYPE_FIRST_DYNAMIC + 3,
            MMAY_TYPE_U8, &fixture->bytes_array
        ) || !mmay_cp33_add_registers(fixture, r)
        || !mmay_mir_add_block(&fixture->function, &block)
        || !mmay_cp33_append_constant_string(
            &fixture->function, block, r[0], text, empty ? 0 : sizeof(text)
        ) || !mmay_cp33_append_constant_string(
            &fixture->function, block, r[1], separator, sizeof(separator)
        ) || !mmay_cp33_append_constant_string(
            &fixture->function, block, r[2], needle, sizeof(needle)
        )) goto failure;
    /* A managed empty value, not just a static empty literal, must feed every
     * subsequent transform. Native empty string storage may legitimately be NULL. */
    if (empty && !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_UNARY,
            MMAY_MIR_TEXT_TRIM_ASCII, r[0], r[0], MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_ARRAY)) goto failure;
    memset(&integer, 0, sizeof(integer));
    integer.kind = MMAY_MIR_CONSTANT_I64;
    integer.target = r[3];
    integer.first = MMAY_MIR_NO_REGISTER;
    integer.second = MMAY_MIR_NO_REGISTER;
    integer.integer = empty ? 0 : 4;
    if (!mmay_mir_append_instruction(&fixture->function, block, &integer)
        || !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_UNARY,
            MMAY_MIR_TEXT_BYTE_LENGTH, r[4], r[0], MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_ARRAY)
        || !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_UNARY,
            MMAY_MIR_TEXT_RUNE_COUNT, r[5], r[0], MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_ARRAY)
        || !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_UNARY,
            MMAY_MIR_TEXT_GRAPHEME_COUNT, r[6], r[0], MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_ARRAY)
        || !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_BINARY,
            MMAY_MIR_TEXT_IS_RUNE_BOUNDARY, r[7], r[0], r[3],
            MMAY_MIR_NO_ARRAY)
        || !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_BINARY,
            MMAY_MIR_TEXT_DROP_PREFIX, r[8], r[0], r[3], MMAY_MIR_NO_ARRAY)
        || !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_BINARY,
            MMAY_MIR_TEXT_TAKE_PREFIX, r[9], r[0], r[3], MMAY_MIR_NO_ARRAY)
        || !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_BINARY,
            MMAY_MIR_TEXT_CONTAINS, r[10], r[0], r[2], MMAY_MIR_NO_ARRAY)
        || !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_BINARY,
            MMAY_MIR_TEXT_STARTS_WITH, r[11], r[0], r[2], MMAY_MIR_NO_ARRAY)
        || !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_BINARY,
            MMAY_MIR_TEXT_ENDS_WITH, r[12], r[0], r[2], MMAY_MIR_NO_ARRAY)
        || !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_UNARY,
            MMAY_MIR_TEXT_TRIM_ASCII, r[13], r[0], MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_ARRAY)
        || !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_BINARY,
            MMAY_MIR_TEXT_SPLIT_COUNT, r[14], r[0], r[1], MMAY_MIR_NO_ARRAY)
        || !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_BINARY,
            MMAY_MIR_TEXT_SPLIT, r[15], r[0], r[1], fixture->strings_array)
        || !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_BINARY,
            MMAY_MIR_TEXT_JOIN, r[16], r[15], r[1], fixture->strings_array)
        || !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_UNARY,
            MMAY_MIR_TEXT_RUNES, r[17], r[0], MMAY_MIR_NO_REGISTER,
            fixture->runes_array)
        || !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_UNARY,
            MMAY_MIR_TEXT_GRAPHEMES, r[18], r[0], MMAY_MIR_NO_REGISTER,
            fixture->strings_array)
        || !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_UNARY,
            MMAY_MIR_TEXT_ENCODE_UTF8, r[19], r[0], MMAY_MIR_NO_REGISTER,
            fixture->bytes_array)
        || !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_UNARY,
            MMAY_MIR_TEXT_INVALID_UTF8_OFFSET, r[20], r[19],
            MMAY_MIR_NO_REGISTER, fixture->bytes_array)
        || !mmay_cp33_append_text(fixture, block, MMAY_MIR_TEXT_UNARY,
            MMAY_MIR_TEXT_DECODE_UTF8, r[21], r[19], MMAY_MIR_NO_REGISTER,
            fixture->bytes_array)
        || !mmay_cp33_append_substring(fixture, block)
        || !mmay_mir_set_terminator(
            &fixture->function, block, mmay_test_mir_return(r[21])
        )) goto failure;
    fixture->decoded = r[21];
    return true;
failure:
    mmay_mir_function_dispose(&fixture->function);
    return false;
}

static void mmay_cp33_expect_mir_result(
    const MarimayCp33TextFixture *fixture
) {
    static const unsigned char expected[] = {
        '\t', 'A', 0xc3U, 0xa9U, 0xccU, 0x81U, '\r', '\n', 'Z', ' '
    };
    static const size_t expected_lengths[] = {2, 8, 6, 10};
    static const size_t empty_lengths[] = {1, 0, 0, 0};
    const size_t expected_length = fixture->empty ? 0 : sizeof(expected);
    MarimayMirExecution execution;
    MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
    MarimayMirValue result;
    const unsigned char *bytes = NULL;
    size_t length = 0;
    size_t index = 0;
    memset(&execution, 0, sizeof(execution));
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_mir_execution_init(
        &execution, mmay_system_allocator(),
        mmay_mir_execution_default_options()
    ));
    options.execution = &execution;
    MMAY_EXPECT(mmay_mir_interpret(
        &fixture->function, NULL, 0, mmay_system_allocator(), options, &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(mmay_mir_execution_string_bytes(
        &execution, result, &bytes, &length
    ));
    MMAY_EXPECT(length == expected_length
        && (length == 0 || memcmp(bytes, expected, expected_length) == 0));
    for (index = 0; index < sizeof(expected_lengths)
        / sizeof(expected_lengths[0]); index += 1U) {
        MarimayMirValue array;
        size_t array_length = SIZE_MAX;
        MMAY_EXPECT(mmay_mir_execution_array_at(&execution, index, &array));
        MMAY_EXPECT(mmay_mir_execution_array_length(
            &execution, array, &array_length
        ));
        MMAY_EXPECT(array_length == (fixture->empty
            ? empty_lengths[index] : expected_lengths[index]));
    }
    mmay_mir_execution_dispose(&execution);
}

static void mmay_cp33_hostile_mir(MarimayCp33TextFixture *fixture) {
    MarimayMirBlock *block = mmay_vector_at(&fixture->function.blocks, 0);
    size_t index = 0;
    MMAY_EXPECT(block != NULL && block->instructions.length == 25);
    for (index = 4; block != NULL && index < block->instructions.length;
        index += 1U) {
        MarimayMirInstruction *instruction = mmay_vector_at(
            &block->instructions, index
        );
        MarimayMirRegister saved = instruction != NULL
            ? instruction->target : MMAY_MIR_NO_REGISTER;
        MMAY_EXPECT(instruction != NULL);
        if (instruction == NULL) continue;
        instruction->target = MMAY_MIR_NO_REGISTER;
        MMAY_EXPECT(mmay_mir_verify(&fixture->function)
            == MMAY_MIR_VERIFY_INVALID_REGISTER);
        instruction->target = saved;
        if (instruction->kind == MMAY_MIR_TEXT_TERNARY) {
            MarimayMirRegister end = instruction->third;
            instruction->third = MMAY_MIR_NO_REGISTER;
            MMAY_EXPECT(mmay_mir_verify(&fixture->function) == MMAY_MIR_VERIFY_INVALID_REGISTER);
            instruction->third = 0;
            MMAY_EXPECT(mmay_mir_verify(&fixture->function) == MMAY_MIR_VERIFY_TYPE_MISMATCH);
            instruction->third = end;
        }
    }
    if (block != NULL) {
        MarimayMirInstruction *first = mmay_vector_at(&block->instructions, 4);
        MarimayMirInstruction *split = mmay_vector_at(&block->instructions, 15);
        MMAY_EXPECT(first != NULL && split != NULL);
        if (first != NULL) {
            first->kind = MMAY_MIR_TEXT_BINARY;
            first->second = 3;
            MMAY_EXPECT(mmay_mir_verify(&fixture->function)
                == MMAY_MIR_VERIFY_TYPE_MISMATCH);
            first->kind = MMAY_MIR_TEXT_UNARY;
            first->second = MMAY_MIR_NO_REGISTER;
            first->text_operation = (MarimayMirTextOperation)99;
            MMAY_EXPECT(mmay_mir_verify(&fixture->function)
                == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
            first->text_operation = MMAY_MIR_TEXT_BYTE_LENGTH;
        }
        if (split != NULL) {
            split->array = fixture->runes_array;
            MMAY_EXPECT(mmay_mir_verify(&fixture->function)
                == MMAY_MIR_VERIFY_TYPE_MISMATCH);
            split->array = fixture->strings_array;
        }
    }
    MMAY_EXPECT(mmay_mir_verify(&fixture->function) == MMAY_MIR_VERIFY_OK);
}

static void mmay_cp33_mir_allocation_failures(
    const MarimayCp33TextFixture *fixture
) {
    size_t failure_index = 0;
    size_t attempts = 0;
    for (failure_index = 0; failure_index <= attempts; failure_index += 1U) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayMirExecution execution;
        MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
        MarimayMirValue result;
        MarimayMirInterpretStatus status;
        memset(&execution, 0, sizeof(execution));
        memset(&result, 0, sizeof(result));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(),
            failure_index == 0 ? SIZE_MAX : failure_index - 1
        );
        allocator = mmay_fail_allocator(&state);
        if (!mmay_mir_execution_init(
                &execution, allocator, mmay_mir_execution_default_options()
            )) {
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
            continue;
        }
        options.execution = &execution;
        status = mmay_mir_interpret(
            &fixture->function, NULL, 0, allocator, options, &result
        );
        MMAY_EXPECT(status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED
            || status == MMAY_MIR_INTERPRET_OK);
        if (failure_index == 0) {
            attempts = state.attempts;
            MMAY_EXPECT(status == MMAY_MIR_INTERPRET_OK);
        }
        mmay_mir_execution_dispose(&execution);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(attempts > 0);
}

static void mmay_cp33_hostile_bytecode(MarimayBytecodeFunction *function) {
    MarimayBytecodeBlock *block = mmay_vector_at(&function->blocks, 0);
    MarimayBytecodeInstruction *split = NULL;
    size_t index = 0;
    size_t text_count = 0;
    MMAY_EXPECT(block != NULL);
    for (index = 0; block != NULL && index < block->instructions.length;
        index += 1U) {
        MarimayBytecodeInstruction *instruction = mmay_vector_at(
            &block->instructions, index
        );
        if (instruction == NULL
            || (instruction->opcode != MMAY_OPCODE_TEXT_SUBSTRING
                && (instruction->opcode < MMAY_OPCODE_TEXT_BYTE_LENGTH
                    || instruction->opcode > MMAY_OPCODE_TEXT_DECODE_UTF8))) continue;
        {
            uint64_t saved = instruction->operands[0].unsigned_value;
            instruction->operands[0].unsigned_value =
                function->register_types.length;
            MMAY_EXPECT(mmay_bytecode_verify(function)
                == MMAY_BYTECODE_VERIFY_INVALID_REGISTER);
            instruction->operands[0].unsigned_value = saved;
        }
        if (instruction->opcode == MMAY_OPCODE_TEXT_SUBSTRING) {
            uint64_t end = instruction->operands[3].unsigned_value;
            uint32_t unassigned = UINT32_MAX;
            MMAY_EXPECT(mmay_bytecode_add_register(function, MMAY_BYTECODE_TYPE_I64, &unassigned));
            instruction->operands[3].unsigned_value = unassigned;
            MMAY_EXPECT(mmay_bytecode_verify(function) == MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER);
            instruction->operands[3].unsigned_value = UINT64_MAX;
            MMAY_EXPECT(mmay_bytecode_verify(function) == MMAY_BYTECODE_VERIFY_INVALID_REGISTER);
            instruction->operands[3].unsigned_value = 0;
            MMAY_EXPECT(mmay_bytecode_verify(function) == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
            instruction->operands[3].unsigned_value = end;
        }
        if (instruction->opcode == MMAY_OPCODE_TEXT_SPLIT) split = instruction;
        text_count += 1U;
    }
    MMAY_EXPECT(text_count == 19);
    MMAY_EXPECT(split != NULL);
    if (split != NULL) {
        split->opcode = MMAY_OPCODE_TEXT_JOIN;
        MMAY_EXPECT(mmay_bytecode_verify(function)
            == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
        split->opcode = MMAY_OPCODE_TEXT_SPLIT;
    }
    MMAY_EXPECT(mmay_bytecode_verify(function) == MMAY_BYTECODE_VERIFY_OK);
}

static void mmay_cp33_vm_allocation_failures(
    const MarimayBytecodeFunction *function
) {
    size_t failure_index = 0;
    size_t attempts = 0;
    for (failure_index = 0; failure_index <= attempts; failure_index += 1U) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayBytecodeExecution execution;
        MarimayBytecodeExecutionOptions setup = mmay_bytecode_execution_default_options();
        MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
        MarimayBytecodeValue result;
        MarimayBytecodeRunStatus status;
        memset(&execution, 0, sizeof(execution));
        memset(&result, 0, sizeof(result));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(),
            failure_index == 0 ? SIZE_MAX : failure_index - 1
        );
        allocator = mmay_fail_allocator(&state);
        setup.managed_heap.collect_before_allocation = true;
        if (!mmay_bytecode_execution_init(
                &execution, allocator,
                setup
            )) {
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
            continue;
        }
        options.execution = &execution;
        status = mmay_bytecode_run(
            function, NULL, 0, allocator, options, &result
        );
        MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED
            || status == MMAY_BYTECODE_RUN_OK);
        if (failure_index == 0) {
            attempts = state.attempts;
            MMAY_EXPECT(status == MMAY_BYTECODE_RUN_OK);
        }
        mmay_bytecode_execution_dispose(&execution);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(attempts > 0);
}

static void mmay_cp33_lowering_allocation_failures(
    const MarimayCp33TextFixture *fixture
) {
    size_t failure_index = 0;
    size_t attempts = 0;
    for (failure_index = 0; failure_index <= attempts; failure_index += 1U) {
        MarimayFailAllocator state;
        MarimayBytecodeFunction lowered;
        MarimayMirBytecodeLowerStatus status;
        memset(&lowered, 0, sizeof(lowered));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(),
            failure_index == 0 ? SIZE_MAX : failure_index - 1
        );
        status = mmay_mir_lower_bytecode_function(
            &fixture->function, mmay_fail_allocator(&state),
            mmay_mir_bytecode_lower_default_options(), &lowered
        );
        MMAY_EXPECT(status == MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED
            || status == MMAY_MIR_BYTECODE_LOWER_OK);
        if (failure_index == 0) {
            attempts = state.attempts;
            MMAY_EXPECT(status == MMAY_MIR_BYTECODE_LOWER_OK);
        }
        mmay_bytecode_function_dispose(&lowered);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(attempts > 0);
}

static void mmay_cp33_native_text(
    const MarimayCp33TextFixture *fixture
) {
    const MarimayMirFunction *functions[1] = {&fixture->function};
    MarimayNativeCProgram program;
    MarimayOwnedBytes generated;
    size_t failure_index = 0;
    size_t attempts = 0;
    memset(&program, 0, sizeof(program));
    memset(&generated, 0, sizeof(generated));
    program.functions = functions;
    program.function_count = 1;
    program.entry_symbol = fixture->function.symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &program, mmay_system_allocator(), mmay_native_c_emit_default_options(),
        &generated
    ) == MMAY_NATIVE_C_EMIT_OK);
    MMAY_EXPECT(mmay_test_native_contains(
        &generated, "mmay_g_text_string_array"
    ));
    MMAY_EXPECT(mmay_test_native_contains(
        &generated, "mmay_text_range_start(text.data, start)"
    ));
    MMAY_EXPECT(mmay_test_native_contains(
        &generated, "mmay_text_range_start(mmay_g_v_0.data, (size_t)mmay_g_v_3)"
    ));
    MMAY_EXPECT(mmay_test_native_contains(
        &generated, "mmay_text_range_start(mmay_g_v_0.data, mmay_g_text_start)"
    ));
#if !defined(_WIN32)
    if (generated.data != NULL) {
        mmay_test_native_compile_and_run(
            &generated, fixture->empty ? "ok:string:0:\n"
                : "ok:string:10:0941c3a9cc810d0a5a20\n", 0
        );
        mmay_test_native_compile_and_run_configured(
            &generated, "trap:5:0:0\n", 1,
            "-DMMAY_G_FAIL_ALLOCATION_AT=0"
        );
        mmay_test_native_compile_and_run_configured(
            &generated, "trap:5:0:0\n", 1,
            "-DMMAY_G_FAIL_ALLOCATION_AT=1"
        );
        mmay_test_native_text_failures(
            &generated, fixture->function.symbol, fixture->empty ? 0 : 10
        );
    }
#endif
    mmay_owned_bytes_dispose(&generated);

    for (failure_index = 0; failure_index <= attempts; failure_index += 1U) {
        MarimayFailAllocator state;
        MarimayNativeCEmitStatus status;
        memset(&generated, 0, sizeof(generated));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(),
            failure_index == 0 ? SIZE_MAX : failure_index - 1
        );
        status = mmay_native_c_emit_program(
            &program, mmay_fail_allocator(&state),
            mmay_native_c_emit_default_options(), &generated
        );
        MMAY_EXPECT(status == MMAY_NATIVE_C_EMIT_ALLOCATION_FAILED
            || status == MMAY_NATIVE_C_EMIT_OK);
        if (failure_index == 0) {
            attempts = state.attempts;
            MMAY_EXPECT(status == MMAY_NATIVE_C_EMIT_OK);
        }
        mmay_owned_bytes_dispose(&generated);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(attempts > 0);
}

static void mmay_cp33_text_engines(MarimayCp33TextFixture *fixture) {
    static const unsigned char expected[] = {
        '\t', 'A', 0xc3U, 0xa9U, 0xccU, 0x81U, '\r', '\n', 'Z', ' '
    };
    const size_t expected_length = fixture->empty ? 0 : sizeof(expected);
    MarimayBytecodeFunction lowered;
    MarimayBytecodeFunction decoded;
    MarimayOwnedBytes encoded;
    MarimayBytecodeExecution execution;
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    MarimayBytecodeValue result;
    const unsigned char *bytes = NULL;
    size_t length = 0;
    memset(&lowered, 0, sizeof(lowered));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&execution, 0, sizeof(execution));
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(
        &fixture->function, mmay_system_allocator(),
        mmay_mir_bytecode_lower_default_options(), &lowered
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_verify(&lowered) == MMAY_BYTECODE_VERIFY_OK);
    if (!fixture->empty) mmay_cp33_hostile_bytecode(&lowered);
    MMAY_EXPECT(mmay_bytecode_encode_function(
        &lowered, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_function(
        encoded.data, encoded.length, mmay_system_allocator(),
        mmay_bytecode_function_default_options(), &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run(
        &decoded, NULL, 0, mmay_system_allocator(), options, &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(mmay_bytecode_execution_string_bytes(
        &execution, result, &bytes, &length
    ));
    MMAY_EXPECT(length == expected_length
        && (length == 0 || memcmp(bytes, expected, expected_length) == 0));
    mmay_bytecode_execution_dispose(&execution);
    mmay_cp33_vm_allocation_failures(&decoded);
    mmay_cp33_lowering_allocation_failures(fixture);
    mmay_bytecode_function_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_function_dispose(&lowered);
}

void mmay_test_cp33_text_bytes(void) {
    MarimayCp33TextFixture fixture;
    mmay_cp33_text_properties();
    mmay_cp33_text_base_edges();
    MMAY_EXPECT(mmay_cp33_text_fixture_init(&fixture, false));
    MMAY_EXPECT(mmay_mir_verify(&fixture.function) == MMAY_MIR_VERIFY_OK);
    mmay_cp33_hostile_mir(&fixture);
    mmay_cp33_expect_mir_result(&fixture);
    mmay_cp33_mir_allocation_failures(&fixture);
    mmay_cp33_text_engines(&fixture);
    mmay_cp33_native_text(&fixture);
    mmay_mir_function_dispose(&fixture.function);
    MMAY_EXPECT(mmay_cp33_text_fixture_init(&fixture, true));
    MMAY_EXPECT(mmay_mir_verify(&fixture.function) == MMAY_MIR_VERIFY_OK);
    mmay_cp33_expect_mir_result(&fixture);
    mmay_cp33_mir_allocation_failures(&fixture);
    mmay_cp33_text_engines(&fixture);
    mmay_cp33_native_text(&fixture);
    mmay_mir_function_dispose(&fixture.function);
}
