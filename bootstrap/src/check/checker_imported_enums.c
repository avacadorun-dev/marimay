#include "checker_imported_enums_api.h"

#include "checker_internal.h"
#include "package_imports_api.h"

static void ruda_checker_private_imported_enum_view_dispose(
    RudaCheckEnumDeclaration *declaration
) {
    size_t variant_index = 0;
    if (declaration == NULL) {
        return;
    }
    for (variant_index = 0; variant_index < declaration->variants.length;
        variant_index += 1) {
        RudaCheckEnumVariantDeclaration *variant = ruda_vector_at(
            &declaration->variants, variant_index
        );
        if (variant != NULL && variant->payload_initialized) {
            ruda_vector_dispose(&variant->payload);
        }
    }
    if (declaration->variants_initialized) {
        ruda_vector_dispose(&declaration->variants);
    }
    if (declaration->type_parameters_initialized) {
        ruda_vector_dispose(&declaration->type_parameters);
    }
    memset(declaration, 0, sizeof(*declaration));
}

bool ruda_checker_private_imported_enums_init(RudaChecker *checker) {
    if (checker == NULL) {
        return false;
    }
    checker->imported_enums_initialized = ruda_vector_init(
        &checker->imported_enums,
        checker->result->allocator,
        sizeof(RudaCheckEnumDeclaration)
    );
    return checker->imported_enums_initialized;
}

void ruda_checker_private_imported_enums_dispose(RudaChecker *checker) {
    size_t index = 0;
    if (checker == NULL || !checker->imported_enums_initialized) {
        return;
    }
    for (index = 0; index < checker->imported_enums.length; index += 1) {
        ruda_checker_private_imported_enum_view_dispose(
            ruda_vector_at(&checker->imported_enums, index)
        );
    }
    ruda_vector_dispose(&checker->imported_enums);
    checker->imported_enums_initialized = false;
}

static bool ruda_checker_private_imported_enum_view_init(
    RudaChecker *checker,
    const RudaCheckedEnum *source,
    RudaTypeId type,
    RudaCheckEnumDeclaration *view
) {
    size_t index = 0;
    memset(view, 0, sizeof(*view));
    if (!ruda_vector_init(
            &view->type_parameters,
            checker->result->allocator,
            sizeof(RudaCheckTypeParameterDeclaration)
        )) {
        return false;
    }
    view->type_parameters_initialized = true;
    if (!ruda_vector_reserve(
            &view->type_parameters, source->type_parameters.length
        ) || !ruda_vector_init(
            &view->variants,
            checker->result->allocator,
            sizeof(RudaCheckEnumVariantDeclaration)
        )) {
        ruda_checker_private_imported_enum_view_dispose(view);
        return false;
    }
    view->variants_initialized = true;
    if (!ruda_vector_reserve(&view->variants, source->variants.length)) {
        ruda_checker_private_imported_enum_view_dispose(view);
        return false;
    }
    for (index = 0; index < source->type_parameters.length; index += 1) {
        const RudaTypeId *source_parameter = ruda_vector_at_const(
            &source->type_parameters, index
        );
        RudaCheckTypeParameterDeclaration parameter;
        memset(&parameter, 0, sizeof(parameter));
        if (source_parameter == NULL) {
            ruda_checker_private_imported_enum_view_dispose(view);
            return false;
        }
        parameter.type = *source_parameter;
        if (!ruda_vector_push(&view->type_parameters, &parameter)) {
            ruda_checker_private_imported_enum_view_dispose(view);
            return false;
        }
    }
    for (index = 0; index < source->variants.length; index += 1) {
        const RudaCheckedEnumVariant *source_variant = ruda_vector_at_const(
            &source->variants, index
        );
        RudaCheckEnumVariantDeclaration variant;
        size_t payload_index = 0;
        memset(&variant, 0, sizeof(variant));
        if (source_variant == NULL || !ruda_vector_init(
                &variant.payload,
                checker->result->allocator,
                sizeof(RudaCheckEnumPayloadItemDeclaration)
            )) {
            ruda_checker_private_imported_enum_view_dispose(view);
            return false;
        }
        variant.payload_initialized = true;
        variant.owned_name = source_variant->name;
        variant.kind = source_variant->kind;
        if (!ruda_vector_reserve(
                &variant.payload, source_variant->payload.length
            )) {
            ruda_vector_dispose(&variant.payload);
            ruda_checker_private_imported_enum_view_dispose(view);
            return false;
        }
        for (payload_index = 0;
            payload_index < source_variant->payload.length;
            payload_index += 1) {
            const RudaCheckedEnumPayloadItem *source_payload =
                ruda_vector_at_const(&source_variant->payload, payload_index);
            RudaCheckEnumPayloadItemDeclaration payload;
            memset(&payload, 0, sizeof(payload));
            if (source_payload == NULL) {
                ruda_vector_dispose(&variant.payload);
                ruda_checker_private_imported_enum_view_dispose(view);
                return false;
            }
            payload.owned_name = source_payload->name;
            payload.type = source_payload->type;
            payload.has_name = source_payload->has_name;
            if (!ruda_vector_push(&variant.payload, &payload)) {
                ruda_vector_dispose(&variant.payload);
                ruda_checker_private_imported_enum_view_dispose(view);
                return false;
            }
        }
        if (!ruda_vector_push(&view->variants, &variant)) {
            ruda_vector_dispose(&variant.payload);
            ruda_checker_private_imported_enum_view_dispose(view);
            return false;
        }
        memset(&variant, 0, sizeof(variant));
    }
    view->owned_name = source->name;
    view->symbol = source->symbol;
    view->type = type;
    view->is_public = true;
    view->declaration_valid = true;
    view->variants_resolved = true;
    view->is_generic = source->type_parameters.length != 0;
    return true;
}

RudaCheckEnumDeclaration *
ruda_checker_private_imported_enum_declaration_by_type(
    RudaChecker *checker,
    RudaTypeId type
) {
    const RudaCheckedEnum *source = NULL;
    RudaCheckEnumDeclaration view;
    size_t index = 0;
    if (checker == NULL || !checker->imported_enums_initialized
        || type == RUDA_TYPE_INVALID) {
        return NULL;
    }
    for (index = 0; index < checker->imported_enums.length; index += 1) {
        RudaCheckEnumDeclaration *existing = ruda_vector_at(
            &checker->imported_enums, index
        );
        if (existing != NULL && existing->type == type) {
            return existing;
        }
    }
    source = ruda_check_imported_enum_by_type(checker, type);
    if (source == NULL) {
        return NULL;
    }
    if (!ruda_checker_private_imported_enum_view_init(
            checker, source, type, &view
        ) || !ruda_vector_push(&checker->imported_enums, &view)) {
        ruda_checker_private_imported_enum_view_dispose(&view);
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return NULL;
    }
    memset(&view, 0, sizeof(view));
    return ruda_vector_at(
        &checker->imported_enums, checker->imported_enums.length - 1
    );
}
