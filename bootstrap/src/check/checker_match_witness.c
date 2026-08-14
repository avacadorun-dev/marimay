#include "checker_match_witness_api.h"
#include "checker_core_api.h"






 bool ruda_checker_private_match_witness_append(
    RudaChecker *checker,
    RudaVector *buffer,
    const void *bytes,
    size_t length
) {
    if (!ruda_vector_append(buffer, bytes, length)) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

 bool ruda_checker_private_match_witness_append_text(
    RudaChecker *checker,
    RudaVector *buffer,
    const char *text
) {
    return text != NULL && ruda_checker_private_match_witness_append(
        checker,
        buffer,
        text,
        strlen(text)
    );
}

 bool ruda_checker_private_match_witness_append_name(
    RudaChecker *checker,
    RudaVector *buffer,
    RudaCheckName name
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    size_t displayed = 0;
    if (!ruda_checker_private_check_name_bytes(name, &bytes, &length)) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    displayed = length < 64 ? length : 64;
    return ruda_checker_private_match_witness_append(
        checker,
        buffer,
        bytes,
        displayed
    ) && (displayed == length || ruda_checker_private_match_witness_append_text(
        checker,
        buffer,
        "..."
    ));
}

 bool ruda_checker_private_match_witness_append_size(
    RudaChecker *checker,
    RudaVector *buffer,
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
    return ruda_checker_private_match_witness_append(
        checker,
        buffer,
        digits + cursor,
        sizeof(digits) - cursor
    );
}

 bool ruda_checker_private_match_witness_append_u64(
    RudaChecker *checker,
    RudaVector *buffer,
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
    return ruda_checker_private_match_witness_append(
        checker,
        buffer,
        digits + cursor,
        sizeof(digits) - cursor
    );
}

 bool ruda_checker_private_render_enum_match_witness(
    RudaChecker *checker,
    RudaVector *buffer,
    const RudaCheckEnumVariantDeclaration *variant
) {
    size_t payload_index = 0;
    size_t displayed_payloads = 0;
    if (variant == NULL || !ruda_checker_private_match_witness_append_name(
            checker,
            buffer,
            variant->name
        )) {
        if (variant == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    displayed_payloads = variant->payload.length < 4
        ? variant->payload.length
        : 4;
    if (variant->kind == RUDA_CHECKED_ENUM_VARIANT_UNIT) {
        return true;
    }
    if (variant->kind == RUDA_CHECKED_ENUM_VARIANT_POSITIONAL) {
        if (!ruda_checker_private_match_witness_append_text(checker, buffer, "(")) {
            return false;
        }
        for (payload_index = 0; payload_index < displayed_payloads;
            payload_index += 1) {
            if ((payload_index != 0 && !ruda_checker_private_match_witness_append_text(
                    checker,
                    buffer,
                    ", "
                )) || !ruda_checker_private_match_witness_append_text(
                    checker,
                    buffer,
                    "_"
                )) {
                return false;
            }
        }
        if (displayed_payloads != variant->payload.length
            && !ruda_checker_private_match_witness_append_text(
                checker,
                buffer,
                displayed_payloads == 0 ? "..." : ", ..."
            )) {
            return false;
        }
        return ruda_checker_private_match_witness_append_text(checker, buffer, ")");
    }
    if (variant->kind == RUDA_CHECKED_ENUM_VARIANT_NAMED) {
        if (!ruda_checker_private_match_witness_append_text(checker, buffer, " { ")) {
            return false;
        }
        for (payload_index = 0; payload_index < displayed_payloads;
            payload_index += 1) {
            const RudaCheckEnumPayloadItemDeclaration *payload =
                ruda_vector_at_const(&variant->payload, payload_index);
            if (payload == NULL || !payload->has_name
                || (payload_index != 0 && !ruda_checker_private_match_witness_append_text(
                    checker,
                    buffer,
                    ", "
                )) || !ruda_checker_private_match_witness_append_name(
                    checker,
                    buffer,
                    payload->name
                ) || !ruda_checker_private_match_witness_append_text(
                    checker,
                    buffer,
                    ": _"
                )) {
                if (payload == NULL || !payload->has_name) {
                    ruda_checker_fail(
                        checker,
                        RUDA_CHECK_INTERNAL_ERROR
                    );
                }
                return false;
            }
        }
        if (displayed_payloads != variant->payload.length
            && !ruda_checker_private_match_witness_append_text(
                checker,
                buffer,
                displayed_payloads == 0 ? "..." : ", ..."
            )) {
            return false;
        }
        return ruda_checker_private_match_witness_append_text(checker, buffer, " }");
    }
    ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
    return false;
}

 bool ruda_checker_private_add_enum_match_witness_diagnostic(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaCheckEnumDeclaration *declaration,
    const unsigned char *seen_variants,
    const RudaCheckEnumMatchArm *arms,
    size_t arm_count
) {
    RudaVector buffer;
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
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (!ruda_vector_init(
            &buffer,
            function->checker->result->allocator,
            sizeof(unsigned char)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
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
            const RudaCheckEnumMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arms[arm_index].alternatives,
                    alternative_index
                );
            if (alternative == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            guarded_witness = guarded_witness
                || alternative->is_catch_all
                || seen_variants[alternative->variant_index] == 0;
        }
    }
    if (missing_count == 0 || !ruda_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            missing_count == 1
                ? "enum match is missing witness: "
                : "enum match is missing witnesses: "
        )) {
        if (missing_count == 0) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INTERNAL_ERROR
            );
        }
        goto cleanup;
    }
    for (variant_index = 0; variant_index < declaration->variants.length
        && displayed_count < 4; variant_index += 1) {
        const RudaCheckEnumVariantDeclaration *variant = NULL;
        if (seen_variants[variant_index] != 0) {
            continue;
        }
        variant = ruda_vector_at_const(
            &declaration->variants,
            variant_index
        );
        if ((displayed_count != 0 && !ruda_checker_private_match_witness_append_text(
                function->checker,
                &buffer,
                ", "
            )) || !ruda_checker_private_render_enum_match_witness(
                function->checker,
                &buffer,
                variant
            )) {
            goto cleanup;
        }
        displayed_count += 1;
    }
    if (displayed_count != missing_count
        && (!ruda_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "; +"
        ) || !ruda_checker_private_match_witness_append_size(
            function->checker,
            &buffer,
            missing_count - displayed_count
        ) || !ruda_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            " more"
        ))) {
        goto cleanup;
    }
    if (guarded_witness && !ruda_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "; guarded patterns do not count toward exhaustiveness"
        )) {
        goto cleanup;
    }
    accepted = ruda_checker_private_checker_add_owned_diagnostic(
        function->checker,
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        buffer.data,
        buffer.length,
        node->start,
        node->end
    );

