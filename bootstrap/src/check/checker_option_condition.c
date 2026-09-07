#include "checker_option_condition_api.h"

#include "checker_cfg_api.h"
#include "checker_core_api.h"
#include "checker_defer_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_lookup_api.h"
#include "checker_match_pattern_api.h"

#include <string.h>

const MarimayAstNode *mmay_checker_private_condition_node(const MarimayAstNode *statement) {
    const MarimayAstNode *binding = mmay_checker_private_check_child_node(
        statement, MMAY_SYNTAX_OPTION_CONDITION, 0);
    return binding != NULL ? binding : mmay_checker_private_check_child_node(
        statement, MMAY_SYNTAX_EXPRESSION, 0);
}

bool mmay_checker_private_compile_option_condition(
    MarimayFunctionChecker *function, const MarimayAstNode *condition,
    MarimayExpressionValue *test, MarimayOptionCondition *binding
) {
    const MarimayAstNode *initializer = NULL;
    const MarimaySyntaxToken *name = NULL;
    const MarimayTypeDescriptor *type = NULL;
    const MarimayCheckEnumDeclaration *base = NULL;
    MarimayCheckEnumDeclaration concrete;
    if (function == NULL || condition == NULL || test == NULL || binding == NULL) {
        if (function != NULL) mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    memset(binding, 0, sizeof(*binding));
    if (condition->kind != MMAY_SYNTAX_OPTION_CONDITION) {
        return mmay_checker_private_compile_condition(function, condition, test);
    }
    initializer = mmay_checker_private_check_child_node(condition, MMAY_SYNTAX_EXPRESSION, 0);
    name = mmay_checker_private_check_child_token(condition, MMAY_TOKEN_IDENTIFIER, 0);
    if (initializer == NULL || name == NULL || name->synthetic) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (!mmay_compile_expression(function, initializer, MMAY_TYPE_INVALID, &binding->option)) {
        return false;
    }
    if (mmay_expression_is_never(function, &binding->option)) {
        *test = binding->option;
        return true;
    }
    type = mmay_type_descriptor(&function->checker->result->types, binding->option.type);
    base = function->checker->prelude_enums.length != 0
        ? mmay_vector_at_const(&function->checker->prelude_enums, 0) : NULL;
    if (type == NULL || base == NULL || type->kind != MMAY_TYPE_KIND_NOMINAL
        || type->argument_count != 1 || !mmay_symbol_id_equal(type->symbol, base->symbol)) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "Option binding requires the exact prelude Option<T> type",
            initializer->start, initializer->end);
        return false;
    }
    binding->payload_type = type->arguments[0];
    if (binding->payload_type == MMAY_TYPE_VOID) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "void cannot be stored in a named binding", name->start, name->end);
        return false;
    }
    concrete = *base;
    concrete.type = binding->option.type;
    if (!mmay_checker_private_function_enum_layout(function, &concrete, &binding->layout)
        || !mmay_checker_private_function_emit_enum_access(function, MMAY_MIR_ENUM_IS_VARIANT,
            MMAY_TYPE_BOOL, binding->option.register_id, binding->layout,
            0, 0, condition->start, condition->end, test)) {
        return false;
    }
    binding->name = name;
    binding->mutable_binding = mmay_checker_private_check_child_token(
        condition, MMAY_TOKEN_KW_LET, 0) != NULL;
    return true;
}

static bool mmay_option_bind_payload(
    MarimayFunctionChecker *function, const MarimayOptionCondition *binding
) {
    MarimayExpressionValue guarded;
    MarimayExpressionValue payload;
    MarimayCheckName name;
    MarimayCheckLocal *local = NULL;
    memset(&guarded, 0, sizeof(guarded));
    memset(&payload, 0, sizeof(payload));
    name.snapshot = function->checker->ast->snapshot;
    name.start = binding->name->start;
    name.end = binding->name->end;
    return mmay_checker_private_function_emit_enum_access(function,
            MMAY_MIR_ENUM_REQUIRE_VARIANT, binding->option.type,
            binding->option.register_id, binding->layout, 0, 0,
            name.start, name.end, &guarded)
        && mmay_checker_private_function_emit_enum_access(function,
            MMAY_MIR_ENUM_GET_PAYLOAD, binding->payload_type, guarded.register_id,
            binding->layout, 0, 0, name.start, name.end, &payload)
        && mmay_checker_private_function_add_local(function, name, binding->payload_type,
            binding->mutable_binding, true, &local)
        && local != NULL
        && mmay_checker_private_function_emit_copy(function, local->register_id,
            payload.register_id, name.start, name.end);
}

bool mmay_checker_private_compile_condition_body(
    MarimayFunctionChecker *function, const MarimayAstNode *body,
    const MarimayOptionCondition *binding
) {
    size_t local_count = function->locals.length;
    size_t defer_count = function->defers.length;
    size_t previous_depth = function->scope_depth;
    bool compiled = false;
    if (binding->name == NULL) {
        return mmay_checker_private_compile_scoped_block(function, body);
    }
    if (body == NULL || previous_depth == SIZE_MAX) {
        mmay_checker_fail(function->checker, body == NULL
            ? MMAY_CHECK_INVALID_ARGUMENT : MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    function->scope_depth = previous_depth + 1;
    compiled = mmay_option_bind_payload(function, binding)
        && mmay_checker_private_compile_block(function, body);
    if (compiled && !function->terminated) {
        compiled = mmay_checker_private_emit_defers_from(function, defer_count);
    }
    function->defers.length = defer_count;
    function->locals.length = local_count;
    function->scope_depth = previous_depth;
    return compiled;
}
