#include "test_bytecode_support.h"

typedef struct MarimayTestVerifyCase {
    int mutation;
    MarimayBytecodeVerifyStatus expected;
} MarimayTestVerifyCase;

typedef enum MarimayTestRecordMutation {
    MMAY_TEST_RECORD_VALID,
    MMAY_TEST_RECORD_NEW_TARGET_OOB,
    MMAY_TEST_RECORD_NEW_TARGET_TYPE,
    MMAY_TEST_RECORD_NEW_TARGET_LAYOUT,
    MMAY_TEST_RECORD_NEW_CONSTRUCTION_OOB,
    MMAY_TEST_RECORD_NEW_ARGUMENT_UNINITIALIZED,
    MMAY_TEST_RECORD_NEW_ARGUMENT_LAYOUT,
    MMAY_TEST_RECORD_GET_TARGET_OOB,
    MMAY_TEST_RECORD_GET_TARGET_TYPE,
    MMAY_TEST_RECORD_GET_TARGET_LAYOUT,
    MMAY_TEST_RECORD_GET_SOURCE_OOB,
    MMAY_TEST_RECORD_GET_SOURCE_UNINITIALIZED,
    MMAY_TEST_RECORD_GET_SOURCE_TYPE,
    MMAY_TEST_RECORD_GET_FIELD_HIGH,
    MMAY_TEST_RECORD_GET_FIELD_NEGATIVE,
    MMAY_TEST_RECORD_SET_VALUE_OOB,
    MMAY_TEST_RECORD_SET_VALUE_UNINITIALIZED,
    MMAY_TEST_RECORD_SET_VALUE_TYPE,
    MMAY_TEST_RECORD_SET_VALUE_LAYOUT,
    MMAY_TEST_RECORD_SET_SOURCE_OOB,
    MMAY_TEST_RECORD_SET_SOURCE_UNINITIALIZED,
    MMAY_TEST_RECORD_SET_SOURCE_TYPE,
    MMAY_TEST_RECORD_SET_FIELD_HIGH,
    MMAY_TEST_RECORD_SET_FIELD_NEGATIVE,
    MMAY_TEST_RECORD_SET_IMMUTABLE,
    MMAY_TEST_RECORD_ID_TARGET_OOB,
    MMAY_TEST_RECORD_ID_TARGET_TYPE,
    MMAY_TEST_RECORD_ID_LEFT_OOB,
    MMAY_TEST_RECORD_ID_LEFT_UNINITIALIZED,
    MMAY_TEST_RECORD_ID_LEFT_TYPE,
    MMAY_TEST_RECORD_ID_RIGHT_OOB,
    MMAY_TEST_RECORD_ID_RIGHT_UNINITIALIZED,
    MMAY_TEST_RECORD_ID_RIGHT_TYPE,
    MMAY_TEST_RECORD_ID_RIGHT_LAYOUT
} MarimayTestRecordMutation;

