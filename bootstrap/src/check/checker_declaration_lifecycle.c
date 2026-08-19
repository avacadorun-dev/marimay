#include "checker_declaration_lifecycle_api.h"
#include "package_imports_api.h"
#include "checker_core_api.h"
#include "checker_generic_types_api.h"

 void ruda_checker_private_function_declaration_dispose(
    RudaCheckFunctionDeclaration *declaration
) {
    if (declaration == NULL) {
        return;
    }
    if (declaration->parameter_types_initialized) {
        ruda_vector_dispose(&declaration->parameter_types);
    }
    if (declaration->generic_arguments_initialized) {
        ruda_vector_dispose(&declaration->generic_arguments);
    }
    if (declaration->type_parameters_initialized) {
        ruda_checker_private_type_parameter_vector_dispose(
            &declaration->type_parameters
        );
    }
    if (declaration->parameter_names_initialized) {
        ruda_vector_dispose(&declaration->parameter_names);
    }
    if (declaration->name_initialized) {
        ruda_owned_bytes_dispose(&declaration->owned_name);
    }
    memset(declaration, 0, sizeof(*declaration));
}

void ruda_checker_private_type_parameter_vector_dispose(
    RudaVector *parameters
) {
    size_t index = 0;
    if (parameters == NULL) {
        return;
    }
    for (index = 0; index < parameters->length; index += 1) {
        RudaCheckTypeParameterDeclaration *parameter = ruda_vector_at(
            parameters, index
        );
        if (parameter != NULL && parameter->name_initialized) {
            ruda_owned_bytes_dispose(&parameter->owned_name);
        }
        if (parameter != NULL && parameter->constraints_initialized) {
            ruda_vector_dispose(&parameter->constraints);
        }
    }
    ruda_vector_dispose(parameters);
}

 void ruda_checker_private_constant_declaration_dispose(
    RudaCheckConstantDeclaration *declaration
) {
    if (declaration == NULL) {
        return;
    }
    if (declaration->name_initialized) {
        ruda_owned_bytes_dispose(&declaration->owned_name);
    }
    memset(declaration, 0, sizeof(*declaration));
}

 void ruda_checker_private_type_alias_declaration_dispose(
    RudaCheckTypeAliasDeclaration *declaration
) {
    if (declaration == NULL) {
        return;
    }
    if (declaration->name_initialized) {
        ruda_owned_bytes_dispose(&declaration->owned_name);
    }
    memset(declaration, 0, sizeof(*declaration));
}

 void ruda_checker_private_record_field_declaration_dispose(
    RudaCheckRecordFieldDeclaration *field
) {
    if (field == NULL) {
        return;
    }
    if (field->name_initialized) {
        ruda_owned_bytes_dispose(&field->owned_name);
    }
    memset(field, 0, sizeof(*field));
}

 void ruda_checker_private_record_declaration_dispose(
    RudaCheckRecordDeclaration *declaration
) {
    size_t index = 0;
    if (declaration == NULL) {
        return;
    }
    if (declaration->fields_initialized) {
        for (index = 0; index < declaration->fields.length; index += 1) {
            RudaCheckRecordFieldDeclaration *field = ruda_vector_at(
                &declaration->fields,
                index
            );
            ruda_checker_private_record_field_declaration_dispose(field);
        }
        ruda_vector_dispose(&declaration->fields);
    }
    if (declaration->type_parameters_initialized) {
        ruda_checker_private_type_parameter_vector_dispose(
            &declaration->type_parameters
        );
    }
    if (declaration->name_initialized) {
        ruda_owned_bytes_dispose(&declaration->owned_name);
    }
    memset(declaration, 0, sizeof(*declaration));
}

 void ruda_checker_private_enum_payload_item_declaration_dispose(
    RudaCheckEnumPayloadItemDeclaration *item
) {
    if (item == NULL) {
        return;
    }
    if (item->name_initialized) {
        ruda_owned_bytes_dispose(&item->owned_name);
    }
    memset(item, 0, sizeof(*item));
}

 void ruda_checker_private_enum_variant_declaration_dispose(
    RudaCheckEnumVariantDeclaration *variant
) {
    size_t index = 0;
    if (variant == NULL) {
        return;
    }
    if (variant->payload_initialized) {
        for (index = 0; index < variant->payload.length; index += 1) {
            RudaCheckEnumPayloadItemDeclaration *item = ruda_vector_at(
                &variant->payload,
                index
            );
            ruda_checker_private_enum_payload_item_declaration_dispose(item);
        }
        ruda_vector_dispose(&variant->payload);
    }
    if (variant->name_initialized) {
        ruda_owned_bytes_dispose(&variant->owned_name);
    }
    memset(variant, 0, sizeof(*variant));
}

