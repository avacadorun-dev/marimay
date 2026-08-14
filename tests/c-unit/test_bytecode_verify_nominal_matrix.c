#include "test_bytecode_support.h"

typedef struct RudaTestVerifyCase {
    int mutation;
    RudaBytecodeVerifyStatus expected;
} RudaTestVerifyCase;

typedef enum RudaTestRecordMutation {
    RUDA_TEST_RECORD_VALID,
    RUDA_TEST_RECORD_NEW_TARGET_OOB,
    RUDA_TEST_RECORD_NEW_TARGET_TYPE,
    RUDA_TEST_RECORD_NEW_TARGET_LAYOUT,
    RUDA_TEST_RECORD_NEW_CONSTRUCTION_OOB,
    RUDA_TEST_RECORD_NEW_ARGUMENT_UNINITIALIZED,
    RUDA_TEST_RECORD_NEW_ARGUMENT_LAYOUT,
    RUDA_TEST_RECORD_GET_TARGET_OOB,
    RUDA_TEST_RECORD_GET_TARGET_TYPE,
    RUDA_TEST_RECORD_GET_TARGET_LAYOUT,
    RUDA_TEST_RECORD_GET_SOURCE_OOB,
    RUDA_TEST_RECORD_GET_SOURCE_UNINITIALIZED,
    RUDA_TEST_RECORD_GET_SOURCE_TYPE,
    RUDA_TEST_RECORD_GET_FIELD_HIGH,
    RUDA_TEST_RECORD_GET_FIELD_NEGATIVE,
    RUDA_TEST_RECORD_SET_VALUE_OOB,
    RUDA_TEST_RECORD_SET_VALUE_UNINITIALIZED,
    RUDA_TEST_RECORD_SET_VALUE_TYPE,
    RUDA_TEST_RECORD_SET_VALUE_LAYOUT,
    RUDA_TEST_RECORD_SET_SOURCE_OOB,
    RUDA_TEST_RECORD_SET_SOURCE_UNINITIALIZED,
    RUDA_TEST_RECORD_SET_SOURCE_TYPE,
    RUDA_TEST_RECORD_SET_FIELD_HIGH,
    RUDA_TEST_RECORD_SET_FIELD_NEGATIVE,
    RUDA_TEST_RECORD_SET_IMMUTABLE,
    RUDA_TEST_RECORD_ID_TARGET_OOB,
    RUDA_TEST_RECORD_ID_TARGET_TYPE,
    RUDA_TEST_RECORD_ID_LEFT_OOB,
    RUDA_TEST_RECORD_ID_LEFT_UNINITIALIZED,
    RUDA_TEST_RECORD_ID_LEFT_TYPE,
    RUDA_TEST_RECORD_ID_RIGHT_OOB,
    RUDA_TEST_RECORD_ID_RIGHT_UNINITIALIZED,
    RUDA_TEST_RECORD_ID_RIGHT_TYPE,
    RUDA_TEST_RECORD_ID_RIGHT_LAYOUT
} RudaTestRecordMutation;

