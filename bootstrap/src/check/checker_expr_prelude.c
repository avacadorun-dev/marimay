#include "checker_expr_prelude_api.h"

#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_generic_types_api.h"
#include "checker_lookup_api.h"

#include <string.h>

static size_t ruda_checker_private_prelude_argument_count(
    const RudaAstNode *arguments
) {
    size_t count = 0;
    size_t index = 0;
    for (index = 0; arguments != NULL && index < arguments->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(arguments, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE) {
            count += 1;
        }
    }
    return count;
}

static const RudaAstNode *ruda_checker_private_prelude_first_argument(
    const RudaAstNode *arguments
) {
    size_t index = 0;
    for (index = 0; arguments != NULL && index < arguments->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(arguments, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE) {
            return element->as.node;
        }
    }
    return NULL;
}

static bool ruda_checker_private_prelude_constructor_name(
    RudaChecker *checker,
    const RudaSyntaxToken *token,
    size_t *constructor
) {
    static const char *const names[] = {"Some", "None", "Ok", "Err"};
    size_t index = 0;
    if (checker == NULL || token == NULL || token->synthetic || constructor == NULL) {
        return false;
    }
    for (index = 0; index < sizeof(names) / sizeof(names[0]); index += 1) {
        if (ruda_checker_private_check_text_equal(
                checker, token->start, token->end, names[index]
            )) {
            *constructor = index;
            return true;
        }
    }
    return false;
}

