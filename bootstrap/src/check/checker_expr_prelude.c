#include "checker_expr_prelude_api.h"

#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_generic_types_api.h"
#include "checker_lookup_api.h"

#include <string.h>

static size_t mmay_checker_private_prelude_argument_count(
    const MarimayAstNode *arguments
) {
    size_t count = 0;
    size_t index = 0;
    for (index = 0; arguments != NULL && index < arguments->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(arguments, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE) {
            count += 1;
        }
    }
    return count;
}

static const MarimayAstNode *mmay_checker_private_prelude_first_argument(
    const MarimayAstNode *arguments
) {
    size_t index = 0;
    for (index = 0; arguments != NULL && index < arguments->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(arguments, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE) {
            return element->as.node;
        }
    }
    return NULL;
}

static bool mmay_checker_private_prelude_constructor_name(
    MarimayChecker *checker,
    const MarimaySyntaxToken *token,
    size_t *constructor
) {
    static const char *const names[] = {"Some", "None", "Ok", "Err"};
    size_t index = 0;
    if (checker == NULL || token == NULL || token->synthetic || constructor == NULL) {
        return false;
    }
    for (index = 0; index < sizeof(names) / sizeof(names[0]); index += 1) {
        if (mmay_checker_private_check_text_equal(
                checker, token->start, token->end, names[index]
            )) {
            *constructor = index;
            return true;
        }
    }
    return false;
}

bool mmay_checker_private_compile_prelude_unit_value(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
) {
    const MarimaySyntaxToken *token = mmay_checker_private_check_child_token(
        node,
        MMAY_TOKEN_IDENTIFIER,
        0
    );
    const MarimayTypeDescriptor *descriptor = NULL;
    MarimayCheckEnumDeclaration *base = NULL;
    MarimayCheckEnumDeclaration concrete;
    if (function == NULL || node == NULL || resolved == NULL
        || result == NULL) {
        return false;
    }
    *resolved = false;
    descriptor = expected != MMAY_TYPE_INVALID
        ? mmay_type_descriptor(&function->checker->result->types, expected)
        : NULL;
    base = function->checker->prelude_enums.length != 0
        ? mmay_vector_at(&function->checker->prelude_enums, 0)
        : NULL;
    if (token == NULL || token->synthetic
        || !mmay_checker_private_check_text_equal(
            function->checker,
            token->start,
            token->end,
            "None"
        )) {
        return true;
    }
    *resolved = true;
    if (base == NULL || descriptor == NULL
        || descriptor->kind != MMAY_TYPE_KIND_NOMINAL
        || !mmay_symbol_id_equal(descriptor->symbol, base->symbol)
        || descriptor->argument_count != base->type_parameters.length) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "bare None requires an expected concrete Option<T> type",
            node->start,
            node->end
        );
        return false;
    }
    concrete = *base;
    concrete.type = expected;
    return mmay_checker_private_function_emit_enum_construction(
        function,
        &concrete,
        1,
        NULL,
        0,
        node->start,
        node->end,
        result
    );
}

