#include "checker_mapper_api.h"
#include "checker_core_api.h"
#include "checker_expression_api.h"
#include "checker_expr_scalar_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_function_values_api.h"
#include "checker_generic_types_api.h"
#include "checker_lookup_api.h"
#include "checker_match_pattern_api.h"

static bool mapper_constructor(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayTypeId input, MarimayTypeId context, MarimayErrorMapper *mapper) {
    MarimayCheckEnumDeclaration *declaration = NULL;
    MarimayCheckEnumVariantDeclaration *variant = NULL;
    if (!mmay_checker_private_resolve_enum_member(function, node, context,
            &declaration, &variant, &mapper->variant, &mapper->is_constructor)) return false;
    if (!mapper->is_constructor) return true;
    if (declaration == NULL || variant == NULL
        || variant->kind != MMAY_CHECKED_ENUM_VARIANT_POSITIONAL || variant->payload.length != 1) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "mapError enum mapper must be a positional constructor with one payload", node->start, node->end);
        return false;
    }
    mapper->constructor = *declaration;
    if (declaration->is_generic) {
        const MarimayTypeDescriptor *expected = mmay_type_descriptor(&function->checker->result->types, context);
        if (expected == NULL || expected->kind != MMAY_TYPE_KIND_NOMINAL
            || !mmay_symbol_id_equal(expected->symbol, declaration->symbol)) {
            (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "generic enum constructor mapper requires an expected concrete Result error type", node->start, node->end);
            return false;
        }
        mapper->constructor.type = context;
    }
    const MarimayCheckEnumPayloadItemDeclaration *payload = mmay_vector_at_const(&variant->payload, 0);
    MarimayTypeId parameter;
    if (!mmay_checker_private_substitute_nominal_member_type(function, mapper->constructor.type,
            &declaration->type_parameters, payload->type, &parameter)) return false;
    if (parameter != input) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "mapError mapper parameter must match the Result error type", node->start, node->end); return false;
    }
    mapper->result_type = mapper->constructor.type;
    return true;
}

bool mmay_error_mapper_prepare(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayTypeId input, MarimayTypeId context, MarimayErrorMapper *mapper) {
    memset(mapper, 0, sizeof(*mapper));
    while (node->kind == MMAY_SYNTAX_EXPRESSION || node->kind == MMAY_SYNTAX_PAREN_GROUP) {
        const MarimayAstNode *inner = mmay_checker_private_expression_only_node(node);
        if (inner == NULL || inner == node) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT); return false;
        }
        node = inner;
    }
    bool named = false;
    if (node->kind == MMAY_SYNTAX_CLOSURE_EXPRESSION) {
        if (!mmay_function_value_mapper_closure(function, node, input, context, &mapper->value)) return false;
    } else {
        if (!mmay_function_value_named_input(function, node, input, &named, &mapper->value)) return false;
        if (!named && node->kind == MMAY_SYNTAX_MEMBER_EXPRESSION) {
            if (!mapper_constructor(function, node, input, context, mapper)) return false;
            if (mapper->is_constructor) return true;
        }
        if (!named && !mmay_compile_expression(function, node, MMAY_TYPE_INVALID, &mapper->value)) return false;
    }
    if (mmay_expression_is_never(function, &mapper->value)) return true;
    const MarimayTypeDescriptor *signature = mmay_type_descriptor(&function->checker->result->types, mapper->value.type);
    if (signature == NULL || signature->kind != MMAY_TYPE_KIND_FUNCTION
        || signature->parameter_count != 1 || signature->arguments[0] != input) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "mapError mapper requires one parameter with the exact error type", node->start, node->end); return false;
    }
    mapper->result_type = signature->arguments[1];
    if (mapper->result_type == MMAY_TYPE_NEVER) {
        /* The common enum transport does not yet admit uninhabited payloads.
         * Diagnose before publishing a Result<T, never> that backends cannot lower. */
        mmay_checker_fail_incomplete(function->checker, node, "Result with an uninhabited error payload");
        return false;
    }
    return true;
}

bool mmay_error_mapper_invoke(MarimayFunctionChecker *function, const MarimayAstNode *node,
    const MarimayErrorMapper *mapper, MarimayExpressionValue argument, MarimayExpressionValue *result) {
    if (mapper->is_constructor) {
        const MarimayMirEnumArgument payload = {argument.register_id, argument.type};
        return mmay_checker_private_function_emit_enum_construction(function, &mapper->constructor,
            mapper->variant, &payload, 1, node->start, node->end, result);
    }
    const MarimayMirCallArgument arguments[] = {
        {mapper->value.register_id, mapper->value.type}, {argument.register_id, argument.type}
    };
    return mmay_function_value_emit_call(function, node, arguments, 2, result);
}