void ruda_checker_private_enum_declaration_dispose(
    RudaCheckEnumDeclaration *declaration
) {
    size_t index = 0;
    if (declaration == NULL) {
        return;
    }
    if (declaration->variants_initialized) {
        for (index = 0; index < declaration->variants.length; index += 1) {
            RudaCheckEnumVariantDeclaration *variant = ruda_vector_at(
                &declaration->variants,
                index
            );
            ruda_checker_private_enum_variant_declaration_dispose(variant);
        }
        ruda_vector_dispose(&declaration->variants);
    }
    if (declaration->type_parameters_initialized) {
        ruda_checker_private_type_parameter_vector_dispose(
            &declaration->type_parameters
        );
    }
    if (declaration->name_initialized) {
        ruda_owned_bytes_dispose(&declaration->owned_name);
    }
    memset(declaration, 0, sizeof(*declaration));
}

void ruda_checker_private_interface_method_declaration_dispose(
    RudaCheckInterfaceMethodDeclaration *method
) {
    if (method == NULL) {
        return;
    }
    if (method->parameter_types_initialized) {
        ruda_vector_dispose(&method->parameter_types);
    }
    if (method->name_initialized) {
        ruda_owned_bytes_dispose(&method->owned_name);
    }
    memset(method, 0, sizeof(*method));
}

