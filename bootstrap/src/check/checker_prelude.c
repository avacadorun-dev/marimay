#include "checker_prelude_api.h"

#include "checker_declaration_lifecycle_api.h"

#include <stdio.h>
#include <string.h>

static bool mmay_checker_private_prelude_name(
    MarimayChecker *checker,
    MarimayOwnedBytes *owned,
    const char *text
) {
    return mmay_owned_bytes_copy(
        owned,
        checker->result->allocator,
        (const unsigned char *)text,
        strlen(text)
    );
}

static bool mmay_checker_private_prelude_symbol(
    const char *name,
    MarimaySymbolId *result
) {
    return mmay_symbol_id_from_bytes(
        "std", 3, "prelude", 7,
        (const unsigned char *)name, strlen(name), 0, result
    );
}

static bool mmay_checker_private_prelude_parameter(
    MarimayChecker *checker,
    const char *owner,
    size_t index,
    const char *name,
    MarimayCheckTypeParameterDeclaration *result
) {
    char symbol_name[64];
    int symbol_name_length = 0;
    MarimaySymbolId symbol;
    memset(result, 0, sizeof(*result));
    symbol_name_length = snprintf(
        symbol_name, sizeof(symbol_name), "%s.%s.%zu", owner, name, index
    );
    if (symbol_name_length < 0 || (size_t)symbol_name_length >= sizeof(symbol_name)) {
        return false;
    }
    if (!mmay_checker_private_prelude_name(
            checker, &result->owned_name, name
        ) || !mmay_checker_private_prelude_symbol(symbol_name, &symbol)
        || !mmay_type_intern_nominal(
            &checker->result->types,
            MMAY_TYPE_KIND_TYPE_PARAMETER,
            symbol,
            NULL,
            0,
            &result->type
        ) || !mmay_vector_init(
            &result->constraints,
            checker->result->allocator,
            sizeof(MarimayTypeId)
        )) {
        mmay_owned_bytes_dispose(&result->owned_name);
        return false;
    }
    result->name_initialized = true;
    result->constraints_initialized = true;
    return true;
}

static bool mmay_checker_private_prelude_variant(
    MarimayChecker *checker,
    const char *name,
    MarimayCheckedEnumVariantKind kind,
    MarimayTypeId payload_type,
    const char *payload_name,
    MarimayCheckEnumVariantDeclaration *result
) {
    MarimayCheckEnumPayloadItemDeclaration payload;
    memset(result, 0, sizeof(*result));
    memset(&payload, 0, sizeof(payload));
    if (!mmay_checker_private_prelude_name(
            checker, &result->owned_name, name
        )) {
        return false;
    }
    result->name_initialized = true;
    if (!mmay_vector_init(
            &result->payload,
            checker->result->allocator,
            sizeof(MarimayCheckEnumPayloadItemDeclaration)
        )) {
        mmay_owned_bytes_dispose(&result->owned_name);
        result->name_initialized = false;
        return false;
    }
    result->payload_initialized = true;
    result->kind = kind;
    if (payload_type != MMAY_TYPE_INVALID) {
        payload.type = payload_type;
        payload.has_name = payload_name != NULL;
        if (payload_name != NULL && !mmay_checker_private_prelude_name(
                checker, &payload.owned_name, payload_name
            )) {
            mmay_checker_private_enum_variant_declaration_dispose(result);
            return false;
        }
        payload.name_initialized = payload_name != NULL;
        if (!mmay_vector_push(&result->payload, &payload)) {
            mmay_owned_bytes_dispose(&payload.owned_name);
            mmay_checker_private_enum_variant_declaration_dispose(result);
            return false;
        }
    }
    return true;
}

