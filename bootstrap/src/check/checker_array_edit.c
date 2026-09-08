#include "checker_array_edit_api.h"

#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_generic_types_api.h"
#include "checker_lookup_api.h"

#include <string.h>

static bool mmay_array_edit_emit(
    MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayMirInstructionKind kind, MarimayTypeId type, MarimayMirArrayId array,
    MarimayMirRegister first, MarimayMirRegister second, MarimayExpressionValue *result
) {
    MarimayMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    if (!mmay_function_add_register(function, type, &instruction.target)) return false;
    instruction.kind = kind;
    instruction.first = first;
    instruction.second = second;
    instruction.third = MMAY_MIR_NO_REGISTER;
    instruction.array = array;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!mmay_function_append_instruction(function, &instruction)) return false;
    result->register_id = instruction.target;
    result->type = type;
    result->valid = true;
    return true;
}

static bool mmay_array_edit_option(
    MarimayFunctionChecker *function, MarimayTypeId element,
    MarimayCheckEnumDeclaration *option
) {
    const MarimayCheckEnumDeclaration *base = mmay_vector_at_const(
        &function->checker->prelude_enums, 0);
    MarimayVector arguments;
    bool ok;
    if (base == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    *option = *base;
    if (!mmay_vector_init(&arguments, function->checker->result->allocator,
            sizeof(element))) return false;
    ok = mmay_vector_push(&arguments, &element)
        && mmay_checker_private_intern_generic_nominal(function->checker,
            MMAY_TYPE_KIND_NOMINAL, base->symbol, &arguments, &option->type);
    mmay_vector_dispose(&arguments);
    if (!ok && !mmay_checker_has_failed(function->checker)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
    }
    return ok;
}

static bool mmay_array_edit_remove(
    MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayExpressionValue receiver, MarimayExpressionValue index,
    MarimayTypeId element, MarimayMirArrayId array, MarimayExpressionValue *result
) {
    MarimayCheckEnumDeclaration option;
    MarimayExpressionValue zero, negative, length, in_bounds, payload, wrapped, ignored;
    MarimayMirBlockId entry = function->block;
    MarimayMirBlockId none_block, check_block, some_block, join;
    MarimayMirRegister target;
    if (!mmay_array_edit_option(function, element, &option)
        || !mmay_function_add_register(function, option.type, &target)
        || !mmay_function_emit_instruction(function, MMAY_MIR_CONSTANT_I64,
            MMAY_TYPE_I64, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER,
            0, false, node->start, node->end, &zero)
        || !mmay_function_emit_instruction(function, MMAY_MIR_LESS_I64,
            MMAY_TYPE_BOOL, index.register_id, zero.register_id, 0, false,
            node->start, node->end, &negative)
        || !mmay_checker_private_function_add_block(function, &none_block)
        || !mmay_checker_private_function_add_block(function, &check_block)
        || !mmay_checker_private_function_add_block(function, &some_block)
        || !mmay_checker_private_function_add_block(function, &join)
        || !mmay_checker_private_function_set_branch(function, entry,
            negative.register_id, none_block, check_block, node->start, node->end)) return false;
    function->block = check_block;
    function->terminated = false;
    if (!mmay_array_edit_emit(function, node, MMAY_MIR_ARRAY_LENGTH, MMAY_TYPE_I64,
            array, receiver.register_id, MMAY_MIR_NO_REGISTER, &length)
        || !mmay_function_emit_instruction(function, MMAY_MIR_LESS_I64,
            MMAY_TYPE_BOOL, index.register_id, length.register_id, 0, false,
            node->start, node->end, &in_bounds)
        || !mmay_checker_private_function_set_branch(function, check_block,
            in_bounds.register_id, some_block, none_block, node->start, node->end)) return false;
    function->block = none_block;
    function->terminated = false;
    if (!mmay_checker_private_function_emit_enum_construction(function, &option,
            1, NULL, 0, node->start, node->end, &wrapped)
        || !mmay_checker_private_function_emit_copy(function, target,
            wrapped.register_id, node->start, node->end)
        || !mmay_checker_private_function_set_goto(function, function->block,
            join, node->end, node->end)) return false;
    function->block = some_block;
    function->terminated = false;
    /* Prepare the exact, rooted Option payload before the mutation. No
       allocation-capable instruction may occur between removal and join. */
    if (!mmay_array_edit_emit(function, node, MMAY_MIR_ARRAY_GET, element,
            array, receiver.register_id, index.register_id, &payload)
        || !mmay_checker_private_function_emit_enum_construction(function, &option,
            0, &(MarimayMirEnumArgument){.register_id = payload.register_id, .type = element},
            1, node->start, node->end, &wrapped)
        || !mmay_array_edit_emit(function, node, MMAY_MIR_ARRAY_REMOVE_AT,
            receiver.type, array, receiver.register_id, index.register_id, &ignored)
        || !mmay_checker_private_function_emit_copy(function, target,
            wrapped.register_id, node->start, node->end)
        || !mmay_checker_private_function_set_goto(function, function->block,
            join, node->end, node->end)) return false;
    function->block = join;
    function->terminated = false;
    result->register_id = target;
    result->type = option.type;
    result->valid = true;
    return true;
}

bool mmay_checker_private_compile_array_edit_call(
    MarimayFunctionChecker *function, const MarimayAstNode *node,
    const MarimayExpressionValue *receiver, bool *resolved,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *member = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_MEMBER_EXPRESSION, 0);
    const MarimayAstNode *callee = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_NAME_EXPRESSION, 0);
    const MarimayAstNode *arguments = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_ARGUMENTS, 0);
    const MarimayAstNode *unused = NULL, *index_node = NULL;
    const MarimaySyntaxToken *name = NULL;
    MarimayTypeId element = MMAY_TYPE_INVALID;
    MarimayMirArrayId array = MMAY_MIR_NO_ARRAY;
    MarimayExpressionValue index;
    size_t count = 0, child = 0;
    bool copy = false;
    *resolved = false;
    if (member != NULL) {
        if (!mmay_checker_private_member_parts(member, &unused, &name)) return true;
    } else {
        name = mmay_checker_private_check_child_token(callee, MMAY_TOKEN_IDENTIFIER, 0);
    }
    if (name == NULL || name->synthetic) return true;
    copy = mmay_checker_private_check_text_equal(
        function->checker, name->start, name->end, "copy");
    if (!copy && !mmay_checker_private_check_text_equal(
            function->checker, name->start, name->end, "removeAt")) return true;
    *resolved = true;
    if (mmay_checker_private_check_child_node(node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0) != NULL) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "Array edit operations infer types from their receiver", node->start, node->end);
        return false;
    }
    for (child = 0; arguments != NULL && child < arguments->child_count; child += 1) {
        const MarimayAstElement *argument = mmay_ast_child(arguments, child);
        if (argument != NULL && argument->kind == MMAY_AST_ELEMENT_NODE) {
            if (count == (member != NULL ? 0U : 1U)) index_node = argument->as.node;
            count += 1;
        }
    }
    if (count != (member != NULL ? 0U : 1U) + (copy ? 0U : 1U)) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_CALL_ARITY,
            "Array edit call has an invalid argument count", node->start, node->end);
        return false;
    }
    if (!mmay_type_array_element(&function->checker->result->types, receiver->type, &element)
        || !mmay_array_layout(function, receiver->type, &array)) return false;
    if (copy) return mmay_array_edit_emit(function, node, MMAY_MIR_ARRAY_COPY,
        receiver->type, array, receiver->register_id, MMAY_MIR_NO_REGISTER, result);
    if (!mmay_compile_expression(function, index_node, MMAY_TYPE_I64, &index)) return false;
    if (mmay_expression_is_never(function, &index)) { *result = index; return true; }
    if (index.type != MMAY_TYPE_I64) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "removeAt index must have exact type int", index_node->start, index_node->end);
        return false;
    }
    return mmay_array_edit_remove(function, node, *receiver, index, element, array, result);
}
