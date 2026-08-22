#include "checker_match_witness_api.h"
#include "checker_core_api.h"






 bool mmay_checker_private_match_witness_append(
    MarimayChecker *checker,
    MarimayVector *buffer,
    const void *bytes,
    size_t length
) {
    if (!mmay_vector_append(buffer, bytes, length)) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

 bool mmay_checker_private_match_witness_append_text(
    MarimayChecker *checker,
    MarimayVector *buffer,
    const char *text
) {
    return text != NULL && mmay_checker_private_match_witness_append(
        checker,
        buffer,
        text,
        strlen(text)
    );
}

 bool mmay_checker_private_match_witness_append_name(
    MarimayChecker *checker,
    MarimayVector *buffer,
    MarimayCheckName name
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    size_t displayed = 0;
    if (!mmay_checker_private_check_name_bytes(name, &bytes, &length)) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    displayed = length < 64 ? length : 64;
    return mmay_checker_private_match_witness_append(
        checker,
        buffer,
        bytes,
        displayed
    ) && (displayed == length || mmay_checker_private_match_witness_append_text(
        checker,
        buffer,
        "..."
    ));
}

static bool mmay_checker_private_match_witness_append_declared_name(
    MarimayChecker *checker,
    MarimayVector *buffer,
    MarimayCheckName name,
    const MarimayOwnedBytes *owned_name
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    size_t displayed = 0;
    if (mmay_checker_private_check_name_bytes(name, &bytes, &length)) {
        return mmay_checker_private_match_witness_append_name(
            checker, buffer, name
        );
    }
    if (owned_name == NULL || owned_name->data == NULL) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    displayed = owned_name->length < 64 ? owned_name->length : 64;
    return mmay_checker_private_match_witness_append(
        checker, buffer, owned_name->data, displayed
    ) && (displayed == owned_name->length
        || mmay_checker_private_match_witness_append_text(
            checker, buffer, "..."
        ));
}

 bool mmay_checker_private_match_witness_append_size(
    MarimayChecker *checker,
    MarimayVector *buffer,
    size_t value
) {
    char digits[sizeof(size_t) * 3 + 1];
    size_t cursor = sizeof(digits);
    do {
        size_t digit = value % 10;
        cursor -= 1;
        digits[cursor] = (char)('0' + digit);
        value /= 10;
    } while (value != 0 && cursor != 0);
    return mmay_checker_private_match_witness_append(
        checker,
        buffer,
        digits + cursor,
        sizeof(digits) - cursor
    );
}

 bool mmay_checker_private_match_witness_append_u64(
    MarimayChecker *checker,
    MarimayVector *buffer,
    uint64_t value
) {
    char digits[21];
    size_t cursor = sizeof(digits);
    do {
        uint64_t digit = value % UINT64_C(10);
        cursor -= 1;
        digits[cursor] = (char)('0' + digit);
        value /= UINT64_C(10);
    } while (value != 0 && cursor != 0);
    return mmay_checker_private_match_witness_append(
        checker,
        buffer,
        digits + cursor,
        sizeof(digits) - cursor
    );
}

 bool mmay_checker_private_render_enum_match_witness(
    MarimayChecker *checker,
    MarimayVector *buffer,
    const MarimayCheckEnumVariantDeclaration *variant
) {
    size_t payload_index = 0;
    size_t displayed_payloads = 0;
    if (variant == NULL
        || !mmay_checker_private_match_witness_append_declared_name(
            checker,
            buffer,
            variant->name,
            &variant->owned_name
        )) {
        if (variant == NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    displayed_payloads = variant->payload.length < 4
        ? variant->payload.length
        : 4;
    if (variant->kind == MMAY_CHECKED_ENUM_VARIANT_UNIT) {
        return true;
    }
    if (variant->kind == MMAY_CHECKED_ENUM_VARIANT_POSITIONAL) {
        if (!mmay_checker_private_match_witness_append_text(checker, buffer, "(")) {
            return false;
        }
        for (payload_index = 0; payload_index < displayed_payloads;
            payload_index += 1) {
            if ((payload_index != 0 && !mmay_checker_private_match_witness_append_text(
                    checker,
                    buffer,
                    ", "
                )) || !mmay_checker_private_match_witness_append_text(
                    checker,
                    buffer,
                    "_"
                )) {
                return false;
            }
        }
        if (displayed_payloads != variant->payload.length
            && !mmay_checker_private_match_witness_append_text(
                checker,
                buffer,
                displayed_payloads == 0 ? "..." : ", ..."
            )) {
            return false;
        }
        return mmay_checker_private_match_witness_append_text(checker, buffer, ")");
    }
    if (variant->kind == MMAY_CHECKED_ENUM_VARIANT_NAMED) {
        if (!mmay_checker_private_match_witness_append_text(checker, buffer, " { ")) {
            return false;
        }
        for (payload_index = 0; payload_index < displayed_payloads;
            payload_index += 1) {
            const MarimayCheckEnumPayloadItemDeclaration *payload =
                mmay_vector_at_const(&variant->payload, payload_index);
            if (payload == NULL || !payload->has_name
                || (payload_index != 0 && !mmay_checker_private_match_witness_append_text(
                    checker,
                    buffer,
                    ", "
                )) || !mmay_checker_private_match_witness_append_declared_name(
                    checker,
                    buffer,
                    payload->name,
                    &payload->owned_name
                ) || !mmay_checker_private_match_witness_append_text(
                    checker,
                    buffer,
                    ": _"
                )) {
                if (payload == NULL || !payload->has_name) {
                    mmay_checker_fail(
                        checker,
                        MMAY_CHECK_INTERNAL_ERROR
                    );
                }
                return false;
            }
        }
        if (displayed_payloads != variant->payload.length
            && !mmay_checker_private_match_witness_append_text(
                checker,
                buffer,
                displayed_payloads == 0 ? "..." : ", ..."
            )) {
            return false;
        }
        return mmay_checker_private_match_witness_append_text(checker, buffer, " }");
    }
    mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
    return false;
}

 bool mmay_checker_private_add_enum_match_witness_diagnostic(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayCheckEnumDeclaration *declaration,
    const unsigned char *seen_variants,
    const MarimayCheckEnumMatchArm *arms,
    size_t arm_count
) {
    MarimayVector buffer;
    size_t variant_index = 0;
    size_t missing_count = 0;
    size_t displayed_count = 0;
    size_t arm_index = 0;
    bool guarded_witness = false;
    bool initialized = false;
    bool accepted = false;
    memset(&buffer, 0, sizeof(buffer));
    if (node == NULL || declaration == NULL
        || seen_variants == NULL
        || (arms == NULL && arm_count != 0)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (!mmay_vector_init(
            &buffer,
            function->checker->result->allocator,
            sizeof(unsigned char)
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    initialized = true;
    for (variant_index = 0; variant_index < declaration->variants.length;
        variant_index += 1) {
        if (seen_variants[variant_index] == 0) {
            missing_count += 1;
        }
    }
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        size_t alternative_index = 0;
        if (arms[arm_index].guard == NULL) {
            continue;
        }
        for (alternative_index = 0;
            alternative_index < arms[arm_index].alternatives.length;
            alternative_index += 1) {
            const MarimayCheckEnumMatchAlternative *alternative =
                mmay_vector_at_const(
                    &arms[arm_index].alternatives,
                    alternative_index
                );
            if (alternative == NULL) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            guarded_witness = guarded_witness
                || alternative->is_catch_all
                || seen_variants[alternative->variant_index] == 0;
        }
    }
    if (missing_count == 0 || !mmay_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            missing_count == 1
                ? "enum match is missing witness: "
                : "enum match is missing witnesses: "
        )) {
        if (missing_count == 0) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_INTERNAL_ERROR
            );
        }
        goto cleanup;
    }
    for (variant_index = 0; variant_index < declaration->variants.length
        && displayed_count < 4; variant_index += 1) {
        const MarimayCheckEnumVariantDeclaration *variant = NULL;
        if (seen_variants[variant_index] != 0) {
            continue;
        }
        variant = mmay_vector_at_const(
            &declaration->variants,
            variant_index
        );
        if ((displayed_count != 0 && !mmay_checker_private_match_witness_append_text(
                function->checker,
                &buffer,
                ", "
            )) || !mmay_checker_private_render_enum_match_witness(
                function->checker,
                &buffer,
                variant
            )) {
            goto cleanup;
        }
        displayed_count += 1;
    }
    if (displayed_count != missing_count
        && (!mmay_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "; +"
        ) || !mmay_checker_private_match_witness_append_size(
            function->checker,
            &buffer,
            missing_count - displayed_count
        ) || !mmay_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            " more"
        ))) {
        goto cleanup;
    }
    if (guarded_witness && !mmay_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "; guarded patterns do not count toward exhaustiveness"
        )) {
        goto cleanup;
    }
    accepted = mmay_checker_private_checker_add_owned_diagnostic(
        function->checker,
        MMAY_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        buffer.data,
        buffer.length,
        node->start,
        node->end
    );