cleanup:
    if (initialized) {
        ruda_vector_dispose(&buffer);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

 bool ruda_checker_private_add_bool_match_witness_diagnostic(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const unsigned char seen[2],
    const RudaCheckScalarMatchArm *arms,
    size_t arm_count
) {
    RudaVector buffer;
    size_t missing_count = 0;
    size_t arm_index = 0;
    bool guarded_witness = false;
    bool initialized = false;
    bool accepted = false;
    memset(&buffer, 0, sizeof(buffer));
    if (node == NULL || seen == NULL
        || (arms == NULL && arm_count != 0)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (seen[0] == 0) {
        missing_count += 1;
    }
    if (seen[1] == 0) {
        missing_count += 1;
    }
    if (missing_count == 0) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (!ruda_vector_init(
            &buffer,
            function->checker->result->allocator,
            sizeof(unsigned char)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
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
            const RudaCheckScalarMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arms[arm_index].alternatives,
                    alternative_index
                );
            if (alternative == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            guarded_witness = guarded_witness
                || alternative->is_catch_all
                || seen[alternative->literal ? 1 : 0] == 0;
        }
    }
    if (!ruda_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            missing_count == 1
                ? "boolean match is missing witness: "
                : "boolean match is missing witnesses: "
        ) || (seen[0] == 0 && !ruda_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "false"
        )) || (seen[0] == 0 && seen[1] == 0
            && !ruda_checker_private_match_witness_append_text(
                function->checker,
                &buffer,
                ", "
            )) || (seen[1] == 0 && !ruda_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "true"
        )) || (guarded_witness && !ruda_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "; guarded patterns do not count toward exhaustiveness"
        ))) {
        goto cleanup;
    }
    accepted = ruda_checker_private_checker_add_owned_diagnostic(
        function->checker,
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        buffer.data,
        buffer.length,
        node->start,
        node->end
    );