void ruda_checker_private_interface_declaration_dispose(
    RudaCheckInterfaceDeclaration *declaration
) {
    size_t index = 0;
    if (declaration == NULL) {
        return;
    }
    if (declaration->methods_initialized) {
        for (index = 0; index < declaration->methods.length; index += 1) {
            ruda_checker_private_interface_method_declaration_dispose(
                ruda_vector_at(&declaration->methods, index)
            );
        }
        ruda_vector_dispose(&declaration->methods);
    }
    if (declaration->name_initialized) {
        ruda_owned_bytes_dispose(&declaration->owned_name);
    }
    memset(declaration, 0, sizeof(*declaration));
}

 bool ruda_checker_private_declaration_name_is_duplicate(
    const RudaChecker *checker,
    RudaCheckName name
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    return ruda_checker_private_check_name_bytes(name, &bytes, &length)
        && (ruda_map_get_const(
                &checker->top_level_index,
                bytes,
                length
            ) != NULL
            || ruda_check_import_binding_exists(checker, bytes, length));
}

 bool ruda_checker_private_parameter_name_is_duplicate(
    const RudaChecker *checker,
    const RudaCheckFunctionDeclaration *declaration,
    RudaCheckName name
) {
    size_t index = 0;
    for (index = 0; index < declaration->parameter_names.length; index += 1) {
        const RudaCheckName *existing = ruda_vector_at_const(
            &declaration->parameter_names,
            index
        );
        if (existing != NULL
            && ruda_checker_private_check_names_equal(checker, *existing, name)) {
            return true;
        }
    }
    return false;
}

 bool ruda_checker_private_check_name_is_primitive_type(
    const RudaChecker *checker,
    RudaCheckName name
) {
    static const char *const names[] = {
        "bool", "i8", "i16", "i32", "i64", "int", "u8", "byte",
        "u16", "u32", "u64", "uint", "f32", "f64", "float", "rune",
        "string", "void", "never", "Array"
    };
    size_t index = 0;
    for (index = 0; index < sizeof(names) / sizeof(names[0]); index += 1) {
        if (ruda_checker_private_check_text_equal(
                checker,
                name.start,
                name.end,
                names[index]
            )) {
            return true;
        }
    }
    return false;
}

 bool ruda_checker_private_check_public_type_reference(
    RudaChecker *checker,
    const RudaAstNode *node
) {
    const RudaSyntaxToken *name_token = NULL;
    RudaCheckName name;
    RudaCheckRecordDeclaration *record = NULL;
    RudaCheckEnumDeclaration *enum_declaration = NULL;
    RudaCheckInterfaceDeclaration *interface_declaration = NULL;
    RudaCheckTypeAliasDeclaration *alias = NULL;
    const RudaAstNode *type_arguments = NULL;
    const RudaAstNode *argument_type = NULL;
    bool is_public = false;
    bool import_resolved = false;
    RudaTypeId imported_type = RUDA_TYPE_INVALID;
    if (checker == NULL || node == NULL || node->kind != RUDA_SYNTAX_TYPE) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    name_token = ruda_checker_private_check_child_token(node, RUDA_TOKEN_IDENTIFIER, 0);
    if (name_token == NULL || name_token->synthetic) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    name.snapshot = checker->ast->snapshot;
    name.start = name_token->start;
    name.end = name_token->end;
    type_arguments = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_TYPE_ARGUMENTS,
        0
    );
    if (ruda_checker_private_find_type_parameter(checker, name) != NULL) {
        if (type_arguments != NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        return true;
    }
    if (ruda_checker_private_check_text_equal(
            checker,
            name.start,
            name.end,
            "Array"
        )) {
        type_arguments = ruda_checker_private_check_child_node(
            node,
            RUDA_SYNTAX_TYPE_ARGUMENTS,
            0
        );
        argument_type = ruda_checker_private_check_child_node(
            type_arguments,
            RUDA_SYNTAX_TYPE,
            0
        );
        if (type_arguments == NULL || argument_type == NULL
            || ruda_checker_private_check_child_node(
                type_arguments,
                RUDA_SYNTAX_TYPE,
                1
            ) != NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        return ruda_checker_private_check_public_type_reference(checker, argument_type);
    }
    if (!ruda_check_import_type(
            checker, node, &imported_type, &import_resolved
        )) {
        return false;
    }
    if (import_resolved) {
        return imported_type != RUDA_TYPE_INVALID;
    }
    if (type_arguments != NULL) {
        size_t argument_index = 0;
        bool arguments_public = true;
        record = ruda_checker_private_checker_find_record(checker, name);
        enum_declaration = ruda_checker_private_checker_find_enum(checker, name);
        if (record != NULL) {
            is_public = record->is_public;
        } else if (enum_declaration != NULL) {
            is_public = enum_declaration->is_public;
        } else {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        for (argument_index = 0; argument_index < type_arguments->child_count;
            argument_index += 1) {
            const RudaAstElement *element = ruda_ast_child(
                type_arguments, argument_index
            );
            if (element == NULL) {
                ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
                return false;
            }
            if (element->kind == RUDA_AST_ELEMENT_TOKEN) {
                if (element->as.token.synthetic) {
                    ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
                    return false;
                }
                continue;
            }
            if (element->kind != RUDA_AST_ELEMENT_NODE
                || element->as.node == NULL
                || element->as.node->kind != RUDA_SYNTAX_TYPE
                || !ruda_checker_private_check_public_type_reference(
                    checker, element->as.node
                )) {
                arguments_public = false;
                if (ruda_checker_has_failed(checker)) {
                    return false;
                }
            }
        }
        if (!is_public) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
                "public declaration exposes a package-private type",
                name.start,
                name.end
            );
            return false;
        }
        return arguments_public;
    }
    if (node->child_count != 1) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (ruda_checker_private_check_name_is_primitive_type(checker, name)) {
        return true;
    }
    record = ruda_checker_private_checker_find_record(checker, name);
    if (record != NULL) {
        is_public = record->is_public;
    } else {
        enum_declaration = ruda_checker_private_checker_find_enum(checker, name);
        if (enum_declaration != NULL) {
            is_public = enum_declaration->is_public;
        } else {
            interface_declaration =
                ruda_checker_private_checker_find_interface(checker, name);
            if (interface_declaration != NULL) {
                is_public = interface_declaration->is_public;
            } else {
                alias = ruda_checker_private_checker_find_type_alias(checker, name);
                if (alias == NULL) {
                    ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
                    return false;
                }
                is_public = alias->is_public;
            }
        }
    }
    if (!is_public) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
            "public declaration exposes a package-private type",
            name.start,
            name.end
        );
        return false;
    }
    return true;
}
