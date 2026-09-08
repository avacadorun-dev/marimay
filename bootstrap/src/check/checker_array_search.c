#include "checker_array_search_api.h"
#include "checker_array_mutation_api.h"
#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_function_values_api.h"
#include "checker_lookup_api.h"
#include "checker_match_pattern_api.h"

static bool search_type_failure(MarimayFunctionChecker *function) {
    size_t maximum = function->checker->options.type_options.max_types;
    size_t count = function->checker->result->types.descriptors.length;
    mmay_checker_fail(function->checker, maximum <= MMAY_TYPE_FIRST_DYNAMIC
        || count >= maximum - MMAY_TYPE_FIRST_DYNAMIC
        ? MMAY_CHECK_LIMIT_REACHED : MMAY_CHECK_ALLOCATION_FAILED);
    return false;
}

static bool search_array_read(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayMirInstructionKind kind, MarimayTypeId type, MarimayMirArrayId array,
    MarimayMirRegister receiver, MarimayMirRegister index, MarimayExpressionValue *result) {
    MarimayMirInstruction instruction = {0};
    instruction.kind = kind;
    instruction.first = receiver;
    instruction.second = index;
    instruction.third = MMAY_MIR_NO_REGISTER;
    instruction.array = array;
    instruction.span = (MarimayMirSpan){node->start, node->end};
    if (!mmay_function_add_register(function, type, &instruction.target)
        || !mmay_function_append_instruction(function, &instruction)) return false;
    *result = (MarimayExpressionValue){type, instruction.target, true};
    return true;
}

static bool search_finish(MarimayFunctionChecker *function, const MarimayAstNode *node,
    const MarimayCheckEnumDeclaration *option, MarimayMirRegister index,
    MarimayMirRegister target, MarimayMirBlockId join) {
    MarimayExpressionValue wrapped;
    MarimayMirEnumArgument argument = {index, MMAY_TYPE_I64};
    bool some = index != MMAY_MIR_NO_REGISTER;
    return mmay_checker_private_function_emit_enum_construction(function, option,
            some ? 0 : 1, some ? &argument : NULL, some ? 1 : 0,
            node->start, node->end, &wrapped)
        && mmay_checker_private_function_emit_copy(function, target,
            wrapped.register_id, node->start, node->end)
        && mmay_checker_private_function_set_goto(function, function->block,
            join, node->start, node->end);
}

static bool search_loop(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayExpressionValue receiver, MarimayExpressionValue predicate,
    MarimayTypeId element, MarimayMirArrayId array, MarimayExpressionValue *result) {
    const MarimayCheckEnumDeclaration *base = mmay_vector_at_const(&function->checker->prelude_enums, 0);
    if (base == NULL) { mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR); return false; }
    MarimayCheckEnumDeclaration option = *base;
    MarimayTypeId integer = MMAY_TYPE_I64;
    if (!mmay_type_intern_nominal(&function->checker->result->types,
            MMAY_TYPE_KIND_NOMINAL, base->symbol, &integer, 1, &option.type)) return search_type_failure(function);
    MarimayExpressionValue zero, one, length, condition, item, matched, next;
    MarimayMirRegister generation, checked, postchecked, index, target;
    MarimayMirBlockId header, visit, step, some, none, join;
    if (!mmay_function_add_register(function, MMAY_TYPE_I64, &index)
        || !mmay_function_add_register(function, option.type, &target)
        || !mmay_function_emit_instruction(function, MMAY_MIR_CONSTANT_I64, MMAY_TYPE_I64,
            MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0, false, node->start, node->end, &zero)
        || !mmay_function_emit_instruction(function, MMAY_MIR_CONSTANT_I64, MMAY_TYPE_I64,
            MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 1, false, node->start, node->end, &one)
        || !mmay_checker_private_function_emit_copy(function, index, zero.register_id, node->start, node->end)
        || !mmay_checker_private_array_capture_generation(function, array,
            receiver.register_id, node->start, node->end, &generation)
        || !search_array_read(function, node, MMAY_MIR_ARRAY_LENGTH, MMAY_TYPE_I64,
            array, receiver.register_id, MMAY_MIR_NO_REGISTER, &length)
        || !mmay_checker_private_function_add_block(function, &header)
        || !mmay_checker_private_function_add_block(function, &visit)
        || !mmay_checker_private_function_add_block(function, &step)
        || !mmay_checker_private_function_add_block(function, &some)
        || !mmay_checker_private_function_add_block(function, &none)
        || !mmay_checker_private_function_add_block(function, &join)
        || !mmay_checker_private_function_set_goto(function, function->block, header, node->start, node->end)) return false;
    function->block = header;
    function->terminated = false;
    if (!mmay_checker_private_array_require_generation(function, array, receiver.type,
            receiver.register_id, generation, node->start, node->end, &checked)
        || !mmay_function_emit_instruction(function, MMAY_MIR_LESS_I64, MMAY_TYPE_BOOL,
            index, length.register_id, 0, false, node->start, node->end, &condition)
        || !mmay_checker_private_function_set_branch(function, header,
            condition.register_id, visit, none, node->start, node->end)) return false;
    function->block = visit;
    if (!search_array_read(function, node, MMAY_MIR_ARRAY_GET, element, array, checked, index, &item)) return false;
    const MarimayMirCallArgument arguments[] = {
        {predicate.register_id, predicate.type}, {item.register_id, item.type}
    };
    /* The same post-call check precedes both a match and continued traversal.
     * It is not reached when the callback panics, preserving that panic. */
    if (!mmay_function_value_emit_call(function, node, arguments, 2, &matched)
        || !mmay_checker_private_array_require_generation(function, array, receiver.type,
            receiver.register_id, generation, node->start, node->end, &postchecked)
        || !mmay_checker_private_function_set_branch(function, function->block,
            matched.register_id, some, step, node->start, node->end)) return false;
    function->block = step;
    if (!mmay_function_emit_instruction(function, MMAY_MIR_ADD_CHECKED_I64, MMAY_TYPE_I64,
            index, one.register_id, 0, false, node->start, node->end, &next)
        || !mmay_checker_private_function_emit_copy(function, index, next.register_id, node->start, node->end)
        || !mmay_checker_private_function_set_goto(function, step, header, node->start, node->end)) return false;
    function->block = some;
    if (!search_finish(function, node, &option, index, target, join)) return false;
    function->block = none;
    if (!search_finish(function, node, &option, MMAY_MIR_NO_REGISTER, target, join)) return false;
    function->block = join;
    function->terminated = false;
    *result = (MarimayExpressionValue){option.type, target, true};
    return true;
}