static bool mmay_test_build_record_matrix(
    MarimayBytecodeFunction *function,
    MarimayTestRecordMutation mutation
) {
    MarimayBytecodeRecordField holder_field;
    MarimayBytecodeRecordArgument holder_argument;
    MarimayBytecodeRecordId leaf_a = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordId leaf_b = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordId holder = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordConstructionId leaf_a_new =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeRecordConstructionId leaf_b_new =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeRecordConstructionId holder_new =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeInstruction *instruction = NULL;
    MarimayBytecodeRecordArgument *argument = NULL;
    MarimayBytecodeRecordField *field = NULL;
    uint32_t scalar = UINT32_MAX;
    uint32_t leaf_a_value = UINT32_MAX;
    uint32_t leaf_b_value = UINT32_MAX;
    uint32_t holder_value = UINT32_MAX;
    uint32_t projected = UINT32_MAX;
    uint32_t same = UINT32_MAX;
    uint32_t uninitialized_scalar = UINT32_MAX;
    uint32_t uninitialized_leaf = UINT32_MAX;
    uint32_t uninitialized_holder = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    size_t span = 0;
    const MarimaySymbolId symbols[] = {{{0xa1}}, {{0xa2}}, {{0xa3}}};
    memset(function, 0, sizeof(*function));
    memset(&holder_field, 0, sizeof(holder_field));
    memset(&holder_argument, 0, sizeof(holder_argument));
    holder_field.type = MMAY_BYTECODE_TYPE_RECORD;
    holder_field.layout = 0;
    holder_field.is_mutable = true;
    if (!mmay_bytecode_function_init(
            function, mmay_system_allocator(), MMAY_BYTECODE_TYPE_BOOL,
            1, NULL, 0, mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_I64, &scalar
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &leaf_a_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &leaf_b_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &holder_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &projected
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_BOOL, &same
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_I64, &uninitialized_scalar
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &uninitialized_leaf
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &uninitialized_holder
        ) || !mmay_bytecode_add_record_layout(
            function, symbols[0], NULL, 0, &leaf_a
        ) || !mmay_bytecode_add_record_layout(
            function, symbols[1], NULL, 0, &leaf_b
        ) || !mmay_bytecode_add_record_layout(
            function, symbols[2], &holder_field, 1, &holder
        ) || !mmay_bytecode_set_register_layout(
            function, leaf_a_value, leaf_a
        ) || !mmay_bytecode_set_register_layout(
            function, leaf_b_value, leaf_b
        ) || !mmay_bytecode_set_register_layout(
            function, holder_value, holder
        ) || !mmay_bytecode_set_register_layout(
            function, projected, leaf_a
        ) || !mmay_bytecode_set_register_layout(
            function, uninitialized_leaf, leaf_a
        ) || !mmay_bytecode_set_register_layout(
            function, uninitialized_holder, holder
        ) || !mmay_bytecode_add_record_construction(
            function, leaf_a, NULL, 0, &leaf_a_new
        ) || !mmay_bytecode_add_record_construction(
            function, leaf_b, NULL, 0, &leaf_b_new
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    holder_argument.register_id = leaf_a_value;
    holder_argument.type = MMAY_BYTECODE_TYPE_RECORD;
    holder_argument.layout = leaf_a;
    if (!mmay_bytecode_add_record_construction(
            function, holder, &holder_argument, 1, &holder_new
        ) || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_CONST_I64,
            scalar, 7, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_RECORD_NEW,
            leaf_a_value, leaf_a_new, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_RECORD_NEW,
            leaf_b_value, leaf_b_new, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_RECORD_NEW,
            holder_value, holder_new, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_RECORD_SET,
            leaf_a_value, holder_value, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_RECORD_GET,
            projected, holder_value, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_RECORD_IDENTITY_SAME,
            same, leaf_a_value, projected, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_RETURN,
            same, 0, 0, &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    argument = mmay_vector_at(&function->record_arguments, 0);
    field = mmay_vector_at(&function->record_fields, 0);
    switch (mutation) {
        case MMAY_TEST_RECORD_VALID:
            return true;
        case MMAY_TEST_RECORD_NEW_TARGET_OOB:
        case MMAY_TEST_RECORD_NEW_TARGET_TYPE:
        case MMAY_TEST_RECORD_NEW_TARGET_LAYOUT:
        case MMAY_TEST_RECORD_NEW_CONSTRUCTION_OOB:
            instruction = mmay_test_bytecode_at(function, 0, 1);
            break;
        case MMAY_TEST_RECORD_NEW_ARGUMENT_UNINITIALIZED:
        case MMAY_TEST_RECORD_NEW_ARGUMENT_LAYOUT:
            if (argument == NULL) {
                return false;
            }
            if (mutation == MMAY_TEST_RECORD_NEW_ARGUMENT_UNINITIALIZED) {
                argument->register_id = uninitialized_leaf;
            } else {
                argument->layout = leaf_b;
            }
            return true;
        case MMAY_TEST_RECORD_GET_TARGET_OOB:
        case MMAY_TEST_RECORD_GET_TARGET_TYPE:
        case MMAY_TEST_RECORD_GET_TARGET_LAYOUT:
        case MMAY_TEST_RECORD_GET_SOURCE_OOB:
        case MMAY_TEST_RECORD_GET_SOURCE_UNINITIALIZED:
        case MMAY_TEST_RECORD_GET_SOURCE_TYPE:
        case MMAY_TEST_RECORD_GET_FIELD_HIGH:
        case MMAY_TEST_RECORD_GET_FIELD_NEGATIVE:
            instruction = mmay_test_bytecode_at(function, 0, 5);
            break;
        case MMAY_TEST_RECORD_SET_VALUE_OOB:
        case MMAY_TEST_RECORD_SET_VALUE_UNINITIALIZED:
        case MMAY_TEST_RECORD_SET_VALUE_TYPE:
        case MMAY_TEST_RECORD_SET_VALUE_LAYOUT:
        case MMAY_TEST_RECORD_SET_SOURCE_OOB:
        case MMAY_TEST_RECORD_SET_SOURCE_UNINITIALIZED:
        case MMAY_TEST_RECORD_SET_SOURCE_TYPE:
        case MMAY_TEST_RECORD_SET_FIELD_HIGH:
        case MMAY_TEST_RECORD_SET_FIELD_NEGATIVE:
            instruction = mmay_test_bytecode_at(function, 0, 4);
            break;
        case MMAY_TEST_RECORD_SET_IMMUTABLE:
            if (field == NULL) {
                return false;
            }
            field->is_mutable = false;
            return true;
        default:
            instruction = mmay_test_bytecode_at(function, 0, 6);
            break;
    }
    if (instruction == NULL) {
        return false;
    }
    switch (mutation) {
        case MMAY_TEST_RECORD_NEW_TARGET_OOB:
        case MMAY_TEST_RECORD_GET_TARGET_OOB:
        case MMAY_TEST_RECORD_SET_VALUE_OOB:
        case MMAY_TEST_RECORD_ID_TARGET_OOB:
            instruction->operands[0].unsigned_value =
                function->register_types.length;
            break;
        case MMAY_TEST_RECORD_NEW_TARGET_TYPE:
        case MMAY_TEST_RECORD_GET_TARGET_TYPE:
        case MMAY_TEST_RECORD_ID_TARGET_TYPE:
            instruction->operands[0].unsigned_value = scalar;
            break;
        case MMAY_TEST_RECORD_NEW_TARGET_LAYOUT:
        case MMAY_TEST_RECORD_GET_TARGET_LAYOUT:
            instruction->operands[0].unsigned_value = leaf_b_value;
            break;
        case MMAY_TEST_RECORD_NEW_CONSTRUCTION_OOB:
            instruction->operands[1].unsigned_value =
                function->record_constructions.length;
            break;
        case MMAY_TEST_RECORD_GET_SOURCE_OOB:
        case MMAY_TEST_RECORD_SET_SOURCE_OOB:
        case MMAY_TEST_RECORD_ID_LEFT_OOB:
            instruction->operands[1].unsigned_value =
                function->register_types.length;
            break;
        case MMAY_TEST_RECORD_GET_SOURCE_UNINITIALIZED:
        case MMAY_TEST_RECORD_SET_SOURCE_UNINITIALIZED:
            instruction->operands[1].unsigned_value = uninitialized_holder;
            break;
        case MMAY_TEST_RECORD_ID_LEFT_UNINITIALIZED:
            instruction->operands[1].unsigned_value = uninitialized_leaf;
            break;
        case MMAY_TEST_RECORD_GET_SOURCE_TYPE:
        case MMAY_TEST_RECORD_SET_SOURCE_TYPE:
        case MMAY_TEST_RECORD_ID_LEFT_TYPE:
            instruction->operands[1].unsigned_value = scalar;
            break;
        case MMAY_TEST_RECORD_GET_FIELD_HIGH:
        case MMAY_TEST_RECORD_SET_FIELD_HIGH:
            instruction->operands[2].signed_value = 1;
            break;
        case MMAY_TEST_RECORD_GET_FIELD_NEGATIVE:
        case MMAY_TEST_RECORD_SET_FIELD_NEGATIVE:
            instruction->operands[2].signed_value = -1;
            break;
        case MMAY_TEST_RECORD_SET_VALUE_UNINITIALIZED:
            instruction->operands[0].unsigned_value = uninitialized_leaf;
            break;
        case MMAY_TEST_RECORD_SET_VALUE_TYPE:
            instruction->operands[0].unsigned_value = scalar;
            break;
        case MMAY_TEST_RECORD_SET_VALUE_LAYOUT:
            instruction->operands[0].unsigned_value = leaf_b_value;
            break;
        case MMAY_TEST_RECORD_ID_RIGHT_OOB:
            instruction->operands[2].unsigned_value =
                function->register_types.length;
            break;
        case MMAY_TEST_RECORD_ID_RIGHT_UNINITIALIZED:
            instruction->operands[2].unsigned_value = uninitialized_leaf;
            break;
        case MMAY_TEST_RECORD_ID_RIGHT_TYPE:
            instruction->operands[2].unsigned_value = scalar;
            break;
        case MMAY_TEST_RECORD_ID_RIGHT_LAYOUT:
            instruction->operands[2].unsigned_value = leaf_b_value;
            break;
        default:
            return false;
    }
    (void)uninitialized_scalar;
    return true;
}

static void mmay_test_record_verifier_matrix(void) {
    static const MarimayTestVerifyCase cases[] = {
        {MMAY_TEST_RECORD_VALID, MMAY_BYTECODE_VERIFY_OK},
        {MMAY_TEST_RECORD_NEW_TARGET_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_RECORD_NEW_TARGET_TYPE, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_RECORD_NEW_TARGET_LAYOUT, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_RECORD_NEW_CONSTRUCTION_OOB, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_RECORD_NEW_ARGUMENT_UNINITIALIZED, MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {MMAY_TEST_RECORD_NEW_ARGUMENT_LAYOUT, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_RECORD_GET_TARGET_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_RECORD_GET_TARGET_TYPE, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_RECORD_GET_TARGET_LAYOUT, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_RECORD_GET_SOURCE_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_RECORD_GET_SOURCE_UNINITIALIZED, MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {MMAY_TEST_RECORD_GET_SOURCE_TYPE, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_RECORD_GET_FIELD_HIGH, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_RECORD_GET_FIELD_NEGATIVE, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_RECORD_SET_VALUE_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_RECORD_SET_VALUE_UNINITIALIZED, MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {MMAY_TEST_RECORD_SET_VALUE_TYPE, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_RECORD_SET_VALUE_LAYOUT, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_RECORD_SET_SOURCE_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_RECORD_SET_SOURCE_UNINITIALIZED, MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {MMAY_TEST_RECORD_SET_SOURCE_TYPE, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_RECORD_SET_FIELD_HIGH, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_RECORD_SET_FIELD_NEGATIVE, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_RECORD_SET_IMMUTABLE, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_RECORD_ID_TARGET_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_RECORD_ID_TARGET_TYPE, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_RECORD_ID_LEFT_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_RECORD_ID_LEFT_UNINITIALIZED, MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {MMAY_TEST_RECORD_ID_LEFT_TYPE, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_RECORD_ID_RIGHT_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_RECORD_ID_RIGHT_UNINITIALIZED, MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {MMAY_TEST_RECORD_ID_RIGHT_TYPE, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_RECORD_ID_RIGHT_LAYOUT, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        MarimayBytecodeFunction function;
        MMAY_EXPECT(mmay_test_build_record_matrix(
            &function, (MarimayTestRecordMutation)cases[index].mutation
        ));
        MMAY_EXPECT(mmay_bytecode_verify(&function) == cases[index].expected);
        mmay_bytecode_function_dispose(&function);
    }
}

typedef enum MarimayTestEnumMutation {
    MMAY_TEST_ENUM_VALID,
    MMAY_TEST_ENUM_NEW_TARGET_OOB,
    MMAY_TEST_ENUM_NEW_TARGET_TYPE,
    MMAY_TEST_ENUM_NEW_TARGET_LAYOUT,
    MMAY_TEST_ENUM_NEW_CONSTRUCTION_OOB,
    MMAY_TEST_ENUM_NEW_ARGUMENT_UNINITIALIZED,
    MMAY_TEST_ENUM_NEW_ARGUMENT_LAYOUT,
    MMAY_TEST_ENUM_IS_TARGET_OOB,
    MMAY_TEST_ENUM_IS_TARGET_TYPE,
    MMAY_TEST_ENUM_IS_SOURCE_OOB,
    MMAY_TEST_ENUM_IS_SOURCE_UNINITIALIZED,
    MMAY_TEST_ENUM_IS_SOURCE_TYPE,
    MMAY_TEST_ENUM_IS_VARIANT_HIGH,
    MMAY_TEST_ENUM_IS_VARIANT_NEGATIVE,
    MMAY_TEST_ENUM_REQUIRE_TARGET_OOB,
    MMAY_TEST_ENUM_REQUIRE_TARGET_TYPE,
    MMAY_TEST_ENUM_REQUIRE_TARGET_LAYOUT,
    MMAY_TEST_ENUM_REQUIRE_SOURCE_OOB,
    MMAY_TEST_ENUM_REQUIRE_SOURCE_UNINITIALIZED,
    MMAY_TEST_ENUM_REQUIRE_SOURCE_TYPE,
    MMAY_TEST_ENUM_REQUIRE_VARIANT_HIGH,
    MMAY_TEST_ENUM_REQUIRE_VARIANT_NEGATIVE,
    MMAY_TEST_ENUM_GET_TARGET_OOB,
    MMAY_TEST_ENUM_GET_TARGET_TYPE,
    MMAY_TEST_ENUM_GET_TARGET_LAYOUT,
    MMAY_TEST_ENUM_GET_SOURCE_OOB,
    MMAY_TEST_ENUM_GET_SOURCE_UNINITIALIZED,
    MMAY_TEST_ENUM_GET_SOURCE_TYPE,
    MMAY_TEST_ENUM_GET_VARIANT_HIGH,
    MMAY_TEST_ENUM_GET_PAYLOAD_HIGH
} MarimayTestEnumMutation;

static bool mmay_test_build_enum_matrix(
    MarimayBytecodeFunction *function,
    MarimayTestEnumMutation mutation
) {
    MarimayBytecodeEnumVariant variant;
    MarimayBytecodeEnumPayload payloads[2];
    MarimayBytecodeEnumArgument arguments[2];
    MarimayBytecodeRecordId leaf_a = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordId leaf_b = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordConstructionId leaf_a_new =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeRecordConstructionId leaf_b_new =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeEnumId enum_a = MMAY_BYTECODE_NO_ENUM;
    MarimayBytecodeEnumId enum_b = MMAY_BYTECODE_NO_ENUM;
    MarimayBytecodeEnumConstructionId enum_a_new =
        MMAY_BYTECODE_NO_ENUM_CONSTRUCTION;
    MarimayBytecodeEnumConstructionId enum_b_new =
        MMAY_BYTECODE_NO_ENUM_CONSTRUCTION;
    MarimayBytecodeInstruction *instruction = NULL;
    MarimayBytecodeEnumArgument *argument = NULL;
    uint32_t scalar = UINT32_MAX;
    uint32_t leaf_a_value = UINT32_MAX;
    uint32_t leaf_b_value = UINT32_MAX;
    uint32_t enum_a_value = UINT32_MAX;
    uint32_t enum_b_value = UINT32_MAX;
    uint32_t checked = UINT32_MAX;
    uint32_t projected = UINT32_MAX;
    uint32_t is_variant = UINT32_MAX;
    uint32_t uninitialized_leaf = UINT32_MAX;
    uint32_t uninitialized_enum = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    size_t span = 100;
    const MarimaySymbolId record_symbols[] = {{{0xb1}}, {{0xb2}}};
    const MarimaySymbolId enum_symbols[] = {{{0xb3}}, {{0xb4}}};
    memset(function, 0, sizeof(*function));
    memset(&variant, 0, sizeof(variant));
    memset(payloads, 0, sizeof(payloads));
    memset(arguments, 0, sizeof(arguments));
    variant.payload_count = 1;
    payloads[0].type = MMAY_BYTECODE_TYPE_RECORD;
    payloads[0].layout = 0;
    payloads[1].type = MMAY_BYTECODE_TYPE_RECORD;
    payloads[1].layout = 1;
    if (!mmay_bytecode_function_init(
            function, mmay_system_allocator(), MMAY_BYTECODE_TYPE_BOOL,
            1, NULL, 0, mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_I64, &scalar
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &leaf_a_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &leaf_b_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_ENUM, &enum_a_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_ENUM, &enum_b_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_ENUM, &checked
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &projected
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_BOOL, &is_variant
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &uninitialized_leaf
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_ENUM, &uninitialized_enum
        ) || !mmay_bytecode_add_record_layout(
            function, record_symbols[0], NULL, 0, &leaf_a
        ) || !mmay_bytecode_add_record_layout(
            function, record_symbols[1], NULL, 0, &leaf_b
        ) || !mmay_bytecode_add_record_construction(
            function, leaf_a, NULL, 0, &leaf_a_new
        ) || !mmay_bytecode_add_record_construction(
            function, leaf_b, NULL, 0, &leaf_b_new
        ) || !mmay_bytecode_add_enum_layout(
            function, enum_symbols[0], &variant, 1,
            &payloads[0], 1, &enum_a
        ) || !mmay_bytecode_add_enum_layout(
            function, enum_symbols[1], &variant, 1,
            &payloads[1], 1, &enum_b
        ) || !mmay_bytecode_set_register_layout(
            function, leaf_a_value, leaf_a
        ) || !mmay_bytecode_set_register_layout(
            function, leaf_b_value, leaf_b
        ) || !mmay_bytecode_set_register_layout(
            function, enum_a_value, enum_a
        ) || !mmay_bytecode_set_register_layout(
            function, enum_b_value, enum_b
        ) || !mmay_bytecode_set_register_layout(
            function, checked, enum_a
        ) || !mmay_bytecode_set_register_layout(
            function, projected, leaf_a
        ) || !mmay_bytecode_set_register_layout(
            function, uninitialized_leaf, leaf_a
        ) || !mmay_bytecode_set_register_layout(
            function, uninitialized_enum, enum_a
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    arguments[0].register_id = leaf_a_value;
    arguments[0].type = MMAY_BYTECODE_TYPE_RECORD;
    arguments[0].layout = leaf_a;
    arguments[1].register_id = leaf_b_value;
    arguments[1].type = MMAY_BYTECODE_TYPE_RECORD;
    arguments[1].layout = leaf_b;
    if (!mmay_bytecode_add_enum_construction(
            function, enum_a, 0, &arguments[0], 1, &enum_a_new
        ) || !mmay_bytecode_add_enum_construction(
            function, enum_b, 0, &arguments[1], 1, &enum_b_new
        ) || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_CONST_I64,
            scalar, 9, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_RECORD_NEW,
            leaf_a_value, leaf_a_new, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_RECORD_NEW,
            leaf_b_value, leaf_b_new, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_ENUM_NEW,
            enum_a_value, enum_a_new, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_ENUM_NEW,
            enum_b_value, enum_b_new, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_ENUM_IS_VARIANT,
            is_variant, enum_a_value, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_ENUM_REQUIRE_VARIANT,
            checked, enum_a_value, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_ENUM_GET_PAYLOAD,
            projected, checked, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_RETURN,
            is_variant, 0, 0, &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    argument = mmay_vector_at(&function->enum_arguments, 0);
    switch (mutation) {
        case MMAY_TEST_ENUM_VALID:
            return true;
        case MMAY_TEST_ENUM_NEW_TARGET_OOB:
        case MMAY_TEST_ENUM_NEW_TARGET_TYPE:
        case MMAY_TEST_ENUM_NEW_TARGET_LAYOUT:
        case MMAY_TEST_ENUM_NEW_CONSTRUCTION_OOB:
            instruction = mmay_test_bytecode_at(function, 0, 3);
            break;
        case MMAY_TEST_ENUM_NEW_ARGUMENT_UNINITIALIZED:
        case MMAY_TEST_ENUM_NEW_ARGUMENT_LAYOUT:
            if (argument == NULL) {
                return false;
            }
            if (mutation == MMAY_TEST_ENUM_NEW_ARGUMENT_UNINITIALIZED) {
                argument->register_id = uninitialized_leaf;
            } else {
                argument->layout = leaf_b;
            }
            return true;
        case MMAY_TEST_ENUM_IS_TARGET_OOB:
        case MMAY_TEST_ENUM_IS_TARGET_TYPE:
        case MMAY_TEST_ENUM_IS_SOURCE_OOB:
        case MMAY_TEST_ENUM_IS_SOURCE_UNINITIALIZED:
        case MMAY_TEST_ENUM_IS_SOURCE_TYPE:
        case MMAY_TEST_ENUM_IS_VARIANT_HIGH:
        case MMAY_TEST_ENUM_IS_VARIANT_NEGATIVE:
            instruction = mmay_test_bytecode_at(function, 0, 5);
            break;
        case MMAY_TEST_ENUM_REQUIRE_TARGET_OOB:
        case MMAY_TEST_ENUM_REQUIRE_TARGET_TYPE:
        case MMAY_TEST_ENUM_REQUIRE_TARGET_LAYOUT:
        case MMAY_TEST_ENUM_REQUIRE_SOURCE_OOB:
        case MMAY_TEST_ENUM_REQUIRE_SOURCE_UNINITIALIZED:
        case MMAY_TEST_ENUM_REQUIRE_SOURCE_TYPE:
        case MMAY_TEST_ENUM_REQUIRE_VARIANT_HIGH:
        case MMAY_TEST_ENUM_REQUIRE_VARIANT_NEGATIVE:
            instruction = mmay_test_bytecode_at(function, 0, 6);
            break;
        default:
            instruction = mmay_test_bytecode_at(function, 0, 7);
            break;
    }
    if (instruction == NULL) {
        return false;
    }
    switch (mutation) {
        case MMAY_TEST_ENUM_NEW_TARGET_OOB:
        case MMAY_TEST_ENUM_IS_TARGET_OOB:
        case MMAY_TEST_ENUM_REQUIRE_TARGET_OOB:
        case MMAY_TEST_ENUM_GET_TARGET_OOB:
            instruction->operands[0].unsigned_value =
                function->register_types.length;
            break;
        case MMAY_TEST_ENUM_NEW_TARGET_TYPE:
        case MMAY_TEST_ENUM_IS_TARGET_TYPE:
        case MMAY_TEST_ENUM_REQUIRE_TARGET_TYPE:
        case MMAY_TEST_ENUM_GET_TARGET_TYPE:
            instruction->operands[0].unsigned_value = scalar;
            break;
        case MMAY_TEST_ENUM_NEW_TARGET_LAYOUT:
        case MMAY_TEST_ENUM_REQUIRE_TARGET_LAYOUT:
            instruction->operands[0].unsigned_value = enum_b_value;
            break;
        case MMAY_TEST_ENUM_GET_TARGET_LAYOUT:
            instruction->operands[0].unsigned_value = leaf_b_value;
            break;
        case MMAY_TEST_ENUM_NEW_CONSTRUCTION_OOB:
            instruction->operands[1].unsigned_value =
                function->enum_constructions.length;
            break;
        case MMAY_TEST_ENUM_IS_SOURCE_OOB:
        case MMAY_TEST_ENUM_REQUIRE_SOURCE_OOB:
        case MMAY_TEST_ENUM_GET_SOURCE_OOB:
            instruction->operands[1].unsigned_value =
                function->register_types.length;
            break;
        case MMAY_TEST_ENUM_IS_SOURCE_UNINITIALIZED:
        case MMAY_TEST_ENUM_REQUIRE_SOURCE_UNINITIALIZED:
        case MMAY_TEST_ENUM_GET_SOURCE_UNINITIALIZED:
            instruction->operands[1].unsigned_value = uninitialized_enum;
            break;
        case MMAY_TEST_ENUM_IS_SOURCE_TYPE:
        case MMAY_TEST_ENUM_REQUIRE_SOURCE_TYPE:
        case MMAY_TEST_ENUM_GET_SOURCE_TYPE:
            instruction->operands[1].unsigned_value = leaf_a_value;
            break;
        case MMAY_TEST_ENUM_IS_VARIANT_HIGH:
        case MMAY_TEST_ENUM_REQUIRE_VARIANT_HIGH:
            instruction->operands[2].signed_value = 1;
            break;
        case MMAY_TEST_ENUM_IS_VARIANT_NEGATIVE:
        case MMAY_TEST_ENUM_REQUIRE_VARIANT_NEGATIVE:
            instruction->operands[2].signed_value = -1;
            break;
        case MMAY_TEST_ENUM_GET_VARIANT_HIGH:
            instruction->operands[2].signed_value = INT64_C(1) << 32;
            break;
        case MMAY_TEST_ENUM_GET_PAYLOAD_HIGH:
            instruction->operands[2].signed_value = 1;
            break;
        default:
            return false;
    }
    return true;
}

static void mmay_test_enum_verifier_matrix(void) {
    static const MarimayTestVerifyCase cases[] = {
        {MMAY_TEST_ENUM_VALID, MMAY_BYTECODE_VERIFY_OK},
        {MMAY_TEST_ENUM_NEW_TARGET_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_ENUM_NEW_TARGET_TYPE, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_ENUM_NEW_TARGET_LAYOUT, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_ENUM_NEW_CONSTRUCTION_OOB, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_ENUM_NEW_ARGUMENT_UNINITIALIZED, MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {MMAY_TEST_ENUM_NEW_ARGUMENT_LAYOUT, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_ENUM_IS_TARGET_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_ENUM_IS_TARGET_TYPE, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_ENUM_IS_SOURCE_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_ENUM_IS_SOURCE_UNINITIALIZED, MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {MMAY_TEST_ENUM_IS_SOURCE_TYPE, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_ENUM_IS_VARIANT_HIGH, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_ENUM_IS_VARIANT_NEGATIVE, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_ENUM_REQUIRE_TARGET_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_ENUM_REQUIRE_TARGET_TYPE, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_ENUM_REQUIRE_TARGET_LAYOUT, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_ENUM_REQUIRE_SOURCE_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_ENUM_REQUIRE_SOURCE_UNINITIALIZED, MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {MMAY_TEST_ENUM_REQUIRE_SOURCE_TYPE, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_ENUM_REQUIRE_VARIANT_HIGH, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_ENUM_REQUIRE_VARIANT_NEGATIVE, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_ENUM_GET_TARGET_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_ENUM_GET_TARGET_TYPE, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_ENUM_GET_TARGET_LAYOUT, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_ENUM_GET_SOURCE_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_ENUM_GET_SOURCE_UNINITIALIZED, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_ENUM_GET_SOURCE_TYPE, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_ENUM_GET_VARIANT_HIGH, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_ENUM_GET_PAYLOAD_HIGH, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        MarimayBytecodeFunction function;
        MMAY_EXPECT(mmay_test_build_enum_matrix(
            &function, (MarimayTestEnumMutation)cases[index].mutation
        ));
        MMAY_EXPECT(mmay_bytecode_verify(&function) == cases[index].expected);
        mmay_bytecode_function_dispose(&function);
    }
}

typedef enum MarimayTestInterfaceMutation {
    MMAY_TEST_INTERFACE_VALID,
    MMAY_TEST_INTERFACE_NEW_TARGET_OOB,
    MMAY_TEST_INTERFACE_NEW_TARGET_TYPE,
    MMAY_TEST_INTERFACE_NEW_TARGET_LAYOUT,
    MMAY_TEST_INTERFACE_NEW_CONSTRUCTION_OOB,
    MMAY_TEST_INTERFACE_NEW_PAYLOAD_OOB,
    MMAY_TEST_INTERFACE_NEW_PAYLOAD_UNINITIALIZED,
    MMAY_TEST_INTERFACE_NEW_PAYLOAD_TYPE,
    MMAY_TEST_INTERFACE_NEW_PAYLOAD_LAYOUT,
    MMAY_TEST_INTERFACE_NEW_METHOD_RANGE,
    MMAY_TEST_INTERFACE_CALL_TARGET_OOB,
    MMAY_TEST_INTERFACE_CALL_TARGET_TYPE,
    MMAY_TEST_INTERFACE_CALL_TARGET_LAYOUT,
    MMAY_TEST_INTERFACE_CALL_OOB,
    MMAY_TEST_INTERFACE_CALL_SLOT_HIGH,
    MMAY_TEST_INTERFACE_CALL_SLOT_MISSING,
    MMAY_TEST_INTERFACE_CALL_RECEIVER_MISSING,
    MMAY_TEST_INTERFACE_CALL_RECEIVER_OOB,
    MMAY_TEST_INTERFACE_CALL_RECEIVER_UNINITIALIZED,
    MMAY_TEST_INTERFACE_CALL_RECEIVER_TYPE,
    MMAY_TEST_INTERFACE_CALL_RECEIVER_LAYOUT,
    MMAY_TEST_INTERFACE_CALL_ARGUMENT_OOB,
    MMAY_TEST_INTERFACE_CALL_ARGUMENT_UNINITIALIZED,
    MMAY_TEST_INTERFACE_CALL_ARGUMENT_TYPE,
    MMAY_TEST_INTERFACE_CALL_ARGUMENT_LAYOUT,
    MMAY_TEST_INTERFACE_CALL_RESULT_LAYOUT
} MarimayTestInterfaceMutation;

static bool mmay_test_build_interface_matrix(
    MarimayBytecodeFunction *function,
    MarimayTestInterfaceMutation mutation
) {
    MarimayBytecodeRecordId leaf_a = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordId leaf_b = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordConstructionId leaf_a_new =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeRecordConstructionId leaf_b_new =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeInterfaceId interface_a = MMAY_BYTECODE_NO_INTERFACE;
    MarimayBytecodeInterfaceId interface_b = MMAY_BYTECODE_NO_INTERFACE;
    MarimayBytecodeInterfaceConstructionId interface_a_new =
        MMAY_BYTECODE_NO_INTERFACE_CONSTRUCTION;
    MarimayBytecodeInterfaceConstructionId interface_b_new =
        MMAY_BYTECODE_NO_INTERFACE_CONSTRUCTION;
    MarimayBytecodeCallArgument call_arguments[2];
    MarimayBytecodeCallId call_id = MMAY_BYTECODE_NO_CALL;
    MarimayBytecodeCall *call = NULL;
    MarimayBytecodeCallArgument *receiver = NULL;
    MarimayBytecodeCallArgument *argument = NULL;
    MarimayBytecodeInterfaceConstruction *construction = NULL;
    MarimayBytecodeInstruction *instruction = NULL;
    uint32_t scalar = UINT32_MAX;
    uint32_t leaf_a_value = UINT32_MAX;
    uint32_t leaf_b_value = UINT32_MAX;
    uint32_t interface_a_value = UINT32_MAX;
    uint32_t interface_b_value = UINT32_MAX;
    uint32_t result = UINT32_MAX;
    uint32_t same = UINT32_MAX;
    uint32_t uninitialized_leaf = UINT32_MAX;
    uint32_t uninitialized_interface = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    size_t span = 200;
    const MarimaySymbolId record_symbols[] = {{{0xc1}}, {{0xc2}}};
    const MarimaySymbolId interface_symbols[] = {{{0xc3}}, {{0xc4}}};
    const MarimaySymbolId method_symbols[] = {{{0xc5}}, {{0xc6}}};
    const MarimaySymbolId placeholder = {{0xcf}};
    memset(function, 0, sizeof(*function));
    memset(call_arguments, 0, sizeof(call_arguments));
    if (!mmay_bytecode_function_init(
            function, mmay_system_allocator(), MMAY_BYTECODE_TYPE_BOOL,
            1, NULL, 0, mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_I64, &scalar
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &leaf_a_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &leaf_b_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_INTERFACE, &interface_a_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_INTERFACE, &interface_b_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &result
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_BOOL, &same
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &uninitialized_leaf
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_INTERFACE, &uninitialized_interface
        ) || !mmay_bytecode_add_record_layout(
            function, record_symbols[0], NULL, 0, &leaf_a
        ) || !mmay_bytecode_add_record_layout(
            function, record_symbols[1], NULL, 0, &leaf_b
        ) || !mmay_bytecode_add_record_construction(
            function, leaf_a, NULL, 0, &leaf_a_new
        ) || !mmay_bytecode_add_record_construction(
            function, leaf_b, NULL, 0, &leaf_b_new
        ) || !mmay_bytecode_add_interface_layout(
            function, interface_symbols[0], 1, &interface_a
        ) || !mmay_bytecode_add_interface_layout(
            function, interface_symbols[1], 1, &interface_b
        ) || !mmay_bytecode_set_register_layout(
            function, leaf_a_value, leaf_a
        ) || !mmay_bytecode_set_register_layout(
            function, leaf_b_value, leaf_b
        ) || !mmay_bytecode_set_register_layout(
            function, interface_a_value, interface_a
        ) || !mmay_bytecode_set_register_layout(
            function, interface_b_value, interface_b
        ) || !mmay_bytecode_set_register_layout(
            function, result, leaf_a
        ) || !mmay_bytecode_set_register_layout(
            function, uninitialized_leaf, leaf_a
        ) || !mmay_bytecode_set_register_layout(
            function, uninitialized_interface, interface_a
        ) || !mmay_bytecode_add_interface_construction(
            function, interface_a, leaf_a_value,
            MMAY_BYTECODE_TYPE_RECORD, leaf_a,
            &method_symbols[0], 1, &interface_a_new
        ) || !mmay_bytecode_add_interface_construction(
            function, interface_b, leaf_b_value,
            MMAY_BYTECODE_TYPE_RECORD, leaf_b,
            &method_symbols[1], 1, &interface_b_new
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    call_arguments[0].register_id = interface_a_value;
    call_arguments[0].type = MMAY_BYTECODE_TYPE_INTERFACE;
    call_arguments[0].layout = interface_a;
    call_arguments[1].register_id = leaf_a_value;
    call_arguments[1].type = MMAY_BYTECODE_TYPE_RECORD;
    call_arguments[1].layout = leaf_a;
    if (!mmay_bytecode_add_call(
            function, placeholder, MMAY_BYTECODE_TYPE_RECORD,
            call_arguments, 2, &call_id
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    call = mmay_vector_at(&function->calls, call_id);
    if (call == NULL) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    call->result_layout = leaf_a;
    call->interface_slot = 0;
    if (!mmay_bytecode_add_block(function, &block)
        || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_CONST_I64,
            scalar, 11, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_RECORD_NEW,
            leaf_a_value, leaf_a_new, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_RECORD_NEW,
            leaf_b_value, leaf_b_new, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_INTERFACE_NEW,
            interface_a_value, interface_a_new, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_INTERFACE_NEW,
            interface_b_value, interface_b_new, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_CALL_INTERFACE,
            result, call_id, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_RECORD_IDENTITY_SAME,
            same, leaf_a_value, result, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_RETURN,
            same, 0, 0, &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    construction = mmay_vector_at(&function->interface_constructions, 0);
    receiver = mmay_vector_at(&function->call_arguments, 0);
    argument = mmay_vector_at(&function->call_arguments, 1);
    if (construction == NULL || receiver == NULL || argument == NULL) {
        return false;
    }
    switch (mutation) {
        case MMAY_TEST_INTERFACE_VALID:
            return true;
        case MMAY_TEST_INTERFACE_NEW_TARGET_OOB:
        case MMAY_TEST_INTERFACE_NEW_TARGET_TYPE:
        case MMAY_TEST_INTERFACE_NEW_TARGET_LAYOUT:
        case MMAY_TEST_INTERFACE_NEW_CONSTRUCTION_OOB:
            instruction = mmay_test_bytecode_at(function, 0, 3);
            break;
        case MMAY_TEST_INTERFACE_NEW_PAYLOAD_OOB:
            construction->payload_register =
                (uint32_t)function->register_types.length;
            return true;
        case MMAY_TEST_INTERFACE_NEW_PAYLOAD_UNINITIALIZED:
            construction->payload_register = uninitialized_leaf;
            return true;
        case MMAY_TEST_INTERFACE_NEW_PAYLOAD_TYPE:
            construction->payload_register = interface_a_value;
            construction->payload_type = MMAY_BYTECODE_TYPE_INTERFACE;
            construction->payload_layout = interface_a;
            return true;
        case MMAY_TEST_INTERFACE_NEW_PAYLOAD_LAYOUT:
            construction->payload_register = leaf_b_value;
            return true;
        case MMAY_TEST_INTERFACE_NEW_METHOD_RANGE:
            construction->method_count = 2;
            return true;
        case MMAY_TEST_INTERFACE_CALL_RECEIVER_MISSING:
            call->argument_count = 0;
            return true;
        case MMAY_TEST_INTERFACE_CALL_RECEIVER_OOB:
            receiver->register_id = (uint32_t)function->register_types.length;
            return true;
        case MMAY_TEST_INTERFACE_CALL_RECEIVER_UNINITIALIZED:
            receiver->register_id = uninitialized_interface;
            return true;
        case MMAY_TEST_INTERFACE_CALL_RECEIVER_TYPE:
            receiver->register_id = leaf_a_value;
            receiver->type = MMAY_BYTECODE_TYPE_RECORD;
            receiver->layout = leaf_a;
            return true;
        case MMAY_TEST_INTERFACE_CALL_RECEIVER_LAYOUT:
            receiver->register_id = interface_b_value;
            return true;
        case MMAY_TEST_INTERFACE_CALL_ARGUMENT_OOB:
            argument->register_id =
                (uint32_t)function->register_types.length;
            return true;
        case MMAY_TEST_INTERFACE_CALL_ARGUMENT_UNINITIALIZED:
            argument->register_id = uninitialized_leaf;
            return true;
        case MMAY_TEST_INTERFACE_CALL_ARGUMENT_TYPE:
            argument->register_id = scalar;
            return true;
        case MMAY_TEST_INTERFACE_CALL_ARGUMENT_LAYOUT:
            argument->register_id = leaf_b_value;
            return true;
        case MMAY_TEST_INTERFACE_CALL_RESULT_LAYOUT:
            call->result_layout = leaf_b;
            return true;
        default:
            instruction = mmay_test_bytecode_at(function, 0, 5);
            break;
    }
    if (instruction == NULL) {
        return false;
    }
    switch (mutation) {
        case MMAY_TEST_INTERFACE_NEW_TARGET_OOB:
        case MMAY_TEST_INTERFACE_CALL_TARGET_OOB:
            instruction->operands[0].unsigned_value =
                function->register_types.length;
            break;
        case MMAY_TEST_INTERFACE_NEW_TARGET_TYPE:
            instruction->operands[0].unsigned_value = leaf_a_value;
            break;
        case MMAY_TEST_INTERFACE_CALL_TARGET_TYPE:
            instruction->operands[0].unsigned_value = scalar;
            break;
        case MMAY_TEST_INTERFACE_NEW_TARGET_LAYOUT:
            instruction->operands[0].unsigned_value = interface_b_value;
            break;
        case MMAY_TEST_INTERFACE_CALL_TARGET_LAYOUT:
            instruction->operands[0].unsigned_value = leaf_b_value;
            break;
        case MMAY_TEST_INTERFACE_NEW_CONSTRUCTION_OOB:
            instruction->operands[1].unsigned_value =
                function->interface_constructions.length;
            break;
        case MMAY_TEST_INTERFACE_CALL_OOB:
            instruction->operands[1].unsigned_value = function->calls.length;
            break;
        case MMAY_TEST_INTERFACE_CALL_SLOT_HIGH:
            call->interface_slot = 1;
            break;
        case MMAY_TEST_INTERFACE_CALL_SLOT_MISSING:
            call->interface_slot = UINT32_MAX;
            break;
        default:
            return false;
    }
    return true;
}

static void mmay_test_interface_verifier_matrix(void) {
    static const MarimayTestVerifyCase cases[] = {
        {MMAY_TEST_INTERFACE_VALID, MMAY_BYTECODE_VERIFY_OK},
        {MMAY_TEST_INTERFACE_NEW_TARGET_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_INTERFACE_NEW_TARGET_TYPE, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_INTERFACE_NEW_TARGET_LAYOUT, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_INTERFACE_NEW_CONSTRUCTION_OOB, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_INTERFACE_NEW_PAYLOAD_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_INTERFACE_NEW_PAYLOAD_UNINITIALIZED, MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {MMAY_TEST_INTERFACE_NEW_PAYLOAD_TYPE, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_INTERFACE_NEW_PAYLOAD_LAYOUT, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_INTERFACE_NEW_METHOD_RANGE, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_INTERFACE_CALL_TARGET_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_INTERFACE_CALL_TARGET_TYPE, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_INTERFACE_CALL_TARGET_LAYOUT, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_INTERFACE_CALL_OOB, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_INTERFACE_CALL_SLOT_HIGH, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_INTERFACE_CALL_SLOT_MISSING, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_INTERFACE_CALL_RECEIVER_MISSING, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_INTERFACE_CALL_RECEIVER_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_INTERFACE_CALL_RECEIVER_UNINITIALIZED, MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {MMAY_TEST_INTERFACE_CALL_RECEIVER_TYPE, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_INTERFACE_CALL_RECEIVER_LAYOUT, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_INTERFACE_CALL_ARGUMENT_OOB, MMAY_BYTECODE_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_INTERFACE_CALL_ARGUMENT_UNINITIALIZED, MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {MMAY_TEST_INTERFACE_CALL_ARGUMENT_TYPE, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_INTERFACE_CALL_ARGUMENT_LAYOUT, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_INTERFACE_CALL_RESULT_LAYOUT, MMAY_BYTECODE_VERIFY_TYPE_MISMATCH}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        MarimayBytecodeFunction function;
        MMAY_EXPECT(mmay_test_build_interface_matrix(
            &function, (MarimayTestInterfaceMutation)cases[index].mutation
        ));
        MMAY_EXPECT(mmay_bytecode_verify(&function) == cases[index].expected);
        mmay_bytecode_function_dispose(&function);
    }
}

void mmay_test_bytecode_nominal_verifier_matrix(void) {
    mmay_test_record_verifier_matrix();
    mmay_test_enum_verifier_matrix();
    mmay_test_interface_verifier_matrix();
}