bool ruda_checker_private_compile_prelude_constructor(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    bool *resolved,
    RudaExpressionValue *result
) {
    const RudaAstNode *callee = NULL;
    const RudaAstNode *arguments_node = NULL;
    const RudaAstNode *first_argument = NULL;
    const RudaAstNode *type_arguments = NULL;
    const RudaSyntaxToken *token = NULL;
    RudaCheckName name;
    RudaCheckEnumDeclaration *base = NULL;
    RudaCheckEnumVariantDeclaration *variant = NULL;
    RudaVector arguments;
    RudaExpressionValue payload_value;
    RudaMirEnumArgument payload_argument;
    RudaTypeId concrete_type = RUDA_TYPE_INVALID;
    RudaTypeId payload_type = RUDA_TYPE_INVALID;
    size_t constructor = SIZE_MAX;
    size_t variant_index = 0;
    size_t argument_count = 0;
    size_t index = 0;
    bool initialized = false;
    bool payload_compiled = false;
    bool accepted = false;
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) {
        if (function != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    *resolved = false;
    callee = ruda_checker_private_check_child_node(
        node, RUDA_SYNTAX_NAME_EXPRESSION, 0
    );
    token = ruda_checker_private_check_child_token(callee, RUDA_TOKEN_IDENTIFIER, 0);
    if (!ruda_checker_private_prelude_constructor_name(
            function->checker, token, &constructor
        )) {
        return true;
    }
    name.snapshot = function->checker->ast->snapshot;
    name.start = token->start;
    name.end = token->end;
    if (ruda_checker_private_function_find_local(function, name) != NULL
        || ruda_checker_private_checker_find_top_level(function->checker, name) != NULL) {
        return true;
    }
    *resolved = true;
    arguments_node = ruda_checker_private_check_child_node(
        node, RUDA_SYNTAX_ARGUMENTS, 0
    );
    type_arguments = ruda_checker_private_check_child_node(
        node, RUDA_SYNTAX_TYPE_ARGUMENTS, 0
    );
    if (arguments_node == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    argument_count = ruda_checker_private_prelude_argument_count(arguments_node);
    first_argument = ruda_checker_private_prelude_first_argument(arguments_node);
    base = ruda_vector_at(
        &function->checker->prelude_enums,
        constructor < 2 ? 0 : 1
    );
    if (base == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    variant = ruda_checker_private_checker_find_enum_variant(
        function->checker, base, name, &variant_index
    );
    if (variant == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    memset(&arguments, 0, sizeof(arguments));
    memset(&payload_value, 0, sizeof(payload_value));
    memset(&payload_argument, 0, sizeof(payload_argument));
    if (!ruda_vector_init(
            &arguments,
            function->checker->result->allocator,
            sizeof(RudaTypeId)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    initialized = true;
    if (type_arguments != NULL
        && !ruda_checker_private_parse_type_arguments(
            function->checker, type_arguments, &arguments
        )) {
        goto cleanup;
    }
    if (arguments.length == 0 && expected != RUDA_TYPE_INVALID) {
        const RudaTypeDescriptor *descriptor = ruda_type_descriptor(
            &function->checker->result->types, expected
        );
        if (descriptor != NULL && descriptor->kind == RUDA_TYPE_KIND_NOMINAL
            && ruda_symbol_id_equal(descriptor->symbol, base->symbol)
            && descriptor->argument_count == base->type_parameters.length) {
            for (index = 0; index < descriptor->argument_count; index += 1) {
                RudaTypeId type = descriptor->arguments[index];
                if (!ruda_vector_push(&arguments, &type)) {
                    ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
                    goto cleanup;
                }
            }
        }
    }
    if (arguments.length == 0 && argument_count == 1
        && (constructor == 0 || constructor == 2 || constructor == 3)) {
        if (!ruda_compile_expression(
                function, first_argument, RUDA_TYPE_INVALID, &payload_value
            )) {
            goto cleanup;
        }
        if (ruda_expression_is_never(function, &payload_value)) {
            *result = payload_value;
            accepted = true;
            goto cleanup;
        }
        payload_compiled = true;
        payload_type = payload_value.type;
        for (index = 0; index < base->type_parameters.length; index += 1) {
            RudaTypeId type = RUDA_TYPE_INVALID;
            if (index == (constructor == 3 ? 1 : 0)) {
                type = payload_type;
            }
            if (!ruda_vector_push(&arguments, &type)) {
                ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
                goto cleanup;
            }
        }
    }
    if (arguments.length != base->type_parameters.length) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "prelude constructor needs a complete concrete generic type",
            node->start,
            node->end
        );
        goto cleanup;
    }
    for (index = 0; index < arguments.length; index += 1) {
        const RudaTypeId *type = ruda_vector_at_const(&arguments, index);
        if (type == NULL || *type == RUDA_TYPE_INVALID) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "prelude constructor generic argument cannot be inferred",
                node->start,
                node->end
            );
            goto cleanup;
        }
    }
    if (!ruda_checker_private_intern_generic_nominal(
            function->checker, RUDA_TYPE_KIND_NOMINAL, base->symbol,
            &arguments, &concrete_type
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (expected != RUDA_TYPE_INVALID && expected != concrete_type) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "prelude constructor result type does not match the expected type",
            node->start,
            node->end
        );
        goto cleanup;
    }
    if (variant->kind == RUDA_CHECKED_ENUM_VARIANT_UNIT) {
        if (argument_count != 0) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_CALL_ARITY,
                "unit prelude constructor takes no arguments",
                node->start,
                node->end
            );
            goto cleanup;
        }
        accepted = ruda_checker_private_function_emit_enum_construction(
            function,
            &(RudaCheckEnumDeclaration){
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
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (!ruda_checker_private_substitute_nominal_member_type(
            function,
            concrete_type,
            &base->type_parameters,
            ((const RudaCheckEnumPayloadItemDeclaration *)
                ruda_vector_at_const(&variant->payload, 0))->type,
            &payload_type
        )) {
        goto cleanup;
    }
    if (payload_type == RUDA_TYPE_VOID && argument_count == 0) {
        accepted = ruda_checker_private_function_emit_enum_construction(
            function,
            &(RudaCheckEnumDeclaration){
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
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_CALL_ARITY,
            "payload prelude constructor requires exactly one argument",
            node->start,
            node->end
        );
        goto cleanup;
    }
    if (!payload_compiled && !ruda_compile_expression(
            function, first_argument, payload_type, &payload_value
        )) {
        goto cleanup;
    }
    if (ruda_expression_is_never(function, &payload_value)) {
        *result = payload_value;
        accepted = true;
        goto cleanup;
    }
    if (payload_value.type != payload_type) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "prelude constructor payload type does not match its result type",
            first_argument->start,
            first_argument->end
        );
        goto cleanup;
    }
    payload_argument.register_id = payload_value.register_id;
    payload_argument.type = payload_value.type;
    accepted = ruda_checker_private_function_emit_enum_construction(
        function,
        &(RudaCheckEnumDeclaration){
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
        ruda_vector_dispose(&arguments);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}
