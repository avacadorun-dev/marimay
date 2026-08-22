#include "test_support.h"

#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/bytecode/mir_lower.h"
#include "mmay/bytecode/vm.h"
#include "mmay/mir/mir.h"
#include "mmay/native/c_emitter.h"

#if !defined(_WIN32)
/* Owned by test_native_c_emitter.c; this fixture only supplies its MIR input. */
extern void mmay_test_native_compile_and_run(
    const MarimayOwnedBytes *source,
    const char *expected_output,
    uint32_t expected_exit
);
#endif

static MarimayMirInstruction mmay_test_native_matrix_instruction(
    MarimayMirInstructionKind kind,
    MarimayMirRegister target,
    MarimayMirRegister first,
    MarimayMirRegister second,
    MarimayMirRegister third,
    size_t start
) {
    MarimayMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = first;
    instruction.second = second;
    instruction.third = third;
    instruction.span.start = start;
    instruction.span.end = start + 1;
    return instruction;
}

static MarimayMirTerminator mmay_test_native_matrix_goto(MarimayMirBlockId target) {
    MarimayMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = MMAY_MIR_GOTO;
    terminator.value = MMAY_MIR_NO_REGISTER;
    terminator.target = target;
    terminator.otherwise = MMAY_MIR_NO_BLOCK;
    return terminator;
}

static MarimayMirTerminator mmay_test_native_matrix_branch(
    MarimayMirRegister condition,
    MarimayMirBlockId when_true,
    MarimayMirBlockId when_false
) {
    MarimayMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = MMAY_MIR_BRANCH;
    terminator.value = condition;
    terminator.target = when_true;
    terminator.otherwise = when_false;
    return terminator;
}

static MarimayMirTerminator mmay_test_native_matrix_return(MarimayMirRegister value) {
    MarimayMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = MMAY_MIR_RETURN;
    terminator.value = value;
    terminator.target = MMAY_MIR_NO_BLOCK;
    terminator.otherwise = MMAY_MIR_NO_BLOCK;
    return terminator;
}

static bool mmay_test_native_matrix_append(
    MarimayMirFunction *function,
    MarimayMirBlockId block,
    MarimayMirInstruction instruction
) {
    return mmay_mir_append_instruction(function, block, &instruction);
}

