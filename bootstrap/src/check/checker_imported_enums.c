#include "checker_imported_enums_api.h"

#include "checker_internal.h"
#include "package_imports_api.h"

static void mmay_checker_private_imported_enum_view_dispose(
    MarimayCheckEnumDeclaration *declaration
) {
    size_t variant_index = 0;
    if (declaration == NULL) {
        return;
    }
    for (variant_index = 0; variant_index < declaration->variants.length;
        variant_index += 1) {
        MarimayCheckEnumVariantDeclaration *variant = mmay_vector_at(
            &declaration->variants, variant_index
        );
        if (variant != NULL && variant->payload_initialized) {
            mmay_vector_dispose(&variant->payload);
        }
    }
    if (declaration->variants_initialized) {
        mmay_vector_dispose(&declaration->variants);
    }
    if (declaration->type_parameters_initialized) {
        mmay_vector_dispose(&declaration->type_parameters);
    }
    memset(declaration, 0, sizeof(*declaration));
}

bool mmay_checker_private_imported_enums_init(MarimayChecker *checker) {
    if (checker == NULL) {
        return false;
    }
    checker->imported_enums_initialized = mmay_vector_init(
        &checker->imported_enums,
        checker->result->allocator,
        sizeof(MarimayCheckEnumDeclaration)
    );
    return checker->imported_enums_initialized;
}

void mmay_checker_private_imported_enums_dispose(MarimayChecker *checker) {
    size_t index = 0;
    if (checker == NULL || !checker->imported_enums_initialized) {
        return;
    }
    for (index = 0; index < checker->imported_enums.length; index += 1) {
        mmay_checker_private_imported_enum_view_dispose(
            mmay_vector_at(&checker->imported_enums, index)
        );
    }
    mmay_vector_dispose(&checker->imported_enums);
    checker->imported_enums_initialized = false;
}

static bool mmay_checker_private_imported_enum_view_init(
    MarimayChecker *checker,
    const MarimayCheckedEnum *source,
    MarimayTypeId type,
    MarimayCheckEnumDeclaration *view
) {
    size_t index = 0;
    memset(view, 0, sizeof(*view));
    if (!mmay_vector_init(
            &view->type_parameters,
            checker->result->allocator,
            sizeof(MarimayCheckTypeParameterDeclaration)
        )) {
        return false;
    }
    view->type_parameters_initialized = true;
    if (!mmay_vector_reserve(
            &view->type_parameters, source->type_parameters.length
        ) || !mmay_vector_init(
            &view->variants,
            checker->result->allocator,
            sizeof(MarimayCheckEnumVariantDeclaration)
        )) {
        mmay_checker_private_imported_enum_view_dispose(view);
        return false;
    }
    view->variants_initialized = true;
    if (!mmay_vector_reserve(&view->variants, source->variants.length)) {
        mmay_checker_private_imported_enum_view_dispose(view);
        return false;
    }
    for (index = 0; index < source->type_parameters.length; index += 1) {
        const MarimayTypeId *source_parameter = mmay_vector_at_const(
            &source->type_parameters, index
        );
        MarimayCheckTypeParameterDeclaration parameter;
        memset(&parameter, 0, sizeof(parameter));
        if (source_parameter == NULL) {
            mmay_checker_private_imported_enum_view_dispose(view);
            return false;
        }
        parameter.type = *source_parameter;
        if (!mmay_vector_push(&view->type_parameters, &parameter)) {
            mmay_checker_private_imported_enum_view_dispose(view);
            return false;
        }
    }
    for (index = 0; index < source->variants.length; index += 1) {
        const MarimayCheckedEnumVariant *source_variant = mmay_vector_at_const(
            &source->variants, index
        );
        MarimayCheckEnumVariantDeclaration variant;
        size_t payload_index = 0;
        memset(&variant, 0, sizeof(variant));
        if (source_variant == NULL || !mmay_vector_init(
                &variant.payload,
                checker->result->allocator,
                sizeof(MarimayCheckEnumPayloadItemDeclaration)
            )) {
            mmay_checker_private_imported_enum_view_dispose(view);
            return false;
        }
        variant.payload_initialized = true;
        variant.owned_name = source_variant->name;
        variant.kind = source_variant->kind;
        if (!mmay_vector_reserve(
                &variant.payload, source_variant->payload.length
            )) {
            mmay_vector_dispose(&variant.payload);
            mmay_checker_private_imported_enum_view_dispose(view);
            return false;
        }
        for (payload_index = 0;
            payload_index < source_variant->payload.length;
            payload_index += 1) {
            const MarimayCheckedEnumPayloadItem *source_payload =
                mmay_vector_at_const(&source_variant->payload, payload_index);
            MarimayCheckEnumPayloadItemDeclaration payload;
            memset(&payload, 0, sizeof(payload));
            if (source_payload == NULL) {
                mmay_vector_dispose(&variant.payload);
                mmay_checker_private_imported_enum_view_dispose(view);
                return false;
            }
            payload.owned_name = source_payload->name;
            payload.type = source_payload->type;
            payload.has_name = source_payload->has_name;
            if (!mmay_vector_push(&variant.payload, &payload)) {
                mmay_vector_dispose(&variant.payload);
                mmay_checker_private_imported_enum_view_dispose(view);
                return false;
            }
        }
        if (!mmay_vector_push(&view->variants, &variant)) {
            mmay_vector_dispose(&variant.payload);
            mmay_checker_private_imported_enum_view_dispose(view);
            return false;
        }
        memset(&variant, 0, sizeof(variant));
    }
    view->owned_name = source->name;
    view->symbol = source->symbol;
    view->type = type;
    view->is_public = true;
    view->derives_eq_hash = source->derives_eq_hash;
    view->declaration_valid = true;
    view->variants_resolved = true;
    view->is_generic = source->type_parameters.length != 0;
    return true;
}

MarimayCheckEnumDeclaration *
mmay_checker_private_imported_enum_declaration_by_type(
    MarimayChecker *checker,
    MarimayTypeId type
) {
    const MarimayCheckedEnum *source = NULL;
    MarimayCheckEnumDeclaration view;
    size_t index = 0;
    if (checker == NULL || !checker->imported_enums_initialized
        || type == MMAY_TYPE_INVALID) {
        return NULL;
    }
    for (index = 0; index < checker->imported_enums.length; index += 1) {
        MarimayCheckEnumDeclaration *existing = mmay_vector_at(
            &checker->imported_enums, index
        );
        if (existing != NULL && existing->type == type) {
            return existing;
        }
    }
    source = mmay_check_imported_enum_by_type(checker, type);
    if (source == NULL) {
        return NULL;
    }
    if (!mmay_checker_private_imported_enum_view_init(
            checker, source, type, &view
        ) || !mmay_vector_push(&checker->imported_enums, &view)) {
        mmay_checker_private_imported_enum_view_dispose(&view);
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return NULL;
    }
    memset(&view, 0, sizeof(view));
    return mmay_vector_at(
        &checker->imported_enums, checker->imported_enums.length - 1
    );
}
