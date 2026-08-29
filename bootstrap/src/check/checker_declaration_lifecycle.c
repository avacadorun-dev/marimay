#include "checker_declaration_lifecycle_api.h"
#include "package_imports_api.h"
#include "checker_core_api.h"
#include "checker_generic_types_api.h"
#include "checker_array_types_api.h"

 void mmay_checker_private_function_declaration_dispose(
    MarimayCheckFunctionDeclaration *declaration
) {
    if (declaration == NULL) {
        return;
    }
    if (declaration->parameter_types_initialized) {
        mmay_vector_dispose(&declaration->parameter_types);
    }
    if (declaration->generic_arguments_initialized) {
        mmay_vector_dispose(&declaration->generic_arguments);
    }
    if (declaration->type_parameters_initialized) {
        mmay_checker_private_type_parameter_vector_dispose(
            &declaration->type_parameters
        );
    }
    if (declaration->parameter_names_initialized) {
        mmay_vector_dispose(&declaration->parameter_names);
    }
    if (declaration->name_initialized) {
        mmay_owned_bytes_dispose(&declaration->owned_name);
    }
    memset(declaration, 0, sizeof(*declaration));
}

void mmay_checker_private_type_parameter_vector_dispose(
    MarimayVector *parameters
) {
    size_t index = 0;
    if (parameters == NULL) {
        return;
    }
    for (index = 0; index < parameters->length; index += 1) {
        MarimayCheckTypeParameterDeclaration *parameter = mmay_vector_at(
            parameters, index
        );
        if (parameter != NULL && parameter->name_initialized) {
            mmay_owned_bytes_dispose(&parameter->owned_name);
        }
        if (parameter != NULL && parameter->constraints_initialized) {
            mmay_vector_dispose(&parameter->constraints);
        }
    }
    mmay_vector_dispose(parameters);
}

 void mmay_checker_private_constant_declaration_dispose(
    MarimayCheckConstantDeclaration *declaration
) {
    if (declaration == NULL) {
        return;
    }
    if (declaration->name_initialized) {
        mmay_owned_bytes_dispose(&declaration->owned_name);
    }
    memset(declaration, 0, sizeof(*declaration));
}

 void mmay_checker_private_type_alias_declaration_dispose(
    MarimayCheckTypeAliasDeclaration *declaration
) {
    if (declaration == NULL) {
        return;
    }
    if (declaration->name_initialized) {
        mmay_owned_bytes_dispose(&declaration->owned_name);
    }
    memset(declaration, 0, sizeof(*declaration));
}

 void mmay_checker_private_record_field_declaration_dispose(
    MarimayCheckRecordFieldDeclaration *field
) {
    if (field == NULL) {
        return;
    }
    if (field->name_initialized) {
        mmay_owned_bytes_dispose(&field->owned_name);
    }
    memset(field, 0, sizeof(*field));
}

 void mmay_checker_private_record_declaration_dispose(
    MarimayCheckRecordDeclaration *declaration
) {
    size_t index = 0;
    if (declaration == NULL) {
        return;
    }
    if (declaration->fields_initialized) {
        for (index = 0; index < declaration->fields.length; index += 1) {
            MarimayCheckRecordFieldDeclaration *field = mmay_vector_at(
                &declaration->fields,
                index
            );
            mmay_checker_private_record_field_declaration_dispose(field);
        }
        mmay_vector_dispose(&declaration->fields);
    }
    if (declaration->type_parameters_initialized) {
        mmay_checker_private_type_parameter_vector_dispose(
            &declaration->type_parameters
        );
    }
    if (declaration->name_initialized) {
        mmay_owned_bytes_dispose(&declaration->owned_name);
    }
    memset(declaration, 0, sizeof(*declaration));
}

 void mmay_checker_private_enum_payload_item_declaration_dispose(
    MarimayCheckEnumPayloadItemDeclaration *item
) {
    if (item == NULL) {
        return;
    }
    if (item->name_initialized) {
        mmay_owned_bytes_dispose(&item->owned_name);
    }
    memset(item, 0, sizeof(*item));
}

 void mmay_checker_private_enum_variant_declaration_dispose(
    MarimayCheckEnumVariantDeclaration *variant
) {
    size_t index = 0;
    if (variant == NULL) {
        return;
    }
    if (variant->payload_initialized) {
        for (index = 0; index < variant->payload.length; index += 1) {
            MarimayCheckEnumPayloadItemDeclaration *item = mmay_vector_at(
                &variant->payload,
                index
            );
            mmay_checker_private_enum_payload_item_declaration_dispose(item);
        }
        mmay_vector_dispose(&variant->payload);
    }
    if (variant->name_initialized) {
        mmay_owned_bytes_dispose(&variant->owned_name);
    }
    memset(variant, 0, sizeof(*variant));
}