cleanup:
    if (initialized) {
        ruda_vector_dispose(&buffer);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

 bool ruda_checker_private_add_i64_match_witness_diagnostic(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaMap *seen_literals,
    const RudaCheckScalarMatchArm *arms,
    size_t arm_count
) {
    RudaVector buffer;
    uint64_t candidate = 0;
    int64_t signed_candidate = 0;
    size_t arm_index = 0;
    bool guarded_witness = false;
    bool initialized = false;
    bool accepted = false;
    memset(&buffer, 0, sizeof(buffer));
    if (node == NULL || seen_literals == NULL
        || (arms == NULL && arm_count != 0)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    for (;;) {
        signed_candidate = (int64_t)candidate;
        if (ruda_map_get_const(
                seen_literals,
                &signed_candidate,
                sizeof(signed_candidate)
            ) == NULL) {
            break;
        }
        if (candidate == (uint64_t)INT64_MAX) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        candidate += 1;
    }
    if (!ruda_vector_init(
            &buffer,
            function->checker->result->allocator,
            sizeof(unsigned char)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
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
            const RudaCheckScalarMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arms[arm_index].alternatives,
                    alternative_index
                );
            if (alternative == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            guarded_witness = guarded_witness
                || alternative->is_catch_all
                || alternative->integer == signed_candidate;
        }
    }
    if (!ruda_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "integer match is missing witness: "
        ) || !ruda_checker_private_match_witness_append_u64(
            function->checker,
            &buffer,
            candidate
        ) || (guarded_witness && !ruda_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "; guarded patterns do not count toward exhaustiveness"
        ))) {
        goto cleanup;
    }
    accepted = ruda_checker_private_checker_add_owned_diagnostic(
        function->checker,
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        buffer.data,
        buffer.length,
        node->start,
        node->end
    );

cleanup:
    if (initialized) {
        ruda_vector_dispose(&buffer);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

 const RudaMirStringConstant *ruda_checker_private_check_match_string_constant(
    const RudaFunctionChecker *function,
    RudaMirStringId string_id
) {
    return function != NULL && string_id != RUDA_MIR_NO_STRING
        ? ruda_vector_at_const(&function->mir.string_constants, string_id)
        : NULL;
}

 bool ruda_checker_private_check_match_string_equal(
    const RudaFunctionChecker *function,
    RudaMirStringId left_id,
    RudaMirStringId right_id
) {
    const RudaMirStringConstant *left = ruda_checker_private_check_match_string_constant(
        function,
        left_id
    );
    const RudaMirStringConstant *right = ruda_checker_private_check_match_string_constant(
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

 bool ruda_checker_private_add_string_match_witness_diagnostic(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaMap *seen_literals,
    const RudaCheckScalarMatchArm *arms,
    size_t arm_count
) {
    RudaVector candidate;
    RudaVector buffer;
    size_t arm_index = 0;
    bool guarded_witness = false;
    bool candidate_initialized = false;
    bool buffer_initialized = false;
    bool accepted = false;
    static const char candidate_prefix[] = "ruda#";
    size_t candidate_index = 0;
    memset(&candidate, 0, sizeof(candidate));
    memset(&buffer, 0, sizeof(buffer));
    if (node == NULL || seen_literals == NULL
        || (arms == NULL && arm_count != 0)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (!ruda_vector_init(
            &candidate,
            function->checker->result->allocator,
            sizeof(unsigned char)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    candidate_initialized = true;
    if (!ruda_vector_init(
            &buffer,
            function->checker->result->allocator,
            sizeof(unsigned char)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    buffer_initialized = true;
    for (candidate_index = 0;
        candidate_index <= seen_literals->length;
        candidate_index += 1) {
        candidate.length = 0;
        if (!ruda_checker_private_match_witness_append_text(
                function->checker,
                &candidate,
                candidate_prefix
            ) || !ruda_checker_private_match_witness_append_size(
                function->checker,
                &candidate,
                candidate_index
            )) {
            goto cleanup;
        }
        if (ruda_map_get_const(
                seen_literals,
                candidate.data,
                candidate.length
            ) == NULL) {
            break;
        }
    }
    if (candidate_index > seen_literals->length) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
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
            const RudaCheckScalarMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arms[arm_index].alternatives,
                    alternative_index
                );
            const RudaMirStringConstant *literal = alternative != NULL
                ? ruda_checker_private_check_match_string_constant(
                    function,
                    alternative->string_constant
                )
                : NULL;
            if (alternative == NULL
                || (!alternative->is_catch_all && literal == NULL)) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
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
    if (!ruda_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "string match is missing witness: \""
        ) || !ruda_checker_private_match_witness_append(
            function->checker,
            &buffer,
            candidate.data,
            candidate.length
        ) || !ruda_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "\""
        ) || (guarded_witness && !ruda_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "; guarded patterns do not count toward exhaustiveness"
        ))) {
        goto cleanup;
    }
    accepted = ruda_checker_private_checker_add_owned_diagnostic(
        function->checker,
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        buffer.data,
        buffer.length,
        node->start,
        node->end
    );

cleanup:
    if (buffer_initialized) {
        ruda_vector_dispose(&buffer);
    }
    if (candidate_initialized) {
        ruda_vector_dispose(&candidate);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

 bool ruda_checker_private_add_record_match_witness_diagnostic(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaCheckRecordDeclaration *record,
    bool guarded_witness
) {
    RudaVector buffer;
    size_t field_index = 0;
    size_t displayed_fields = 0;
    bool initialized = false;
    bool accepted = false;
    memset(&buffer, 0, sizeof(buffer));
    if (node == NULL || record == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (!ruda_vector_init(
            &buffer,
            function->checker->result->allocator,
            sizeof(unsigned char)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    initialized = true;
    displayed_fields = record->fields.length < 4
        ? record->fields.length
        : 4;
    if (!ruda_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "record match is missing witness: "
        ) || !ruda_checker_private_match_witness_append_name(
            function->checker,
            &buffer,
            record->name
        ) || !ruda_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            record->fields.length == 0 ? " {}" : " { "
        )) {
        goto cleanup;
    }
    for (field_index = 0; field_index < displayed_fields;
        field_index += 1) {
        const RudaCheckRecordFieldDeclaration *field = ruda_vector_at_const(
            &record->fields,
            field_index
        );
        if (field == NULL
            || (field_index != 0 && !ruda_checker_private_match_witness_append_text(
                function->checker,
                &buffer,
                ", "
            )) || !ruda_checker_private_match_witness_append_name(
                function->checker,
                &buffer,
                field->name
            ) || !ruda_checker_private_match_witness_append_text(
                function->checker,
                &buffer,
                ": _"
            )) {
            if (field == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
            }
            goto cleanup;
        }
    }
    if ((displayed_fields != record->fields.length
            && !ruda_checker_private_match_witness_append_text(
                function->checker,
                &buffer,
                displayed_fields == 0 ? "..." : ", ..."
            )) || (record->fields.length != 0
            && !ruda_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            " }"
        )) || (guarded_witness && !ruda_checker_private_match_witness_append_text(
            function->checker,
            &buffer,
            "; guarded patterns do not count toward exhaustiveness"
        ))) {
        goto cleanup;
    }
    accepted = ruda_checker_private_checker_add_owned_diagnostic(
        function->checker,
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        buffer.data,
        buffer.length,
        node->start,
        node->end
    );

cleanup:
    if (initialized) {
        ruda_vector_dispose(&buffer);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}
