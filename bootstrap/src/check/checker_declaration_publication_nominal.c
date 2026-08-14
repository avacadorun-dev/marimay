#include "checker_declaration_publication_nominal_api.h"





#include "checker_internal.h"

 bool ruda_checker_private_publish_records(RudaChecker *checker) {
    size_t record_index = 0;
    if (!ruda_vector_reserve(
            &checker->result->records,
            checker->records.length
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (record_index = 0; record_index < checker->records.length;
        record_index += 1) {
        RudaCheckRecordDeclaration *declaration = ruda_vector_at(
            &checker->records,
            record_index
        );
        RudaCheckedRecord record;
        size_t field_index = 0;
        memset(&record, 0, sizeof(record));
        if (declaration == NULL || !declaration->declaration_valid
            || !declaration->fields_resolved) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        if (!ruda_vector_init(
                &record.fields,
                checker->result->allocator,
                sizeof(RudaCheckedRecordField)
            ) || !ruda_vector_reserve(
                &record.fields,
                declaration->fields.length
            )) {
            ruda_checked_record_value_dispose(&record);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        record.name = declaration->owned_name;
        memset(&declaration->owned_name, 0, sizeof(declaration->owned_name));
        declaration->name_initialized = false;
        record.symbol = declaration->symbol;
        record.type = declaration->type;
        record.is_public = declaration->is_public;
        for (field_index = 0; field_index < declaration->fields.length;
            field_index += 1) {
            RudaCheckRecordFieldDeclaration *source = ruda_vector_at(
                &declaration->fields,
                field_index
            );
            RudaCheckedRecordField field;
            if (source == NULL || !source->name_initialized
                || source->type == RUDA_TYPE_INVALID) {
                ruda_checked_record_value_dispose(&record);
                ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
                return false;
            }
            memset(&field, 0, sizeof(field));
            field.name = source->owned_name;
            memset(&source->owned_name, 0, sizeof(source->owned_name));
            source->name_initialized = false;
            field.type = source->type;
            field.is_public = source->is_public;
            field.is_mutable = source->is_mutable;
            if (!ruda_vector_push(&record.fields, &field)) {
                ruda_owned_bytes_dispose(&field.name);
                ruda_checked_record_value_dispose(&record);
                ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
                return false;
            }
        }
        if (!ruda_vector_push(&checker->result->records, &record)) {
            ruda_checked_record_value_dispose(&record);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        memset(&record, 0, sizeof(record));
    }
    return true;
}

 bool ruda_checker_private_publish_enums(RudaChecker *checker) {
    size_t enum_index = 0;
    if (!ruda_vector_reserve(
            &checker->result->enums,
            checker->enums.length
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (enum_index = 0; enum_index < checker->enums.length;
        enum_index += 1) {
        RudaCheckEnumDeclaration *declaration = ruda_vector_at(
            &checker->enums,
            enum_index
        );
        RudaCheckedEnum enum_value;
        size_t variant_index = 0;
        memset(&enum_value, 0, sizeof(enum_value));
        if (declaration == NULL || !declaration->declaration_valid
            || !declaration->variants_resolved) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        if (!ruda_vector_init(
                &enum_value.variants,
                checker->result->allocator,
                sizeof(RudaCheckedEnumVariant)
            ) || !ruda_vector_reserve(
                &enum_value.variants,
                declaration->variants.length
            )) {
            ruda_checked_enum_value_dispose(&enum_value);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        enum_value.name = declaration->owned_name;
        memset(&declaration->owned_name, 0, sizeof(declaration->owned_name));
        declaration->name_initialized = false;
        enum_value.symbol = declaration->symbol;
        enum_value.type = declaration->type;
        enum_value.is_public = declaration->is_public;
        for (variant_index = 0; variant_index < declaration->variants.length;
            variant_index += 1) {
            RudaCheckEnumVariantDeclaration *source_variant = ruda_vector_at(
                &declaration->variants,
                variant_index
            );
            RudaCheckedEnumVariant variant;
            size_t payload_index = 0;
            memset(&variant, 0, sizeof(variant));
            if (source_variant == NULL || !source_variant->name_initialized
                || !source_variant->payload_initialized) {
                ruda_checked_enum_value_dispose(&enum_value);
                ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
                return false;
            }
            if (!ruda_vector_init(
                    &variant.payload,
                    checker->result->allocator,
                    sizeof(RudaCheckedEnumPayloadItem)
                ) || !ruda_vector_reserve(
                    &variant.payload,
                    source_variant->payload.length
                )) {
                ruda_checked_enum_variant_value_dispose(&variant);
                ruda_checked_enum_value_dispose(&enum_value);
                ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
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
                RudaCheckEnumPayloadItemDeclaration *source_item =
                    ruda_vector_at(&source_variant->payload, payload_index);
                RudaCheckedEnumPayloadItem item;
                if (source_item == NULL || source_item->type == RUDA_TYPE_INVALID
                    || (source_item->has_name
                        && !source_item->name_initialized)) {
                    ruda_checked_enum_variant_value_dispose(&variant);
                    ruda_checked_enum_value_dispose(&enum_value);
                    ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
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
                if (!ruda_vector_push(&variant.payload, &item)) {
                    ruda_owned_bytes_dispose(&item.name);
                    ruda_checked_enum_variant_value_dispose(&variant);
                    ruda_checked_enum_value_dispose(&enum_value);
                    ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
                    return false;
                }
            }
            if (!ruda_vector_push(&enum_value.variants, &variant)) {
                ruda_checked_enum_variant_value_dispose(&variant);
                ruda_checked_enum_value_dispose(&enum_value);
                ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
                return false;
            }
            memset(&variant, 0, sizeof(variant));
        }
        if (!ruda_vector_push(&checker->result->enums, &enum_value)) {
            ruda_checked_enum_value_dispose(&enum_value);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        memset(&enum_value, 0, sizeof(enum_value));
    }
    return true;
}