void mmay_checker_private_enum_declaration_dispose(
    MarimayCheckEnumDeclaration *declaration
) {
    size_t index = 0;
    if (declaration == NULL) {
        return;
    }
    if (declaration->variants_initialized) {
        for (index = 0; index < declaration->variants.length; index += 1) {
            MarimayCheckEnumVariantDeclaration *variant = mmay_vector_at(
                &declaration->variants,
                index
            );
            mmay_checker_private_enum_variant_declaration_dispose(variant);
        }
        mmay_vector_dispose(&declaration->variants);
    }
    if (declaration->type_parameters_initialized) {
        mmay_checker_private_type_parameter_vector_dispose(
            &declaration->type_parameters
        );
    }
    if (declaration->name_initialized) {
        mmay_owned_bytes_dispose(&declaration->owned_name);
    }
    memset(declaration, 0, sizeof(*declaration));
}

void mmay_checker_private_interface_method_declaration_dispose(
    MarimayCheckInterfaceMethodDeclaration *method
) {
    if (method == NULL) {
        return;
    }
    if (method->parameter_types_initialized) {
        mmay_vector_dispose(&method->parameter_types);
    }
    if (method->name_initialized) {
        mmay_owned_bytes_dispose(&method->owned_name);
    }
    memset(method, 0, sizeof(*method));
}

