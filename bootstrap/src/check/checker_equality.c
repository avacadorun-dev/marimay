#include "checker_equality_api.h"

#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_imported_nominal_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

static bool mmay_equality_record_layout(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    bool require_derive,
    MarimayMirRecordId *layout,
    bool *supported
) {
    MarimayCheckRecordDeclaration *local =
        mmay_checker_private_checker_find_record_by_type(
            function->checker, type
        );
    const MarimayCheckedRecord *imported = local == NULL
        ? mmay_check_imported_record_by_type(function->checker, type) : NULL;
    *supported = false;
    if (local != NULL) {
        MarimayCheckRecordDeclaration concrete = *local;
        if (require_derive && !local->derives_eq
            && !local->derives_eq_hash) {
            return true;
        }
        concrete.type = type;
        if (!mmay_checker_private_function_record_layout(
                function, &concrete, layout
            )) {
            return false;
        }
        *supported = true;
        return true;
    }
    if (imported != NULL) {
        if (require_derive && !imported->derives_eq
            && !imported->derives_eq_hash) {
            return true;
        }
        if (!mmay_checker_private_imported_record_layout(
                function, imported, type, layout
            )) {
            return false;
        }
        *supported = true;
    }
    return true;
}

static bool mmay_equality_enum_layout(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    MarimayMirEnumId *layout,
    bool *supported
) {
    MarimayCheckEnumDeclaration *declaration =
        mmay_checker_private_checker_find_enum_by_type(
            function->checker, type
        );
    *supported = false;
    if (declaration != NULL) {
        MarimayCheckEnumDeclaration concrete = *declaration;
        concrete.type = type;
        if (!mmay_checker_private_function_enum_layout(
                function, &concrete, layout
            )) {
            return false;
        }
        *supported = true;
    }
    return true;
}