cleanup:
    if (initialized) {
        mmay_vector_dispose(&buffer);
    }
    return accepted && !mmay_checker_has_failed(function->checker);
}

 bool mmay_checker_private_add_bool_match_witness_diagnostic(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const unsigned char seen[2],
    const MarimayCheckScalarMatchArm *arms,
    size_t arm_count
) {
    MarimayVector buffer;
    size_t missing_count = 0;
    size_t arm_index = 0;
    bool guarded_witness = false;
    bool initialized = false;
    bool accepted = false;
    memset(&buffer, 0, sizeof(buffer));
    if (node == NULL || seen == NULL
        || (arms == NULL && arm_count != 0)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (seen[0] == 0) {
        missing_count += 1;
    }
    if (seen[1] == 0) {
        missing_count += 1;
    }
    if (missing_count == 0) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (!mmay_vector_init(
            &buffer,
            function->checker->result->allocator,
            sizeof(unsigned char)
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    initialized = true;
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        size_t alternative_index = 0;
        if (arms[arm_index].guard == NULL) {
            continue;
        }
        for (alternative_index = 0;
            alternative_index < arms[arm_index].alternatives.length;
            alternative_index += 1) {
            const MarimayCheckScalarMatchAlternative *alternative =
                mmay_vector_at_const(
                    &arms[arm_index].alternatives,
                    alternative_index
                );
            if (alternative == NULL) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            guarded_witness = guarded_witness
                || alternative->is_catch_all
                || seen[alternative->literal ? 1 : 0] == 0;
        }
    }
    if (!mmay_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            missing_count == 1
                ? "boolean match is missing witness: "
                : "boolean match is missing witnesses: "
        ) || (seen[0] == 0 && !mmay_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "false"
        )) || (seen[0] == 0 && seen[1] == 0
            && !mmay_checker_private_match_witness_append_text(
                function->checker,
                &buffer,
                ", "
            )) || (seen[1] == 0 && !mmay_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "true"
        )) || (guarded_witness && !mmay_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "; guarded patterns do not count toward exhaustiveness"
        ))) {
        goto cleanup;
    }
    accepted = mmay_checker_private_checker_add_owned_diagnostic(
        function->checker,
        MMAY_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        buffer.data,
        buffer.length,
        node->start,
        node->end
    );