void mmay_checker_private_interface_declaration_dispose(
    MarimayCheckInterfaceDeclaration *declaration
) {
    size_t index = 0;
    if (declaration == NULL) {
        return;
    }
    if (declaration->methods_initialized) {
        for (index = 0; index < declaration->methods.length; index += 1) {
            mmay_checker_private_interface_method_declaration_dispose(
                mmay_vector_at(&declaration->methods, index)
            );
        }
        mmay_vector_dispose(&declaration->methods);
    }
    if (declaration->name_initialized) {
        mmay_owned_bytes_dispose(&declaration->owned_name);
    }
    memset(declaration, 0, sizeof(*declaration));
}

 bool mmay_checker_private_declaration_name_is_duplicate(
    const MarimayChecker *checker,
    MarimayCheckName name
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    return mmay_checker_private_check_name_bytes(name, &bytes, &length)
        && (mmay_map_get_const(
                &checker->top_level_index,
                bytes,
                length
            ) != NULL
            || mmay_check_import_binding_exists(checker, bytes, length));
}

 bool mmay_checker_private_parameter_name_is_duplicate(
    const MarimayChecker *checker,
    const MarimayCheckFunctionDeclaration *declaration,
    MarimayCheckName name
) {
    size_t index = 0;
    for (index = 0; index < declaration->parameter_names.length; index += 1) {
        const MarimayCheckName *existing = mmay_vector_at_const(
            &declaration->parameter_names,
            index
        );
        if (existing != NULL
            && mmay_checker_private_check_names_equal(checker, *existing, name)) {
            return true;
        }
    }
    return false;
}

 bool mmay_checker_private_check_name_is_primitive_type(
    const MarimayChecker *checker,
    MarimayCheckName name
) {
    static const char *const names[] = {
        "bool", "i8", "i16", "i32", "i64", "int", "u8", "byte",
        "u16", "u32", "u64", "uint", "f32", "f64", "float", "rune",
        "string", "void", "never"
    };
    size_t index = 0;
    for (index = 0; index < sizeof(names) / sizeof(names[0]); index += 1) {
        if (mmay_checker_private_check_text_equal(
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

 bool mmay_checker_private_check_public_type_reference(
    MarimayChecker *checker,
    const MarimayAstNode *node
) {
    const MarimaySyntaxToken *name_token = NULL;
    MarimayCheckName name;
    MarimayCheckRecordDeclaration *record = NULL;
    MarimayCheckEnumDeclaration *enum_declaration = NULL;
    MarimayCheckInterfaceDeclaration *interface_declaration = NULL;
    MarimayCheckTypeAliasDeclaration *alias = NULL;
    const MarimayAstNode *type_arguments = NULL;
    const MarimayAstNode *array_element = NULL;
    bool is_public = false;
    bool import_resolved = false;
    MarimayTypeId imported_type = MMAY_TYPE_INVALID;
    if (checker == NULL || node == NULL || node->kind != MMAY_SYNTAX_TYPE) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    array_element = mmay_checker_private_array_type_element(node);
    if (array_element != NULL) {
        return mmay_checker_private_check_public_type_reference(
            checker, array_element
        );
    }
    name_token = mmay_checker_private_check_child_token(node, MMAY_TOKEN_IDENTIFIER, 0);
    if (name_token == NULL) {
        size_t child_index = 0;
        bool found_type = false;
        bool all_public = true;
        for (child_index = 0; child_index < node->child_count;
            child_index += 1) {
            const MarimayAstElement *child = mmay_ast_child(node, child_index);
            if (child != NULL && child->kind == MMAY_AST_ELEMENT_NODE
                && child->as.node != NULL
                && child->as.node->kind == MMAY_SYNTAX_TYPE) {
                found_type = true;
                if (!mmay_checker_private_check_public_type_reference(
                        checker, child->as.node
                    )) {
                    all_public = false;
                    if (mmay_checker_has_failed(checker)) {
                        return false;
                    }
                }
            }
        }
        if (found_type) {
            return all_public;
        }
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (name_token->synthetic) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    name.snapshot = checker->ast->snapshot;
    name.start = name_token->start;
    name.end = name_token->end;
    type_arguments = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_TYPE_ARGUMENTS,
        0
    );
    if (mmay_checker_private_find_type_parameter(checker, name) != NULL) {
        if (type_arguments != NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        return true;
    }
    if (!mmay_check_import_type(
            checker, node, &imported_type, &import_resolved
        )) {
        return false;
    }
    if (import_resolved) {
        return imported_type != MMAY_TYPE_INVALID;
    }
    if (type_arguments != NULL) {
        size_t argument_index = 0;
        bool arguments_public = true;
        record = mmay_checker_private_checker_find_record(checker, name);
        enum_declaration = mmay_checker_private_checker_find_enum(checker, name);
        if (mmay_checker_private_check_text_equal(
                checker, name.start, name.end, "Map"
            ) || mmay_checker_private_check_text_equal(
                checker, name.start, name.end, "Set"
            )) {
            is_public = true;
        } else if (record != NULL) {
            is_public = record->is_public;
        } else if (enum_declaration != NULL) {
            is_public = enum_declaration->is_public;
        } else {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        for (argument_index = 0; argument_index < type_arguments->child_count;
            argument_index += 1) {
            const MarimayAstElement *element = mmay_ast_child(
                type_arguments, argument_index
            );
            if (element == NULL) {
                mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
                return false;
            }
            if (element->kind == MMAY_AST_ELEMENT_TOKEN) {
                if (element->as.token.synthetic) {
                    mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
                    return false;
                }
                continue;
            }
            if (element->kind != MMAY_AST_ELEMENT_NODE
                || element->as.node == NULL
                || element->as.node->kind != MMAY_SYNTAX_TYPE
                || !mmay_checker_private_check_public_type_reference(
                    checker, element->as.node
                )) {
                arguments_public = false;
                if (mmay_checker_has_failed(checker)) {
                    return false;
                }
            }
        }
        if (!is_public) {
            (void)mmay_checker_add_diagnostic(
                checker,
                MMAY_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
                "public declaration exposes a package-private type",
                name.start,
                name.end
            );
            return false;
        }
        return arguments_public;
    }
    if (node->child_count != 1) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (mmay_checker_private_check_name_is_primitive_type(checker, name)) {
        return true;
    }
    record = mmay_checker_private_checker_find_record(checker, name);
    if (record != NULL) {
        is_public = record->is_public;
    } else {
        enum_declaration = mmay_checker_private_checker_find_enum(checker, name);
        if (enum_declaration != NULL) {
            is_public = enum_declaration->is_public;
        } else {
            interface_declaration =
                mmay_checker_private_checker_find_interface(checker, name);
            if (interface_declaration != NULL) {
                is_public = interface_declaration->is_public;
            } else {
                alias = mmay_checker_private_checker_find_type_alias(checker, name);
                if (alias == NULL) {
                    mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
                    return false;
                }
                is_public = alias->is_public;
            }
        }
    }
    if (!is_public) {
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
            "public declaration exposes a package-private type",
            name.start,
            name.end
        );
        return false;
    }
    return true;
}
