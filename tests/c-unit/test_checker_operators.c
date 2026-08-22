#include "test_checker_support.h"

void mmay_test_checker_short_circuit(void) {
    static const char source[] =
        "const safeAnd = false && (1 / 0 == 0);\n"
        "const safeOr = true || (1 / 0 == 0);\n"
        "fn andSkip(): bool { return false && (1 / 0 == 0); }\n"
        "fn orSkip(): bool { return true || (1 / 0 == 0); }\n"
        "fn andEvaluate(): bool { return true && (6 / 2 == 3); }\n"
        "fn orEvaluate(): bool { return false || (6 / 2 == 3); }\n"
        "fn nestedRight(): bool { return true && (false || true); }\n"
        "fn nestedLeft(): bool { return (false || true) && true; }\n";
    static const char invalid[] =
        "fn bad(): bool { return 1 && true; }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedConstant *safe_and = NULL;
    const MarimayCheckedConstant *safe_or = NULL;
    size_t index = 0;
    MarimayMirValue result;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/short-circuit",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.constants.length == 2);
        MMAY_EXPECT(checked.functions.length == 6);
        safe_and = mmay_checked_constant(&checked, 0);
        safe_or = mmay_checked_constant(&checked, 1);
        MMAY_EXPECT(safe_and != NULL);
        MMAY_EXPECT(safe_or != NULL);
        if (safe_and != NULL && safe_or != NULL) {
            MMAY_EXPECT(safe_and->value.type == MMAY_TYPE_BOOL);
            MMAY_EXPECT(!safe_and->value.as.boolean);
            MMAY_EXPECT(safe_or->value.type == MMAY_TYPE_BOOL);
            MMAY_EXPECT(safe_or->value.as.boolean);
        }
        for (index = 0; index < checked.functions.length; index += 1) {
            const MarimayCheckedFunction *function = mmay_checked_function(
                &checked,
                index
            );
            MMAY_EXPECT(function != NULL);
            if (function == NULL) {
                continue;
            }
            MMAY_EXPECT(function->mir.blocks.length == (index < 4 ? 4 : 7));
            MMAY_EXPECT(mmay_mir_interpret(
                &function->mir,
                NULL,
                0,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_BOOL);
            if (index == 0) {
                MMAY_EXPECT(!result.as.boolean);
            } else {
                MMAY_EXPECT(result.as.boolean);
            }
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
    mmay_checker_expect_one_diagnostic(
        invalid,
        MMAY_DIAGNOSTIC_INVALID_OPERATOR,
        "&&",
        0
    );
}

void mmay_test_checker_bitwise(void) {
    static const char source[] =
        "const mask: int = (((~0) & 255) ^ 15) | 256;\n"
        "fn apply(value: int): int {\n"
        "  let current: int = value;\n"
        "  current &= 255;\n"
        "  current ^= 15;\n"
        "  current |= 256;\n"
        "  return current;\n"
        "}\n"
        "fn negative(): int { return ~0; }\n";
    static const char invalid[] =
        "fn bad(): bool { return true & false; }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedConstant *mask = NULL;
    const MarimayCheckedFunction *apply = NULL;
    const MarimayCheckedFunction *negative = NULL;
    MarimayMirValue argument;
    MarimayMirValue result;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/bitwise",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.constants.length == 1);
        MMAY_EXPECT(checked.functions.length == 2);
        mask = mmay_checked_constant(&checked, 0);
        apply = mmay_checked_function(&checked, 0);
        negative = mmay_checked_function(&checked, 1);
        MMAY_EXPECT(mask != NULL);
        MMAY_EXPECT(apply != NULL);
        MMAY_EXPECT(negative != NULL);
        if (mask != NULL) {
            MMAY_EXPECT(mask->value.type == MMAY_TYPE_I64);
            MMAY_EXPECT(mask->value.as.i64 == 496);
        }
        if (apply != NULL) {
            argument.type = MMAY_TYPE_I64;
            argument.as.i64 = 291;
            MMAY_EXPECT(mmay_mir_interpret(
                &apply->mir,
                &argument,
                1,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == 300);
        }
        if (negative != NULL) {
            MMAY_EXPECT(mmay_mir_interpret(
                &negative->mir,
                NULL,
                0,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == -1);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
    mmay_checker_expect_one_diagnostic(
        invalid,
        MMAY_DIAGNOSTIC_INVALID_OPERATOR,
        "&",
        0
    );

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "fn bad(left: i32, right: i32): i32 { return left & right; }\n"
    ));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/bitwise",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized);
    if (checked.initialized) {
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_numeric_widths(void) {
    static const char source[] =
        "const i8MinValue: i8 = -128;\n"
        "const i16MinValue: i16 = -32768;\n"
        "const i32MinValue: i32 = -2147483648;\n"
        "const i64MinValue: i64 = -9223372036854775808;\n"
        "const u8MaxValue: u8 = 255;\n"
        "const u16MaxValue: u16 = 65535;\n"
        "const u32MaxValue: u32 = 4294967295;\n"
        "const u64MaxValue: u64 = 18446744073709551615;\n"
        "fn i8Ops(value: i8): i8 {\n"
        "  let x: i8 = value; x += 1; x -= 1; x *= 2; x /= 2; x %= 3;\n"
        "  x &= 15; x ^= 3; x |= 1; x <<= 1; x >>= 1; return x;\n"
        "}\n"
        "fn i16Ops(value: i16): i16 {\n"
        "  let x: i16 = value; x += 1; x -= 1; x *= 2; x /= 2; x %= 3;\n"
        "  x &= 15; x ^= 3; x |= 1; x <<= 1; x >>= 1; return x;\n"
        "}\n"
        "fn i32Ops(value: i32): i32 {\n"
        "  let x: i32 = value; x += 1; x -= 1; x *= 2; x /= 2; x %= 3;\n"
        "  x &= 15; x ^= 3; x |= 1; x <<= 1; x >>= 1; return x;\n"
        "}\n"
        "fn i64Ops(value: i64): i64 {\n"
        "  let x: i64 = value; x += 1; x -= 1; x *= 2; x /= 2; x %= 3;\n"
        "  x &= 15; x ^= 3; x |= 1; x <<= 1; x >>= 1; return x;\n"
        "}\n"
        "fn u8Ops(value: u8): u8 {\n"
        "  let x: u8 = value; x += 1; x -= 1; x *= 2; x /= 2; x %= 3;\n"
        "  x &= 15; x ^= 3; x |= 1; x <<= 1; x >>= 1; return x;\n"
        "}\n"
        "fn u16Ops(value: u16): u16 {\n"
        "  let x: u16 = value; x += 1; x -= 1; x *= 2; x /= 2; x %= 3;\n"
        "  x &= 15; x ^= 3; x |= 1; x <<= 1; x >>= 1; return x;\n"
        "}\n"
        "fn u32Ops(value: u32): u32 {\n"
        "  let x: u32 = value; x += 1; x -= 1; x *= 2; x /= 2; x %= 3;\n"
        "  x &= 15; x ^= 3; x |= 1; x <<= 1; x >>= 1; return x;\n"
        "}\n"
        "fn u64Ops(value: u64): u64 {\n"
        "  let x: u64 = value; x += 1; x -= 1; x *= 2; x /= 2; x %= 3;\n"
        "  x &= 15; x ^= 3; x |= 1; x <<= 1; x >>= 1; return x;\n"
        "}\n"
        "fn i8Min(): i8 { return i8MinValue; }\n"
        "fn i16Min(): i16 { return i16MinValue; }\n"
        "fn i32Min(): i32 { return i32MinValue; }\n"
        "fn i64Min(): i64 { return i64MinValue; }\n"
        "fn u8Max(): u8 { return u8MaxValue; }\n"
        "fn u16Max(): u16 { return u16MaxValue; }\n"
        "fn u32Max(): u32 { return u32MaxValue; }\n"
        "fn u64Max(): u64 { return u64MaxValue; }\n";
    static const MarimayTypeId types[] = {
        MMAY_TYPE_I8, MMAY_TYPE_I16, MMAY_TYPE_I32, MMAY_TYPE_I64,
        MMAY_TYPE_U8, MMAY_TYPE_U16, MMAY_TYPE_U32, MMAY_TYPE_U64
    };
    static const uint64_t minimum_bits[] = {
        UINT64_C(0x80), UINT64_C(0x8000), UINT64_C(0x80000000),
        UINT64_C(0x8000000000000000), UINT64_C(0), UINT64_C(0),
        UINT64_C(0), UINT64_C(0)
    };
    static const uint64_t maximum_bits[] = {
        UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0),
        UINT64_C(0xff), UINT64_C(0xffff), UINT64_C(0xffffffff),
        UINT64_C(0xffffffffffffffff)
    };
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayCheckStatus status;
    size_t index = 0;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    status = mmay_check_ast(
        &fixture.ast,
        "test/numeric-widths",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    );
    MMAY_EXPECT(status == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.constants.length == 8);
        MMAY_EXPECT(checked.functions.length == 16);
        for (index = 0; index < 16; index += 1) {
            const MarimayCheckedFunction *function = mmay_checked_function(
                &checked, index
            );
            MarimayMirValue argument;
            MarimayMirValue result;
            uint64_t expected_bits = index < 8 ? UINT64_C(3)
                : (index < 12 ? minimum_bits[index - 8]
                               : maximum_bits[index - 8]);
            MMAY_EXPECT(function != NULL);
            if (function == NULL) {
                continue;
            }
            argument.type = types[index < 8 ? index : index - 8];
            argument.as.integer_bits = UINT64_C(6);
            MMAY_EXPECT(mmay_mir_interpret(
                &function->mir,
                index < 8 ? &argument : NULL,
                index < 8 ? 1 : 0,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == types[index < 8 ? index : index - 8]);
            MMAY_EXPECT(result.as.integer_bits == expected_bits);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_shifts(void) {
    static const char source[] =
        "const lowest: int = -1 << 63;\n"
        "const signed: int = -7 >> 1;\n"
        "const precedence: int = 1 + 1 << 2;\n"
        "fn compound(value: int, count: int): int {\n"
        "  let current: int = value;\n"
        "  current <<= count;\n"
        "  current >>= 1;\n"
        "  return current;\n"
        "}\n"
        "fn right(value: int, count: int): int { return value >> count; }\n";
    static const char overflow[] =
        "const bad: int = 1 << 63;\n";
    static const char negative_count[] =
        "const bad: int = 1 << -1;\n";
    static const char large_count[] =
        "const bad: int = 1 >> 64;\n";
    static const char invalid[] =
        "fn bad(): bool { return true << false; }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedConstant *lowest = NULL;
    const MarimayCheckedConstant *signed_value = NULL;
    const MarimayCheckedConstant *precedence = NULL;
    const MarimayCheckedFunction *compound = NULL;
    const MarimayCheckedFunction *right = NULL;
    MarimayMirValue arguments[2];
    MarimayMirValue result;

    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/shifts",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.constants.length == 3);
        MMAY_EXPECT(checked.functions.length == 2);
        lowest = mmay_checked_constant(&checked, 0);
        signed_value = mmay_checked_constant(&checked, 1);
        precedence = mmay_checked_constant(&checked, 2);
        compound = mmay_checked_function(&checked, 0);
        right = mmay_checked_function(&checked, 1);
        MMAY_EXPECT(lowest != NULL);
        MMAY_EXPECT(signed_value != NULL);
        MMAY_EXPECT(precedence != NULL);
        MMAY_EXPECT(compound != NULL);
        MMAY_EXPECT(right != NULL);
        if (lowest != NULL && signed_value != NULL && precedence != NULL) {
            MMAY_EXPECT(lowest->value.as.i64 == INT64_MIN);
            MMAY_EXPECT(signed_value->value.as.i64 == -4);
            MMAY_EXPECT(precedence->value.as.i64 == 8);
        }
        arguments[0].type = MMAY_TYPE_I64;
        arguments[1].type = MMAY_TYPE_I64;
        if (compound != NULL) {
            arguments[0].as.i64 = 3;
            arguments[1].as.i64 = 2;
            MMAY_EXPECT(mmay_mir_interpret(
                &compound->mir,
                arguments,
                2,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == 6);
            arguments[0].as.i64 = 1;
            arguments[1].as.i64 = 63;
            MMAY_EXPECT(mmay_mir_interpret(
                &compound->mir,
                arguments,
                2,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OVERFLOW);
        }
        if (right != NULL) {
            arguments[0].as.i64 = -7;
            arguments[1].as.i64 = 1;
            MMAY_EXPECT(mmay_mir_interpret(
                &right->mir,
                arguments,
                2,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == -4);
            arguments[1].as.i64 = 64;
            MMAY_EXPECT(mmay_mir_interpret(
                &right->mir,
                arguments,
                2,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_INVALID_SHIFT);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    mmay_checker_expect_one_diagnostic(
        overflow,
        MMAY_DIAGNOSTIC_CONSTANT_EVALUATION,
        "1 << 63",
        0
    );
    mmay_checker_expect_one_diagnostic(
        negative_count,
        MMAY_DIAGNOSTIC_CONSTANT_EVALUATION,
        "1 << -1",
        0
    );
    mmay_checker_expect_one_diagnostic(
        large_count,
        MMAY_DIAGNOSTIC_CONSTANT_EVALUATION,
        "1 >> 64",
        0
    );
    mmay_checker_expect_one_diagnostic(
        invalid,
        MMAY_DIAGNOSTIC_INVALID_OPERATOR,
        "<<",
        0
    );

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "fn bad(left: i32, right: i32): i32 { return left << right; }\n"
    ));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/shifts",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized);
    if (checked.initialized) {
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_division_and_remainder(void) {
    static const char source[] =
        "fn quotient(left: int, right: int): int { return left / right; }\n"
        "fn remainder(left: int, right: int): int { return left % right; }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayMirValue arguments[2];
    MarimayMirValue result;
    const MarimayCheckedFunction *quotient = NULL;
    const MarimayCheckedFunction *remainder = NULL;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/division-remainder",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        quotient = mmay_checked_function(&checked, 0);
        remainder = mmay_checked_function(&checked, 1);
        MMAY_EXPECT(quotient != NULL);
        MMAY_EXPECT(remainder != NULL);
        arguments[0].type = MMAY_TYPE_I64;
        arguments[1].type = MMAY_TYPE_I64;

        if (quotient != NULL) {
            arguments[0].as.i64 = -7;
            arguments[1].as.i64 = 3;
            MMAY_EXPECT(mmay_mir_interpret(
                &quotient->mir,
                arguments,
                2,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == -2);
            arguments[0].as.i64 = 7;
            arguments[1].as.i64 = -3;
            MMAY_EXPECT(mmay_mir_interpret(
                &quotient->mir,
                arguments,
                2,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == -2);
            arguments[0].as.i64 = INT64_MIN;
            arguments[1].as.i64 = -1;
            MMAY_EXPECT(mmay_mir_interpret(
                &quotient->mir,
                arguments,
                2,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OVERFLOW);
            arguments[0].as.i64 = 1;
            arguments[1].as.i64 = 0;
            MMAY_EXPECT(mmay_mir_interpret(
                &quotient->mir,
                arguments,
                2,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_DIVISION_BY_ZERO);
        }

        if (remainder != NULL) {
            arguments[0].as.i64 = -7;
            arguments[1].as.i64 = 3;
            MMAY_EXPECT(mmay_mir_interpret(
                &remainder->mir,
                arguments,
                2,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == -1);
            arguments[0].as.i64 = 7;
            arguments[1].as.i64 = -3;
            MMAY_EXPECT(mmay_mir_interpret(
                &remainder->mir,
                arguments,
                2,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == 1);
            arguments[0].as.i64 = INT64_MIN;
            arguments[1].as.i64 = -1;
            MMAY_EXPECT(mmay_mir_interpret(
                &remainder->mir,
                arguments,
                2,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OVERFLOW);
            arguments[0].as.i64 = 1;
            arguments[1].as.i64 = 0;
            MMAY_EXPECT(mmay_mir_interpret(
                &remainder->mir,
                arguments,
                2,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_DIVISION_BY_ZERO);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_f64_literals_and_operators(void) {
    static const char half_min_subnormal[] =
        "24703282292062327208828439643411068618252990130716238221279284125033775363510437593264991818081799618989828234772285886546332835517796989819938739800539093906315035659515570226392290858392449105184435931802849936536152500319370457678249219365623669863658480757001585769269903706311928279558551332927834338409351978015531246597263579574622766465272827220056374006485499977096599470454020828166226237857393450736339007967761930577506740176324673600968951340535537458516661134223766678604162159680461914467291840300530057530849048765391711386591646239524912623653881879636239373280423891018672348497668235089863388587925628302755995657524455507255189313690836254779186948667994968324049705821028513185451396213837722826145437693412532098591327667236328125";
    static const char beyond_half_min_subnormal[] =
        "24703282292062327208828439643411068618252990130716238221279284125033775363510437593264991818081799618989828234772285886546332835517796989819938739800539093906315035659515570226392290858392449105184435931802849936536152500319370457678249219365623669863658480757001585769269903706311928279558551332927834338409351978015531246597263579574622766465272827220056374006485499977096599470454020828166226237857393450736339007967761930577506740176324673600968951340535537458516661134223766678604162159680461914467291840300530057530849048765391711386591646239524912623653881879636239373280423891018672348497668235089863388587925628302755995657524455507255189313690836254779186948667994968324049705821028513185451396213837722826145437693412532098591327667236328126";
    static const uint64_t expected_f64[] = {
        UINT64_C(0x4340000000000000),
        UINT64_C(0x0000000000000001),
        UINT64_C(0x0000000000000000),
        UINT64_C(0x0000000000000001),
        UINT64_C(0x7fefffffffffffff),
        UINT64_C(0x7ff0000000000000),
        UINT64_C(0x4029000000000000),
        UINT64_C(0x8000000000000000),
        UINT64_C(0x400e000000000000),
        UINT64_C(0x4008000000000000),
        UINT64_C(0xc018000000000000),
        UINT64_C(0x7ff0000000000000)
    };
    static const MarimayMirInstructionKind arithmetic_kinds[] = {
        MMAY_MIR_ADD_F64,
        MMAY_MIR_SUBTRACT_F64,
        MMAY_MIR_MULTIPLY_F64,
        MMAY_MIR_DIVIDE_F64
    };
    static const MarimayMirInstructionKind comparison_kinds[] = {
        MMAY_MIR_EQUAL_F64,
        MMAY_MIR_EQUAL_F64,
        MMAY_MIR_LESS_F64,
        MMAY_MIR_LESS_EQUAL_F64,
        MMAY_MIR_LESS_F64,
        MMAY_MIR_LESS_EQUAL_F64
    };
    char source[8192];
    int written = snprintf(
        source,
        sizeof(source),
        "const topF64: f64 = -0.0;\n"
        "fn rounded(): f64 { return 9007199254740993.0; }\n"
        "fn minSubnormal(): f64 { return 5e-324; }\n"
        "fn halfTie(): f64 { return %se-1075; }\n"
        "fn beyondHalfTie(): f64 { return %se-1075; }\n"
        "fn maxFinite(): f64 { return 1.7976931348623157e308; }\n"
        "fn infinity(): f64 { return 1.7976931348623159e308; }\n"
        "fn separated(): f64 { return 1_2.5_0; }\n"
        "fn negativeZero(): f64 { return -0.0; }\n"
        "fn add(): f64 { return 1.5 + 2.25; }\n"
        "fn subtract(): f64 { return 5.0 - 2.0; }\n"
        "fn multiply(): f64 { return -2.0 * 3.0; }\n"
        "fn divide(): f64 { return 1.0 / 0.0; }\n"
        "fn equal(): bool { return 0.0 == -0.0; }\n"
        "fn notEqual(): bool { return 1.0 != 2.0; }\n"
        "fn less(): bool { return -1.0 < 0.0; }\n"
        "fn lessEqual(): bool { return 1.0 <= 1.0; }\n"
        "fn greater(): bool { return 2.0 > 1.0; }\n"
        "fn greaterEqual(): bool { return 2.0 >= 2.0; }\n"
        "fn identity(value: f64): f64 { return value; }\n"
        "fn direct(): f64 { return identity(-0.0); }\n"
        "fn negate(value: f64): f64 { return -value; }\n"
        "fn compound(value: f64): f64 {\n"
        "  let result: f64 = value;\n"
        "  result += 1.0; result -= 2.0;\n"
        "  result *= 4.0; result /= 2.0;\n"
        "  return result;\n"
        "}\n"
        "fn fromTop(): f64 { return topF64; }\n",
        half_min_subnormal,
        beyond_half_min_subnormal
    );
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayMirValue argument;
    MarimayMirValue result;
    size_t function_index = 0;

    MMAY_EXPECT(written > 0);
    MMAY_EXPECT(written > 0 && (size_t)written < sizeof(source));
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker-f64",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 23);
        MMAY_EXPECT(checked.constants.length == 1);
        for (function_index = 0; function_index < checked.functions.length;
            function_index += 1) {
            const MarimayCheckedFunction *function = mmay_checked_function(
                &checked,
                function_index
            );
            MMAY_EXPECT(function != NULL);
            if (function != NULL) {
                MMAY_EXPECT(mmay_mir_verify(&function->mir)
                    == MMAY_MIR_VERIFY_OK);
            }
        }
        for (function_index = 0;
            function_index < sizeof(expected_f64) / sizeof(expected_f64[0]);
            function_index += 1) {
            const MarimayCheckedFunction *function = mmay_checked_function(
                &checked,
                function_index
            );
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked,
                function_index,
                NULL,
                0,
                mmay_system_allocator(),
                mmay_check_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_F64);
            MMAY_EXPECT(result.as.f64_bits == expected_f64[function_index]);
            if (function_index < 8 && function != NULL) {
                const MarimayMirInstruction *constant =
                    mmay_test_mir_instruction_kind_at(
                        &function->mir,
                        MMAY_MIR_CONSTANT_F64,
                        0
                    );
                MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                    &function->mir,
                    MMAY_MIR_CONSTANT_F64
                ) == 1);
                MMAY_EXPECT(constant != NULL);
                if (constant != NULL) {
                    MMAY_EXPECT(constant->f64_bits
                        == expected_f64[function_index]);
                }
            }
            if (function_index >= 8 && function_index < 12
                && function != NULL) {
                MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                    &function->mir,
                    arithmetic_kinds[function_index - 8]
                ) == 1);
            }
        }
        for (function_index = 12; function_index < 18;
            function_index += 1) {
            const MarimayCheckedFunction *function = mmay_checked_function(
                &checked,
                function_index
            );
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked,
                function_index,
                NULL,
                0,
                mmay_system_allocator(),
                mmay_check_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_BOOL);
            MMAY_EXPECT(result.as.boolean);
            if (function != NULL) {
                MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                    &function->mir,
                    comparison_kinds[function_index - 12]
                ) == 1);
                if (function_index == 13) {
                    MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                        &function->mir,
                        MMAY_MIR_NOT_BOOL
                    ) == 1);
                }
            }
        }
        argument.type = MMAY_TYPE_F64;
        argument.as.f64_bits = UINT64_C(0x7ff8000000000042);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            18,
            &argument,
            1,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_F64);
        MMAY_EXPECT(result.as.f64_bits == argument.as.f64_bits);

        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            19,
            NULL,
            0,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_F64);
        MMAY_EXPECT(result.as.f64_bits == UINT64_C(0x8000000000000000));
        {
            const MarimayCheckedFunction *direct = mmay_checked_function(
                &checked,
                19
            );
            MMAY_EXPECT(direct != NULL);
            if (direct != NULL) {
                MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                    &direct->mir,
                    MMAY_MIR_CALL
                ) == 1);
            }
        }
        argument.as.f64_bits = UINT64_C(0x4008000000000000);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            21,
            &argument,
            1,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_F64);
        MMAY_EXPECT(result.as.f64_bits == UINT64_C(0x4010000000000000));
        {
            const MarimayCheckedFunction *compound = mmay_checked_function(
                &checked,
                21
            );
            MMAY_EXPECT(compound != NULL);
            if (compound != NULL) {
                MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                    &compound->mir, MMAY_MIR_ADD_F64
                ) == 1);
                MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                    &compound->mir, MMAY_MIR_SUBTRACT_F64
                ) == 1);
                MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                    &compound->mir, MMAY_MIR_MULTIPLY_F64
                ) == 1);
                MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                    &compound->mir, MMAY_MIR_DIVIDE_F64
                ) == 1);
            }
        }
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            22,
            NULL,
            0,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_F64);
        MMAY_EXPECT(result.as.f64_bits == UINT64_C(0x8000000000000000));

        argument.as.f64_bits = UINT64_C(0x3ff8000000000000);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            20,
            &argument,
            1,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_F64);
        MMAY_EXPECT(result.as.f64_bits == UINT64_C(0xbff8000000000000));
        {
            const MarimayCheckedFunction *negate = mmay_checked_function(
                &checked,
                20
            );
            MMAY_EXPECT(negate != NULL);
            if (negate != NULL) {
                MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                    &negate->mir,
                    MMAY_MIR_NEGATE_F64
                ) == 1);
            }
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    mmay_checker_expect_one_diagnostic(
        "fn bad(): int { return 1.0; }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "1.0",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(left: f64, right: int): f64 { return left + right; }\n",
        MMAY_DIAGNOSTIC_INVALID_OPERATOR,
        "+",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(left: f64, right: f64): f64 { return left % right; }\n",
        MMAY_DIAGNOSTIC_INVALID_OPERATOR,
        "%",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(left: f64, right: f64): f64 { return left & right; }\n",
        MMAY_DIAGNOSTIC_INVALID_OPERATOR,
        "&",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(): f64 { let value: f64 = 1.0; value %= 1.0; return value; }\n",
        MMAY_DIAGNOSTIC_INVALID_OPERATOR,
        "%=",
        0
    );
}

void mmay_test_checker_float_literal_digit_limit(void) {
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayCheckOptions options = mmay_check_default_options();
    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "fn limited(): f64 { return 1.234; }\n"
    ));
    options.max_float_literal_digits = 3;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker-f64-limit",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_locals_and_literal_boundary(void) {
    static const char source[] =
        "fn update(): int {\n"
        "  let value: int;\n"
        "  value = 41;\n"
        "  value += 1;\n"
        "  return value;\n"
        "}\n"
        "fn minimum(): int {\n"
        "  return -9223372036854775808;\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *update = NULL;
    const MarimayCheckedFunction *minimum = NULL;
    MarimayMirValue result;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 2);
        update = mmay_checked_function(&checked, 0);
        minimum = mmay_checked_function(&checked, 1);
        MMAY_EXPECT(update != NULL);
        MMAY_EXPECT(minimum != NULL);
        if (update != NULL) {
            MMAY_EXPECT(mmay_mir_interpret(
                &update->mir,
                NULL,
                0,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == 42);
        }
        if (minimum != NULL) {
            MMAY_EXPECT(mmay_mir_interpret(
                &minimum->mir,
                NULL,
                0,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == INT64_MIN);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}
