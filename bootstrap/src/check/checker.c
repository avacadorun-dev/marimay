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
#include "checker_prelude_api.h"
#include "checker_imported_enums_api.h"
#include "package_imports_api.h"

 int mmay_checker_private_check_compare_ast_paths(const void *left, const void *right) {
    const MarimayAstTree *left_ast = *(const MarimayAstTree *const *)left;
    const MarimayAstTree *right_ast = *(const MarimayAstTree *const *)right;
    const MarimaySourceSnapshot *left_snapshot = left_ast->snapshot;
    const MarimaySourceSnapshot *right_snapshot = right_ast->snapshot;
    const unsigned char *left_path = (const unsigned char *)mmay_source_path(
        left_snapshot
    );
    const unsigned char *right_path = (const unsigned char *)mmay_source_path(
        right_snapshot
    );
    size_t left_length = mmay_source_path_length(left_snapshot);
    size_t right_length = mmay_source_path_length(right_snapshot);
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

 bool mmay_checker_private_check_bounded_c_string_length(
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

bool mmay_check_options_valid(
    MarimayAllocator allocator,
    MarimayCheckOptions options
) {
    return allocator.allocate != NULL && allocator.resize != NULL
        && allocator.deallocate != NULL && options.max_source_files != 0
        && options.max_functions != 0 && options.max_symbols != 0
        && options.max_occurrences != 0 && options.max_completions != 0
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
        && options.type_options.max_types >= MMAY_TYPE_FIRST_DYNAMIC
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

 bool mmay_checker_private_check_ast_valid(const MarimayAstTree *ast) {
    return ast != NULL && ast->initialized && ast->root != NULL
        && ast->root->kind == MMAY_SYNTAX_ROOT && ast->snapshot != NULL
        && mmay_source_path(ast->snapshot) != NULL
        && mmay_source_path_length(ast->snapshot) != 0
        && (mmay_source_bytes(ast->snapshot) != NULL
            || mmay_source_length(ast->snapshot) == 0);
}

MarimayCheckStatus mmay_checker_check_package_unit(
    const MarimayAstTree *const *asts,
    size_t ast_count,
    const char *package_name,
    size_t package_name_length,
    const MarimayCheckImportEnvironment *imports,
    bool initialize_result,
    MarimayAllocator allocator,
    MarimayCheckOptions options,
    MarimayCheckResult *result
) {
    MarimayChecker checker;
    MarimayVector sorted_asts;
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
        return MMAY_CHECK_INVALID_ARGUMENT;
    }
    if (initialize_result) {
        memset(result, 0, sizeof(*result));
    }
    if (asts == NULL || ast_count == 0 || package_name == NULL
        || package_name_length == 0
        || !mmay_check_options_valid(allocator, options)
        || (!initialize_result && !result->initialized)) {
        return MMAY_CHECK_INVALID_ARGUMENT;
    }
    if (package_name_length > options.max_package_name_bytes) {
        return MMAY_CHECK_LIMIT_REACHED;
    }
    if (ast_count > options.max_source_files) {
        return MMAY_CHECK_LIMIT_REACHED;
    }
    sorted_asts_initialized = mmay_vector_init(
        &sorted_asts,
        allocator,
        sizeof(MarimayAstTree *)
    );
    if (!sorted_asts_initialized) {
        return MMAY_CHECK_INVALID_ARGUMENT;
    }
    for (ast_index = 0; ast_index < ast_count; ast_index += 1) {
        const MarimayAstTree *ast = asts[ast_index];
        if (!mmay_checker_private_check_ast_valid(ast)) {
            mmay_vector_dispose(&sorted_asts);
            return MMAY_CHECK_INVALID_ARGUMENT;
        }
        if (!mmay_vector_push(&sorted_asts, &ast)) {
            mmay_vector_dispose(&sorted_asts);
            return MMAY_CHECK_ALLOCATION_FAILED;
        }
    }
    qsort(
        sorted_asts.data,
        sorted_asts.length,
        sorted_asts.element_size,
        mmay_checker_private_check_compare_ast_paths
    );
    for (ast_index = 1; ast_index < sorted_asts.length; ast_index += 1) {
        const MarimayAstTree *const *previous = mmay_vector_at_const(
            &sorted_asts,
            ast_index - 1
        );
        const MarimayAstTree *const *current = mmay_vector_at_const(
            &sorted_asts,
            ast_index
        );
        if (previous == NULL || current == NULL
            || mmay_checker_private_check_compare_ast_paths(previous, current) == 0) {
            mmay_vector_dispose(&sorted_asts);
            return MMAY_CHECK_INVALID_ARGUMENT;
        }
    }
    memset(&checker, 0, sizeof(checker));
    checker.package_name = package_name;
    checker.package_name_length = package_name_length;
    checker.imports = imports;
    checker.options = options;
    checker.result = result;
    checker.failure = MMAY_CHECK_OK;
    checker.remaining_constant_steps = options.max_constant_steps;
    if (initialize_result && !mmay_check_result_init(
            result,
            allocator,
            options,
            (*(const MarimayAstTree *const *)sorted_asts.data)->snapshot,
            package_name,
            package_name_length
        )) {
        mmay_vector_dispose(&sorted_asts);
        return MMAY_CHECK_ALLOCATION_FAILED;
    }
    if (initialize_result) {
        checker.package_name = (const char *)result->package_name.data;
    }
    declarations_initialized = mmay_vector_init(
        &checker.declarations,
        allocator,
        sizeof(MarimayCheckFunctionDeclaration)
    );
    constant_declarations_initialized = mmay_vector_init(
        &checker.constants,
        allocator,
        sizeof(MarimayCheckConstantDeclaration)
    );
    type_alias_declarations_initialized = mmay_vector_init(
        &checker.type_aliases,
        allocator,
        sizeof(MarimayCheckTypeAliasDeclaration)
    );
    record_declarations_initialized = mmay_vector_init(
        &checker.records,
        allocator,
        sizeof(MarimayCheckRecordDeclaration)
    );
    enum_declarations_initialized = mmay_vector_init(
        &checker.enums,
        allocator,
        sizeof(MarimayCheckEnumDeclaration)
    );
    interface_declarations_initialized = mmay_vector_init(
        &checker.interfaces,
        allocator,
        sizeof(MarimayCheckInterfaceDeclaration)
    );
    top_level_index_initialized = mmay_checker_private_checker_top_level_index_init(
        &checker
    );
    local_nominal_index_initialized = mmay_checker_private_checker_local_nominal_index_init(
        &checker
    );
    callable_index_initialized = mmay_check_callable_index_init(&checker);
    if (!declarations_initialized || !constant_declarations_initialized
        || !type_alias_declarations_initialized
        || !record_declarations_initialized
        || !enum_declarations_initialized || !interface_declarations_initialized
        || !top_level_index_initialized || !local_nominal_index_initialized
        || !callable_index_initialized
        || !mmay_checker_private_imported_enums_init(&checker)) {
        mmay_checker_private_checker_dispose_declarations(&checker);
        mmay_check_result_dispose(result);
        mmay_vector_dispose(&sorted_asts);
        return MMAY_CHECK_ALLOCATION_FAILED;
    }
    if (!mmay_checker_private_prelude_init(&checker)) {
        mmay_checker_private_checker_dispose_declarations(&checker);
        mmay_check_result_dispose(result);
        mmay_vector_dispose(&sorted_asts);
        return checker.failure == MMAY_CHECK_OK
            ? MMAY_CHECK_ALLOCATION_FAILED : checker.failure;
    }
    for (ast_index = 0; ast_index < sorted_asts.length
        && !mmay_checker_has_failed(&checker); ast_index += 1) {
        const MarimayAstTree *const *ast = mmay_vector_at_const(
            &sorted_asts,
            ast_index
        );
        if (ast == NULL || !mmay_checker_private_checker_set_ast(&checker, *ast)) {
            mmay_checker_fail(&checker, MMAY_CHECK_INTERNAL_ERROR);
            break;
        }
        for (index = 0; index < (*ast)->root->child_count
            && !mmay_checker_has_failed(&checker); index += 1) {
            const MarimayAstElement *element = mmay_ast_child(
                (*ast)->root,
                index
            );
            if (element == NULL || element->kind != MMAY_AST_ELEMENT_NODE
                || element->as.node == NULL) {
                mmay_checker_fail(&checker, MMAY_CHECK_INVALID_ARGUMENT);
                break;
            }
            mmay_checker_set_incomplete_context(
                &checker, element->as.node
            );
            if (element->as.node->kind == MMAY_SYNTAX_TYPE_ALIAS_DECL) {
                (void)mmay_checker_private_collect_type_alias_declaration(
                    &checker,
                    element->as.node
                );
            } else if (element->as.node->kind == MMAY_SYNTAX_RECORD_DECL) {
                (void)mmay_checker_private_collect_record_declaration(
                    &checker,
                    element->as.node
                );
            } else if (element->as.node->kind == MMAY_SYNTAX_ENUM_DECL) {
                (void)mmay_checker_private_collect_enum_declaration(
                    &checker,
                    element->as.node
                );
            } else if (element->as.node->kind == MMAY_SYNTAX_INTERFACE_DECL) {
                (void)mmay_checker_private_collect_interface_declaration(
                    &checker,
                    element->as.node
                );
            }
        }
    }
    if (!mmay_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)mmay_checker_private_evaluate_all_type_aliases(&checker);
    }
    if (!mmay_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)mmay_checker_private_resolve_all_record_fields(&checker);
    }
    if (!mmay_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)mmay_checker_private_resolve_all_enum_variants(&checker);
    }
    if (!mmay_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)mmay_checker_private_resolve_all_interface_methods(&checker);
    }
    for (ast_index = 0; ast_index < sorted_asts.length
        && !mmay_checker_has_failed(&checker)
        && result->diagnostics.length == 0; ast_index += 1) {
        const MarimayAstTree *const *ast = mmay_vector_at_const(
            &sorted_asts,
            ast_index
        );
        if (ast == NULL || !mmay_checker_private_checker_set_ast(&checker, *ast)) {
            mmay_checker_fail(&checker, MMAY_CHECK_INTERNAL_ERROR);
            break;
        }
        for (index = 0; index < (*ast)->root->child_count
            && !mmay_checker_has_failed(&checker); index += 1) {
            const MarimayAstElement *element = mmay_ast_child(
                (*ast)->root,
                index
            );
            if (element == NULL || element->kind != MMAY_AST_ELEMENT_NODE
                || element->as.node == NULL) {
                mmay_checker_fail(&checker, MMAY_CHECK_INVALID_ARGUMENT);
                break;
            }
            mmay_checker_set_incomplete_context(
                &checker, element->as.node
            );
            if (element->as.node->kind == MMAY_SYNTAX_FUNCTION_DECL
                || element->as.node->kind == MMAY_SYNTAX_EXTERN_DECL) {
                (void)mmay_checker_private_collect_function_signature(
                    &checker,
                    element->as.node
                );
            } else if (element->as.node->kind == MMAY_SYNTAX_CONST_DECL) {
                (void)mmay_checker_private_collect_constant_declaration(
                    &checker,
                    element->as.node
                );
            } else if (element->as.node->kind
                == MMAY_SYNTAX_TYPE_ALIAS_DECL) {
                continue;
            } else if (element->as.node->kind == MMAY_SYNTAX_RECORD_DECL) {
                continue;
            } else if (element->as.node->kind == MMAY_SYNTAX_ENUM_DECL) {
                continue;
            } else if (element->as.node->kind == MMAY_SYNTAX_INTERFACE_DECL) {
                continue;
            } else if (element->as.node->kind == MMAY_SYNTAX_IMPORT_DECL) {
                continue;
            } else {
                mmay_checker_fail(&checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
                break;
            }
        }
    }
    for (ast_index = 0; ast_index < sorted_asts.length
        && !mmay_checker_has_failed(&checker)
        && result->diagnostics.length == 0; ast_index += 1) {
        const MarimayAstTree *const *ast = mmay_vector_at_const(
            &sorted_asts,
            ast_index
        );
        if (ast == NULL || !mmay_checker_private_checker_set_ast(&checker, *ast)
            || !mmay_check_publish_completions_for_ast(&checker)) {
            if (!mmay_checker_has_failed(&checker)) {
                mmay_checker_fail(&checker, MMAY_CHECK_INTERNAL_ERROR);
            }
            break;
        }
    }
    if (!mmay_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        size_t symbol_total = 0;
        if (!mmay_checker_private_check_symbol_fact_total(&checker, &symbol_total)
            || symbol_total > checker.options.max_symbols) {
            mmay_checker_fail(&checker, MMAY_CHECK_LIMIT_REACHED);
        }
    }
    if (!mmay_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)mmay_checker_private_evaluate_all_constants(&checker);
    }
    if (!mmay_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)mmay_checker_private_publish_constants(&checker);
    }
    if (!mmay_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        for (index = 0; index < checker.declarations.length
            && !mmay_checker_has_failed(&checker); index += 1) {
            MarimayCheckFunctionDeclaration *declaration = mmay_vector_at(
                &checker.declarations,
                index
            );
            if (declaration == NULL || !declaration->signature_valid) {
                mmay_checker_fail(&checker, MMAY_CHECK_INTERNAL_ERROR);
                break;
            }
            if (declaration->is_generic && !declaration->is_instance) {
                continue;
            }
            (void)mmay_checker_private_check_function_body(&checker, declaration);
        }
    }
    if (!mmay_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)mmay_checker_private_publish_type_aliases(&checker);
    }
    if (!mmay_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)mmay_checker_private_publish_records(&checker);
    }
    if (!mmay_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)mmay_checker_private_publish_enums(&checker);
    }
    if (!mmay_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)mmay_checker_private_publish_interfaces(&checker);
    }
    if (!mmay_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)mmay_checker_private_publish_symbol_facts(&checker);
    }
    if (!mmay_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)mmay_check_finalize_semantic_facts(&checker);
    }
    if (!mmay_checker_has_failed(&checker)
        && result->diagnostics.length != 0) {
        if (!mmay_check_clear_semantics(result)) {
            mmay_checker_fail(&checker, MMAY_CHECK_ALLOCATION_FAILED);
        }
    }
    mmay_checker_private_checker_dispose_declarations(&checker);
    mmay_vector_dispose(&sorted_asts);
    if (checker.failure != MMAY_CHECK_OK) {
        MarimayCheckStatus status = checker.failure;
        if (status == MMAY_CHECK_INCOMPLETE_LANGUAGE
            && result->initialized
            && result->diagnostics.length != 0
            && mmay_check_clear_semantics(result)) {
            return status;
        }
        mmay_check_result_dispose(result);
        return status == MMAY_CHECK_INCOMPLETE_LANGUAGE
            ? MMAY_CHECK_ALLOCATION_FAILED : status;
    }
    return MMAY_CHECK_OK;
}

MarimayCheckStatus mmay_check_package(
    const MarimayAstTree *const *asts,
    size_t ast_count,
    const char *package_name,
    MarimayAllocator allocator,
    MarimayCheckOptions options,
    MarimayCheckResult *result
) {
    size_t package_name_length = 0;
    if (result == NULL) {
        return MMAY_CHECK_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (!mmay_checker_private_check_bounded_c_string_length(
            package_name,
            options.max_package_name_bytes,
            &package_name_length
        )) {
        return MMAY_CHECK_LIMIT_REACHED;
    }
    return mmay_checker_check_package_unit(
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

MarimayCheckStatus mmay_check_ast(
    const MarimayAstTree *ast,
    const char *package_name,
    MarimayAllocator allocator,
    MarimayCheckOptions options,
    MarimayCheckResult *result
) {
    const MarimayAstTree *asts[1];
    asts[0] = ast;
    return mmay_check_package(
        asts,
        1,
        package_name,
        allocator,
        options,
        result
    );
}
