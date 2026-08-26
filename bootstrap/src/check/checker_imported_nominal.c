#include "checker_imported_nominal_api.h"

#include "checker_core_api.h"
#include "checker_generic_types_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

bool mmay_checker_private_imported_record_layout(
    MarimayFunctionChecker *function,
    const MarimayCheckedRecord *record,
    MarimayTypeId type,
    MarimayMirRecordId *layout
) {
    MarimayMirRecordField *fields = NULL;
    size_t bytes = 0;
    size_t index = 0;
    if (function == NULL || record == NULL || layout == NULL
        || !mmay_size_multiply(
            record->fields.length, sizeof(*fields), &bytes
        )) {
        if (function != NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        }
        return false;
    }
    fields = bytes == 0 ? NULL : mmay_allocate(
        &function->checker->result->allocator, bytes
    );
    if (bytes != 0 && fields == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < record->fields.length; index += 1) {
        const MarimayCheckedRecordField *field = mmay_vector_at_const(
            &record->fields, index
        );
        if (field == NULL || fields == NULL) {
            mmay_deallocate(&function->checker->result->allocator,
                fields, bytes);
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        if (!mmay_checker_private_substitute_nominal_member_type(
                function,
                type,
                &record->type_parameters,
                field->type,
                &fields[index].type
            )) {
            mmay_deallocate(&function->checker->result->allocator,
                fields, bytes);
            return false;
        }
        fields[index].is_mutable = field->is_mutable;
    }
    if (!mmay_mir_add_record(
            &function->mir, type, fields, record->fields.length, layout
        )) {
        mmay_deallocate(&function->checker->result->allocator, fields, bytes);
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    mmay_deallocate(&function->checker->result->allocator, fields, bytes);
    return true;
}

bool mmay_checker_private_imported_enum_layout(
    MarimayFunctionChecker *function,
    const MarimayCheckedEnum *enum_value,
    MarimayTypeId type,
    MarimayMirEnumId *layout
) {
    MarimayMirEnumVariant *variants = NULL;
    MarimayMirEnumPayload *payloads = NULL;
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
        const MarimayCheckedEnumVariant *variant = mmay_vector_at_const(
            &enum_value->variants, variant_index
        );
        if (variant == NULL || !mmay_size_add(
                payload_count, variant->payload.length, &payload_count
            )) {
            mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
            return false;
        }
    }
    if (!mmay_size_multiply(enum_value->variants.length, sizeof(*variants),
            &variant_bytes)
        || !mmay_size_multiply(payload_count, sizeof(*payloads),
            &payload_bytes)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    variants = variant_bytes == 0 ? NULL : mmay_allocate(
        &function->checker->result->allocator, variant_bytes
    );
    payloads = payload_bytes == 0 ? NULL : mmay_allocate(
        &function->checker->result->allocator, payload_bytes
    );
    if ((variant_bytes != 0 && variants == NULL)
        || (payload_bytes != 0 && payloads == NULL)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    for (variant_index = 0; variant_index < enum_value->variants.length;
        variant_index += 1) {
        const MarimayCheckedEnumVariant *variant = mmay_vector_at_const(
            &enum_value->variants, variant_index
        );
        size_t item_index = 0;
        if (variant == NULL || variants == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        switch (variant->kind) {
            case MMAY_CHECKED_ENUM_VARIANT_UNIT:
                variants[variant_index].kind = MMAY_MIR_ENUM_VARIANT_UNIT;
                break;
            case MMAY_CHECKED_ENUM_VARIANT_POSITIONAL:
                variants[variant_index].kind =
                    MMAY_MIR_ENUM_VARIANT_POSITIONAL;
                break;
            case MMAY_CHECKED_ENUM_VARIANT_NAMED:
                variants[variant_index].kind = MMAY_MIR_ENUM_VARIANT_NAMED;
                break;
            default:
                mmay_checker_fail(function->checker,
                    MMAY_CHECK_INTERNAL_ERROR);
                goto cleanup;
        }
        variants[variant_index].payload_start = payload_cursor;
        variants[variant_index].payload_count = variant->payload.length;
        for (item_index = 0; item_index < variant->payload.length;
            item_index += 1) {
            const MarimayCheckedEnumPayloadItem *item = mmay_vector_at_const(
                &variant->payload, item_index
            );
            if (item == NULL || payloads == NULL) {
                mmay_checker_fail(function->checker,
                    MMAY_CHECK_INTERNAL_ERROR);
                goto cleanup;
            }
            if (!mmay_checker_private_substitute_nominal_member_type(
                    function,
                    type,
                    &enum_value->type_parameters,
                    item->type,
                    &payloads[payload_cursor].type
                )) goto cleanup;
            payload_cursor += 1;
        }
    }
    if (!mmay_mir_add_enum_with_symbol(
            &function->mir, enum_value->symbol, type, variants,
            enum_value->variants.length, payloads, payload_count, layout
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    accepted = true;
cleanup:
    mmay_deallocate(&function->checker->result->allocator, payloads,
        payload_bytes);
    mmay_deallocate(&function->checker->result->allocator, variants,
        variant_bytes);
    return accepted;
}

static bool mmay_checker_private_layout_is_visiting(
    const MarimayVector *visiting,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; visiting != NULL && index < visiting->length; index += 1) {
        const MarimayTypeId *item = mmay_vector_at_const(visiting, index);
        if (item != NULL && *item == type) return true;
    }
    return false;
}

static const MarimayCheckedRecord *mmay_checker_private_dependency_record(
    const MarimayFunctionChecker *function,
    MarimayTypeId type
) {
    size_t index = 0;
    const MarimayCheckResult *result = function != NULL
        ? function->checker->result : NULL;
    for (index = 0; result != NULL && index < result->records.length;
        index += 1) {
        const MarimayCheckedRecord *record = mmay_checked_record(result, index);
        if (record != NULL && record->is_public
            && mmay_checker_private_type_matches_nominal(
                function->checker,
                type,
                record->symbol,
                record->type_parameters.length
            )) {
            return record;
        }
    }
    return NULL;
}

static const MarimayCheckedEnum *mmay_checker_private_dependency_enum(
    const MarimayFunctionChecker *function,
    MarimayTypeId type
) {
    size_t index = 0;
    const MarimayCheckResult *result = function != NULL
        ? function->checker->result : NULL;
    for (index = 0; result != NULL && index < result->enums.length;
        index += 1) {
        const MarimayCheckedEnum *enum_value = mmay_checked_enum(result, index);
        if (enum_value != NULL && enum_value->is_public
            && mmay_checker_private_type_matches_nominal(
                function->checker,
                type,
                enum_value->symbol,
                enum_value->type_parameters.length
            )) {
            return enum_value;
        }
    }
    return NULL;
}

static bool mmay_checker_private_ensure_layout_closure(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    MarimayVector *visiting
) {
    const MarimayTypeDescriptor *descriptor = mmay_type_descriptor(
        &function->checker->result->types, type
    );
    MarimayCheckRecordDeclaration *record =
        mmay_checker_private_checker_find_record_by_type(
            function->checker, type
        );
    MarimayCheckEnumDeclaration *enum_value = record == NULL
        ? mmay_checker_private_checker_find_enum_by_type(
            function->checker, type
        ) : NULL;
    const MarimayCheckedRecord *imported_record = record == NULL
            && enum_value == NULL
        ? mmay_check_imported_record_by_type(function->checker, type) : NULL;
    const MarimayCheckedEnum *imported_enum = record == NULL
            && enum_value == NULL && imported_record == NULL
        ? mmay_check_imported_enum_by_type(function->checker, type) : NULL;
    if (record == NULL && enum_value == NULL && imported_record == NULL) {
        imported_record = mmay_checker_private_dependency_record(
            function, type
        );
    }
    if (record == NULL && enum_value == NULL && imported_record == NULL
        && imported_enum == NULL) {
        imported_enum = mmay_checker_private_dependency_enum(function, type);
    }
    bool accepted = true;
    if (type < MMAY_TYPE_FIRST_DYNAMIC) return true;
    if (descriptor == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (mmay_checker_private_layout_is_visiting(visiting, type)) return true;
    if (!mmay_vector_push(visiting, &type)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    if (descriptor->kind == MMAY_TYPE_KIND_ARRAY) {
        MarimayMirArrayId layout = MMAY_MIR_NO_ARRAY;
        if (descriptor->argument_count != 1
            || !mmay_array_layout(function, type, &layout)
            || !mmay_checker_private_ensure_layout_closure(
                function, descriptor->arguments[0], visiting
            )) accepted = false;
        goto cleanup;
    }
    if (record != NULL) {
        MarimayMirRecordId layout = MMAY_MIR_NO_RECORD;
        MarimayCheckRecordDeclaration concrete = *record;
        const MarimayMirRecord *mir_record = NULL;
        size_t field_index = 0;
        if (record->is_generic) concrete.type = type;
        if (!mmay_checker_private_function_record_layout(
                function, &concrete, &layout
            )) accepted = false;
        mir_record = accepted
            ? mmay_vector_at_const(&function->mir.records, layout) : NULL;
        for (field_index = 0; accepted && mir_record != NULL
            && field_index < mir_record->field_count; field_index += 1) {
            const MarimayMirRecordField *field = mmay_vector_at_const(
                &function->mir.record_fields,
                mir_record->field_start + field_index
            );
            if (field == NULL || !mmay_checker_private_ensure_layout_closure(
                    function, field->type, visiting
                )) accepted = false;
        }
    } else if (enum_value != NULL) {
        MarimayMirEnumId layout = MMAY_MIR_NO_ENUM;
        MarimayCheckEnumDeclaration concrete = *enum_value;
        const MarimayMirEnum *mir_enum = NULL;
        size_t payload_index = 0;
        if (enum_value->is_generic) {
            concrete.type = type;
        }
        if (!mmay_checker_private_function_enum_layout(
                function, &concrete, &layout
            )) accepted = false;
        mir_enum = accepted
            ? mmay_vector_at_const(&function->mir.enums, layout) : NULL;
        for (payload_index = 0; accepted && mir_enum != NULL
            && payload_index < mir_enum->payload_count; payload_index += 1) {
            const MarimayMirEnumPayload *payload = mmay_vector_at_const(
                &function->mir.enum_payloads,
                mir_enum->payload_start + payload_index
            );
            if (payload == NULL || !mmay_checker_private_ensure_layout_closure(
                    function, payload->type, visiting
                )) accepted = false;
        }
    } else if (imported_record != NULL) {
        MarimayMirRecordId layout = MMAY_MIR_NO_RECORD;
        const MarimayMirRecord *mir_record = NULL;
        size_t field_index = 0;
        if (!mmay_checker_private_imported_record_layout(
                function, imported_record, type, &layout
            )) accepted = false;
        mir_record = accepted
            ? mmay_vector_at_const(&function->mir.records, layout) : NULL;
        for (field_index = 0; accepted && mir_record != NULL
            && field_index < mir_record->field_count; field_index += 1) {
            const MarimayMirRecordField *field = mmay_vector_at_const(
                &function->mir.record_fields,
                mir_record->field_start + field_index
            );
            if (field == NULL || !mmay_checker_private_ensure_layout_closure(
                    function, field->type, visiting
                )) accepted = false;
        }
    } else if (imported_enum != NULL) {
        MarimayMirEnumId layout = MMAY_MIR_NO_ENUM;
        const MarimayMirEnum *mir_enum = NULL;
        size_t payload_index = 0;
        if (!mmay_checker_private_imported_enum_layout(
                function, imported_enum, type, &layout
            )) accepted = false;
        mir_enum = accepted
            ? mmay_vector_at_const(&function->mir.enums, layout) : NULL;
        for (payload_index = 0; accepted && mir_enum != NULL
            && payload_index < mir_enum->payload_count; payload_index += 1) {
            const MarimayMirEnumPayload *payload = mmay_vector_at_const(
                &function->mir.enum_payloads,
                mir_enum->payload_start + payload_index
            );
            if (payload == NULL || !mmay_checker_private_ensure_layout_closure(
                    function, payload->type, visiting
                )) accepted = false;
        }
    }
cleanup:
    visiting->length -= 1;
    return accepted;
}

bool mmay_checker_private_function_ensure_nominal_layouts(
    MarimayFunctionChecker *function
) {
    MarimayVector visiting;
    size_t index = 0;
    bool accepted = false;
    memset(&visiting, 0, sizeof(visiting));
    if (function == NULL || !function->mir.initialized) {
        return false;
    }
    if (!mmay_vector_init(
            &visiting,
            function->checker->result->allocator,
            sizeof(MarimayTypeId)
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    if (!mmay_checker_private_ensure_layout_closure(
            function, function->mir.result_type, &visiting
        )) goto cleanup;
    for (index = 0; index < function->mir.register_types.length; index += 1) {
        const MarimayTypeId *type = mmay_vector_at_const(
            &function->mir.register_types, index
        );
        if (type == NULL || !mmay_checker_private_ensure_layout_closure(
                function, *type, &visiting
            )) {
            if (type == NULL) {
                mmay_checker_fail(function->checker,
                    MMAY_CHECK_INTERNAL_ERROR);
            }
            goto cleanup;
        }
    }
    accepted = true;
cleanup:
    mmay_vector_dispose(&visiting);
    return accepted;
}
