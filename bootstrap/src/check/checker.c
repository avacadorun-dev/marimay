#include "checker_api.h"
#include "checker_core_api.h"
#include "checker_declaration_constants_api.h"
#include "checker_declaration_enums_api.h"
#include "checker_declaration_functions_api.h"
#include "checker_declaration_interfaces_api.h"
#include "checker_declaration_nominal_api.h"
#include "checker_declaration_publication_nominal_api.h"
#include "checker_declaration_records_api.h"
#include "checker_publication_api.h"
#include "package_imports_api.h"






 int ruda_checker_private_check_compare_ast_paths(const void *left, const void *right) {
    const RudaAstTree *left_ast = *(const RudaAstTree *const *)left;
    const RudaAstTree *right_ast = *(const RudaAstTree *const *)right;
    const RudaSourceSnapshot *left_snapshot = left_ast->snapshot;
    const RudaSourceSnapshot *right_snapshot = right_ast->snapshot;
    const unsigned char *left_path = (const unsigned char *)ruda_source_path(
        left_snapshot
    );
    const unsigned char *right_path = (const unsigned char *)ruda_source_path(
        right_snapshot
    );
    size_t left_length = ruda_source_path_length(left_snapshot);
    size_t right_length = ruda_source_path_length(right_snapshot);
    size_t common_length = left_length < right_length
        ? left_length
        : right_length;
    int comparison = memcmp(left_path, right_path, common_length);
    if (comparison != 0) {
        return comparison;
    }
    if (left_length < right_length) {
        return -1;
    }
    return left_length > right_length ? 1 : 0;
}

 bool ruda_checker_private_check_bounded_c_string_length(
    const char *text,
    size_t maximum,
    size_t *length
) {
    size_t index = 0;
    if (text == NULL || length == NULL) {
        return false;
    }
    for (index = 0; index <= maximum; index += 1) {
        if (text[index] == '\0') {
            *length = index;
            return true;
        }
        if (index == SIZE_MAX) {
            return false;
        }
    }
    return false;
}

bool ruda_check_options_valid(
    RudaAllocator allocator,
    RudaCheckOptions options
) {
    return allocator.allocate != NULL && allocator.resize != NULL
        && allocator.deallocate != NULL && options.max_source_files != 0
        && options.max_functions != 0 && options.max_symbols != 0
        && options.max_package_name_bytes != 0
        && options.max_constants != 0
        && options.max_type_aliases != 0
        && options.max_records != 0 && options.max_fields_per_record != 0
        && options.max_enums != 0 && options.max_variants_per_enum != 0
        && options.max_payload_items_per_variant != 0
        && options.max_interfaces != 0 && options.max_methods_per_interface != 0
        && options.max_parameters_per_interface_method != 0
        && options.max_locals_per_function != 0
        && options.max_diagnostics != 0 && options.max_constant_steps != 0
        && options.max_constant_depth != 0
        && options.max_type_alias_depth != 0
        && options.max_float_literal_digits != 0
        && options.type_options.max_types >= RUDA_TYPE_FIRST_DYNAMIC
        && options.type_options.max_types <= UINT32_MAX
        && options.type_options.max_type_arguments != 0
        && options.mir_options.max_registers != 0
        && options.mir_options.max_registers <= UINT32_MAX
        && options.mir_options.max_blocks != 0
        && options.mir_options.max_blocks <= UINT32_MAX
        && options.mir_options.max_instructions != 0
        && options.mir_options.max_calls != 0
        && options.mir_options.max_calls <= UINT32_MAX
        && options.mir_options.max_call_arguments != 0
        && options.mir_options.max_record_layouts != 0
        && options.mir_options.max_record_layouts <= UINT32_MAX
        && options.mir_options.max_record_fields != 0
        && options.mir_options.max_record_constructions != 0
        && options.mir_options.max_record_constructions <= UINT32_MAX
        && options.mir_options.max_record_arguments != 0
        && options.mir_options.max_enum_layouts != 0
        && options.mir_options.max_enum_layouts <= UINT32_MAX
        && options.mir_options.max_enum_variants != 0
        && options.mir_options.max_enum_payloads != 0
        && options.mir_options.max_enum_constructions != 0
        && options.mir_options.max_enum_constructions <= UINT32_MAX
        && options.mir_options.max_enum_arguments != 0
        && options.mir_options.max_string_constants != 0
        && options.mir_options.max_string_constants <= UINT32_MAX
        && options.mir_options.max_string_bytes != 0
        && options.mir_options.max_verify_work != 0;
}

 bool ruda_checker_private_check_ast_valid(const RudaAstTree *ast) {
    return ast != NULL && ast->initialized && ast->root != NULL
        && ast->root->kind == RUDA_SYNTAX_ROOT && ast->snapshot != NULL
        && ruda_source_path(ast->snapshot) != NULL
        && ruda_source_path_length(ast->snapshot) != 0
        && (ruda_source_bytes(ast->snapshot) != NULL
            || ruda_source_length(ast->snapshot) == 0);
}

