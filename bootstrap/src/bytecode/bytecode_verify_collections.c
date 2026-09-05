#include "mmay/bytecode/bytecode.h"

#include "bytecode_internal.h"

static bool mmay_collection_opcode(MarimayOpcode opcode) {
    return (opcode >= MMAY_OPCODE_COLLECTION_NEW
            && opcode <= MMAY_OPCODE_COLLECTION_ENTRY_VALUE)
        || (opcode >= MMAY_OPCODE_COLLECTION_GET_OPTION
            && opcode <= MMAY_OPCODE_COLLECTION_ADD_RESULT);
}

static const MarimayBytecodeEnumVariant *mmay_collection_variant(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    size_t variant
) {
    return function != NULL && layout != NULL && variant < layout->variant_count
            && layout->variant_start <= function->enum_variants.length
            && variant < function->enum_variants.length - layout->variant_start
        ? mmay_vector_at_const(
            &function->enum_variants, layout->variant_start + variant
        ) : NULL;
}

static const MarimayBytecodeEnumPayload *mmay_collection_payload(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    const MarimayBytecodeEnumVariant *variant
) {
    size_t relative = 0;
    if (function == NULL || layout == NULL || variant == NULL
        || variant->payload_count == 0
        || !mmay_size_add(variant->payload_start, 0, &relative)
        || relative >= layout->payload_count
        || layout->payload_start > function->enum_payloads.length
        || relative >= function->enum_payloads.length - layout->payload_start) {
        return NULL;
    }
    return mmay_vector_at_const(
        &function->enum_payloads, layout->payload_start + relative
    );
}

static MarimayBytecodeVerifyStatus mmay_collection_assigned(
    const unsigned char *assigned,
    size_t count,
    uint64_t register_id
) {
    if (register_id >= count) return MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
    return assigned[register_id] != 0
        ? MMAY_BYTECODE_VERIFY_OK
        : MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER;
}

static const MarimayBytecodeType *mmay_collection_register_type(
    const MarimayBytecodeFunction *function,
    uint64_t register_id
) {
    return register_id < function->register_types.length
        ? mmay_vector_at_const(&function->register_types, (size_t)register_id)
        : NULL;
}

static bool mmay_collection_register_matches(
    const MarimayBytecodeFunction *function,
    uint64_t register_id,
    MarimayBytecodeType type,
    uint32_t layout
) {
    const MarimayBytecodeType *actual = mmay_collection_register_type(
        function, register_id
    );
    return actual != NULL && *actual == type
        && mmay_bytecode_register_layout(function, (uint32_t)register_id)
            == layout;
}

static bool mmay_collection_result_matches(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    const MarimayBytecodeCollectionType *collection,
    uint64_t target
) {
    const uint32_t layout_id = target <= UINT32_MAX
        ? mmay_bytecode_register_layout(function, (uint32_t)target)
        : MMAY_BYTECODE_NO_LAYOUT;
    const MarimayBytecodeEnumLayout *layout = mmay_bytecode_enum_layout(
        function, layout_id
    );
    const MarimayBytecodeEnumVariant *first = mmay_collection_variant(
        function, layout, 0
    );
    const MarimayBytecodeEnumVariant *second = mmay_collection_variant(
        function, layout, 1
    );
    const MarimayBytecodeEnumPayload *first_payload = mmay_collection_payload(
        function, layout, first
    );
    bool option = instruction->opcode == MMAY_OPCODE_COLLECTION_GET_OPTION
        || instruction->opcode == MMAY_OPCODE_COLLECTION_REMOVE_OPTION;
    if (!mmay_collection_register_matches(
            function, target, MMAY_BYTECODE_TYPE_ENUM, layout_id
        ) || layout == NULL || first == NULL || second == NULL
        || layout->variant_count != 2 || first->payload_count != 1
        || first_payload == NULL
        || second->payload_count != (option ? 0U : 1U)) return false;
    if (option) {
        return !collection->is_set
            && first_payload->type == collection->value_type
            && first_payload->layout == collection->value_layout;
    }
    {
        const MarimayBytecodeEnumPayload *error_payload =
            mmay_collection_payload(function, layout, second);
        const MarimayBytecodeEnumLayout *error = error_payload != NULL
                && error_payload->type == MMAY_BYTECODE_TYPE_ENUM
            ? mmay_bytecode_enum_layout(function, error_payload->layout) : NULL;
        const MarimayBytecodeEnumVariant *limit = mmay_collection_variant(
            function, error, 0
        );
        const MarimayBytecodeEnumPayload *limit_payload =
            mmay_collection_payload(function, error, limit);
        return first_payload->type == MMAY_BYTECODE_TYPE_BOOL
            && first_payload->layout == MMAY_BYTECODE_NO_LAYOUT
            && error != NULL && error->variant_count == 1
            && limit != NULL && limit->payload_count == 1
            && limit_payload != NULL
            && limit_payload->type == MMAY_BYTECODE_TYPE_I64
            && limit_payload->layout == MMAY_BYTECODE_NO_LAYOUT;
    }
}