static bool mmay_test_native_matrix_build(MarimayMirFunction *function) {
    static const uint64_t values[] = {
        UINT64_C(0x3ff0000000000000), /* 1 */
        UINT64_C(0x4000000000000000), /* 2 */
        UINT64_C(0x4008000000000000), /* 3 */
        UINT64_C(0x4010000000000000), /* 4 */
        UINT64_C(0x4014000000000000), /* 5 */
        UINT64_C(0x4018000000000000), /* 6 */
        UINT64_C(0x401c000000000000), /* 7 */
        UINT64_C(0x4020000000000000), /* 8 */
        UINT64_C(0x4022000000000000), /* 9 */
        UINT64_C(0x4024000000000000), /* 10 */
        UINT64_C(0x4026000000000000), /* 11 */
        UINT64_C(0x4028000000000000), /* 12 */
        UINT64_C(0x0000000000000000)  /* 0 */
    };
    const MarimaySymbolId symbol = {{0x75}};
    const MarimayTypeId inner_type = MMAY_TYPE_FIRST_DYNAMIC + 130;
    const MarimayTypeId outer_type = MMAY_TYPE_FIRST_DYNAMIC + 131;
    MarimayMirRegister f[13];
    MarimayMirRegister row[7];
    MarimayMirRegister matrix[3];
    MarimayMirRegister i = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister j = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister k = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister two = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister three = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister one = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister condition = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister row_a = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister row_b = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister row_c = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister checked_c = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister left = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister right = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister current = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister product = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister next = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister checksum = MMAY_MIR_NO_REGISTER;
    MarimayMirArrayId inner_array = MMAY_MIR_NO_ARRAY;
    MarimayMirArrayId outer_array = MMAY_MIR_NO_ARRAY;
    MarimayMirArrayConstructionId row_construction[7];
    MarimayMirArrayConstructionId matrix_construction[3];
    MarimayMirBlockId init = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId i_condition = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId i_body = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId j_condition = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId j_body = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId k_condition = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId k_body = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId k_step = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId j_step = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId i_step = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId sum = MMAY_MIR_NO_BLOCK;
    MarimayMirArrayArgument arguments[3];
    MarimayMirInstruction instruction;
    size_t index = 0;

    memset(function, 0, sizeof(*function));
    for (index = 0; index < 13; index += 1) {
        f[index] = MMAY_MIR_NO_REGISTER;
    }
    for (index = 0; index < 7; index += 1) {
        row[index] = MMAY_MIR_NO_REGISTER;
        row_construction[index] = MMAY_MIR_NO_ARRAY_CONSTRUCTION;
    }
    for (index = 0; index < 3; index += 1) {
        matrix[index] = MMAY_MIR_NO_REGISTER;
        matrix_construction[index] = MMAY_MIR_NO_ARRAY_CONSTRUCTION;
    }
    if (!mmay_mir_function_init(
            function, mmay_system_allocator(), symbol, NULL, 0,
            MMAY_TYPE_F64, mmay_mir_function_default_options()
        )) {
        goto failure;
    }
    for (index = 0; index < 13; index += 1) {
        if (!mmay_mir_add_register(function, MMAY_TYPE_F64, &f[index])) {
            goto failure;
        }
    }
    for (index = 0; index < 7; index += 1) {
        if (!mmay_mir_add_register(function, inner_type, &row[index])) {
            goto failure;
        }
    }
    for (index = 0; index < 3; index += 1) {
        if (!mmay_mir_add_register(function, outer_type, &matrix[index])) {
            goto failure;
        }
    }
    if (!mmay_mir_add_register(function, MMAY_TYPE_I64, &i)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &j)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &k)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &two)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &three)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &one)
        || !mmay_mir_add_register(function, MMAY_TYPE_BOOL, &condition)
        || !mmay_mir_add_register(function, inner_type, &row_a)
        || !mmay_mir_add_register(function, inner_type, &row_b)
        || !mmay_mir_add_register(function, inner_type, &row_c)
        || !mmay_mir_add_register(function, inner_type, &checked_c)
        || !mmay_mir_add_register(function, MMAY_TYPE_F64, &left)
        || !mmay_mir_add_register(function, MMAY_TYPE_F64, &right)
        || !mmay_mir_add_register(function, MMAY_TYPE_F64, &current)
        || !mmay_mir_add_register(function, MMAY_TYPE_F64, &product)
        || !mmay_mir_add_register(function, MMAY_TYPE_F64, &next)
        || !mmay_mir_add_register(function, MMAY_TYPE_F64, &checksum)
        || !mmay_mir_add_array(function, inner_type, MMAY_TYPE_F64, &inner_array)
        || !mmay_mir_add_array(function, outer_type, inner_type, &outer_array)) {
        goto failure;
    }

    arguments[0] = (MarimayMirArrayArgument){f[0], MMAY_TYPE_F64};
    arguments[1] = (MarimayMirArrayArgument){f[1], MMAY_TYPE_F64};
    arguments[2] = (MarimayMirArrayArgument){f[2], MMAY_TYPE_F64};
    if (!mmay_mir_add_array_construction(function, inner_array, arguments, 3,
            &row_construction[0])) goto failure;
    arguments[0] = (MarimayMirArrayArgument){f[3], MMAY_TYPE_F64};
    arguments[1] = (MarimayMirArrayArgument){f[4], MMAY_TYPE_F64};
    arguments[2] = (MarimayMirArrayArgument){f[5], MMAY_TYPE_F64};
    if (!mmay_mir_add_array_construction(function, inner_array, arguments, 3,
            &row_construction[1])) goto failure;
    arguments[0] = (MarimayMirArrayArgument){f[6], MMAY_TYPE_F64};
    arguments[1] = (MarimayMirArrayArgument){f[7], MMAY_TYPE_F64};
    if (!mmay_mir_add_array_construction(function, inner_array, arguments, 2,
            &row_construction[2])) goto failure;
    arguments[0] = (MarimayMirArrayArgument){f[8], MMAY_TYPE_F64};
    arguments[1] = (MarimayMirArrayArgument){f[9], MMAY_TYPE_F64};
    if (!mmay_mir_add_array_construction(function, inner_array, arguments, 2,
            &row_construction[3])) goto failure;
    arguments[0] = (MarimayMirArrayArgument){f[10], MMAY_TYPE_F64};
    arguments[1] = (MarimayMirArrayArgument){f[11], MMAY_TYPE_F64};
    if (!mmay_mir_add_array_construction(function, inner_array, arguments, 2,
            &row_construction[4])) goto failure;
    arguments[0] = (MarimayMirArrayArgument){f[12], MMAY_TYPE_F64};
    arguments[1] = (MarimayMirArrayArgument){f[12], MMAY_TYPE_F64};
    if (!mmay_mir_add_array_construction(function, inner_array, arguments, 2,
            &row_construction[5])) goto failure;
    if (!mmay_mir_add_array_construction(function, inner_array, arguments, 2,
            &row_construction[6])) goto failure;

    arguments[0] = (MarimayMirArrayArgument){row[0], inner_type};
    arguments[1] = (MarimayMirArrayArgument){row[1], inner_type};
    if (!mmay_mir_add_array_construction(function, outer_array, arguments, 2,
            &matrix_construction[0])) goto failure;
    arguments[0] = (MarimayMirArrayArgument){row[2], inner_type};
    arguments[1] = (MarimayMirArrayArgument){row[3], inner_type};
    arguments[2] = (MarimayMirArrayArgument){row[4], inner_type};
    if (!mmay_mir_add_array_construction(function, outer_array, arguments, 3,
            &matrix_construction[1])) goto failure;
    arguments[0] = (MarimayMirArrayArgument){row[5], inner_type};
    arguments[1] = (MarimayMirArrayArgument){row[6], inner_type};
    if (!mmay_mir_add_array_construction(function, outer_array, arguments, 2,
            &matrix_construction[2])) goto failure;

    if (!mmay_mir_add_block(function, &init)
        || !mmay_mir_add_block(function, &i_condition)
        || !mmay_mir_add_block(function, &i_body)
        || !mmay_mir_add_block(function, &j_condition)
        || !mmay_mir_add_block(function, &j_body)
        || !mmay_mir_add_block(function, &k_condition)
        || !mmay_mir_add_block(function, &k_body)
        || !mmay_mir_add_block(function, &k_step)
        || !mmay_mir_add_block(function, &j_step)
        || !mmay_mir_add_block(function, &i_step)
        || !mmay_mir_add_block(function, &sum)) {
        goto failure;
    }

    for (index = 0; index < 13; index += 1) {
        instruction = mmay_test_native_matrix_instruction(
            MMAY_MIR_CONSTANT_F64, f[index], MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 100 + index
        );
        instruction.f64_bits = values[index];
        if (!mmay_test_native_matrix_append(function, init, instruction)) goto failure;
    }
    for (index = 0; index < 7; index += 1) {
        instruction = mmay_test_native_matrix_instruction(
            MMAY_MIR_ARRAY_NEW, row[index], MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 120 + index
        );
        instruction.array = inner_array;
        instruction.array_construction = row_construction[index];
        if (!mmay_test_native_matrix_append(function, init, instruction)) goto failure;
    }
    for (index = 0; index < 3; index += 1) {
        instruction = mmay_test_native_matrix_instruction(
            MMAY_MIR_ARRAY_NEW, matrix[index], MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 130 + index
        );
        instruction.array = outer_array;
        instruction.array_construction = matrix_construction[index];
        if (!mmay_test_native_matrix_append(function, init, instruction)) goto failure;
    }
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_CONSTANT_I64, i,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 140);
    instruction.integer = 0;
    if (!mmay_test_native_matrix_append(function, init, instruction)) goto failure;
    instruction.target = two;
    instruction.integer = 2;
    if (!mmay_test_native_matrix_append(function, init, instruction)) goto failure;
    instruction.target = three;
    instruction.integer = 3;
    if (!mmay_test_native_matrix_append(function, init, instruction)) goto failure;
    instruction.target = one;
    instruction.integer = 1;
    if (!mmay_test_native_matrix_append(function, init, instruction)) goto failure;
    instruction.target = j;
    instruction.integer = 0;
    if (!mmay_test_native_matrix_append(function, init, instruction)
        || !mmay_mir_set_terminator(function, init,
            mmay_test_native_matrix_goto(i_condition))) goto failure;

    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_LESS_I64,
        condition, i, two, MMAY_MIR_NO_REGISTER, 150);
    if (!mmay_test_native_matrix_append(function, i_condition, instruction)
        || !mmay_mir_set_terminator(function, i_condition,
            mmay_test_native_matrix_branch(condition, i_body, sum))) goto failure;

    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ARRAY_GET,
        row_a, matrix[0], i, MMAY_MIR_NO_REGISTER, 160);
    instruction.array = outer_array;
    if (!mmay_test_native_matrix_append(function, i_body, instruction)) goto failure;
    instruction.target = row_c;
    instruction.first = matrix[2];
    instruction.span.start = 161;
    instruction.span.end = 162;
    if (!mmay_test_native_matrix_append(function, i_body, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_CONSTANT_I64, j,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 162);
    instruction.integer = 0;
    if (!mmay_test_native_matrix_append(function, i_body, instruction)
        || !mmay_mir_set_terminator(function, i_body,
            mmay_test_native_matrix_goto(j_condition))) goto failure;

    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_LESS_I64,
        condition, j, two, MMAY_MIR_NO_REGISTER, 170);
    if (!mmay_test_native_matrix_append(function, j_condition, instruction)
        || !mmay_mir_set_terminator(function, j_condition,
            mmay_test_native_matrix_branch(condition, j_body, i_step))) goto failure;

    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_CONSTANT_I64, k,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 180);
    instruction.integer = 0;
    if (!mmay_test_native_matrix_append(function, j_body, instruction)
        || !mmay_mir_set_terminator(function, j_body,
            mmay_test_native_matrix_goto(k_condition))) goto failure;

    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_LESS_I64,
        condition, k, three, MMAY_MIR_NO_REGISTER, 190);
    if (!mmay_test_native_matrix_append(function, k_condition, instruction)
        || !mmay_mir_set_terminator(function, k_condition,
            mmay_test_native_matrix_branch(condition, k_body, j_step))) goto failure;

    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ARRAY_REQUIRE_INDEX,
        checked_c, row_c, j, MMAY_MIR_NO_REGISTER, 200);
    instruction.array = inner_array;
    if (!mmay_test_native_matrix_append(function, k_body, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ARRAY_GET,
        row_b, matrix[1], k, MMAY_MIR_NO_REGISTER, 201);
    instruction.array = outer_array;
    if (!mmay_test_native_matrix_append(function, k_body, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ARRAY_GET,
        left, row_a, k, MMAY_MIR_NO_REGISTER, 202);
    instruction.array = inner_array;
    if (!mmay_test_native_matrix_append(function, k_body, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ARRAY_GET,
        right, row_b, j, MMAY_MIR_NO_REGISTER, 203);
    instruction.array = inner_array;
    if (!mmay_test_native_matrix_append(function, k_body, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ARRAY_GET,
        current, row_c, j, MMAY_MIR_NO_REGISTER, 204);
    instruction.array = inner_array;
    if (!mmay_test_native_matrix_append(function, k_body, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_MULTIPLY_F64,
        product, left, right, MMAY_MIR_NO_REGISTER, 205);
    if (!mmay_test_native_matrix_append(function, k_body, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ADD_F64,
        next, current, product, MMAY_MIR_NO_REGISTER, 206);
    if (!mmay_test_native_matrix_append(function, k_body, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ARRAY_SET,
        current, checked_c, j, next, 207);
    instruction.array = inner_array;
    if (!mmay_test_native_matrix_append(function, k_body, instruction)
        || !mmay_mir_set_terminator(function, k_body,
            mmay_test_native_matrix_goto(k_step))) goto failure;

    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ADD_CHECKED_I64,
        k, k, one, MMAY_MIR_NO_REGISTER, 210);
    if (!mmay_test_native_matrix_append(function, k_step, instruction)
        || !mmay_mir_set_terminator(function, k_step,
            mmay_test_native_matrix_goto(k_condition))) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ADD_CHECKED_I64,
        j, j, one, MMAY_MIR_NO_REGISTER, 211);
    if (!mmay_test_native_matrix_append(function, j_step, instruction)
        || !mmay_mir_set_terminator(function, j_step,
            mmay_test_native_matrix_goto(j_condition))) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ADD_CHECKED_I64,
        i, i, one, MMAY_MIR_NO_REGISTER, 212);
    if (!mmay_test_native_matrix_append(function, i_step, instruction)
        || !mmay_mir_set_terminator(function, i_step,
            mmay_test_native_matrix_goto(i_condition))) goto failure;

    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_CONSTANT_I64, i,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 220);
    instruction.integer = 0;
    if (!mmay_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction.target = j;
    if (!mmay_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ARRAY_GET,
        row_c, matrix[2], i, MMAY_MIR_NO_REGISTER, 221);
    instruction.array = outer_array;
    if (!mmay_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ARRAY_GET,
        checksum, row_c, j, MMAY_MIR_NO_REGISTER, 222);
    instruction.array = inner_array;
    if (!mmay_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_CONSTANT_I64, j,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 223);
    instruction.integer = 1;
    if (!mmay_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ARRAY_GET,
        current, row_c, j, MMAY_MIR_NO_REGISTER, 224);
    instruction.array = inner_array;
    if (!mmay_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ADD_F64,
        checksum, checksum, current, MMAY_MIR_NO_REGISTER, 225);
    if (!mmay_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_CONSTANT_I64, i,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 226);
    instruction.integer = 1;
    if (!mmay_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction.target = j;
    instruction.integer = 0;
    if (!mmay_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ARRAY_GET,
        row_c, matrix[2], i, MMAY_MIR_NO_REGISTER, 227);
    instruction.array = outer_array;
    if (!mmay_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ARRAY_GET,
        current, row_c, j, MMAY_MIR_NO_REGISTER, 228);
    instruction.array = inner_array;
    if (!mmay_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ADD_F64,
        checksum, checksum, current, MMAY_MIR_NO_REGISTER, 229);
    if (!mmay_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_CONSTANT_I64, j,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 230);
    instruction.integer = 1;
    if (!mmay_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ARRAY_GET,
        current, row_c, j, MMAY_MIR_NO_REGISTER, 231);
    instruction.array = inner_array;
    if (!mmay_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = mmay_test_native_matrix_instruction(MMAY_MIR_ADD_F64,
        checksum, checksum, current, MMAY_MIR_NO_REGISTER, 232);
    if (!mmay_test_native_matrix_append(function, sum, instruction)
        || !mmay_mir_set_terminator(function, sum,
            mmay_test_native_matrix_return(checksum))) goto failure;
    return true;

failure:
    mmay_mir_function_dispose(function);
    return false;
}

void mmay_test_native_matrix(void) {
    MarimayMirFunction function;
    MarimayMirExecution mir_execution;
    MarimayMirInterpretOptions mir_options;
    MarimayMirValue mir_result;
    MarimayBytecodeFunction lowered;
    MarimayBytecodeFunction decoded;
    MarimayBytecodeExecution vm_execution;
    MarimayBytecodeExecutionOptions execution_options;
    MarimayBytecodeRunOptions vm_options;
    MarimayBytecodeValue vm_result;
    MarimayOwnedBytes encoded;
    MarimayOwnedBytes generated;
    MarimayNativeCProgram native_program;
    const MarimayMirFunction *functions[1];
    const uint64_t expected = UINT64_C(0x4079f00000000000);

    memset(&function, 0, sizeof(function));
    memset(&mir_execution, 0, sizeof(mir_execution));
    memset(&lowered, 0, sizeof(lowered));
    memset(&decoded, 0, sizeof(decoded));
    memset(&vm_execution, 0, sizeof(vm_execution));
    memset(&encoded, 0, sizeof(encoded));
    memset(&generated, 0, sizeof(generated));
    MMAY_EXPECT(mmay_test_native_matrix_build(&function));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_execution_init(
        &mir_execution, mmay_system_allocator(),
        mmay_mir_execution_default_options()
    ));
    mir_options = mmay_mir_interpret_default_options();
    mir_options.execution = &mir_execution;
    memset(&mir_result, 0, sizeof(mir_result));
    MMAY_EXPECT(mmay_mir_interpret(
        &function, NULL, 0, mmay_system_allocator(), mir_options, &mir_result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(mir_result.type == MMAY_TYPE_F64 && mir_result.as.f64_bits == expected);

    MMAY_EXPECT(mmay_mir_lower_bytecode_function(
        &function, mmay_system_allocator(), mmay_mir_bytecode_lower_default_options(),
        &lowered
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_encode_function(
        &lowered, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_function(
        encoded.data, encoded.length, mmay_system_allocator(),
        mmay_bytecode_function_default_options(), &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    execution_options = mmay_bytecode_execution_default_options();
    execution_options.managed_heap.collect_before_allocation = true;
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &vm_execution, mmay_system_allocator(), execution_options
    ));
    vm_options = mmay_bytecode_run_default_options();
    vm_options.execution = &vm_execution;
    memset(&vm_result, 0, sizeof(vm_result));
    MMAY_EXPECT(mmay_bytecode_run(
        &decoded, NULL, 0, mmay_system_allocator(), vm_options, &vm_result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(vm_result.type == MMAY_BYTECODE_TYPE_F64
        && vm_result.as.f64_bits == expected);

    functions[0] = &function;
    native_program.functions = functions;
    native_program.function_count = 1;
    native_program.entry_symbol = function.symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &native_program, mmay_system_allocator(), mmay_native_c_emit_default_options(),
        &generated
    ) == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    mmay_test_native_compile_and_run(
        &generated, "ok:f64:4079f00000000000\n", 0
    );
#endif
    mmay_owned_bytes_dispose(&generated);
    mmay_bytecode_execution_dispose(&vm_execution);
    mmay_bytecode_function_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_function_dispose(&lowered);
    mmay_mir_execution_dispose(&mir_execution);
    mmay_mir_function_dispose(&function);
}