bool mmay_checker_private_compile_array_search_call(MarimayFunctionChecker *function,
    const MarimayAstNode *node, const MarimayExpressionValue *receiver,
    bool *resolved, MarimayExpressionValue *result) {
    const MarimayAstNode *member = mmay_checker_private_check_child_node(node, MMAY_SYNTAX_MEMBER_EXPRESSION, 0);
    const MarimayAstNode *callee = mmay_checker_private_check_child_node(node, MMAY_SYNTAX_NAME_EXPRESSION, 0);
    const MarimayAstNode *arguments = mmay_checker_private_check_child_node(node, MMAY_SYNTAX_ARGUMENTS, 0);
    const MarimayAstNode *unused = NULL, *predicate_node = NULL;
    const MarimaySyntaxToken *name = NULL;
    *resolved = false;
    if (member != NULL) {
        if (!mmay_checker_private_member_parts(member, &unused, &name)) return true;
    } else name = mmay_checker_private_check_child_token(callee, MMAY_TOKEN_IDENTIFIER, 0);
    if (name == NULL || name->synthetic || !mmay_checker_private_check_text_equal(
            function->checker, name->start, name->end, "findIndex")) return true;
    *resolved = true;
    if (mmay_checker_private_check_child_node(node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0) != NULL) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "findIndex infers types from its receiver", node->start, node->end); return false;
    }
    size_t count = 0;
    for (size_t i = 0; arguments != NULL && i < arguments->child_count; i += 1) {
        const MarimayAstElement *argument = mmay_ast_child(arguments, i);
        if (argument->kind == MMAY_AST_ELEMENT_NODE) {
            if (count == (member != NULL ? 0U : 1U)) predicate_node = argument->as.node;
            count += 1;
        }
    }
    if (count != (member != NULL ? 1U : 2U)) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_CALL_ARITY,
            "findIndex requires one predicate", node->start, node->end); return false;
    }
    MarimayTypeId element, signature;
    MarimayMirArrayId array;
    MarimayExpressionValue predicate;
    if (!mmay_type_array_element(&function->checker->result->types, receiver->type, &element)
        || !mmay_array_layout(function, receiver->type, &array)) return false;
    if (!mmay_type_intern_function(&function->checker->result->types, &element, 1,
            MMAY_TYPE_BOOL, &signature)) return search_type_failure(function);
    if (!mmay_compile_expression(function, predicate_node, signature, &predicate)) return false;
    if (mmay_expression_is_never(function, &predicate)) { *result = predicate; return true; }
    if (predicate.type != signature) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "findIndex predicate requires exact type (T) => bool", predicate_node->start, predicate_node->end); return false;
    }
    return search_loop(function, node, *receiver, predicate, element, array, result);
}