static bool mmay_equality_type_supported(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    MarimayVector *visiting,
    bool *supported
) {
    MarimayMirRecordId record_id = MMAY_MIR_NO_RECORD;
    MarimayMirEnumId enum_id = MMAY_MIR_NO_ENUM;
    const MarimayMirRecord *record = NULL;
    const MarimayMirEnum *enum_value = NULL;
    size_t index = 0;
    bool found = false;
    *supported = false;
    if (mmay_checker_private_type_is_integer(type)
        || type == MMAY_TYPE_BOOL || type == MMAY_TYPE_F64
        || type == MMAY_TYPE_STRING || type == MMAY_TYPE_RUNE) {
        *supported = true;
        return true;
    }
    for (index = 0; index < visiting->length; index += 1) {
        const MarimayTypeId *item = mmay_vector_at_const(visiting, index);
        if (item != NULL && *item == type) return true;
    }
    if (!mmay_vector_push(visiting, &type)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    if (!mmay_equality_record_layout(
            function, type, true, &record_id, &found
        )) goto cleanup;
    if (found) {
        record = mmay_vector_at_const(&function->mir.records, record_id);
        if (record == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        *supported = true;
        for (index = 0; index < record->field_count && *supported;
            index += 1) {
            const MarimayMirRecordField *field = mmay_vector_at_const(
                &function->mir.record_fields, record->field_start + index
            );
            if (field == NULL || !mmay_equality_type_supported(
                    function, field->type, visiting, supported
                )) {
                if (field == NULL) {
                    mmay_checker_fail(
                        function->checker, MMAY_CHECK_INTERNAL_ERROR
                    );
                }
                goto cleanup;
            }
        }
        goto cleanup;
    }
    if (!mmay_equality_enum_layout(
            function, type, &enum_id, &found
        )) goto cleanup;
    if (found) {
        enum_value = mmay_vector_at_const(&function->mir.enums, enum_id);
        if (enum_value == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        *supported = true;
        for (index = 0; index < enum_value->payload_count && *supported;
            index += 1) {
            const MarimayMirEnumPayload *payload = mmay_vector_at_const(
                &function->mir.enum_payloads,
                enum_value->payload_start + index
            );
            if (payload == NULL || !mmay_equality_type_supported(
                    function, payload->type, visiting, supported
                )) {
                if (payload == NULL) {
                    mmay_checker_fail(
                        function->checker, MMAY_CHECK_INTERNAL_ERROR
                    );
                }
                goto cleanup;
            }
        }
    }

cleanup:
    visiting->length -= 1;
    return !mmay_checker_has_failed(function->checker);
}

static bool mmay_equality_emit_branch(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    MarimayMirRegister left,
    MarimayMirRegister right,
    MarimayMirBlockId when_equal,
    MarimayMirBlockId when_different,
    size_t start,
    size_t end
);

static bool mmay_equality_emit_scalar_branch(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    MarimayMirRegister left,
    MarimayMirRegister right,
    MarimayMirBlockId when_equal,
    MarimayMirBlockId when_different,
    size_t start,
    size_t end
) {
    MarimayExpressionValue comparison;
    MarimayMirInstructionKind kind = MMAY_MIR_EQUAL_I64;
    memset(&comparison, 0, sizeof(comparison));
    if (type == MMAY_TYPE_BOOL) kind = MMAY_MIR_EQUAL_BOOL;
    else if (type == MMAY_TYPE_STRING) kind = MMAY_MIR_EQUAL_STRING;
    else if (type == MMAY_TYPE_F64) kind = MMAY_MIR_EQUAL_F64;
    if (!mmay_function_emit_instruction(
            function, kind, MMAY_TYPE_BOOL, left, right, 0, false,
            start, end, &comparison
        )) return false;
    return mmay_checker_private_function_set_branch(
        function, function->block, comparison.register_id,
        when_equal, when_different, start, end
    );
}

static bool mmay_equality_emit_record_branch(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    MarimayMirRegister left,
    MarimayMirRegister right,
    MarimayMirBlockId when_equal,
    MarimayMirBlockId when_different,
    size_t start,
    size_t end
) {
    MarimayMirRecordId layout_id = MMAY_MIR_NO_RECORD;
    const MarimayMirRecord *layout = NULL;
    size_t field_index = 0;
    bool supported = false;
    if (!mmay_equality_record_layout(
            function, type, true, &layout_id, &supported
        ) || !supported) return false;
    layout = mmay_vector_at_const(&function->mir.records, layout_id);
    if (layout == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (layout->field_count == 0) {
        return mmay_checker_private_function_set_goto(
            function, function->block, when_equal, start, end
        );
    }
    for (field_index = 0; field_index < layout->field_count;
        field_index += 1) {
        const MarimayMirRecordField *field = mmay_vector_at_const(
            &function->mir.record_fields, layout->field_start + field_index
        );
        MarimayExpressionValue left_field;
        MarimayExpressionValue right_field;
        MarimayMirBlockId next = when_equal;
        memset(&left_field, 0, sizeof(left_field));
        memset(&right_field, 0, sizeof(right_field));
        if (field == NULL
            || (field_index + 1 < layout->field_count
                && !mmay_checker_private_function_add_block(function, &next))
            || !mmay_checker_private_function_emit_record_access(
                function, MMAY_MIR_RECORD_GET, field->type, left,
                MMAY_MIR_NO_REGISTER, layout_id, field_index,
                start, end, &left_field
            ) || !mmay_checker_private_function_emit_record_access(
                function, MMAY_MIR_RECORD_GET, field->type, right,
                MMAY_MIR_NO_REGISTER, layout_id, field_index,
                start, end, &right_field
            ) || !mmay_equality_emit_branch(
                function, field->type, left_field.register_id,
                right_field.register_id, next, when_different,
                start, end
            )) {
            if (field == NULL) {
                mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
        if (next != when_equal) {
            function->block = next;
            function->terminated = false;
        }
    }
    return true;
}

static bool mmay_equality_emit_enum_branch(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    MarimayMirRegister left,
    MarimayMirRegister right,
    MarimayMirBlockId when_equal,
    MarimayMirBlockId when_different,
    size_t start,
    size_t end
) {
    MarimayMirEnumId layout_id = MMAY_MIR_NO_ENUM;
    const MarimayMirEnum *layout = NULL;
    size_t variant_index = 0;
    bool supported = false;
    if (!mmay_equality_enum_layout(
            function, type, &layout_id, &supported
        ) || !supported) return false;
    layout = mmay_vector_at_const(&function->mir.enums, layout_id);
    if (layout == NULL || layout->variant_count == 0) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (variant_index = 0; variant_index < layout->variant_count;
        variant_index += 1) {
        const MarimayMirEnumVariant *variant = mmay_vector_at_const(
            &function->mir.enum_variants,
            layout->variant_start + variant_index
        );
        MarimayExpressionValue left_matches;
        MarimayExpressionValue right_matches;
        MarimayExpressionValue guarded_left;
        MarimayExpressionValue guarded_right;
        MarimayMirBlockId selected = MMAY_MIR_NO_BLOCK;
        MarimayMirBlockId next_variant = MMAY_MIR_NO_BLOCK;
        MarimayMirBlockId payload_block = when_equal;
        size_t payload_index = 0;
        memset(&left_matches, 0, sizeof(left_matches));
        memset(&right_matches, 0, sizeof(right_matches));
        memset(&guarded_left, 0, sizeof(guarded_left));
        memset(&guarded_right, 0, sizeof(guarded_right));
        if (variant == NULL
            || !mmay_checker_private_function_add_block(function, &selected)
            || !mmay_checker_private_function_add_block(
                function, &next_variant
            ) || !mmay_checker_private_function_emit_enum_access(
                function, MMAY_MIR_ENUM_IS_VARIANT, MMAY_TYPE_BOOL,
                left, layout_id, variant_index, 0,
                start, end, &left_matches
            ) || !mmay_checker_private_function_set_branch(
                function, function->block, left_matches.register_id,
                selected, next_variant, start, end
            )) {
            if (variant == NULL) {
                mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
        function->block = selected;
        function->terminated = false;
        if (variant->payload_count != 0
            && !mmay_checker_private_function_add_block(
                function, &payload_block
            )) return false;
        if (!mmay_checker_private_function_emit_enum_access(
                function, MMAY_MIR_ENUM_IS_VARIANT, MMAY_TYPE_BOOL,
                right, layout_id, variant_index, 0,
                start, end, &right_matches
            ) || !mmay_checker_private_function_set_branch(
                function, function->block, right_matches.register_id,
                payload_block, when_different, start, end
            )) return false;
        if (variant->payload_count != 0) {
            function->block = payload_block;
            function->terminated = false;
            if (!mmay_checker_private_function_emit_enum_access(
                    function, MMAY_MIR_ENUM_REQUIRE_VARIANT, type,
                    left, layout_id, variant_index, 0,
                    start, end, &guarded_left
                ) || !mmay_checker_private_function_emit_enum_access(
                    function, MMAY_MIR_ENUM_REQUIRE_VARIANT, type,
                    right, layout_id, variant_index, 0,
                    start, end, &guarded_right
                )) return false;
        }
        for (payload_index = 0; payload_index < variant->payload_count;
            payload_index += 1) {
            const MarimayMirEnumPayload *payload = mmay_vector_at_const(
                &function->mir.enum_payloads,
                variant->payload_start + payload_index
            );
            MarimayExpressionValue left_payload;
            MarimayExpressionValue right_payload;
            MarimayMirBlockId next_payload = when_equal;
            memset(&left_payload, 0, sizeof(left_payload));
            memset(&right_payload, 0, sizeof(right_payload));
            if (payload == NULL
                || (payload_index + 1 < variant->payload_count
                    && !mmay_checker_private_function_add_block(
                        function, &next_payload
                    ))
                || !mmay_checker_private_function_emit_enum_access(
                    function, MMAY_MIR_ENUM_GET_PAYLOAD, payload->type,
                    guarded_left.register_id, layout_id,
                    variant_index, payload_index,
                    start, end, &left_payload
                ) || !mmay_checker_private_function_emit_enum_access(
                    function, MMAY_MIR_ENUM_GET_PAYLOAD, payload->type,
                    guarded_right.register_id, layout_id,
                    variant_index, payload_index,
                    start, end, &right_payload
                ) || !mmay_equality_emit_branch(
                    function, payload->type, left_payload.register_id,
                    right_payload.register_id, next_payload,
                    when_different, start, end
                )) {
                if (payload == NULL) {
                    mmay_checker_fail(
                        function->checker, MMAY_CHECK_INTERNAL_ERROR
                    );
                }
                return false;
            }
            if (next_payload != when_equal) {
                function->block = next_payload;
                function->terminated = false;
            }
        }
        function->block = next_variant;
        function->terminated = false;
    }
    return mmay_checker_private_function_set_goto(
        function, function->block, when_different, start, end
    );
}

static bool mmay_equality_emit_branch(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    MarimayMirRegister left,
    MarimayMirRegister right,
    MarimayMirBlockId when_equal,
    MarimayMirBlockId when_different,
    size_t start,
    size_t end
) {
    bool supported = false;
    MarimayMirRecordId record = MMAY_MIR_NO_RECORD;
    MarimayMirEnumId enum_id = MMAY_MIR_NO_ENUM;
    if (mmay_checker_private_type_is_integer(type)
        || type == MMAY_TYPE_BOOL || type == MMAY_TYPE_F64
        || type == MMAY_TYPE_STRING || type == MMAY_TYPE_RUNE) {
        return mmay_equality_emit_scalar_branch(
            function, type, left, right, when_equal, when_different,
            start, end
        );
    }
    if (!mmay_equality_record_layout(
            function, type, true, &record, &supported
        )) return false;
    if (supported) {
        return mmay_equality_emit_record_branch(
            function, type, left, right, when_equal, when_different,
            start, end
        );
    }
    if (!mmay_equality_enum_layout(
            function, type, &enum_id, &supported
        )) return false;
    if (supported) {
        return mmay_equality_emit_enum_branch(
            function, type, left, right, when_equal, when_different,
            start, end
        );
    }
    mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
    return false;
}

bool mmay_checker_private_compile_equality(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayExpressionValue left,
    MarimayExpressionValue right,
    bool negate,
    MarimayExpressionValue *result
) {
    MarimayVector visiting;
    MarimayMirBlockId equal_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId different_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId join_block = MMAY_MIR_NO_BLOCK;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayExpressionValue constant;
    bool supported = false;
    bool initialized = false;
    bool accepted = false;
    memset(&visiting, 0, sizeof(visiting));
    memset(&constant, 0, sizeof(constant));
    if (function == NULL || node == NULL || result == NULL
        || left.type != right.type) {
        if (function != NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    initialized = mmay_vector_init(
        &visiting, function->checker->result->allocator,
        sizeof(MarimayTypeId)
    );
    if (!initialized) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    if (!mmay_equality_type_supported(
            function, left.type, &visiting, &supported
        )) goto cleanup;
    if (!supported) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_INVALID_OPERATOR,
            "equality requires identical primitive, closed enum, Option, or @derive(Eq) record operands",
            node->start, node->end
        );
        goto cleanup;
    }
    if (!mmay_function_add_register(function, MMAY_TYPE_BOOL, &target)
        || !mmay_checker_private_function_add_block(function, &equal_block)
        || !mmay_checker_private_function_add_block(
            function, &different_block
        ) || !mmay_checker_private_function_add_block(function, &join_block)
        || !mmay_equality_emit_branch(
            function, left.type, left.register_id, right.register_id,
            equal_block, different_block, node->start, node->end
        )) goto cleanup;
    function->block = equal_block;
    function->terminated = false;
    if (!mmay_function_emit_instruction(
            function, MMAY_MIR_CONSTANT_BOOL, MMAY_TYPE_BOOL,
            MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0, !negate,
            node->start, node->end, &constant
        ) || !mmay_checker_private_function_emit_copy(
            function, target, constant.register_id, node->start, node->end
        ) || !mmay_checker_private_function_set_goto(
            function, function->block, join_block, node->start, node->end
        )) goto cleanup;
    function->block = different_block;
    function->terminated = false;
    memset(&constant, 0, sizeof(constant));
    if (!mmay_function_emit_instruction(
            function, MMAY_MIR_CONSTANT_BOOL, MMAY_TYPE_BOOL,
            MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0, negate,
            node->start, node->end, &constant
        ) || !mmay_checker_private_function_emit_copy(
            function, target, constant.register_id, node->start, node->end
        ) || !mmay_checker_private_function_set_goto(
            function, function->block, join_block, node->start, node->end
        )) goto cleanup;
    function->block = join_block;
    function->terminated = false;
    result->type = MMAY_TYPE_BOOL;
    result->register_id = target;
    result->valid = true;
    accepted = true;

cleanup:
    mmay_vector_dispose(&visiting);
    return accepted && !mmay_checker_has_failed(function->checker);
}