cleanup:
    if (initialized) {
        mmay_vector_dispose(&buffer);
    }
    return accepted && !mmay_checker_has_failed(function->checker);
}

 bool mmay_checker_private_add_i64_match_witness_diagnostic(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayMap *seen_literals,
    const MarimayCheckScalarMatchArm *arms,
    size_t arm_count
) {
    MarimayVector buffer;
    uint64_t candidate = 0;
    int64_t signed_candidate = 0;
    size_t arm_index = 0;
    bool guarded_witness = false;
    bool initialized = false;
    bool accepted = false;
    memset(&buffer, 0, sizeof(buffer));
    if (node == NULL || seen_literals == NULL
        || (arms == NULL && arm_count != 0)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    for (;;) {
        signed_candidate = (int64_t)candidate;
        if (mmay_map_get_const(
                seen_literals,
                &signed_candidate,
                sizeof(signed_candidate)
            ) == NULL) {
            break;
        }
        if (candidate == (uint64_t)INT64_MAX) {
            mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        candidate += 1;
    }
    if (!mmay_vector_init(
            &buffer,
            function->checker->result->allocator,
            sizeof(unsigned char)
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    initialized = true;
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        size_t alternative_index = 0;
        if (arms[arm_index].guard == NULL) {
            continue;
        }
        for (alternative_index = 0;
            alternative_index < arms[arm_index].alternatives.length;
            alternative_index += 1) {
            const MarimayCheckScalarMatchAlternative *alternative =
                mmay_vector_at_const(
                    &arms[arm_index].alternatives,
                    alternative_index
                );
            if (alternative == NULL) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            guarded_witness = guarded_witness
                || alternative->is_catch_all
                || alternative->integer == signed_candidate;
        }
    }
    if (!mmay_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "integer match is missing witness: "
        ) || !mmay_checker_private_match_witness_append_u64(
            function->checker,
            &buffer,
            candidate
        ) || (guarded_witness && !mmay_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "; guarded patterns do not count toward exhaustiveness"
        ))) {
        goto cleanup;
    }
    accepted = mmay_checker_private_checker_add_owned_diagnostic(
        function->checker,
        MMAY_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        buffer.data,
        buffer.length,
        node->start,
        node->end
    );

cleanup:
    if (initialized) {
        mmay_vector_dispose(&buffer);
    }
    return accepted && !mmay_checker_has_failed(function->checker);
}

 const MarimayMirStringConstant *mmay_checker_private_check_match_string_constant(
    const MarimayFunctionChecker *function,
    MarimayMirStringId string_id
) {
    return function != NULL && string_id != MMAY_MIR_NO_STRING
        ? mmay_vector_at_const(&function->mir.string_constants, string_id)
        : NULL;
}

 bool mmay_checker_private_check_match_string_equal(
    const MarimayFunctionChecker *function,
    MarimayMirStringId left_id,
    MarimayMirStringId right_id
) {
    const MarimayMirStringConstant *left = mmay_checker_private_check_match_string_constant(
        function,
        left_id
    );
    const MarimayMirStringConstant *right = mmay_checker_private_check_match_string_constant(
        function,
        right_id
    );
    return left != NULL && right != NULL
        && left->value.length == right->value.length
        && (left->value.length == 0 || memcmp(
            left->value.data,
            right->value.data,
            left->value.length
        ) == 0);
}

 bool mmay_checker_private_add_string_match_witness_diagnostic(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayMap *seen_literals,
    const MarimayCheckScalarMatchArm *arms,
    size_t arm_count
) {
    MarimayVector candidate;
    MarimayVector buffer;
    size_t arm_index = 0;
    bool guarded_witness = false;
    bool candidate_initialized = false;
    bool buffer_initialized = false;
    bool accepted = false;
    static const char candidate_prefix[] = "mmay#";
    size_t candidate_index = 0;
    memset(&candidate, 0, sizeof(candidate));
    memset(&buffer, 0, sizeof(buffer));
    if (node == NULL || seen_literals == NULL
        || (arms == NULL && arm_count != 0)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (!mmay_vector_init(
            &candidate,
            function->checker->result->allocator,
            sizeof(unsigned char)
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    candidate_initialized = true;
    if (!mmay_vector_init(
            &buffer,
            function->checker->result->allocator,
            sizeof(unsigned char)
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    buffer_initialized = true;
    for (candidate_index = 0;
        candidate_index <= seen_literals->length;
        candidate_index += 1) {
        candidate.length = 0;
        if (!mmay_checker_private_match_witness_append_text(
                function->checker,
                &candidate,
                candidate_prefix
            ) || !mmay_checker_private_match_witness_append_size(
                function->checker,
                &candidate,
                candidate_index
            )) {
            goto cleanup;
        }
        if (mmay_map_get_const(
                seen_literals,
                candidate.data,
                candidate.length
            ) == NULL) {
            break;
        }
    }
    if (candidate_index > seen_literals->length) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        size_t alternative_index = 0;
        if (arms[arm_index].guard == NULL) {
            continue;
        }
        for (alternative_index = 0;
            alternative_index < arms[arm_index].alternatives.length;
            alternative_index += 1) {
            const MarimayCheckScalarMatchAlternative *alternative =
                mmay_vector_at_const(
                    &arms[arm_index].alternatives,
                    alternative_index
                );
            const MarimayMirStringConstant *literal = alternative != NULL
                ? mmay_checker_private_check_match_string_constant(
                    function,
                    alternative->string_constant
                )
                : NULL;
            if (alternative == NULL
                || (!alternative->is_catch_all && literal == NULL)) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            guarded_witness = guarded_witness
                || alternative->is_catch_all
                || (literal->value.length == candidate.length
                    && (candidate.length == 0 || memcmp(
                        literal->value.data,
                        candidate.data,
                        candidate.length
                    ) == 0));
        }
    }
    if (!mmay_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "string match is missing witness: \""
        ) || !mmay_checker_private_match_witness_append(
            function->checker,
            &buffer,
            candidate.data,
            candidate.length
        ) || !mmay_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "\""
        ) || (guarded_witness && !mmay_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "; guarded patterns do not count toward exhaustiveness"
        ))) {
        goto cleanup;
    }
    accepted = mmay_checker_private_checker_add_owned_diagnostic(
        function->checker,
        MMAY_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        buffer.data,
        buffer.length,
        node->start,
        node->end
    );

cleanup:
    if (buffer_initialized) {
        mmay_vector_dispose(&buffer);
    }
    if (candidate_initialized) {
        mmay_vector_dispose(&candidate);
    }
    return accepted && !mmay_checker_has_failed(function->checker);
}

 bool mmay_checker_private_add_record_match_witness_diagnostic(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayCheckRecordDeclaration *record,
    bool guarded_witness
) {
    MarimayVector buffer;
    size_t field_index = 0;
    size_t displayed_fields = 0;
    bool initialized = false;
    bool accepted = false;
    memset(&buffer, 0, sizeof(buffer));
    if (node == NULL || record == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (!mmay_vector_init(
            &buffer,
            function->checker->result->allocator,
            sizeof(unsigned char)
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    initialized = true;
    displayed_fields = record->fields.length < 4
        ? record->fields.length
        : 4;
    if (!mmay_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "record match is missing witness: "
        ) || !mmay_checker_private_match_witness_append_name(
            function->checker,
            &buffer,
            record->name
        ) || !mmay_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            record->fields.length == 0 ? " {}" : " { "
        )) {
        goto cleanup;
    }
    for (field_index = 0; field_index < displayed_fields;
        field_index += 1) {
        const MarimayCheckRecordFieldDeclaration *field = mmay_vector_at_const(
            &record->fields,
            field_index
        );
        if (field == NULL
            || (field_index != 0 && !mmay_checker_private_match_witness_append_text(
                function->checker,
                &buffer,
                ", "
            )) || !mmay_checker_private_match_witness_append_name(
                function->checker,
                &buffer,
                field->name
            ) || !mmay_checker_private_match_witness_append_text(
                function->checker,
                &buffer,
                ": _"
            )) {
            if (field == NULL) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
            }
            goto cleanup;
        }
    }
    if ((displayed_fields != record->fields.length
            && !mmay_checker_private_match_witness_append_text(
                function->checker,
                &buffer,
                displayed_fields == 0 ? "..." : ", ..."
            )) || (record->fields.length != 0
            && !mmay_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            " }"
        )) || (guarded_witness && !mmay_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "; guarded patterns do not count toward exhaustiveness"
        ))) {
        goto cleanup;
    }
    accepted = mmay_checker_private_checker_add_owned_diagnostic(
        function->checker,
        MMAY_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        buffer.data,
        buffer.length,
        node->start,
        node->end
    );

cleanup:
    if (initialized) {
        mmay_vector_dispose(&buffer);
    }
    return accepted && !mmay_checker_has_failed(function->checker);
}
