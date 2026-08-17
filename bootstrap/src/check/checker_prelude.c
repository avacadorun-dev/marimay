#include "checker_prelude_api.h"

#include "checker_declaration_lifecycle_api.h"

#include <stdio.h>
#include <string.h>

static bool ruda_checker_private_prelude_name(
    RudaChecker *checker,
    RudaOwnedBytes *owned,
    const char *text
) {
    return ruda_owned_bytes_copy(
        owned,
        checker->result->allocator,
        (const unsigned char *)text,
        strlen(text)
    );
}

static bool ruda_checker_private_prelude_symbol(
    const char *name,
    RudaSymbolId *result
) {
    return ruda_symbol_id_from_bytes(
        "std", 3, "prelude", 7,
        (const unsigned char *)name, strlen(name), 0, result
    );
}

static bool ruda_checker_private_prelude_parameter(
    RudaChecker *checker,
    const char *owner,
    size_t index,
    const char *name,
    RudaCheckTypeParameterDeclaration *result
) {
    char symbol_name[64];
    int symbol_name_length = 0;
    RudaSymbolId symbol;
    memset(result, 0, sizeof(*result));
    symbol_name_length = snprintf(
        symbol_name, sizeof(symbol_name), "%s.%s.%zu", owner, name, index
    );
    if (symbol_name_length < 0 || (size_t)symbol_name_length >= sizeof(symbol_name)) {
        return false;
    }
    if (!ruda_checker_private_prelude_name(
            checker, &result->owned_name, name
        ) || !ruda_checker_private_prelude_symbol(symbol_name, &symbol)
        || !ruda_type_intern_nominal(
            &checker->result->types,
            RUDA_TYPE_KIND_TYPE_PARAMETER,
            symbol,
            NULL,
            0,
            &result->type
        ) || !ruda_vector_init(
            &result->constraints,
            checker->result->allocator,
            sizeof(RudaTypeId)
        )) {
        ruda_owned_bytes_dispose(&result->owned_name);
        return false;
    }
    result->name_initialized = true;
    result->constraints_initialized = true;
    return true;
}

static bool ruda_checker_private_prelude_variant(
    RudaChecker *checker,
    const char *name,
    RudaCheckedEnumVariantKind kind,
    RudaTypeId payload_type,
    RudaCheckEnumVariantDeclaration *result
) {
    RudaCheckEnumPayloadItemDeclaration payload;
    memset(result, 0, sizeof(*result));
    memset(&payload, 0, sizeof(payload));
    if (!ruda_checker_private_prelude_name(
            checker, &result->owned_name, name
        ) || !ruda_vector_init(
            &result->payload,
            checker->result->allocator,
            sizeof(RudaCheckEnumPayloadItemDeclaration)
        )) {
        ruda_owned_bytes_dispose(&result->owned_name);
        return false;
    }
    result->name_initialized = true;
    result->payload_initialized = true;
    result->kind = kind;
    if (payload_type != RUDA_TYPE_INVALID) {
        payload.type = payload_type;
        if (!ruda_vector_push(&result->payload, &payload)) {
            ruda_checker_private_enum_variant_declaration_dispose(result);
            return false;
        }
    }
    return true;
}

