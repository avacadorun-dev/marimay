#include "mir_internal.h"
#include "mir_collections_internal.h"

static bool mmay_mir_collection_symbol(
    const char *name,
    MarimaySymbolId *symbol
) {
    return mmay_symbol_id_from_parts("std", "prelude", name, 0, symbol);
}

static MarimayMirVerifyStatus mmay_mir_verify_collection_result(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirCollection *collection,
    MarimayTypeId target_type,
    MarimayTypeId third_type
) {
    const MarimayMirEnum *result = mmay_mir_enum(
        function, instruction->enum_id
    );
    const MarimayMirEnumVariant *first = mmay_mir_enum_variant(
        function, result, 0
    );
    const MarimayMirEnumVariant *second = mmay_mir_enum_variant(
        function, result, 1
    );
    const MarimayMirEnumPayload *first_payload = mmay_mir_enum_payload(
        function, first, 0
    );
    MarimaySymbolId expected_symbol;
    const bool option = instruction->kind == MMAY_MIR_COLLECTION_GET_OPTION
        || instruction->kind == MMAY_MIR_COLLECTION_REMOVE_OPTION;
    if (result == NULL || first == NULL || second == NULL
        || !mmay_mir_collection_symbol(
            option ? "Option" : "Result", &expected_symbol
        )) return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
    if ((option && collection->is_set) || target_type != result->type
        || !mmay_symbol_id_equal(result->symbol, expected_symbol)
        || result->variant_count != 2
        || first->kind != MMAY_MIR_ENUM_VARIANT_POSITIONAL
        || first->payload_count != 1 || first_payload == NULL
        || second->payload_count != (option ? 0U : 1U)
        || (option && (first_payload->type != collection->value_type
            || second->kind != MMAY_MIR_ENUM_VARIANT_UNIT))) {
        return MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (option) return MMAY_MIR_VERIFY_OK;
    {
        const MarimayMirEnumPayload *error_payload = mmay_mir_enum_payload(
            function, second, 0
        );
        const MarimayMirEnum *error = mmay_mir_enum(
            function, instruction->option_enum_id
        );
        const MarimayMirEnumVariant *limit = mmay_mir_enum_variant(
            function, error, 0
        );
        const MarimayMirEnumPayload *limit_payload = mmay_mir_enum_payload(
            function, limit, 0
        );
        MarimaySymbolId error_symbol;
        if (error == NULL || limit == NULL || error_payload == NULL
            || !mmay_mir_collection_symbol(
                "CollectionGrowthError", &error_symbol
            )) return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
        if (first_payload->type != MMAY_TYPE_BOOL
            || second->kind != MMAY_MIR_ENUM_VARIANT_POSITIONAL
            || error_payload->type != error->type
            || !mmay_symbol_id_equal(error->symbol, error_symbol)
            || error->variant_count != 1
            || limit->kind != MMAY_MIR_ENUM_VARIANT_NAMED
            || limit->payload_count != 1 || limit_payload == NULL
            || limit_payload->type != MMAY_TYPE_I64
            || (instruction->kind == MMAY_MIR_COLLECTION_ADD_RESULT
                ? !collection->is_set
                : (collection->is_set
                    || third_type != collection->value_type))) {
            return MMAY_MIR_VERIFY_TYPE_MISMATCH;
        }
    }
    return MMAY_MIR_VERIFY_OK;
}

MarimayMirVerifyStatus mmay_mir_verify_collection_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayTypeId target_type,
    MarimayTypeId first_type,
    MarimayTypeId second_type,
    MarimayTypeId third_type
) {
    const MarimayMirCollection *collection = mmay_mir_collection(
        function, instruction->collection
    );
    if (collection == NULL) return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
    if (instruction->kind == MMAY_MIR_COLLECTION_NEW) {
        return target_type == collection->type
            ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (first_type == MMAY_TYPE_INVALID) {
        return MMAY_MIR_VERIFY_INVALID_REGISTER;
    }
    if (first_type != collection->type) return MMAY_MIR_VERIFY_TYPE_MISMATCH;
    if (instruction->kind == MMAY_MIR_COLLECTION_LENGTH) {
        return target_type == MMAY_TYPE_I64
            ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_CLEAR) {
        return target_type == collection->type
            ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_GENERATION) {
        return target_type == MMAY_TYPE_U64
            ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (second_type == MMAY_TYPE_INVALID) {
        return MMAY_MIR_VERIFY_INVALID_REGISTER;
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_REQUIRE_GENERATION) {
        return second_type == MMAY_TYPE_U64 && target_type == collection->type
            ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_NEXT) {
        return second_type == MMAY_TYPE_I64 && target_type == MMAY_TYPE_I64
            ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_ENTRY_KEY
        || instruction->kind == MMAY_MIR_COLLECTION_ENTRY_VALUE
        || instruction->kind == MMAY_MIR_COLLECTION_CURSOR_KEY
        || instruction->kind == MMAY_MIR_COLLECTION_CURSOR_VALUE) {
        MarimayTypeId entry_type = (instruction->kind
                == MMAY_MIR_COLLECTION_ENTRY_KEY
                || instruction->kind == MMAY_MIR_COLLECTION_CURSOR_KEY)
            ? collection->key_type : collection->value_type;
        return second_type == MMAY_TYPE_I64
                && entry_type != MMAY_TYPE_INVALID
                && target_type == entry_type
            ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (second_type != collection->key_type) {
        return MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->kind >= MMAY_MIR_COLLECTION_GET_OPTION) {
        return mmay_mir_verify_collection_result(
            function, instruction, collection, target_type, third_type
        );
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_CONTAINS
        || instruction->kind == MMAY_MIR_COLLECTION_REMOVE) {
        return target_type == MMAY_TYPE_BOOL
            ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_ADD) {
        return collection->is_set && target_type == collection->type
            ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (collection->is_set) return MMAY_MIR_VERIFY_TYPE_MISMATCH;
    if (instruction->kind == MMAY_MIR_COLLECTION_GET) {
        return target_type == collection->value_type
            ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    return third_type == collection->value_type
            && target_type == collection->type
        ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
}