static bool ruda_test_build_record_matrix(
    RudaBytecodeFunction *function,
    RudaTestRecordMutation mutation
) {
    RudaBytecodeRecordField holder_field;
    RudaBytecodeRecordArgument holder_argument;
    RudaBytecodeRecordId leaf_a = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordId leaf_b = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordId holder = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordConstructionId leaf_a_new =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    RudaBytecodeRecordConstructionId leaf_b_new =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    RudaBytecodeRecordConstructionId holder_new =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    RudaBytecodeInstruction *instruction = NULL;
    RudaBytecodeRecordArgument *argument = NULL;
    RudaBytecodeRecordField *field = NULL;
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
    const RudaSymbolId symbols[] = {{{0xa1}}, {{0xa2}}, {{0xa3}}};
    memset(function, 0, sizeof(*function));
    memset(&holder_field, 0, sizeof(holder_field));
    memset(&holder_argument, 0, sizeof(holder_argument));
    holder_field.type = RUDA_BYTECODE_TYPE_RECORD;
    holder_field.layout = 0;
    holder_field.is_mutable = true;
    if (!ruda_bytecode_function_init(
            function, ruda_system_allocator(), RUDA_BYTECODE_TYPE_BOOL,
            1, NULL, 0, ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_I64, &scalar
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &leaf_a_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &leaf_b_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &holder_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &projected
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_BOOL, &same
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_I64, &uninitialized_scalar
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &uninitialized_leaf
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &uninitialized_holder
        ) || !ruda_bytecode_add_record_layout(
            function, symbols[0], NULL, 0, &leaf_a
        ) || !ruda_bytecode_add_record_layout(
            function, symbols[1], NULL, 0, &leaf_b
        ) || !ruda_bytecode_add_record_layout(
            function, symbols[2], &holder_field, 1, &holder
        ) || !ruda_bytecode_set_register_layout(
            function, leaf_a_value, leaf_a
        ) || !ruda_bytecode_set_register_layout(
            function, leaf_b_value, leaf_b
        ) || !ruda_bytecode_set_register_layout(
            function, holder_value, holder
        ) || !ruda_bytecode_set_register_layout(
            function, projected, leaf_a
        ) || !ruda_bytecode_set_register_layout(
            function, uninitialized_leaf, leaf_a
        ) || !ruda_bytecode_set_register_layout(
            function, uninitialized_holder, holder
        ) || !ruda_bytecode_add_record_construction(
            function, leaf_a, NULL, 0, &leaf_a_new
        ) || !ruda_bytecode_add_record_construction(
            function, leaf_b, NULL, 0, &leaf_b_new
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    holder_argument.register_id = leaf_a_value;
    holder_argument.type = RUDA_BYTECODE_TYPE_RECORD;
    holder_argument.layout = leaf_a;
    if (!ruda_bytecode_add_record_construction(
            function, holder, &holder_argument, 1, &holder_new
        ) || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_CONST_I64,
            scalar, 7, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_RECORD_NEW,
            leaf_a_value, leaf_a_new, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_RECORD_NEW,
            leaf_b_value, leaf_b_new, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_RECORD_NEW,
            holder_value, holder_new, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_RECORD_SET,
            leaf_a_value, holder_value, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_RECORD_GET,
            projected, holder_value, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_RECORD_IDENTITY_SAME,
            same, leaf_a_value, projected, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_RETURN,
            same, 0, 0, &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    argument = ruda_vector_at(&function->record_arguments, 0);
    field = ruda_vector_at(&function->record_fields, 0);
    switch (mutation) {
        case RUDA_TEST_RECORD_VALID:
            return true;
        case RUDA_TEST_RECORD_NEW_TARGET_OOB:
        case RUDA_TEST_RECORD_NEW_TARGET_TYPE:
        case RUDA_TEST_RECORD_NEW_TARGET_LAYOUT:
        case RUDA_TEST_RECORD_NEW_CONSTRUCTION_OOB:
            instruction = ruda_test_bytecode_at(function, 0, 1);
            break;
        case RUDA_TEST_RECORD_NEW_ARGUMENT_UNINITIALIZED:
        case RUDA_TEST_RECORD_NEW_ARGUMENT_LAYOUT:
            if (argument == NULL) {
                return false;
            }
            if (mutation == RUDA_TEST_RECORD_NEW_ARGUMENT_UNINITIALIZED) {
                argument->register_id = uninitialized_leaf;
            } else {
                argument->layout = leaf_b;
            }
            return true;
        case RUDA_TEST_RECORD_GET_TARGET_OOB:
        case RUDA_TEST_RECORD_GET_TARGET_TYPE:
        case RUDA_TEST_RECORD_GET_TARGET_LAYOUT:
        case RUDA_TEST_RECORD_GET_SOURCE_OOB:
        case RUDA_TEST_RECORD_GET_SOURCE_UNINITIALIZED:
        case RUDA_TEST_RECORD_GET_SOURCE_TYPE:
        case RUDA_TEST_RECORD_GET_FIELD_HIGH:
        case RUDA_TEST_RECORD_GET_FIELD_NEGATIVE:
            instruction = ruda_test_bytecode_at(function, 0, 5);
            break;
        case RUDA_TEST_RECORD_SET_VALUE_OOB:
        case RUDA_TEST_RECORD_SET_VALUE_UNINITIALIZED:
        case RUDA_TEST_RECORD_SET_VALUE_TYPE:
        case RUDA_TEST_RECORD_SET_VALUE_LAYOUT:
        case RUDA_TEST_RECORD_SET_SOURCE_OOB:
        case RUDA_TEST_RECORD_SET_SOURCE_UNINITIALIZED:
        case RUDA_TEST_RECORD_SET_SOURCE_TYPE:
        case RUDA_TEST_RECORD_SET_FIELD_HIGH:
        case RUDA_TEST_RECORD_SET_FIELD_NEGATIVE:
            instruction = ruda_test_bytecode_at(function, 0, 4);
            break;
        case RUDA_TEST_RECORD_SET_IMMUTABLE:
            if (field == NULL) {
                return false;
            }
            field->is_mutable = false;
            return true;
        default:
            instruction = ruda_test_bytecode_at(function, 0, 6);
            break;
    }
    if (instruction == NULL) {
        return false;
    }
    switch (mutation) {
        case RUDA_TEST_RECORD_NEW_TARGET_OOB:
        case RUDA_TEST_RECORD_GET_TARGET_OOB:
        case RUDA_TEST_RECORD_SET_VALUE_OOB:
        case RUDA_TEST_RECORD_ID_TARGET_OOB:
            instruction->operands[0].unsigned_value =
                function->register_types.length;
            break;
        case RUDA_TEST_RECORD_NEW_TARGET_TYPE:
        case RUDA_TEST_RECORD_GET_TARGET_TYPE:
        case RUDA_TEST_RECORD_ID_TARGET_TYPE:
            instruction->operands[0].unsigned_value = scalar;
            break;
        case RUDA_TEST_RECORD_NEW_TARGET_LAYOUT:
        case RUDA_TEST_RECORD_GET_TARGET_LAYOUT:
            instruction->operands[0].unsigned_value = leaf_b_value;
            break;
        case RUDA_TEST_RECORD_NEW_CONSTRUCTION_OOB:
            instruction->operands[1].unsigned_value =
                function->record_constructions.length;
            break;
        case RUDA_TEST_RECORD_GET_SOURCE_OOB:
        case RUDA_TEST_RECORD_SET_SOURCE_OOB:
        case RUDA_TEST_RECORD_ID_LEFT_OOB:
            instruction->operands[1].unsigned_value =
                function->register_types.length;
            break;
        case RUDA_TEST_RECORD_GET_SOURCE_UNINITIALIZED:
        case RUDA_TEST_RECORD_SET_SOURCE_UNINITIALIZED:
            instruction->operands[1].unsigned_value = uninitialized_holder;
            break;
        case RUDA_TEST_RECORD_ID_LEFT_UNINITIALIZED:
            instruction->operands[1].unsigned_value = uninitialized_leaf;
            break;
        case RUDA_TEST_RECORD_GET_SOURCE_TYPE:
        case RUDA_TEST_RECORD_SET_SOURCE_TYPE:
        case RUDA_TEST_RECORD_ID_LEFT_TYPE:
            instruction->operands[1].unsigned_value = scalar;
            break;
        case RUDA_TEST_RECORD_GET_FIELD_HIGH:
        case RUDA_TEST_RECORD_SET_FIELD_HIGH:
            instruction->operands[2].signed_value = 1;
            break;
        case RUDA_TEST_RECORD_GET_FIELD_NEGATIVE:
        case RUDA_TEST_RECORD_SET_FIELD_NEGATIVE:
            instruction->operands[2].signed_value = -1;
            break;
        case RUDA_TEST_RECORD_SET_VALUE_UNINITIALIZED:
            instruction->operands[0].unsigned_value = uninitialized_leaf;
            break;
        case RUDA_TEST_RECORD_SET_VALUE_TYPE:
            instruction->operands[0].unsigned_value = scalar;
            break;
        case RUDA_TEST_RECORD_SET_VALUE_LAYOUT:
            instruction->operands[0].unsigned_value = leaf_b_value;
            break;
        case RUDA_TEST_RECORD_ID_RIGHT_OOB:
            instruction->operands[2].unsigned_value =
                function->register_types.length;
            break;
        case RUDA_TEST_RECORD_ID_RIGHT_UNINITIALIZED:
            instruction->operands[2].unsigned_value = uninitialized_leaf;
            break;
        case RUDA_TEST_RECORD_ID_RIGHT_TYPE:
            instruction->operands[2].unsigned_value = scalar;
            break;
        case RUDA_TEST_RECORD_ID_RIGHT_LAYOUT:
            instruction->operands[2].unsigned_value = leaf_b_value;
            break;
        default:
            return false;
    }
    (void)uninitialized_scalar;
    return true;
}

static void ruda_test_record_verifier_matrix(void) {
    static const RudaTestVerifyCase cases[] = {
        {RUDA_TEST_RECORD_VALID, RUDA_BYTECODE_VERIFY_OK},
        {RUDA_TEST_RECORD_NEW_TARGET_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_RECORD_NEW_TARGET_TYPE, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_RECORD_NEW_TARGET_LAYOUT, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_RECORD_NEW_CONSTRUCTION_OOB, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_RECORD_NEW_ARGUMENT_UNINITIALIZED, RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {RUDA_TEST_RECORD_NEW_ARGUMENT_LAYOUT, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_RECORD_GET_TARGET_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_RECORD_GET_TARGET_TYPE, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_RECORD_GET_TARGET_LAYOUT, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_RECORD_GET_SOURCE_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_RECORD_GET_SOURCE_UNINITIALIZED, RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {RUDA_TEST_RECORD_GET_SOURCE_TYPE, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_RECORD_GET_FIELD_HIGH, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_RECORD_GET_FIELD_NEGATIVE, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_RECORD_SET_VALUE_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_RECORD_SET_VALUE_UNINITIALIZED, RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {RUDA_TEST_RECORD_SET_VALUE_TYPE, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_RECORD_SET_VALUE_LAYOUT, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_RECORD_SET_SOURCE_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_RECORD_SET_SOURCE_UNINITIALIZED, RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {RUDA_TEST_RECORD_SET_SOURCE_TYPE, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_RECORD_SET_FIELD_HIGH, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_RECORD_SET_FIELD_NEGATIVE, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_RECORD_SET_IMMUTABLE, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_RECORD_ID_TARGET_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_RECORD_ID_TARGET_TYPE, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_RECORD_ID_LEFT_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_RECORD_ID_LEFT_UNINITIALIZED, RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {RUDA_TEST_RECORD_ID_LEFT_TYPE, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_RECORD_ID_RIGHT_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_RECORD_ID_RIGHT_UNINITIALIZED, RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {RUDA_TEST_RECORD_ID_RIGHT_TYPE, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_RECORD_ID_RIGHT_LAYOUT, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        RudaBytecodeFunction function;
        RUDA_EXPECT(ruda_test_build_record_matrix(
            &function, (RudaTestRecordMutation)cases[index].mutation
        ));
        RUDA_EXPECT(ruda_bytecode_verify(&function) == cases[index].expected);
        ruda_bytecode_function_dispose(&function);
    }
}

typedef enum RudaTestEnumMutation {
    RUDA_TEST_ENUM_VALID,
    RUDA_TEST_ENUM_NEW_TARGET_OOB,
    RUDA_TEST_ENUM_NEW_TARGET_TYPE,
    RUDA_TEST_ENUM_NEW_TARGET_LAYOUT,
    RUDA_TEST_ENUM_NEW_CONSTRUCTION_OOB,
    RUDA_TEST_ENUM_NEW_ARGUMENT_UNINITIALIZED,
    RUDA_TEST_ENUM_NEW_ARGUMENT_LAYOUT,
    RUDA_TEST_ENUM_IS_TARGET_OOB,
    RUDA_TEST_ENUM_IS_TARGET_TYPE,
    RUDA_TEST_ENUM_IS_SOURCE_OOB,
    RUDA_TEST_ENUM_IS_SOURCE_UNINITIALIZED,
    RUDA_TEST_ENUM_IS_SOURCE_TYPE,
    RUDA_TEST_ENUM_IS_VARIANT_HIGH,
    RUDA_TEST_ENUM_IS_VARIANT_NEGATIVE,
    RUDA_TEST_ENUM_REQUIRE_TARGET_OOB,
    RUDA_TEST_ENUM_REQUIRE_TARGET_TYPE,
    RUDA_TEST_ENUM_REQUIRE_TARGET_LAYOUT,
    RUDA_TEST_ENUM_REQUIRE_SOURCE_OOB,
    RUDA_TEST_ENUM_REQUIRE_SOURCE_UNINITIALIZED,
    RUDA_TEST_ENUM_REQUIRE_SOURCE_TYPE,
    RUDA_TEST_ENUM_REQUIRE_VARIANT_HIGH,
    RUDA_TEST_ENUM_REQUIRE_VARIANT_NEGATIVE,
    RUDA_TEST_ENUM_GET_TARGET_OOB,
    RUDA_TEST_ENUM_GET_TARGET_TYPE,
    RUDA_TEST_ENUM_GET_TARGET_LAYOUT,
    RUDA_TEST_ENUM_GET_SOURCE_OOB,
    RUDA_TEST_ENUM_GET_SOURCE_UNINITIALIZED,
    RUDA_TEST_ENUM_GET_SOURCE_TYPE,
    RUDA_TEST_ENUM_GET_VARIANT_HIGH,
    RUDA_TEST_ENUM_GET_PAYLOAD_HIGH
} RudaTestEnumMutation;

static bool ruda_test_build_enum_matrix(
    RudaBytecodeFunction *function,
    RudaTestEnumMutation mutation
) {
    RudaBytecodeEnumVariant variant;
    RudaBytecodeEnumPayload payloads[2];
    RudaBytecodeEnumArgument arguments[2];
    RudaBytecodeRecordId leaf_a = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordId leaf_b = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordConstructionId leaf_a_new =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    RudaBytecodeRecordConstructionId leaf_b_new =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    RudaBytecodeEnumId enum_a = RUDA_BYTECODE_NO_ENUM;
    RudaBytecodeEnumId enum_b = RUDA_BYTECODE_NO_ENUM;
    RudaBytecodeEnumConstructionId enum_a_new =
        RUDA_BYTECODE_NO_ENUM_CONSTRUCTION;
    RudaBytecodeEnumConstructionId enum_b_new =
        RUDA_BYTECODE_NO_ENUM_CONSTRUCTION;
    RudaBytecodeInstruction *instruction = NULL;
    RudaBytecodeEnumArgument *argument = NULL;
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
    const RudaSymbolId record_symbols[] = {{{0xb1}}, {{0xb2}}};
    const RudaSymbolId enum_symbols[] = {{{0xb3}}, {{0xb4}}};
    memset(function, 0, sizeof(*function));
    memset(&variant, 0, sizeof(variant));
    memset(payloads, 0, sizeof(payloads));
    memset(arguments, 0, sizeof(arguments));
    variant.payload_count = 1;
    payloads[0].type = RUDA_BYTECODE_TYPE_RECORD;
    payloads[0].layout = 0;
    payloads[1].type = RUDA_BYTECODE_TYPE_RECORD;
    payloads[1].layout = 1;
    if (!ruda_bytecode_function_init(
            function, ruda_system_allocator(), RUDA_BYTECODE_TYPE_BOOL,
            1, NULL, 0, ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_I64, &scalar
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &leaf_a_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &leaf_b_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_ENUM, &enum_a_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_ENUM, &enum_b_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_ENUM, &checked
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &projected
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_BOOL, &is_variant
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &uninitialized_leaf
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_ENUM, &uninitialized_enum
        ) || !ruda_bytecode_add_record_layout(
            function, record_symbols[0], NULL, 0, &leaf_a
        ) || !ruda_bytecode_add_record_layout(
            function, record_symbols[1], NULL, 0, &leaf_b
        ) || !ruda_bytecode_add_record_construction(
            function, leaf_a, NULL, 0, &leaf_a_new
        ) || !ruda_bytecode_add_record_construction(
            function, leaf_b, NULL, 0, &leaf_b_new
        ) || !ruda_bytecode_add_enum_layout(
            function, enum_symbols[0], &variant, 1,
            &payloads[0], 1, &enum_a
        ) || !ruda_bytecode_add_enum_layout(
            function, enum_symbols[1], &variant, 1,
            &payloads[1], 1, &enum_b
        ) || !ruda_bytecode_set_register_layout(
            function, leaf_a_value, leaf_a
        ) || !ruda_bytecode_set_register_layout(
            function, leaf_b_value, leaf_b
        ) || !ruda_bytecode_set_register_layout(
            function, enum_a_value, enum_a
        ) || !ruda_bytecode_set_register_layout(
            function, enum_b_value, enum_b
        ) || !ruda_bytecode_set_register_layout(
            function, checked, enum_a
        ) || !ruda_bytecode_set_register_layout(
            function, projected, leaf_a
        ) || !ruda_bytecode_set_register_layout(
            function, uninitialized_leaf, leaf_a
        ) || !ruda_bytecode_set_register_layout(
            function, uninitialized_enum, enum_a
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    arguments[0].register_id = leaf_a_value;
    arguments[0].type = RUDA_BYTECODE_TYPE_RECORD;
    arguments[0].layout = leaf_a;
    arguments[1].register_id = leaf_b_value;
    arguments[1].type = RUDA_BYTECODE_TYPE_RECORD;
    arguments[1].layout = leaf_b;
    if (!ruda_bytecode_add_enum_construction(
            function, enum_a, 0, &arguments[0], 1, &enum_a_new
        ) || !ruda_bytecode_add_enum_construction(
            function, enum_b, 0, &arguments[1], 1, &enum_b_new
        ) || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_CONST_I64,
            scalar, 9, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_RECORD_NEW,
            leaf_a_value, leaf_a_new, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_RECORD_NEW,
            leaf_b_value, leaf_b_new, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_ENUM_NEW,
            enum_a_value, enum_a_new, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_ENUM_NEW,
            enum_b_value, enum_b_new, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_ENUM_IS_VARIANT,
            is_variant, enum_a_value, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_ENUM_REQUIRE_VARIANT,
            checked, enum_a_value, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_ENUM_GET_PAYLOAD,
            projected, checked, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_RETURN,
            is_variant, 0, 0, &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    argument = ruda_vector_at(&function->enum_arguments, 0);
    switch (mutation) {
        case RUDA_TEST_ENUM_VALID:
            return true;
        case RUDA_TEST_ENUM_NEW_TARGET_OOB:
        case RUDA_TEST_ENUM_NEW_TARGET_TYPE:
        case RUDA_TEST_ENUM_NEW_TARGET_LAYOUT:
        case RUDA_TEST_ENUM_NEW_CONSTRUCTION_OOB:
            instruction = ruda_test_bytecode_at(function, 0, 3);
            break;
        case RUDA_TEST_ENUM_NEW_ARGUMENT_UNINITIALIZED:
        case RUDA_TEST_ENUM_NEW_ARGUMENT_LAYOUT:
            if (argument == NULL) {
                return false;
            }
            if (mutation == RUDA_TEST_ENUM_NEW_ARGUMENT_UNINITIALIZED) {
                argument->register_id = uninitialized_leaf;
            } else {
                argument->layout = leaf_b;
            }
            return true;
        case RUDA_TEST_ENUM_IS_TARGET_OOB:
        case RUDA_TEST_ENUM_IS_TARGET_TYPE:
        case RUDA_TEST_ENUM_IS_SOURCE_OOB:
        case RUDA_TEST_ENUM_IS_SOURCE_UNINITIALIZED:
        case RUDA_TEST_ENUM_IS_SOURCE_TYPE:
        case RUDA_TEST_ENUM_IS_VARIANT_HIGH:
        case RUDA_TEST_ENUM_IS_VARIANT_NEGATIVE:
            instruction = ruda_test_bytecode_at(function, 0, 5);
            break;
        case RUDA_TEST_ENUM_REQUIRE_TARGET_OOB:
        case RUDA_TEST_ENUM_REQUIRE_TARGET_TYPE:
        case RUDA_TEST_ENUM_REQUIRE_TARGET_LAYOUT:
        case RUDA_TEST_ENUM_REQUIRE_SOURCE_OOB:
        case RUDA_TEST_ENUM_REQUIRE_SOURCE_UNINITIALIZED:
        case RUDA_TEST_ENUM_REQUIRE_SOURCE_TYPE:
        case RUDA_TEST_ENUM_REQUIRE_VARIANT_HIGH:
        case RUDA_TEST_ENUM_REQUIRE_VARIANT_NEGATIVE:
            instruction = ruda_test_bytecode_at(function, 0, 6);
            break;
        default:
            instruction = ruda_test_bytecode_at(function, 0, 7);
            break;
    }
    if (instruction == NULL) {
        return false;
    }
    switch (mutation) {
        case RUDA_TEST_ENUM_NEW_TARGET_OOB:
        case RUDA_TEST_ENUM_IS_TARGET_OOB:
        case RUDA_TEST_ENUM_REQUIRE_TARGET_OOB:
        case RUDA_TEST_ENUM_GET_TARGET_OOB:
            instruction->operands[0].unsigned_value =
                function->register_types.length;
            break;
        case RUDA_TEST_ENUM_NEW_TARGET_TYPE:
        case RUDA_TEST_ENUM_IS_TARGET_TYPE:
        case RUDA_TEST_ENUM_REQUIRE_TARGET_TYPE:
        case RUDA_TEST_ENUM_GET_TARGET_TYPE:
            instruction->operands[0].unsigned_value = scalar;
            break;
        case RUDA_TEST_ENUM_NEW_TARGET_LAYOUT:
        case RUDA_TEST_ENUM_REQUIRE_TARGET_LAYOUT:
            instruction->operands[0].unsigned_value = enum_b_value;
            break;
        case RUDA_TEST_ENUM_GET_TARGET_LAYOUT:
            instruction->operands[0].unsigned_value = leaf_b_value;
            break;
        case RUDA_TEST_ENUM_NEW_CONSTRUCTION_OOB:
            instruction->operands[1].unsigned_value =
                function->enum_constructions.length;
            break;
        case RUDA_TEST_ENUM_IS_SOURCE_OOB:
        case RUDA_TEST_ENUM_REQUIRE_SOURCE_OOB:
        case RUDA_TEST_ENUM_GET_SOURCE_OOB:
            instruction->operands[1].unsigned_value =
                function->register_types.length;
            break;
        case RUDA_TEST_ENUM_IS_SOURCE_UNINITIALIZED:
        case RUDA_TEST_ENUM_REQUIRE_SOURCE_UNINITIALIZED:
        case RUDA_TEST_ENUM_GET_SOURCE_UNINITIALIZED:
            instruction->operands[1].unsigned_value = uninitialized_enum;
            break;
        case RUDA_TEST_ENUM_IS_SOURCE_TYPE:
        case RUDA_TEST_ENUM_REQUIRE_SOURCE_TYPE:
        case RUDA_TEST_ENUM_GET_SOURCE_TYPE:
            instruction->operands[1].unsigned_value = leaf_a_value;
            break;
        case RUDA_TEST_ENUM_IS_VARIANT_HIGH:
        case RUDA_TEST_ENUM_REQUIRE_VARIANT_HIGH:
            instruction->operands[2].signed_value = 1;
            break;
        case RUDA_TEST_ENUM_IS_VARIANT_NEGATIVE:
        case RUDA_TEST_ENUM_REQUIRE_VARIANT_NEGATIVE:
            instruction->operands[2].signed_value = -1;
            break;
        case RUDA_TEST_ENUM_GET_VARIANT_HIGH:
            instruction->operands[2].signed_value = INT64_C(1) << 32;
            break;
        case RUDA_TEST_ENUM_GET_PAYLOAD_HIGH:
            instruction->operands[2].signed_value = 1;
            break;
        default:
            return false;
    }
    return true;
}

static void ruda_test_enum_verifier_matrix(void) {
    static const RudaTestVerifyCase cases[] = {
        {RUDA_TEST_ENUM_VALID, RUDA_BYTECODE_VERIFY_OK},
        {RUDA_TEST_ENUM_NEW_TARGET_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_ENUM_NEW_TARGET_TYPE, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_ENUM_NEW_TARGET_LAYOUT, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_ENUM_NEW_CONSTRUCTION_OOB, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_ENUM_NEW_ARGUMENT_UNINITIALIZED, RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {RUDA_TEST_ENUM_NEW_ARGUMENT_LAYOUT, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_ENUM_IS_TARGET_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_ENUM_IS_TARGET_TYPE, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_ENUM_IS_SOURCE_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_ENUM_IS_SOURCE_UNINITIALIZED, RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {RUDA_TEST_ENUM_IS_SOURCE_TYPE, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_ENUM_IS_VARIANT_HIGH, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_ENUM_IS_VARIANT_NEGATIVE, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_ENUM_REQUIRE_TARGET_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_ENUM_REQUIRE_TARGET_TYPE, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_ENUM_REQUIRE_TARGET_LAYOUT, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_ENUM_REQUIRE_SOURCE_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_ENUM_REQUIRE_SOURCE_UNINITIALIZED, RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {RUDA_TEST_ENUM_REQUIRE_SOURCE_TYPE, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_ENUM_REQUIRE_VARIANT_HIGH, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_ENUM_REQUIRE_VARIANT_NEGATIVE, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_ENUM_GET_TARGET_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_ENUM_GET_TARGET_TYPE, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_ENUM_GET_TARGET_LAYOUT, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_ENUM_GET_SOURCE_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_ENUM_GET_SOURCE_UNINITIALIZED, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_ENUM_GET_SOURCE_TYPE, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_ENUM_GET_VARIANT_HIGH, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_ENUM_GET_PAYLOAD_HIGH, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        RudaBytecodeFunction function;
        RUDA_EXPECT(ruda_test_build_enum_matrix(
            &function, (RudaTestEnumMutation)cases[index].mutation
        ));
        RUDA_EXPECT(ruda_bytecode_verify(&function) == cases[index].expected);
        ruda_bytecode_function_dispose(&function);
    }
}

typedef enum RudaTestInterfaceMutation {
    RUDA_TEST_INTERFACE_VALID,
    RUDA_TEST_INTERFACE_NEW_TARGET_OOB,
    RUDA_TEST_INTERFACE_NEW_TARGET_TYPE,
    RUDA_TEST_INTERFACE_NEW_TARGET_LAYOUT,
    RUDA_TEST_INTERFACE_NEW_CONSTRUCTION_OOB,
    RUDA_TEST_INTERFACE_NEW_PAYLOAD_OOB,
    RUDA_TEST_INTERFACE_NEW_PAYLOAD_UNINITIALIZED,
    RUDA_TEST_INTERFACE_NEW_PAYLOAD_TYPE,
    RUDA_TEST_INTERFACE_NEW_PAYLOAD_LAYOUT,
    RUDA_TEST_INTERFACE_NEW_METHOD_RANGE,
    RUDA_TEST_INTERFACE_CALL_TARGET_OOB,
    RUDA_TEST_INTERFACE_CALL_TARGET_TYPE,
    RUDA_TEST_INTERFACE_CALL_TARGET_LAYOUT,
    RUDA_TEST_INTERFACE_CALL_OOB,
    RUDA_TEST_INTERFACE_CALL_SLOT_HIGH,
    RUDA_TEST_INTERFACE_CALL_SLOT_MISSING,
    RUDA_TEST_INTERFACE_CALL_RECEIVER_MISSING,
    RUDA_TEST_INTERFACE_CALL_RECEIVER_OOB,
    RUDA_TEST_INTERFACE_CALL_RECEIVER_UNINITIALIZED,
    RUDA_TEST_INTERFACE_CALL_RECEIVER_TYPE,
    RUDA_TEST_INTERFACE_CALL_RECEIVER_LAYOUT,
    RUDA_TEST_INTERFACE_CALL_ARGUMENT_OOB,
    RUDA_TEST_INTERFACE_CALL_ARGUMENT_UNINITIALIZED,
    RUDA_TEST_INTERFACE_CALL_ARGUMENT_TYPE,
    RUDA_TEST_INTERFACE_CALL_ARGUMENT_LAYOUT,
    RUDA_TEST_INTERFACE_CALL_RESULT_LAYOUT
} RudaTestInterfaceMutation;

static bool ruda_test_build_interface_matrix(
    RudaBytecodeFunction *function,
    RudaTestInterfaceMutation mutation
) {
    RudaBytecodeRecordId leaf_a = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordId leaf_b = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordConstructionId leaf_a_new =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    RudaBytecodeRecordConstructionId leaf_b_new =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    RudaBytecodeInterfaceId interface_a = RUDA_BYTECODE_NO_INTERFACE;
    RudaBytecodeInterfaceId interface_b = RUDA_BYTECODE_NO_INTERFACE;
    RudaBytecodeInterfaceConstructionId interface_a_new =
        RUDA_BYTECODE_NO_INTERFACE_CONSTRUCTION;
    RudaBytecodeInterfaceConstructionId interface_b_new =
        RUDA_BYTECODE_NO_INTERFACE_CONSTRUCTION;
    RudaBytecodeCallArgument call_arguments[2];
    RudaBytecodeCallId call_id = RUDA_BYTECODE_NO_CALL;
    RudaBytecodeCall *call = NULL;
    RudaBytecodeCallArgument *receiver = NULL;
    RudaBytecodeCallArgument *argument = NULL;
    RudaBytecodeInterfaceConstruction *construction = NULL;
    RudaBytecodeInstruction *instruction = NULL;
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
    const RudaSymbolId record_symbols[] = {{{0xc1}}, {{0xc2}}};
    const RudaSymbolId interface_symbols[] = {{{0xc3}}, {{0xc4}}};
    const RudaSymbolId method_symbols[] = {{{0xc5}}, {{0xc6}}};
    const RudaSymbolId placeholder = {{0xcf}};
    memset(function, 0, sizeof(*function));
    memset(call_arguments, 0, sizeof(call_arguments));
    if (!ruda_bytecode_function_init(
            function, ruda_system_allocator(), RUDA_BYTECODE_TYPE_BOOL,
            1, NULL, 0, ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_I64, &scalar
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &leaf_a_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &leaf_b_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_INTERFACE, &interface_a_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_INTERFACE, &interface_b_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &result
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_BOOL, &same
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &uninitialized_leaf
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_INTERFACE, &uninitialized_interface
        ) || !ruda_bytecode_add_record_layout(
            function, record_symbols[0], NULL, 0, &leaf_a
        ) || !ruda_bytecode_add_record_layout(
            function, record_symbols[1], NULL, 0, &leaf_b
        ) || !ruda_bytecode_add_record_construction(
            function, leaf_a, NULL, 0, &leaf_a_new
        ) || !ruda_bytecode_add_record_construction(
            function, leaf_b, NULL, 0, &leaf_b_new
        ) || !ruda_bytecode_add_interface_layout(
            function, interface_symbols[0], 1, &interface_a
        ) || !ruda_bytecode_add_interface_layout(
            function, interface_symbols[1], 1, &interface_b
        ) || !ruda_bytecode_set_register_layout(
            function, leaf_a_value, leaf_a
        ) || !ruda_bytecode_set_register_layout(
            function, leaf_b_value, leaf_b
        ) || !ruda_bytecode_set_register_layout(
            function, interface_a_value, interface_a
        ) || !ruda_bytecode_set_register_layout(
            function, interface_b_value, interface_b
        ) || !ruda_bytecode_set_register_layout(
            function, result, leaf_a
        ) || !ruda_bytecode_set_register_layout(
            function, uninitialized_leaf, leaf_a
        ) || !ruda_bytecode_set_register_layout(
            function, uninitialized_interface, interface_a
        ) || !ruda_bytecode_add_interface_construction(
            function, interface_a, leaf_a_value,
            RUDA_BYTECODE_TYPE_RECORD, leaf_a,
            &method_symbols[0], 1, &interface_a_new
        ) || !ruda_bytecode_add_interface_construction(
            function, interface_b, leaf_b_value,
            RUDA_BYTECODE_TYPE_RECORD, leaf_b,
            &method_symbols[1], 1, &interface_b_new
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    call_arguments[0].register_id = interface_a_value;
    call_arguments[0].type = RUDA_BYTECODE_TYPE_INTERFACE;
    call_arguments[0].layout = interface_a;
    call_arguments[1].register_id = leaf_a_value;
    call_arguments[1].type = RUDA_BYTECODE_TYPE_RECORD;
    call_arguments[1].layout = leaf_a;
    if (!ruda_bytecode_add_call(
            function, placeholder, RUDA_BYTECODE_TYPE_RECORD,
            call_arguments, 2, &call_id
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    call = ruda_vector_at(&function->calls, call_id);
    if (call == NULL) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    call->result_layout = leaf_a;
    call->interface_slot = 0;
    if (!ruda_bytecode_add_block(function, &block)
        || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_CONST_I64,
            scalar, 11, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_RECORD_NEW,
            leaf_a_value, leaf_a_new, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_RECORD_NEW,
            leaf_b_value, leaf_b_new, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_INTERFACE_NEW,
            interface_a_value, interface_a_new, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_INTERFACE_NEW,
            interface_b_value, interface_b_new, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_CALL_INTERFACE,
            result, call_id, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_RECORD_IDENTITY_SAME,
            same, leaf_a_value, result, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_RETURN,
            same, 0, 0, &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    construction = ruda_vector_at(&function->interface_constructions, 0);
    receiver = ruda_vector_at(&function->call_arguments, 0);
    argument = ruda_vector_at(&function->call_arguments, 1);
    if (construction == NULL || receiver == NULL || argument == NULL) {
        return false;
    }
    switch (mutation) {
        case RUDA_TEST_INTERFACE_VALID:
            return true;
        case RUDA_TEST_INTERFACE_NEW_TARGET_OOB:
        case RUDA_TEST_INTERFACE_NEW_TARGET_TYPE:
        case RUDA_TEST_INTERFACE_NEW_TARGET_LAYOUT:
        case RUDA_TEST_INTERFACE_NEW_CONSTRUCTION_OOB:
            instruction = ruda_test_bytecode_at(function, 0, 3);
            break;
        case RUDA_TEST_INTERFACE_NEW_PAYLOAD_OOB:
            construction->payload_register =
                (uint32_t)function->register_types.length;
            return true;
        case RUDA_TEST_INTERFACE_NEW_PAYLOAD_UNINITIALIZED:
            construction->payload_register = uninitialized_leaf;
            return true;
        case RUDA_TEST_INTERFACE_NEW_PAYLOAD_TYPE:
            construction->payload_register = interface_a_value;
            construction->payload_type = RUDA_BYTECODE_TYPE_INTERFACE;
            construction->payload_layout = interface_a;
            return true;
        case RUDA_TEST_INTERFACE_NEW_PAYLOAD_LAYOUT:
            construction->payload_register = leaf_b_value;
            return true;
        case RUDA_TEST_INTERFACE_NEW_METHOD_RANGE:
            construction->method_count = 2;
            return true;
        case RUDA_TEST_INTERFACE_CALL_RECEIVER_MISSING:
            call->argument_count = 0;
            return true;
        case RUDA_TEST_INTERFACE_CALL_RECEIVER_OOB:
            receiver->register_id = (uint32_t)function->register_types.length;
            return true;
        case RUDA_TEST_INTERFACE_CALL_RECEIVER_UNINITIALIZED:
            receiver->register_id = uninitialized_interface;
            return true;
        case RUDA_TEST_INTERFACE_CALL_RECEIVER_TYPE:
            receiver->register_id = leaf_a_value;
            receiver->type = RUDA_BYTECODE_TYPE_RECORD;
            receiver->layout = leaf_a;
            return true;
        case RUDA_TEST_INTERFACE_CALL_RECEIVER_LAYOUT:
            receiver->register_id = interface_b_value;
            return true;
        case RUDA_TEST_INTERFACE_CALL_ARGUMENT_OOB:
            argument->register_id =
                (uint32_t)function->register_types.length;
            return true;
        case RUDA_TEST_INTERFACE_CALL_ARGUMENT_UNINITIALIZED:
            argument->register_id = uninitialized_leaf;
            return true;
        case RUDA_TEST_INTERFACE_CALL_ARGUMENT_TYPE:
            argument->register_id = scalar;
            return true;
        case RUDA_TEST_INTERFACE_CALL_ARGUMENT_LAYOUT:
            argument->register_id = leaf_b_value;
            return true;
        case RUDA_TEST_INTERFACE_CALL_RESULT_LAYOUT:
            call->result_layout = leaf_b;
            return true;
        default:
            instruction = ruda_test_bytecode_at(function, 0, 5);
            break;
    }
    if (instruction == NULL) {
        return false;
    }
    switch (mutation) {
        case RUDA_TEST_INTERFACE_NEW_TARGET_OOB:
        case RUDA_TEST_INTERFACE_CALL_TARGET_OOB:
            instruction->operands[0].unsigned_value =
                function->register_types.length;
            break;
        case RUDA_TEST_INTERFACE_NEW_TARGET_TYPE:
            instruction->operands[0].unsigned_value = leaf_a_value;
            break;
        case RUDA_TEST_INTERFACE_CALL_TARGET_TYPE:
            instruction->operands[0].unsigned_value = scalar;
            break;
        case RUDA_TEST_INTERFACE_NEW_TARGET_LAYOUT:
            instruction->operands[0].unsigned_value = interface_b_value;
            break;
        case RUDA_TEST_INTERFACE_CALL_TARGET_LAYOUT:
            instruction->operands[0].unsigned_value = leaf_b_value;
            break;
        case RUDA_TEST_INTERFACE_NEW_CONSTRUCTION_OOB:
            instruction->operands[1].unsigned_value =
                function->interface_constructions.length;
            break;
        case RUDA_TEST_INTERFACE_CALL_OOB:
            instruction->operands[1].unsigned_value = function->calls.length;
            break;
        case RUDA_TEST_INTERFACE_CALL_SLOT_HIGH:
            call->interface_slot = 1;
            break;
        case RUDA_TEST_INTERFACE_CALL_SLOT_MISSING:
            call->interface_slot = UINT32_MAX;
            break;
        default:
            return false;
    }
    return true;
}

static void ruda_test_interface_verifier_matrix(void) {
    static const RudaTestVerifyCase cases[] = {
        {RUDA_TEST_INTERFACE_VALID, RUDA_BYTECODE_VERIFY_OK},
        {RUDA_TEST_INTERFACE_NEW_TARGET_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_INTERFACE_NEW_TARGET_TYPE, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_INTERFACE_NEW_TARGET_LAYOUT, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_INTERFACE_NEW_CONSTRUCTION_OOB, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_INTERFACE_NEW_PAYLOAD_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_INTERFACE_NEW_PAYLOAD_UNINITIALIZED, RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {RUDA_TEST_INTERFACE_NEW_PAYLOAD_TYPE, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_INTERFACE_NEW_PAYLOAD_LAYOUT, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_INTERFACE_NEW_METHOD_RANGE, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_INTERFACE_CALL_TARGET_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_INTERFACE_CALL_TARGET_TYPE, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_INTERFACE_CALL_TARGET_LAYOUT, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_INTERFACE_CALL_OOB, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_INTERFACE_CALL_SLOT_HIGH, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_INTERFACE_CALL_SLOT_MISSING, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_INTERFACE_CALL_RECEIVER_MISSING, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_INTERFACE_CALL_RECEIVER_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_INTERFACE_CALL_RECEIVER_UNINITIALIZED, RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {RUDA_TEST_INTERFACE_CALL_RECEIVER_TYPE, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_INTERFACE_CALL_RECEIVER_LAYOUT, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_INTERFACE_CALL_ARGUMENT_OOB, RUDA_BYTECODE_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_INTERFACE_CALL_ARGUMENT_UNINITIALIZED, RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER},
        {RUDA_TEST_INTERFACE_CALL_ARGUMENT_TYPE, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_INTERFACE_CALL_ARGUMENT_LAYOUT, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_INTERFACE_CALL_RESULT_LAYOUT, RUDA_BYTECODE_VERIFY_TYPE_MISMATCH}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        RudaBytecodeFunction function;
        RUDA_EXPECT(ruda_test_build_interface_matrix(
            &function, (RudaTestInterfaceMutation)cases[index].mutation
        ));
        RUDA_EXPECT(ruda_bytecode_verify(&function) == cases[index].expected);
        ruda_bytecode_function_dispose(&function);
    }
}

void ruda_test_bytecode_nominal_verifier_matrix(void) {
    ruda_test_record_verifier_matrix();
    ruda_test_enum_verifier_matrix();
    ruda_test_interface_verifier_matrix();
}
