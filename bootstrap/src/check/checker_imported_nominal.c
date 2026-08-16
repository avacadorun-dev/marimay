#include "checker_imported_nominal_api.h"

#include "checker_core_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

bool ruda_checker_private_imported_record_layout(
    RudaFunctionChecker *function,
    const RudaCheckedRecord *record,
    RudaMirRecordId *layout
) {
    RudaMirRecordField *fields = NULL;
    size_t bytes = 0;
    size_t index = 0;
    if (function == NULL || record == NULL || layout == NULL
        || !ruda_size_multiply(
            record->fields.length, sizeof(*fields), &bytes
        )) {
        if (function != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        }
        return false;
    }
    fields = bytes == 0 ? NULL : ruda_allocate(
        &function->checker->result->allocator, bytes
    );
    if (bytes != 0 && fields == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < record->fields.length; index += 1) {
        const RudaCheckedRecordField *field = ruda_vector_at_const(
            &record->fields, index
        );
        if (field == NULL || fields == NULL) {
            ruda_deallocate(&function->checker->result->allocator,
                fields, bytes);
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        fields[index].type = field->type;
        fields[index].is_mutable = field->is_mutable;
    }
    if (!ruda_mir_add_record(
            &function->mir, record->type, fields, record->fields.length, layout
        )) {
        ruda_deallocate(&function->checker->result->allocator, fields, bytes);
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    ruda_deallocate(&function->checker->result->allocator, fields, bytes);
    return true;
}

bool ruda_checker_private_imported_enum_layout(
    RudaFunctionChecker *function,
    const RudaCheckedEnum *enum_value,
    RudaMirEnumId *layout
) {
    RudaMirEnumVariant *variants = NULL;
    RudaMirEnumPayload *payloads = NULL;
    size_t payload_count = 0;
    size_t payload_cursor = 0;
    size_t variant_bytes = 0;
    size_t payload_bytes = 0;
    size_t variant_index = 0;
    bool accepted = false;
    if (function == NULL || enum_value == NULL || layout == NULL) {
        return false;
    }
    for (variant_index = 0; variant_index < enum_value->variants.length;
        variant_index += 1) {
        const RudaCheckedEnumVariant *variant = ruda_vector_at_const(
            &enum_value->variants, variant_index
        );
        if (variant == NULL || !ruda_size_add(
                payload_count, variant->payload.length, &payload_count
            )) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            return false;
        }
    }
    if (!ruda_size_multiply(enum_value->variants.length, sizeof(*variants),
            &variant_bytes)
        || !ruda_size_multiply(payload_count, sizeof(*payloads),
            &payload_bytes)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    variants = variant_bytes == 0 ? NULL : ruda_allocate(
        &function->checker->result->allocator, variant_bytes
    );
    payloads = payload_bytes == 0 ? NULL : ruda_allocate(
        &function->checker->result->allocator, payload_bytes
    );
    if ((variant_bytes != 0 && variants == NULL)
        || (payload_bytes != 0 && payloads == NULL)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    for (variant_index = 0; variant_index < enum_value->variants.length;
        variant_index += 1) {
        const RudaCheckedEnumVariant *variant = ruda_vector_at_const(
            &enum_value->variants, variant_index
        );
        size_t item_index = 0;
        if (variant == NULL || variants == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        switch (variant->kind) {
            case RUDA_CHECKED_ENUM_VARIANT_UNIT:
                variants[variant_index].kind = RUDA_MIR_ENUM_VARIANT_UNIT;
                break;
            case RUDA_CHECKED_ENUM_VARIANT_POSITIONAL:
                variants[variant_index].kind =
                    RUDA_MIR_ENUM_VARIANT_POSITIONAL;
                break;
            case RUDA_CHECKED_ENUM_VARIANT_NAMED:
                variants[variant_index].kind = RUDA_MIR_ENUM_VARIANT_NAMED;
                break;
            default:
                ruda_checker_fail(function->checker,
                    RUDA_CHECK_INTERNAL_ERROR);
                goto cleanup;
        }
        variants[variant_index].payload_start = payload_cursor;
        variants[variant_index].payload_count = variant->payload.length;
        for (item_index = 0; item_index < variant->payload.length;
            item_index += 1) {
            const RudaCheckedEnumPayloadItem *item = ruda_vector_at_const(
                &variant->payload, item_index
            );
            if (item == NULL || payloads == NULL) {
                ruda_checker_fail(function->checker,
                    RUDA_CHECK_INTERNAL_ERROR);
                goto cleanup;
            }
            payloads[payload_cursor].type = item->type;
            payload_cursor += 1;
        }
    }
    if (!ruda_mir_add_enum(&function->mir, enum_value->type, variants,
            enum_value->variants.length, payloads, payload_count, layout)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    accepted = true;
cleanup:
    ruda_deallocate(&function->checker->result->allocator, payloads,
        payload_bytes);
    ruda_deallocate(&function->checker->result->allocator, variants,
        variant_bytes);
    return accepted;
}

static bool ruda_checker_private_function_ensure_nominal_type(
    RudaFunctionChecker *function,
    RudaTypeId type
) {
    RudaCheckRecordDeclaration *record =
        ruda_checker_private_checker_find_record_by_type(
            function->checker, type
        );
    RudaCheckEnumDeclaration *enum_value = record == NULL
        ? ruda_checker_private_checker_find_enum_by_type(
            function->checker, type
        ) : NULL;
    const RudaCheckedRecord *imported_record = record == NULL
            && enum_value == NULL
        ? ruda_check_imported_record_by_type(function->checker, type) : NULL;
    const RudaCheckedEnum *imported_enum = record == NULL
            && enum_value == NULL && imported_record == NULL
        ? ruda_check_imported_enum_by_type(function->checker, type) : NULL;
    if (record != NULL) {
        RudaMirRecordId layout = RUDA_MIR_NO_RECORD;
        return ruda_checker_private_function_record_layout(
            function, record, &layout
        );
    }
    if (enum_value != NULL) {
        RudaMirEnumId layout = RUDA_MIR_NO_ENUM;
        return ruda_checker_private_function_enum_layout(
            function, enum_value, &layout
        );
    }
    if (imported_record != NULL) {
        RudaMirRecordId layout = RUDA_MIR_NO_RECORD;
        return ruda_checker_private_imported_record_layout(
            function, imported_record, &layout
        );
    }
    if (imported_enum != NULL) {
        RudaMirEnumId layout = RUDA_MIR_NO_ENUM;
        return ruda_checker_private_imported_enum_layout(
            function, imported_enum, &layout
        );
    }
    return true;
}

bool ruda_checker_private_function_ensure_nominal_layouts(
    RudaFunctionChecker *function
) {
    size_t index = 0;
    if (function == NULL || !function->mir.initialized
        || !ruda_checker_private_function_ensure_nominal_type(
            function, function->mir.result_type
        )) {
        return false;
    }
    for (index = 0; index < function->mir.register_types.length; index += 1) {
        const RudaTypeId *type = ruda_vector_at_const(
            &function->mir.register_types, index
        );
        if (type == NULL || !ruda_checker_private_function_ensure_nominal_type(
                function, *type
            )) {
            if (type == NULL) {
                ruda_checker_fail(function->checker,
                    RUDA_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    return true;
}
