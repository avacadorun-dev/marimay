#include "checker_declaration_publication_nominal_api.h"





#include "checker_internal.h"

 bool mmay_checker_private_publish_records(MarimayChecker *checker) {
    size_t record_index = 0;
    if (!mmay_vector_reserve(
            &checker->result->records,
            checker->records.length
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (record_index = 0; record_index < checker->records.length;
        record_index += 1) {
        MarimayCheckRecordDeclaration *declaration = mmay_vector_at(
            &checker->records,
            record_index
        );
        MarimayCheckedRecord record;
        size_t field_index = 0;
        memset(&record, 0, sizeof(record));
        if (declaration == NULL || !declaration->declaration_valid
            || !declaration->fields_resolved) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        if (!mmay_vector_init(
                &record.type_parameters,
                checker->result->allocator,
                sizeof(MarimayTypeId)
            ) || !mmay_vector_reserve(
                &record.type_parameters,
                declaration->type_parameters.length
            ) || !mmay_vector_init(
                &record.fields,
                checker->result->allocator,
                sizeof(MarimayCheckedRecordField)
            ) || !mmay_vector_reserve(
                &record.fields,
                declaration->fields.length
            )) {
            mmay_checked_record_value_dispose(&record);
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
        record.name = declaration->owned_name;
        memset(&declaration->owned_name, 0, sizeof(declaration->owned_name));
        declaration->name_initialized = false;
        record.symbol = declaration->symbol;
        record.type = declaration->type;
        record.is_public = declaration->is_public;
        record.derives_debug = declaration->derives_debug;
        record.derives_eq = declaration->derives_eq;
        record.derives_eq_hash = declaration->derives_eq_hash;
        for (field_index = 0;
            field_index < declaration->type_parameters.length;
            field_index += 1) {
            const MarimayCheckTypeParameterDeclaration *parameter =
                mmay_vector_at_const(
                    &declaration->type_parameters, field_index
                );
            if (parameter == NULL || !mmay_vector_push(
                    &record.type_parameters, &parameter->type
                )) {
                mmay_checked_record_value_dispose(&record);
                mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
                return false;
            }
        }
        for (field_index = 0; field_index < declaration->fields.length;
            field_index += 1) {
            MarimayCheckRecordFieldDeclaration *source = mmay_vector_at(
                &declaration->fields,
                field_index
            );
            MarimayCheckedRecordField field;
            if (source == NULL || !source->name_initialized
                || source->type == MMAY_TYPE_INVALID) {
                mmay_checked_record_value_dispose(&record);
                mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
                return false;
            }
            memset(&field, 0, sizeof(field));
            field.name = source->owned_name;
            memset(&source->owned_name, 0, sizeof(source->owned_name));
            source->name_initialized = false;
            field.type = source->type;
            field.is_public = source->is_public;
            field.is_mutable = source->is_mutable;
            if (!mmay_vector_push(&record.fields, &field)) {
                mmay_owned_bytes_dispose(&field.name);
                mmay_checked_record_value_dispose(&record);
                mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
                return false;
            }
        }
        if (!mmay_vector_push(&checker->result->records, &record)) {
            mmay_checked_record_value_dispose(&record);
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
        memset(&record, 0, sizeof(record));
    }
    return true;
}

 bool mmay_checker_private_publish_enums(MarimayChecker *checker) {
    size_t enum_index = 0;
    if (!mmay_vector_reserve(
            &checker->result->enums,
            checker->enums.length
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (enum_index = 0; enum_index < checker->enums.length;
        enum_index += 1) {
        MarimayCheckEnumDeclaration *declaration = mmay_vector_at(
            &checker->enums,
            enum_index
        );
        MarimayCheckedEnum enum_value;
        size_t variant_index = 0;
        memset(&enum_value, 0, sizeof(enum_value));
        if (declaration == NULL || !declaration->declaration_valid
            || !declaration->variants_resolved) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        if (!mmay_vector_init(
                &enum_value.type_parameters,
                checker->result->allocator,
                sizeof(MarimayTypeId)
            ) || !mmay_vector_reserve(
                &enum_value.type_parameters,
                declaration->type_parameters.length
            ) || !mmay_vector_init(
                &enum_value.variants,
                checker->result->allocator,
                sizeof(MarimayCheckedEnumVariant)
            ) || !mmay_vector_reserve(
                &enum_value.variants,
                declaration->variants.length
            )) {
            mmay_checked_enum_value_dispose(&enum_value);
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
        enum_value.name = declaration->owned_name;
        memset(&declaration->owned_name, 0, sizeof(declaration->owned_name));
        declaration->name_initialized = false;
        enum_value.symbol = declaration->symbol;
        enum_value.type = declaration->type;
        enum_value.is_public = declaration->is_public;
        enum_value.derives_debug = declaration->derives_debug;
        enum_value.derives_eq = declaration->derives_eq;
        enum_value.derives_eq_hash = declaration->derives_eq_hash;
        for (variant_index = 0;
            variant_index < declaration->type_parameters.length;
            variant_index += 1) {
            const MarimayCheckTypeParameterDeclaration *parameter =
                mmay_vector_at_const(
                    &declaration->type_parameters, variant_index
                );
            if (parameter == NULL || !mmay_vector_push(
                    &enum_value.type_parameters, &parameter->type
                )) {
                mmay_checked_enum_value_dispose(&enum_value);
                mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
                return false;
            }
        }
        for (variant_index = 0; variant_index < declaration->variants.length;
            variant_index += 1) {
            MarimayCheckEnumVariantDeclaration *source_variant = mmay_vector_at(
                &declaration->variants,
                variant_index
            );
            MarimayCheckedEnumVariant variant;
            size_t payload_index = 0;
            memset(&variant, 0, sizeof(variant));
            if (source_variant == NULL || !source_variant->name_initialized
                || !source_variant->payload_initialized) {
                mmay_checked_enum_value_dispose(&enum_value);
                mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
                return false;
            }
            if (!mmay_vector_init(
                    &variant.payload,
                    checker->result->allocator,
                    sizeof(MarimayCheckedEnumPayloadItem)
                ) || !mmay_vector_reserve(
                    &variant.payload,
                    source_variant->payload.length
                )) {
                mmay_checked_enum_variant_value_dispose(&variant);
                mmay_checked_enum_value_dispose(&enum_value);
                mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
                return false;
            }
            variant.name = source_variant->owned_name;
            memset(
                &source_variant->owned_name,
                0,
                sizeof(source_variant->owned_name)
            );
            source_variant->name_initialized = false;
            variant.kind = source_variant->kind;
            for (payload_index = 0;
                payload_index < source_variant->payload.length;
                payload_index += 1) {
                MarimayCheckEnumPayloadItemDeclaration *source_item =
                    mmay_vector_at(&source_variant->payload, payload_index);
                MarimayCheckedEnumPayloadItem item;
                if (source_item == NULL || source_item->type == MMAY_TYPE_INVALID
                    || (source_item->has_name
                        && !source_item->name_initialized)) {
                    mmay_checked_enum_variant_value_dispose(&variant);
                    mmay_checked_enum_value_dispose(&enum_value);
                    mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
                    return false;
                }
                memset(&item, 0, sizeof(item));
                if (source_item->has_name) {
                    item.name = source_item->owned_name;
                    memset(
                        &source_item->owned_name,
                        0,
                        sizeof(source_item->owned_name)
                    );
                    source_item->name_initialized = false;
                }
                item.type = source_item->type;
                item.has_name = source_item->has_name;
                if (!mmay_vector_push(&variant.payload, &item)) {
                    mmay_owned_bytes_dispose(&item.name);
                    mmay_checked_enum_variant_value_dispose(&variant);
                    mmay_checked_enum_value_dispose(&enum_value);
                    mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
                    return false;
                }
            }
            if (!mmay_vector_push(&enum_value.variants, &variant)) {
                mmay_checked_enum_variant_value_dispose(&variant);
                mmay_checked_enum_value_dispose(&enum_value);
                mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
                return false;
            }
            memset(&variant, 0, sizeof(variant));
        }
        if (!mmay_vector_push(&checker->result->enums, &enum_value)) {
            mmay_checked_enum_value_dispose(&enum_value);
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
        memset(&enum_value, 0, sizeof(enum_value));
    }
    return true;
}