MarimayBytecodeVerifyStatus mmay_bytecode_verify_collection_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t collection_register = instruction->operands[1].unsigned_value;
    uint64_t argument = instruction->operands[2].unsigned_value;
    uint32_t layout = MMAY_BYTECODE_NO_LAYOUT;
    const MarimayBytecodeCollectionType *collection = NULL;
    const MarimayBytecodeType *collection_type = NULL;
    MarimayBytecodeVerifyStatus status;
    if (function == NULL || instruction == NULL || assigned == NULL
        || !mmay_collection_opcode(instruction->opcode)) {
        return MMAY_BYTECODE_VERIFY_INVALID_OPCODE;
    }
    if (instruction->opcode == MMAY_OPCODE_COLLECTION_NEW) {
        collection_type = mmay_collection_register_type(function, target);
        layout = target <= UINT32_MAX
            ? mmay_bytecode_register_layout(function, (uint32_t)target)
            : MMAY_BYTECODE_NO_LAYOUT;
        collection = mmay_bytecode_collection_type(function, layout);
        if (collection_type == NULL || collection == NULL
            || (*collection_type == MMAY_BYTECODE_TYPE_SET)
                != collection->is_set
            || (*collection_type != MMAY_BYTECODE_TYPE_MAP
                && *collection_type != MMAY_BYTECODE_TYPE_SET)) {
            return collection_type == NULL
                ? MMAY_BYTECODE_VERIFY_INVALID_REGISTER
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        assigned[target] = 1;
        return MMAY_BYTECODE_VERIFY_OK;
    }
    if (instruction->opcode == MMAY_OPCODE_COLLECTION_PUT
        || instruction->opcode == MMAY_OPCODE_COLLECTION_ADD
        || instruction->opcode == MMAY_OPCODE_COLLECTION_CLEAR) {
        collection_register = target;
        argument = instruction->operands[1].unsigned_value;
    }
    status = mmay_collection_assigned(
        assigned, function->register_types.length, collection_register
    );
    if (status != MMAY_BYTECODE_VERIFY_OK) return status;
    collection_type = mmay_collection_register_type(
        function, collection_register
    );
    layout = mmay_bytecode_register_layout(
        function, (uint32_t)collection_register
    );
    collection = mmay_bytecode_collection_type(function, layout);
    if (collection_type == NULL || collection == NULL
        || (*collection_type == MMAY_BYTECODE_TYPE_SET) != collection->is_set
        || (*collection_type != MMAY_BYTECODE_TYPE_MAP
            && *collection_type != MMAY_BYTECODE_TYPE_SET)) {
        return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == MMAY_OPCODE_COLLECTION_CLEAR) {
        return MMAY_BYTECODE_VERIFY_OK;
    }
    if (instruction->opcode == MMAY_OPCODE_COLLECTION_LENGTH) {
        if (!mmay_collection_register_matches(
                function, target, MMAY_BYTECODE_TYPE_I64,
                MMAY_BYTECODE_NO_LAYOUT
            )) return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        assigned[target] = 1;
        return MMAY_BYTECODE_VERIFY_OK;
    }
    if (instruction->opcode == MMAY_OPCODE_COLLECTION_GENERATION) {
        if (!mmay_collection_register_matches(
                function, target, MMAY_BYTECODE_TYPE_U64,
                MMAY_BYTECODE_NO_LAYOUT
            )) return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        assigned[target] = 1;
        return MMAY_BYTECODE_VERIFY_OK;
    }
    status = mmay_collection_assigned(
        assigned, function->register_types.length, argument
    );
    if (status != MMAY_BYTECODE_VERIFY_OK) return status;
    if (instruction->opcode == MMAY_OPCODE_COLLECTION_REQUIRE_GENERATION) {
        if (!mmay_collection_register_matches(
                function, argument, MMAY_BYTECODE_TYPE_U64,
                MMAY_BYTECODE_NO_LAYOUT
            ) || !mmay_collection_register_matches(
                function, target, *collection_type, layout
            )) return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        assigned[target] = 1;
        return MMAY_BYTECODE_VERIFY_OK;
    }
    if (instruction->opcode == MMAY_OPCODE_COLLECTION_ENTRY_KEY
        || instruction->opcode == MMAY_OPCODE_COLLECTION_ENTRY_VALUE) {
        MarimayBytecodeType expected = instruction->opcode
                == MMAY_OPCODE_COLLECTION_ENTRY_KEY
            ? collection->key_type : collection->value_type;
        uint32_t expected_layout = instruction->opcode
                == MMAY_OPCODE_COLLECTION_ENTRY_KEY
            ? collection->key_layout : collection->value_layout;
        if (!mmay_collection_register_matches(
                function, argument, MMAY_BYTECODE_TYPE_I64,
                MMAY_BYTECODE_NO_LAYOUT
            ) || (instruction->opcode == MMAY_OPCODE_COLLECTION_ENTRY_VALUE
                && collection->is_set)
            || !mmay_collection_register_matches(
                function, target, expected, expected_layout
            )) return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        assigned[target] = 1;
        return MMAY_BYTECODE_VERIFY_OK;
    }
    if (!mmay_collection_register_matches(
            function, argument, collection->key_type, collection->key_layout
        )) return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    if (instruction->opcode == MMAY_OPCODE_COLLECTION_GET_OPTION
        || instruction->opcode == MMAY_OPCODE_COLLECTION_REMOVE_OPTION
        || instruction->opcode == MMAY_OPCODE_COLLECTION_PUT_RESULT
        || instruction->opcode == MMAY_OPCODE_COLLECTION_ADD_RESULT) {
        if (!mmay_collection_result_matches(
                function, instruction, collection, target
            )) return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        if (instruction->opcode == MMAY_OPCODE_COLLECTION_PUT_RESULT) {
            uint64_t value = instruction->operands[3].unsigned_value;
            status = mmay_collection_assigned(
                assigned, function->register_types.length, value
            );
            if (status != MMAY_BYTECODE_VERIFY_OK) return status;
            if (collection->is_set || !mmay_collection_register_matches(
                    function, value, collection->value_type,
                    collection->value_layout
                )) return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        } else if (instruction->opcode == MMAY_OPCODE_COLLECTION_ADD_RESULT
            && !collection->is_set) {
            return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        assigned[target] = 1;
        return MMAY_BYTECODE_VERIFY_OK;
    }
    if (instruction->opcode == MMAY_OPCODE_COLLECTION_PUT) {
        uint64_t value = instruction->operands[2].unsigned_value;
        status = mmay_collection_assigned(
            assigned, function->register_types.length, value
        );
        if (status != MMAY_BYTECODE_VERIFY_OK) return status;
        return !collection->is_set && mmay_collection_register_matches(
                function, value, collection->value_type,
                collection->value_layout
            ) ? MMAY_BYTECODE_VERIFY_OK
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == MMAY_OPCODE_COLLECTION_ADD) {
        return collection->is_set ? MMAY_BYTECODE_VERIFY_OK
            : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == MMAY_OPCODE_COLLECTION_CONTAINS
        || instruction->opcode == MMAY_OPCODE_COLLECTION_REMOVE) {
        if (!mmay_collection_register_matches(
                function, target, MMAY_BYTECODE_TYPE_BOOL,
                MMAY_BYTECODE_NO_LAYOUT
            )) return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    } else if (instruction->opcode == MMAY_OPCODE_COLLECTION_GET) {
        if (collection->is_set || !mmay_collection_register_matches(
                function, target, collection->value_type,
                collection->value_layout
            )) return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    } else {
        return MMAY_BYTECODE_VERIFY_INVALID_OPCODE;
    }
    assigned[target] = 1;
    return MMAY_BYTECODE_VERIFY_OK;
}
