#include "test_support.h"

#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/bytecode/mir_lower.h"
#include "ruda/bytecode/vm.h"
#include "ruda/mir/mir.h"
#include "ruda/native/c_emitter.h"

#if !defined(_WIN32)
/* Owned by test_native_c_emitter.c; this fixture only supplies its MIR input. */
extern void ruda_test_native_compile_and_run(
    const RudaOwnedBytes *source,
    const char *expected_output,
    uint32_t expected_exit
);
#endif

static RudaMirInstruction ruda_test_native_matrix_instruction(
    RudaMirInstructionKind kind,
    RudaMirRegister target,
    RudaMirRegister first,
    RudaMirRegister second,
    RudaMirRegister third,
    size_t start
) {
    RudaMirInstruction instruction;
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

static RudaMirTerminator ruda_test_native_matrix_goto(RudaMirBlockId target) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_GOTO;
    terminator.value = RUDA_MIR_NO_REGISTER;
    terminator.target = target;
    terminator.otherwise = RUDA_MIR_NO_BLOCK;
    return terminator;
}

static RudaMirTerminator ruda_test_native_matrix_branch(
    RudaMirRegister condition,
    RudaMirBlockId when_true,
    RudaMirBlockId when_false
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_BRANCH;
    terminator.value = condition;
    terminator.target = when_true;
    terminator.otherwise = when_false;
    return terminator;
}

static RudaMirTerminator ruda_test_native_matrix_return(RudaMirRegister value) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_RETURN;
    terminator.value = value;
    terminator.target = RUDA_MIR_NO_BLOCK;
    terminator.otherwise = RUDA_MIR_NO_BLOCK;
    return terminator;
}

static bool ruda_test_native_matrix_append(
    RudaMirFunction *function,
    RudaMirBlockId block,
    RudaMirInstruction instruction
) {
    return ruda_mir_append_instruction(function, block, &instruction);
}