static bool ruda_checker_private_prelude_enum(
    RudaChecker *checker,
    const char *name,
    const char *first_parameter,
    const char *second_parameter,
    const char *first_variant,
    const char *second_variant,
    RudaCheckedEnumVariantKind first_kind,
    RudaCheckedEnumVariantKind second_kind
) {
    RudaCheckEnumDeclaration declaration;
    RudaCheckTypeParameterDeclaration parameter;
    RudaCheckEnumVariantDeclaration variant;
    RudaSymbolId symbol;
    RudaTypeId parameter_types[2] = {RUDA_TYPE_INVALID, RUDA_TYPE_INVALID};
    size_t parameter_count = second_parameter == NULL ? 1 : 2;
    size_t index = 0;
    memset(&declaration, 0, sizeof(declaration));
    memset(&variant, 0, sizeof(variant));
    if (!ruda_checker_private_prelude_name(
            checker, &declaration.owned_name, name
        ) || !ruda_checker_private_prelude_symbol(name, &symbol)
        || !ruda_vector_init(
            &declaration.type_parameters,
            checker->result->allocator,
            sizeof(RudaCheckTypeParameterDeclaration)
        ) || !ruda_vector_init(
            &declaration.variants,
            checker->result->allocator,
            sizeof(RudaCheckEnumVariantDeclaration)
        )) {
        ruda_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    declaration.name_initialized = true;
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
        if (!ruda_checker_private_prelude_parameter(
                checker, name, index, parameter_name, &parameter
            ) || !ruda_vector_push(
                &declaration.type_parameters, &parameter
            )) {
            if (parameter.name_initialized) {
                ruda_owned_bytes_dispose(&parameter.owned_name);
            }
            if (parameter.constraints_initialized) {
                ruda_vector_dispose(&parameter.constraints);
            }
            ruda_checker_private_enum_declaration_dispose(&declaration);
            return false;
        }
        parameter_types[index] = parameter.type;
        memset(&parameter, 0, sizeof(parameter));
    }
    if (!ruda_type_intern_nominal(
            &checker->result->types,
            RUDA_TYPE_KIND_NOMINAL,
            symbol,
            NULL,
            0,
            &declaration.type
        ) || !ruda_checker_private_prelude_variant(
            checker, first_variant, first_kind, parameter_types[0], &variant
        ) || !ruda_vector_push(&declaration.variants, &variant)) {
        ruda_checker_private_enum_variant_declaration_dispose(&variant);
        ruda_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    memset(&variant, 0, sizeof(variant));
    if (!ruda_checker_private_prelude_variant(
            checker,
            second_variant,
            second_kind,
            second_kind == RUDA_CHECKED_ENUM_VARIANT_POSITIONAL
                ? parameter_types[parameter_count - 1] : RUDA_TYPE_INVALID,
            &variant
        ) || !ruda_vector_push(&declaration.variants, &variant)) {
        ruda_checker_private_enum_variant_declaration_dispose(&variant);
        ruda_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    memset(&variant, 0, sizeof(variant));
    if (!ruda_vector_push(&checker->prelude_enums, &declaration)) {
        ruda_checker_private_enum_declaration_dispose(&declaration);
        return false;
    }
    memset(&declaration, 0, sizeof(declaration));
    return true;
}

bool ruda_checker_private_prelude_init(RudaChecker *checker) {
    size_t original_max_types = 0;
    size_t prelude_max_types = 0;
    if (checker == NULL || checker->result == NULL) {
        return false;
    }
    original_max_types = checker->options.type_options.max_types;
    prelude_max_types = RUDA_TYPE_FIRST_DYNAMIC + 5;
    if (prelude_max_types < original_max_types) {
        prelude_max_types = original_max_types;
    }
    checker->options.type_options.max_types = prelude_max_types;
    checker->result->types.options.max_types = prelude_max_types;
    if (!ruda_vector_init(
            &checker->prelude_enums,
            checker->result->allocator,
            sizeof(RudaCheckEnumDeclaration)
    )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        checker->options.type_options.max_types = original_max_types;
        checker->result->types.options.max_types = original_max_types;
        return false;
    }
    checker->prelude_enums_initialized = true;
    if (!ruda_checker_private_prelude_enum(
            checker, "Option", "T", NULL, "Some", "None",
            RUDA_CHECKED_ENUM_VARIANT_POSITIONAL,
            RUDA_CHECKED_ENUM_VARIANT_UNIT
        ) || !ruda_checker_private_prelude_enum(
            checker, "Result", "T", "E", "Ok", "Err",
            RUDA_CHECKED_ENUM_VARIANT_POSITIONAL,
            RUDA_CHECKED_ENUM_VARIANT_POSITIONAL
        )) {
        ruda_checker_fail(
            checker,
            RUDA_CHECK_ALLOCATION_FAILED
        );
        checker->options.type_options.max_types = original_max_types;
        checker->result->types.options.max_types = original_max_types;
        return false;
    }
    checker->options.type_options.max_types = original_max_types;
    checker->result->types.options.max_types = original_max_types;
    return true;
}