RudaCheckStatus ruda_checker_check_package_unit(
    const RudaAstTree *const *asts,
    size_t ast_count,
    const char *package_name,
    size_t package_name_length,
    const RudaCheckImportEnvironment *imports,
    bool initialize_result,
    RudaAllocator allocator,
    RudaCheckOptions options,
    RudaCheckResult *result
) {
    RudaChecker checker;
    RudaVector sorted_asts;
    bool sorted_asts_initialized = false;
    bool declarations_initialized = false;
    bool constant_declarations_initialized = false;
    bool type_alias_declarations_initialized = false;
    bool record_declarations_initialized = false;
    bool enum_declarations_initialized = false;
    bool interface_declarations_initialized = false;
    bool top_level_index_initialized = false;
    bool local_nominal_index_initialized = false;
    bool callable_index_initialized = false;
    size_t ast_index = 0;
    size_t index = 0;
    if (result == NULL) {
        return RUDA_CHECK_INVALID_ARGUMENT;
    }
    if (initialize_result) {
        memset(result, 0, sizeof(*result));
    }
    if (asts == NULL || ast_count == 0 || package_name == NULL
        || package_name_length == 0
        || !ruda_check_options_valid(allocator, options)
        || (!initialize_result && !result->initialized)) {
        return RUDA_CHECK_INVALID_ARGUMENT;
    }
    if (package_name_length > options.max_package_name_bytes) {
        return RUDA_CHECK_LIMIT_REACHED;
    }
    if (ast_count > options.max_source_files) {
        return RUDA_CHECK_LIMIT_REACHED;
    }
    sorted_asts_initialized = ruda_vector_init(
        &sorted_asts,
        allocator,
        sizeof(RudaAstTree *)
    );
    if (!sorted_asts_initialized) {
        return RUDA_CHECK_INVALID_ARGUMENT;
    }
    for (ast_index = 0; ast_index < ast_count; ast_index += 1) {
        const RudaAstTree *ast = asts[ast_index];
        if (!ruda_checker_private_check_ast_valid(ast)) {
            ruda_vector_dispose(&sorted_asts);
            return RUDA_CHECK_INVALID_ARGUMENT;
        }
        if (!ruda_vector_push(&sorted_asts, &ast)) {
            ruda_vector_dispose(&sorted_asts);
            return RUDA_CHECK_ALLOCATION_FAILED;
        }
    }
    qsort(
        sorted_asts.data,
        sorted_asts.length,
        sorted_asts.element_size,
        ruda_checker_private_check_compare_ast_paths
    );
    for (ast_index = 1; ast_index < sorted_asts.length; ast_index += 1) {
        const RudaAstTree *const *previous = ruda_vector_at_const(
            &sorted_asts,
            ast_index - 1
        );
        const RudaAstTree *const *current = ruda_vector_at_const(
            &sorted_asts,
            ast_index
        );
        if (previous == NULL || current == NULL
            || ruda_checker_private_check_compare_ast_paths(previous, current) == 0) {
            ruda_vector_dispose(&sorted_asts);
            return RUDA_CHECK_INVALID_ARGUMENT;
        }
    }
    memset(&checker, 0, sizeof(checker));
    checker.package_name = package_name;
    checker.package_name_length = package_name_length;
    checker.imports = imports;
    checker.options = options;
    checker.result = result;
    checker.failure = RUDA_CHECK_OK;
    checker.remaining_constant_steps = options.max_constant_steps;
    if (initialize_result && !ruda_check_result_init(
            result,
            allocator,
            options,
            (*(const RudaAstTree *const *)sorted_asts.data)->snapshot,
            package_name,
            package_name_length
        )) {
        ruda_vector_dispose(&sorted_asts);
        return RUDA_CHECK_ALLOCATION_FAILED;
    }
    if (initialize_result) {
        checker.package_name = (const char *)result->package_name.data;
    }
    declarations_initialized = ruda_vector_init(
        &checker.declarations,
        allocator,
        sizeof(RudaCheckFunctionDeclaration)
    );
    constant_declarations_initialized = ruda_vector_init(
        &checker.constants,
        allocator,
        sizeof(RudaCheckConstantDeclaration)
    );
    type_alias_declarations_initialized = ruda_vector_init(
        &checker.type_aliases,
        allocator,
        sizeof(RudaCheckTypeAliasDeclaration)
    );
    record_declarations_initialized = ruda_vector_init(
        &checker.records,
        allocator,
        sizeof(RudaCheckRecordDeclaration)
    );
    enum_declarations_initialized = ruda_vector_init(
        &checker.enums,
        allocator,
        sizeof(RudaCheckEnumDeclaration)
    );
    interface_declarations_initialized = ruda_vector_init(
        &checker.interfaces,
        allocator,
        sizeof(RudaCheckInterfaceDeclaration)
    );
    top_level_index_initialized = ruda_checker_private_checker_top_level_index_init(
        &checker
    );
    local_nominal_index_initialized = ruda_checker_private_checker_local_nominal_index_init(
        &checker
    );
    callable_index_initialized = ruda_check_callable_index_init(&checker);
    if (!declarations_initialized || !constant_declarations_initialized
        || !type_alias_declarations_initialized
        || !record_declarations_initialized
        || !enum_declarations_initialized || !interface_declarations_initialized
        || !top_level_index_initialized || !local_nominal_index_initialized
        || !callable_index_initialized) {
        ruda_checker_private_checker_dispose_declarations(&checker);
        ruda_check_result_dispose(result);
        ruda_vector_dispose(&sorted_asts);
        return RUDA_CHECK_ALLOCATION_FAILED;
    }
    for (ast_index = 0; ast_index < sorted_asts.length
        && !ruda_checker_has_failed(&checker); ast_index += 1) {
        const RudaAstTree *const *ast = ruda_vector_at_const(
            &sorted_asts,
            ast_index
        );
        if (ast == NULL || !ruda_checker_private_checker_set_ast(&checker, *ast)) {
            ruda_checker_fail(&checker, RUDA_CHECK_INTERNAL_ERROR);
            break;
        }
        for (index = 0; index < (*ast)->root->child_count
            && !ruda_checker_has_failed(&checker); index += 1) {
            const RudaAstElement *element = ruda_ast_child(
                (*ast)->root,
                index
            );
            if (element == NULL || element->kind != RUDA_AST_ELEMENT_NODE
                || element->as.node == NULL) {
                ruda_checker_fail(&checker, RUDA_CHECK_INVALID_ARGUMENT);
                break;
            }
            if (element->as.node->kind == RUDA_SYNTAX_TYPE_ALIAS_DECL) {
                (void)ruda_checker_private_collect_type_alias_declaration(
                    &checker,
                    element->as.node
                );
            } else if (element->as.node->kind == RUDA_SYNTAX_RECORD_DECL) {
                (void)ruda_checker_private_collect_record_declaration(
                    &checker,
                    element->as.node
                );
            } else if (element->as.node->kind == RUDA_SYNTAX_ENUM_DECL) {
                (void)ruda_checker_private_collect_enum_declaration(
                    &checker,
                    element->as.node
                );
            } else if (element->as.node->kind == RUDA_SYNTAX_INTERFACE_DECL) {
                (void)ruda_checker_private_collect_interface_declaration(
                    &checker,
                    element->as.node
                );
            }
        }
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_checker_private_evaluate_all_type_aliases(&checker);
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_checker_private_resolve_all_record_fields(&checker);
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_checker_private_resolve_all_enum_variants(&checker);
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_checker_private_resolve_all_interface_methods(&checker);
    }
    for (ast_index = 0; ast_index < sorted_asts.length
        && !ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0; ast_index += 1) {
        const RudaAstTree *const *ast = ruda_vector_at_const(
            &sorted_asts,
            ast_index
        );
        if (ast == NULL || !ruda_checker_private_checker_set_ast(&checker, *ast)) {
            ruda_checker_fail(&checker, RUDA_CHECK_INTERNAL_ERROR);
            break;
        }
        for (index = 0; index < (*ast)->root->child_count
            && !ruda_checker_has_failed(&checker); index += 1) {
            const RudaAstElement *element = ruda_ast_child(
                (*ast)->root,
                index
            );
            if (element == NULL || element->kind != RUDA_AST_ELEMENT_NODE
                || element->as.node == NULL) {
                ruda_checker_fail(&checker, RUDA_CHECK_INVALID_ARGUMENT);
                break;
            }
            if (element->as.node->kind == RUDA_SYNTAX_FUNCTION_DECL) {
                (void)ruda_checker_private_collect_function_signature(
                    &checker,
                    element->as.node
                );
            } else if (element->as.node->kind == RUDA_SYNTAX_CONST_DECL) {
                (void)ruda_checker_private_collect_constant_declaration(
                    &checker,
                    element->as.node
                );
            } else if (element->as.node->kind
                == RUDA_SYNTAX_TYPE_ALIAS_DECL) {
                continue;
            } else if (element->as.node->kind == RUDA_SYNTAX_RECORD_DECL) {
                continue;
            } else if (element->as.node->kind == RUDA_SYNTAX_ENUM_DECL) {
                continue;
            } else if (element->as.node->kind == RUDA_SYNTAX_INTERFACE_DECL) {
                continue;
            } else if (element->as.node->kind == RUDA_SYNTAX_IMPORT_DECL) {
                continue;
            } else {
                ruda_checker_fail(&checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
                break;
            }
        }
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        size_t symbol_total = 0;
        if (!ruda_checker_private_check_symbol_fact_total(&checker, &symbol_total)
            || symbol_total > checker.options.max_symbols) {
            ruda_checker_fail(&checker, RUDA_CHECK_LIMIT_REACHED);
        }
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_checker_private_evaluate_all_constants(&checker);
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_checker_private_publish_constants(&checker);
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        for (index = 0; index < checker.declarations.length
            && !ruda_checker_has_failed(&checker); index += 1) {
            RudaCheckFunctionDeclaration *declaration = ruda_vector_at(
                &checker.declarations,
                index
            );
            if (declaration == NULL || !declaration->signature_valid) {
                ruda_checker_fail(&checker, RUDA_CHECK_INTERNAL_ERROR);
                break;
            }
            (void)ruda_checker_private_check_function_body(&checker, declaration);
        }
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_checker_private_publish_type_aliases(&checker);
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_checker_private_publish_records(&checker);
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_checker_private_publish_enums(&checker);
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_checker_private_publish_interfaces(&checker);
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_checker_private_publish_symbol_facts(&checker);
    }
    if (result->diagnostics.length != 0) {
        if (!ruda_check_clear_semantics(result)) {
            ruda_checker_fail(&checker, RUDA_CHECK_ALLOCATION_FAILED);
        }
    }
    ruda_checker_private_checker_dispose_declarations(&checker);
    ruda_vector_dispose(&sorted_asts);
    if (checker.failure != RUDA_CHECK_OK) {
        RudaCheckStatus status = checker.failure;
        ruda_check_result_dispose(result);
        return status;
    }
    return RUDA_CHECK_OK;
}

RudaCheckStatus ruda_check_package(
    const RudaAstTree *const *asts,
    size_t ast_count,
    const char *package_name,
    RudaAllocator allocator,
    RudaCheckOptions options,
    RudaCheckResult *result
) {
    size_t package_name_length = 0;
    if (result == NULL) {
        return RUDA_CHECK_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (!ruda_checker_private_check_bounded_c_string_length(
            package_name,
            options.max_package_name_bytes,
            &package_name_length
        )) {
        return RUDA_CHECK_LIMIT_REACHED;
    }
    return ruda_checker_check_package_unit(
        asts,
        ast_count,
        package_name,
        package_name_length,
        NULL,
        true,
        allocator,
        options,
        result
    );
}

RudaCheckStatus ruda_check_ast(
    const RudaAstTree *ast,
    const char *package_name,
    RudaAllocator allocator,
    RudaCheckOptions options,
    RudaCheckResult *result
) {
    const RudaAstTree *asts[1];
    asts[0] = ast;
    return ruda_check_package(
        asts,
        1,
        package_name,
        allocator,
        options,
        result
    );
}