static bool ruda_test_native_matrix_build(RudaMirFunction *function) {
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
    const RudaSymbolId symbol = {{0x75}};
    const RudaTypeId inner_type = RUDA_TYPE_FIRST_DYNAMIC + 130;
    const RudaTypeId outer_type = RUDA_TYPE_FIRST_DYNAMIC + 131;
    RudaMirRegister f[13];
    RudaMirRegister row[7];
    RudaMirRegister matrix[3];
    RudaMirRegister i = RUDA_MIR_NO_REGISTER;
    RudaMirRegister j = RUDA_MIR_NO_REGISTER;
    RudaMirRegister k = RUDA_MIR_NO_REGISTER;
    RudaMirRegister two = RUDA_MIR_NO_REGISTER;
    RudaMirRegister three = RUDA_MIR_NO_REGISTER;
    RudaMirRegister one = RUDA_MIR_NO_REGISTER;
    RudaMirRegister condition = RUDA_MIR_NO_REGISTER;
    RudaMirRegister row_a = RUDA_MIR_NO_REGISTER;
    RudaMirRegister row_b = RUDA_MIR_NO_REGISTER;
    RudaMirRegister row_c = RUDA_MIR_NO_REGISTER;
    RudaMirRegister checked_c = RUDA_MIR_NO_REGISTER;
    RudaMirRegister left = RUDA_MIR_NO_REGISTER;
    RudaMirRegister right = RUDA_MIR_NO_REGISTER;
    RudaMirRegister current = RUDA_MIR_NO_REGISTER;
    RudaMirRegister product = RUDA_MIR_NO_REGISTER;
    RudaMirRegister next = RUDA_MIR_NO_REGISTER;
    RudaMirRegister checksum = RUDA_MIR_NO_REGISTER;
    RudaMirArrayId inner_array = RUDA_MIR_NO_ARRAY;
    RudaMirArrayId outer_array = RUDA_MIR_NO_ARRAY;
    RudaMirArrayConstructionId row_construction[7];
    RudaMirArrayConstructionId matrix_construction[3];
    RudaMirBlockId init = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId i_condition = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId i_body = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId j_condition = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId j_body = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId k_condition = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId k_body = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId k_step = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId j_step = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId i_step = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId sum = RUDA_MIR_NO_BLOCK;
    RudaMirArrayArgument arguments[3];
    RudaMirInstruction instruction;
    size_t index = 0;

    memset(function, 0, sizeof(*function));
    for (index = 0; index < 13; index += 1) {
        f[index] = RUDA_MIR_NO_REGISTER;
    }
    for (index = 0; index < 7; index += 1) {
        row[index] = RUDA_MIR_NO_REGISTER;
        row_construction[index] = RUDA_MIR_NO_ARRAY_CONSTRUCTION;
    }
    for (index = 0; index < 3; index += 1) {
        matrix[index] = RUDA_MIR_NO_REGISTER;
        matrix_construction[index] = RUDA_MIR_NO_ARRAY_CONSTRUCTION;
    }
    if (!ruda_mir_function_init(
            function, ruda_system_allocator(), symbol, NULL, 0,
            RUDA_TYPE_F64, ruda_mir_function_default_options()
        )) {
        goto failure;
    }
    for (index = 0; index < 13; index += 1) {
        if (!ruda_mir_add_register(function, RUDA_TYPE_F64, &f[index])) {
            goto failure;
        }
    }
    for (index = 0; index < 7; index += 1) {
        if (!ruda_mir_add_register(function, inner_type, &row[index])) {
            goto failure;
        }
    }
    for (index = 0; index < 3; index += 1) {
        if (!ruda_mir_add_register(function, outer_type, &matrix[index])) {
            goto failure;
        }
    }
    if (!ruda_mir_add_register(function, RUDA_TYPE_I64, &i)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &j)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &k)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &two)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &three)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &one)
        || !ruda_mir_add_register(function, RUDA_TYPE_BOOL, &condition)
        || !ruda_mir_add_register(function, inner_type, &row_a)
        || !ruda_mir_add_register(function, inner_type, &row_b)
        || !ruda_mir_add_register(function, inner_type, &row_c)
        || !ruda_mir_add_register(function, inner_type, &checked_c)
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &left)
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &right)
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &current)
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &product)
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &next)
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &checksum)
        || !ruda_mir_add_array(function, inner_type, RUDA_TYPE_F64, &inner_array)
        || !ruda_mir_add_array(function, outer_type, inner_type, &outer_array)) {
        goto failure;
    }

    arguments[0] = (RudaMirArrayArgument){f[0], RUDA_TYPE_F64};
    arguments[1] = (RudaMirArrayArgument){f[1], RUDA_TYPE_F64};
    arguments[2] = (RudaMirArrayArgument){f[2], RUDA_TYPE_F64};
    if (!ruda_mir_add_array_construction(function, inner_array, arguments, 3,
            &row_construction[0])) goto failure;
    arguments[0] = (RudaMirArrayArgument){f[3], RUDA_TYPE_F64};
    arguments[1] = (RudaMirArrayArgument){f[4], RUDA_TYPE_F64};
    arguments[2] = (RudaMirArrayArgument){f[5], RUDA_TYPE_F64};
    if (!ruda_mir_add_array_construction(function, inner_array, arguments, 3,
            &row_construction[1])) goto failure;
    arguments[0] = (RudaMirArrayArgument){f[6], RUDA_TYPE_F64};
    arguments[1] = (RudaMirArrayArgument){f[7], RUDA_TYPE_F64};
    if (!ruda_mir_add_array_construction(function, inner_array, arguments, 2,
            &row_construction[2])) goto failure;
    arguments[0] = (RudaMirArrayArgument){f[8], RUDA_TYPE_F64};
    arguments[1] = (RudaMirArrayArgument){f[9], RUDA_TYPE_F64};
    if (!ruda_mir_add_array_construction(function, inner_array, arguments, 2,
            &row_construction[3])) goto failure;
    arguments[0] = (RudaMirArrayArgument){f[10], RUDA_TYPE_F64};
    arguments[1] = (RudaMirArrayArgument){f[11], RUDA_TYPE_F64};
    if (!ruda_mir_add_array_construction(function, inner_array, arguments, 2,
            &row_construction[4])) goto failure;
    arguments[0] = (RudaMirArrayArgument){f[12], RUDA_TYPE_F64};
    arguments[1] = (RudaMirArrayArgument){f[12], RUDA_TYPE_F64};
    if (!ruda_mir_add_array_construction(function, inner_array, arguments, 2,
            &row_construction[5])) goto failure;
    if (!ruda_mir_add_array_construction(function, inner_array, arguments, 2,
            &row_construction[6])) goto failure;

    arguments[0] = (RudaMirArrayArgument){row[0], inner_type};
    arguments[1] = (RudaMirArrayArgument){row[1], inner_type};
    if (!ruda_mir_add_array_construction(function, outer_array, arguments, 2,
            &matrix_construction[0])) goto failure;
    arguments[0] = (RudaMirArrayArgument){row[2], inner_type};
    arguments[1] = (RudaMirArrayArgument){row[3], inner_type};
    arguments[2] = (RudaMirArrayArgument){row[4], inner_type};
    if (!ruda_mir_add_array_construction(function, outer_array, arguments, 3,
            &matrix_construction[1])) goto failure;
    arguments[0] = (RudaMirArrayArgument){row[5], inner_type};
    arguments[1] = (RudaMirArrayArgument){row[6], inner_type};
    if (!ruda_mir_add_array_construction(function, outer_array, arguments, 2,
            &matrix_construction[2])) goto failure;

    if (!ruda_mir_add_block(function, &init)
        || !ruda_mir_add_block(function, &i_condition)
        || !ruda_mir_add_block(function, &i_body)
        || !ruda_mir_add_block(function, &j_condition)
        || !ruda_mir_add_block(function, &j_body)
        || !ruda_mir_add_block(function, &k_condition)
        || !ruda_mir_add_block(function, &k_body)
        || !ruda_mir_add_block(function, &k_step)
        || !ruda_mir_add_block(function, &j_step)
        || !ruda_mir_add_block(function, &i_step)
        || !ruda_mir_add_block(function, &sum)) {
        goto failure;
    }

    for (index = 0; index < 13; index += 1) {
        instruction = ruda_test_native_matrix_instruction(
            RUDA_MIR_CONSTANT_F64, f[index], RUDA_MIR_NO_REGISTER,
            RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 100 + index
        );
        instruction.f64_bits = values[index];
        if (!ruda_test_native_matrix_append(function, init, instruction)) goto failure;
    }
    for (index = 0; index < 7; index += 1) {
        instruction = ruda_test_native_matrix_instruction(
            RUDA_MIR_ARRAY_NEW, row[index], RUDA_MIR_NO_REGISTER,
            RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 120 + index
        );
        instruction.array = inner_array;
        instruction.array_construction = row_construction[index];
        if (!ruda_test_native_matrix_append(function, init, instruction)) goto failure;
    }
    for (index = 0; index < 3; index += 1) {
        instruction = ruda_test_native_matrix_instruction(
            RUDA_MIR_ARRAY_NEW, matrix[index], RUDA_MIR_NO_REGISTER,
            RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 130 + index
        );
        instruction.array = outer_array;
        instruction.array_construction = matrix_construction[index];
        if (!ruda_test_native_matrix_append(function, init, instruction)) goto failure;
    }
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_CONSTANT_I64, i,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 140);
    instruction.integer = 0;
    if (!ruda_test_native_matrix_append(function, init, instruction)) goto failure;
    instruction.target = two;
    instruction.integer = 2;
    if (!ruda_test_native_matrix_append(function, init, instruction)) goto failure;
    instruction.target = three;
    instruction.integer = 3;
    if (!ruda_test_native_matrix_append(function, init, instruction)) goto failure;
    instruction.target = one;
    instruction.integer = 1;
    if (!ruda_test_native_matrix_append(function, init, instruction)) goto failure;
    instruction.target = j;
    instruction.integer = 0;
    if (!ruda_test_native_matrix_append(function, init, instruction)
        || !ruda_mir_set_terminator(function, init,
            ruda_test_native_matrix_goto(i_condition))) goto failure;

    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_LESS_I64,
        condition, i, two, RUDA_MIR_NO_REGISTER, 150);
    if (!ruda_test_native_matrix_append(function, i_condition, instruction)
        || !ruda_mir_set_terminator(function, i_condition,
            ruda_test_native_matrix_branch(condition, i_body, sum))) goto failure;

    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ARRAY_GET,
        row_a, matrix[0], i, RUDA_MIR_NO_REGISTER, 160);
    instruction.array = outer_array;
    if (!ruda_test_native_matrix_append(function, i_body, instruction)) goto failure;
    instruction.target = row_c;
    instruction.first = matrix[2];
    instruction.span.start = 161;
    instruction.span.end = 162;
    if (!ruda_test_native_matrix_append(function, i_body, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_CONSTANT_I64, j,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 162);
    instruction.integer = 0;
    if (!ruda_test_native_matrix_append(function, i_body, instruction)
        || !ruda_mir_set_terminator(function, i_body,
            ruda_test_native_matrix_goto(j_condition))) goto failure;

    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_LESS_I64,
        condition, j, two, RUDA_MIR_NO_REGISTER, 170);
    if (!ruda_test_native_matrix_append(function, j_condition, instruction)
        || !ruda_mir_set_terminator(function, j_condition,
            ruda_test_native_matrix_branch(condition, j_body, i_step))) goto failure;

    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_CONSTANT_I64, k,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 180);
    instruction.integer = 0;
    if (!ruda_test_native_matrix_append(function, j_body, instruction)
        || !ruda_mir_set_terminator(function, j_body,
            ruda_test_native_matrix_goto(k_condition))) goto failure;

    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_LESS_I64,
        condition, k, three, RUDA_MIR_NO_REGISTER, 190);
    if (!ruda_test_native_matrix_append(function, k_condition, instruction)
        || !ruda_mir_set_terminator(function, k_condition,
            ruda_test_native_matrix_branch(condition, k_body, j_step))) goto failure;

    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ARRAY_REQUIRE_INDEX,
        checked_c, row_c, j, RUDA_MIR_NO_REGISTER, 200);
    instruction.array = inner_array;
    if (!ruda_test_native_matrix_append(function, k_body, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ARRAY_GET,
        row_b, matrix[1], k, RUDA_MIR_NO_REGISTER, 201);
    instruction.array = outer_array;
    if (!ruda_test_native_matrix_append(function, k_body, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ARRAY_GET,
        left, row_a, k, RUDA_MIR_NO_REGISTER, 202);
    instruction.array = inner_array;
    if (!ruda_test_native_matrix_append(function, k_body, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ARRAY_GET,
        right, row_b, j, RUDA_MIR_NO_REGISTER, 203);
    instruction.array = inner_array;
    if (!ruda_test_native_matrix_append(function, k_body, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ARRAY_GET,
        current, row_c, j, RUDA_MIR_NO_REGISTER, 204);
    instruction.array = inner_array;
    if (!ruda_test_native_matrix_append(function, k_body, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_MULTIPLY_F64,
        product, left, right, RUDA_MIR_NO_REGISTER, 205);
    if (!ruda_test_native_matrix_append(function, k_body, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ADD_F64,
        next, current, product, RUDA_MIR_NO_REGISTER, 206);
    if (!ruda_test_native_matrix_append(function, k_body, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ARRAY_SET,
        current, checked_c, j, next, 207);
    instruction.array = inner_array;
    if (!ruda_test_native_matrix_append(function, k_body, instruction)
        || !ruda_mir_set_terminator(function, k_body,
            ruda_test_native_matrix_goto(k_step))) goto failure;

    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ADD_CHECKED_I64,
        k, k, one, RUDA_MIR_NO_REGISTER, 210);
    if (!ruda_test_native_matrix_append(function, k_step, instruction)
        || !ruda_mir_set_terminator(function, k_step,
            ruda_test_native_matrix_goto(k_condition))) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ADD_CHECKED_I64,
        j, j, one, RUDA_MIR_NO_REGISTER, 211);
    if (!ruda_test_native_matrix_append(function, j_step, instruction)
        || !ruda_mir_set_terminator(function, j_step,
            ruda_test_native_matrix_goto(j_condition))) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ADD_CHECKED_I64,
        i, i, one, RUDA_MIR_NO_REGISTER, 212);
    if (!ruda_test_native_matrix_append(function, i_step, instruction)
        || !ruda_mir_set_terminator(function, i_step,
            ruda_test_native_matrix_goto(i_condition))) goto failure;

    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_CONSTANT_I64, i,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 220);
    instruction.integer = 0;
    if (!ruda_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction.target = j;
    if (!ruda_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ARRAY_GET,
        row_c, matrix[2], i, RUDA_MIR_NO_REGISTER, 221);
    instruction.array = outer_array;
    if (!ruda_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ARRAY_GET,
        checksum, row_c, j, RUDA_MIR_NO_REGISTER, 222);
    instruction.array = inner_array;
    if (!ruda_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_CONSTANT_I64, j,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 223);
    instruction.integer = 1;
    if (!ruda_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ARRAY_GET,
        current, row_c, j, RUDA_MIR_NO_REGISTER, 224);
    instruction.array = inner_array;
    if (!ruda_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ADD_F64,
        checksum, checksum, current, RUDA_MIR_NO_REGISTER, 225);
    if (!ruda_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_CONSTANT_I64, i,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 226);
    instruction.integer = 1;
    if (!ruda_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction.target = j;
    instruction.integer = 0;
    if (!ruda_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ARRAY_GET,
        row_c, matrix[2], i, RUDA_MIR_NO_REGISTER, 227);
    instruction.array = outer_array;
    if (!ruda_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ARRAY_GET,
        current, row_c, j, RUDA_MIR_NO_REGISTER, 228);
    instruction.array = inner_array;
    if (!ruda_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ADD_F64,
        checksum, checksum, current, RUDA_MIR_NO_REGISTER, 229);
    if (!ruda_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_CONSTANT_I64, j,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 230);
    instruction.integer = 1;
    if (!ruda_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ARRAY_GET,
        current, row_c, j, RUDA_MIR_NO_REGISTER, 231);
    instruction.array = inner_array;
    if (!ruda_test_native_matrix_append(function, sum, instruction)) goto failure;
    instruction = ruda_test_native_matrix_instruction(RUDA_MIR_ADD_F64,
        checksum, checksum, current, RUDA_MIR_NO_REGISTER, 232);
    if (!ruda_test_native_matrix_append(function, sum, instruction)
        || !ruda_mir_set_terminator(function, sum,
            ruda_test_native_matrix_return(checksum))) goto failure;
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}

void ruda_test_native_matrix(void) {
    RudaMirFunction function;
    RudaMirExecution mir_execution;
    RudaMirInterpretOptions mir_options;
    RudaMirValue mir_result;
    RudaBytecodeFunction lowered;
    RudaBytecodeFunction decoded;
    RudaBytecodeExecution vm_execution;
    RudaBytecodeExecutionOptions execution_options;
    RudaBytecodeRunOptions vm_options;
    RudaBytecodeValue vm_result;
    RudaOwnedBytes encoded;
    RudaOwnedBytes generated;
    RudaNativeCProgram native_program;
    const RudaMirFunction *functions[1];
    const uint64_t expected = UINT64_C(0x4079f00000000000);

    memset(&function, 0, sizeof(function));
    memset(&mir_execution, 0, sizeof(mir_execution));
    memset(&lowered, 0, sizeof(lowered));
    memset(&decoded, 0, sizeof(decoded));
    memset(&vm_execution, 0, sizeof(vm_execution));
    memset(&encoded, 0, sizeof(encoded));
    memset(&generated, 0, sizeof(generated));
    RUDA_EXPECT(ruda_test_native_matrix_build(&function));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_execution_init(
        &mir_execution, ruda_system_allocator(),
        ruda_mir_execution_default_options()
    ));
    mir_options = ruda_mir_interpret_default_options();
    mir_options.execution = &mir_execution;
    memset(&mir_result, 0, sizeof(mir_result));
    RUDA_EXPECT(ruda_mir_interpret(
        &function, NULL, 0, ruda_system_allocator(), mir_options, &mir_result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(mir_result.type == RUDA_TYPE_F64 && mir_result.as.f64_bits == expected);

    RUDA_EXPECT(ruda_mir_lower_bytecode_function(
        &function, ruda_system_allocator(), ruda_mir_bytecode_lower_default_options(),
        &lowered
    ) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_encode_function(
        &lowered, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_decode_function(
        encoded.data, encoded.length, ruda_system_allocator(),
        ruda_bytecode_function_default_options(), &decoded
    ) == RUDA_BYTECODE_CODEC_OK);
    execution_options = ruda_bytecode_execution_default_options();
    execution_options.managed_heap.collect_before_allocation = true;
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &vm_execution, ruda_system_allocator(), execution_options
    ));
    vm_options = ruda_bytecode_run_default_options();
    vm_options.execution = &vm_execution;
    memset(&vm_result, 0, sizeof(vm_result));
    RUDA_EXPECT(ruda_bytecode_run(
        &decoded, NULL, 0, ruda_system_allocator(), vm_options, &vm_result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(vm_result.type == RUDA_BYTECODE_TYPE_F64
        && vm_result.as.f64_bits == expected);

    functions[0] = &function;
    native_program.functions = functions;
    native_program.function_count = 1;
    native_program.entry_symbol = function.symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(
        &native_program, ruda_system_allocator(), ruda_native_c_emit_default_options(),
        &generated
    ) == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    ruda_test_native_compile_and_run(
        &generated, "ok:f64:4079f00000000000\n", 0
    );
#endif
    ruda_owned_bytes_dispose(&generated);
    ruda_bytecode_execution_dispose(&vm_execution);
    ruda_bytecode_function_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_function_dispose(&lowered);
    ruda_mir_execution_dispose(&mir_execution);
    ruda_mir_function_dispose(&function);
}