bool mmay_checker_private_compile_prelude_constructor(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *callee = NULL;
    const MarimayAstNode *arguments_node = NULL;
    const MarimayAstNode *first_argument = NULL;
    const MarimayAstNode *type_arguments = NULL;
    const MarimaySyntaxToken *token = NULL;
    MarimayCheckName name;
    MarimayCheckEnumDeclaration *base = NULL;
    MarimayCheckEnumVariantDeclaration *variant = NULL;
    MarimayVector arguments;
    MarimayExpressionValue payload_value;
    MarimayMirEnumArgument payload_argument;
    MarimayTypeId concrete_type = MMAY_TYPE_INVALID;
    MarimayTypeId payload_type = MMAY_TYPE_INVALID;
    size_t constructor = SIZE_MAX;
    size_t variant_index = 0;
    size_t argument_count = 0;
    size_t index = 0;
    bool initialized = false;
    bool payload_compiled = false;
    bool accepted = false;
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) {
        if (function != NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    *resolved = false;
    callee = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_NAME_EXPRESSION, 0
    );
    token = mmay_checker_private_check_child_token(callee, MMAY_TOKEN_IDENTIFIER, 0);
    if (!mmay_checker_private_prelude_constructor_name(
            function->checker, token, &constructor
        )) {
        return true;
    }
    name.snapshot = function->checker->ast->snapshot;
    name.start = token->start;
    name.end = token->end;
    if (mmay_checker_private_function_find_local(function, name) != NULL
        || mmay_checker_private_checker_find_top_level(function->checker, name) != NULL) {
        return true;
    }
    *resolved = true;
    arguments_node = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_ARGUMENTS, 0
    );
    type_arguments = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0
    );
    if (arguments_node == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    argument_count = mmay_checker_private_prelude_argument_count(arguments_node);
    first_argument = mmay_checker_private_prelude_first_argument(arguments_node);
    base = mmay_vector_at(
        &function->checker->prelude_enums,
        constructor < 2 ? 0 : 1
    );
    if (base == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    variant = mmay_checker_private_checker_find_enum_variant(
        function->checker, base, name, &variant_index
    );
    if (variant == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    memset(&arguments, 0, sizeof(arguments));
    memset(&payload_value, 0, sizeof(payload_value));
    memset(&payload_argument, 0, sizeof(payload_argument));
    if (!mmay_vector_init(
            &arguments,
            function->checker->result->allocator,
            sizeof(MarimayTypeId)
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    initialized = true;
    if (type_arguments != NULL
        && !mmay_checker_private_parse_type_arguments(
            function->checker, type_arguments, &arguments
        )) {
        goto cleanup;
    }
    if (arguments.length == 0 && expected != MMAY_TYPE_INVALID) {
        const MarimayTypeDescriptor *descriptor = mmay_type_descriptor(
            &function->checker->result->types, expected
        );
        if (descriptor != NULL && descriptor->kind == MMAY_TYPE_KIND_NOMINAL
            && mmay_symbol_id_equal(descriptor->symbol, base->symbol)
            && descriptor->argument_count == base->type_parameters.length) {
            for (index = 0; index < descriptor->argument_count; index += 1) {
                MarimayTypeId type = descriptor->arguments[index];
                if (!mmay_vector_push(&arguments, &type)) {
                    mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
                    goto cleanup;
                }
            }
        }
    }
    if (arguments.length == 0 && argument_count == 1
        && (constructor == 0 || constructor == 2 || constructor == 3)) {
        if (!mmay_compile_expression(
                function, first_argument, MMAY_TYPE_INVALID, &payload_value
            )) {
            goto cleanup;
        }
        if (mmay_expression_is_never(function, &payload_value)) {
            *result = payload_value;
            accepted = true;
            goto cleanup;
        }
        payload_compiled = true;
        payload_type = payload_value.type;
        for (index = 0; index < base->type_parameters.length; index += 1) {
            MarimayTypeId type = MMAY_TYPE_INVALID;
            if (index == (constructor == 3 ? 1 : 0)) {
                type = payload_type;
            }
            if (!mmay_vector_push(&arguments, &type)) {
                mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
                goto cleanup;
            }
        }
    }
    if (arguments.length != base->type_parameters.length) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "prelude constructor needs a complete concrete generic type",
            node->start,
            node->end
        );
        goto cleanup;
    }
    for (index = 0; index < arguments.length; index += 1) {
        const MarimayTypeId *type = mmay_vector_at_const(&arguments, index);
        if (type == NULL || *type == MMAY_TYPE_INVALID) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "prelude constructor generic argument cannot be inferred",
                node->start,
                node->end
            );
            goto cleanup;
        }
    }
    if (!mmay_checker_private_intern_generic_nominal(
            function->checker, MMAY_TYPE_KIND_NOMINAL, base->symbol,
            &arguments, &concrete_type
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (expected != MMAY_TYPE_INVALID && expected != concrete_type) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "prelude constructor result type does not match the expected type",
            node->start,
            node->end
        );
        goto cleanup;
    }
    if (variant->kind == MMAY_CHECKED_ENUM_VARIANT_UNIT) {
        if (argument_count != 0) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_CALL_ARITY,
                "unit prelude constructor takes no arguments",
                node->start,
                node->end
            );
            goto cleanup;
        }
        accepted = mmay_checker_private_function_emit_enum_construction(
            function,
            &(MarimayCheckEnumDeclaration){
                .ast = base->ast, .node = base->node,
                .name = base->name, .owned_name = base->owned_name,
                .symbol = base->symbol, .type = concrete_type,
                .variants = base->variants,
                .type_parameters = base->type_parameters,
                .declaration_valid = true, .variants_resolved = true,
                .is_generic = true, .is_builtin = true
            },
            variant_index, NULL, 0, node->start, node->end, result
        );
        goto cleanup;
    }
    if (variant->payload.length != 1) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (!mmay_checker_private_substitute_nominal_member_type(
            function,
            concrete_type,
            &base->type_parameters,
            ((const MarimayCheckEnumPayloadItemDeclaration *)
                mmay_vector_at_const(&variant->payload, 0))->type,
            &payload_type
        )) {
        goto cleanup;
    }
    if (payload_type == MMAY_TYPE_VOID && argument_count == 0) {
        accepted = mmay_checker_private_function_emit_enum_construction(
            function,
            &(MarimayCheckEnumDeclaration){
                .ast = base->ast, .node = base->node,
                .name = base->name, .owned_name = base->owned_name,
                .symbol = base->symbol, .type = concrete_type,
                .variants = base->variants,
                .type_parameters = base->type_parameters,
                .declaration_valid = true, .variants_resolved = true,
                .is_generic = true, .is_builtin = true
            },
            variant_index, NULL, 0, node->start, node->end, result
        );
        goto cleanup;
    }
    if (argument_count != 1) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_CALL_ARITY,
            "payload prelude constructor requires exactly one argument",
            node->start,
            node->end
        );
        goto cleanup;
    }
    if (!payload_compiled && !mmay_compile_expression(
            function, first_argument, payload_type, &payload_value
        )) {
        goto cleanup;
    }
    if (mmay_expression_is_never(function, &payload_value)) {
        *result = payload_value;
        accepted = true;
        goto cleanup;
    }
    if (payload_value.type != payload_type) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "prelude constructor payload type does not match its result type",
            first_argument->start,
            first_argument->end
        );
        goto cleanup;
    }
    payload_argument.register_id = payload_value.register_id;
    payload_argument.type = payload_value.type;
    accepted = mmay_checker_private_function_emit_enum_construction(
        function,
        &(MarimayCheckEnumDeclaration){
            .ast = base->ast, .node = base->node,
            .name = base->name, .owned_name = base->owned_name,
            .symbol = base->symbol, .type = concrete_type,
            .variants = base->variants,
            .type_parameters = base->type_parameters,
            .declaration_valid = true, .variants_resolved = true,
            .is_generic = true, .is_builtin = true
        },
        variant_index, &payload_argument, 1, node->start, node->end, result
    );

cleanup:
    if (initialized) {
        mmay_vector_dispose(&arguments);
    }
    return accepted && !mmay_checker_has_failed(function->checker);
}