static bool mmay_checker_private_prelude_enum(
    MarimayChecker *checker,
    const char *name,
    const char *first_parameter,
    const char *second_parameter,
    const char *first_variant,
    const char *second_variant,
    MarimayCheckedEnumVariantKind first_kind,
    MarimayCheckedEnumVariantKind second_kind
) {
    MarimayCheckEnumDeclaration declaration;
    MarimayCheckTypeParameterDeclaration parameter;
    MarimayCheckEnumVariantDeclaration variant;
    MarimaySymbolId symbol;
    MarimayTypeId parameter_types[2] = {MMAY_TYPE_INVALID, MMAY_TYPE_INVALID};
    size_t parameter_count = second_parameter == NULL ? 1 : 2;
    size_t index = 0;
    memset(&declaration, 0, sizeof(declaration));
    memset(&variant, 0, sizeof(variant));
    if (!mmay_checker_private_prelude_name(
            checker, &declaration.owned_name, name
        ) || !mmay_checker_private_prelude_symbol(name, &symbol)
        || !mmay_vector_init(
            &declaration.type_parameters,
            checker->result->allocator,
            sizeof(MarimayCheckTypeParameterDeclaration)
        ) || !mmay_vector_init(
            &declaration.variants,
            checker->result->allocator,
            sizeof(MarimayCheckEnumVariantDeclaration)
        )) {
        mmay_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    declaration.name_initialized = true;
    declaration.symbol = symbol;
    declaration.type_parameters_initialized = true;
    declaration.variants_initialized = true;
    declaration.declaration_valid = true;
    declaration.variants_resolved = true;
    declaration.is_generic = true;
    declaration.is_builtin = true;
    declaration.is_public = true;
    for (index = 0; index < parameter_count; index += 1) {
        const char *parameter_name = index == 0
            ? first_parameter : second_parameter;
        if (!mmay_checker_private_prelude_parameter(
                checker, name, index, parameter_name, &parameter
            ) || !mmay_vector_push(
                &declaration.type_parameters, &parameter
            )) {
            if (parameter.name_initialized) {
                mmay_owned_bytes_dispose(&parameter.owned_name);
            }
            if (parameter.constraints_initialized) {
                mmay_vector_dispose(&parameter.constraints);
            }
            mmay_checker_private_enum_declaration_dispose(&declaration);
            return false;
        }
        parameter_types[index] = parameter.type;
        memset(&parameter, 0, sizeof(parameter));
    }
    if (!mmay_type_intern_nominal(
            &checker->result->types,
            MMAY_TYPE_KIND_NOMINAL,
            symbol,
            NULL,
            0,
            &declaration.type
        ) || !mmay_checker_private_prelude_variant(
            checker, first_variant, first_kind, parameter_types[0], NULL, &variant
        ) || !mmay_vector_push(&declaration.variants, &variant)) {
        mmay_checker_private_enum_variant_declaration_dispose(&variant);
        mmay_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    memset(&variant, 0, sizeof(variant));
    if (!mmay_checker_private_prelude_variant(
            checker,
            second_variant,
            second_kind,
            second_kind == MMAY_CHECKED_ENUM_VARIANT_POSITIONAL
                ? parameter_types[parameter_count - 1] : MMAY_TYPE_INVALID,
            NULL,
            &variant
        ) || !mmay_vector_push(&declaration.variants, &variant)) {
        mmay_checker_private_enum_variant_declaration_dispose(&variant);
        mmay_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    memset(&variant, 0, sizeof(variant));
    if (!mmay_vector_push(&checker->prelude_enums, &declaration)) {
        mmay_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    memset(&declaration, 0, sizeof(declaration));
    return true;
}

static bool mmay_checker_private_prelude_array_growth_error(
    MarimayChecker *checker
) {
    MarimayCheckEnumDeclaration declaration;
    MarimayCheckEnumVariantDeclaration variant;
    MarimaySymbolId symbol;
    memset(&declaration, 0, sizeof(declaration));
    memset(&variant, 0, sizeof(variant));
    if (!mmay_checker_private_prelude_name(
            checker, &declaration.owned_name, "ArrayGrowthError"
        )) {
        return false;
    }
    declaration.name_initialized = true;
    if (!mmay_checker_private_prelude_symbol(
            "ArrayGrowthError", &symbol
        ) || !mmay_vector_init(
            &declaration.type_parameters,
            checker->result->allocator,
            sizeof(MarimayCheckTypeParameterDeclaration)
        )) {
        mmay_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    declaration.symbol = symbol;
    declaration.type_parameters_initialized = true;
    if (!mmay_vector_init(
            &declaration.variants,
            checker->result->allocator,
            sizeof(MarimayCheckEnumVariantDeclaration)
        )) {
        mmay_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    declaration.variants_initialized = true;
    if (!mmay_type_intern_nominal(
            &checker->result->types,
            MMAY_TYPE_KIND_NOMINAL,
            symbol,
            NULL,
            0,
            &declaration.type
        )) {
        mmay_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }

    declaration.declaration_valid = true;
    declaration.variants_resolved = true;
    declaration.is_public = true;
    declaration.is_builtin = true;
    if (!mmay_checker_private_prelude_variant(
            checker,
            "LengthLimit",
            MMAY_CHECKED_ENUM_VARIANT_NAMED,
            MMAY_TYPE_I64,
            "limit",
            &variant
        ) || !mmay_vector_push(&declaration.variants, &variant)) {
        mmay_checker_private_enum_variant_declaration_dispose(&variant);
        mmay_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    memset(&variant, 0, sizeof(variant));
    if (!mmay_checker_private_prelude_variant(
            checker,
            "NegativeAdditional",
            MMAY_CHECKED_ENUM_VARIANT_NAMED,
            MMAY_TYPE_I64,
            "additional",
            &variant
        ) || !mmay_vector_push(&declaration.variants, &variant)) {
        mmay_checker_private_enum_variant_declaration_dispose(&variant);
        mmay_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    memset(&variant, 0, sizeof(variant));
    if (!mmay_vector_push(&checker->prelude_enums, &declaration)) {
        mmay_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    memset(&declaration, 0, sizeof(declaration));
    return true;
}

static bool mmay_checker_private_prelude_error_enum(
    MarimayChecker *checker,
    const char *name,
    const char *variant_name,
    const char *const *payload_names,
    size_t payload_count
) {
    MarimayCheckEnumDeclaration declaration;
    MarimayCheckEnumVariantDeclaration variant;
    MarimaySymbolId symbol;
    size_t index = 0;
    memset(&declaration, 0, sizeof(declaration));
    memset(&variant, 0, sizeof(variant));
    if (checker == NULL || name == NULL || variant_name == NULL
        || (payload_names == NULL && payload_count != 0)) {
        return false;
    }
    if (!mmay_checker_private_prelude_name(
            checker, &declaration.owned_name, name
        )) {
        return false;
    }
    declaration.name_initialized = true;
    if (!mmay_checker_private_prelude_symbol(name, &symbol)
        || !mmay_vector_init(
            &declaration.type_parameters,
            checker->result->allocator,
            sizeof(MarimayCheckTypeParameterDeclaration)
        )) {
        mmay_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    declaration.type_parameters_initialized = true;
    if (!mmay_vector_init(
            &declaration.variants,
            checker->result->allocator,
            sizeof(MarimayCheckEnumVariantDeclaration)
        )) {
        mmay_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    declaration.variants_initialized = true;
    if (!mmay_checker_private_prelude_name(
            checker, &variant.owned_name, variant_name
        )) {
        mmay_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    variant.name_initialized = true;
    if (!mmay_vector_init(
            &variant.payload,
            checker->result->allocator,
            sizeof(MarimayCheckEnumPayloadItemDeclaration)
        )) {
        mmay_checker_private_enum_variant_declaration_dispose(&variant);
        mmay_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    declaration.symbol = symbol;
    declaration.declaration_valid = true;
    declaration.variants_resolved = true;
    declaration.is_builtin = true;
    declaration.is_public = true;
    variant.payload_initialized = true;
    variant.kind = MMAY_CHECKED_ENUM_VARIANT_NAMED;
    if (!mmay_type_intern_nominal(
            &checker->result->types,
            MMAY_TYPE_KIND_NOMINAL,
            symbol,
            NULL,
            0,
            &declaration.type
        )) {
        mmay_checker_private_enum_variant_declaration_dispose(&variant);
        mmay_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    for (index = 0; index < payload_count; index += 1) {
        MarimayCheckEnumPayloadItemDeclaration payload;
        memset(&payload, 0, sizeof(payload));
        if (!mmay_checker_private_prelude_name(
                checker, &payload.owned_name, payload_names[index]
            )) {
            mmay_checker_private_enum_variant_declaration_dispose(&variant);
            mmay_checker_private_enum_declaration_dispose(&declaration);
            return false;
        }
        payload.type = MMAY_TYPE_I64;
        payload.name_initialized = true;
        payload.has_name = true;
        if (!mmay_vector_push(&variant.payload, &payload)) {
            mmay_owned_bytes_dispose(&payload.owned_name);
            mmay_checker_private_enum_variant_declaration_dispose(&variant);
            mmay_checker_private_enum_declaration_dispose(&declaration);
            return false;
        }
        memset(&payload, 0, sizeof(payload));
    }
    if (!mmay_vector_push(&declaration.variants, &variant)) {
        mmay_checker_private_enum_variant_declaration_dispose(&variant);
        mmay_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    memset(&variant, 0, sizeof(variant));
    if (!mmay_vector_push(&checker->prelude_enums, &declaration)) {
        mmay_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    memset(&declaration, 0, sizeof(declaration));
    return true;
}

bool mmay_checker_private_prelude_init(MarimayChecker *checker) {
    size_t original_max_types = 0;
    size_t prelude_max_types = 0;
    if (checker == NULL || checker->result == NULL) {
        return false;
    }
    original_max_types = checker->options.type_options.max_types;
    prelude_max_types = MMAY_TYPE_FIRST_DYNAMIC + 7;
    if (prelude_max_types < original_max_types) {
        prelude_max_types = original_max_types;
    }
    checker->options.type_options.max_types = prelude_max_types;
    checker->result->types.options.max_types = prelude_max_types;
    if (!mmay_vector_init(
            &checker->prelude_enums,
            checker->result->allocator,
            sizeof(MarimayCheckEnumDeclaration)
    )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        checker->options.type_options.max_types = original_max_types;
        checker->result->types.options.max_types = original_max_types;
        return false;
    }
    checker->prelude_enums_initialized = true;
    {
        static const char *const invalid_length[] = {"requested", "length"};
        if (!mmay_checker_private_prelude_enum(
        checker, "Option", "T", NULL, "Some", "None",
            MMAY_CHECKED_ENUM_VARIANT_POSITIONAL,
            MMAY_CHECKED_ENUM_VARIANT_UNIT
        ) || !mmay_checker_private_prelude_enum(
            checker, "Result", "T", "E", "Ok", "Err",
            MMAY_CHECKED_ENUM_VARIANT_POSITIONAL,
            MMAY_CHECKED_ENUM_VARIANT_POSITIONAL
        ) || !mmay_checker_private_prelude_array_growth_error(checker)
        || !mmay_checker_private_prelude_error_enum(
            checker, "ArrayTruncateError", "InvalidLength",
            invalid_length, 2
        )) {
        mmay_checker_fail(
            checker,
            MMAY_CHECK_ALLOCATION_FAILED
        );
        checker->options.type_options.max_types = original_max_types;
        checker->result->types.options.max_types = original_max_types;
        return false;
    }
    }
    checker->options.type_options.max_types = original_max_types;
    checker->result->types.options.max_types = original_max_types;
    return true;
}
