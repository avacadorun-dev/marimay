#include "checker_array_mutation_api.h"

#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_generic_types_api.h"
#include "checker_lookup_api.h"

#include <limits.h>
#include <string.h>

typedef enum MarimayArrayMutation {
    MMAY_ARRAY_MUTATION_PUSH,
    MMAY_ARRAY_MUTATION_POP,
    MMAY_ARRAY_MUTATION_TRUNCATE,
    MMAY_ARRAY_MUTATION_CLEAR,
    MMAY_ARRAY_MUTATION_RESERVE
} MarimayArrayMutation;

bool mmay_checker_private_array_capture_generation(
    MarimayFunctionChecker *function, MarimayMirArrayId array, MarimayMirRegister receiver,
    size_t start, size_t end, MarimayMirRegister *generation
) {
    MarimayMirInstruction instruction;
    if (function == NULL || generation == NULL
        || !mmay_function_add_register(function, MMAY_TYPE_U64, generation)) return false;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ARRAY_GENERATION;
    instruction.target = *generation; instruction.first = receiver;
    instruction.second = MMAY_MIR_NO_REGISTER; instruction.third = MMAY_MIR_NO_REGISTER;
    instruction.array = array; instruction.span.start = start; instruction.span.end = end;
    return mmay_function_append_instruction(function, &instruction);
}

bool mmay_checker_private_array_require_generation(
    MarimayFunctionChecker *function, MarimayMirArrayId array, MarimayTypeId array_type,
    MarimayMirRegister receiver, MarimayMirRegister generation, size_t start, size_t end,
    MarimayMirRegister *checked_receiver
) {
    MarimayMirInstruction instruction;
    if (function == NULL || checked_receiver == NULL
        || !mmay_function_add_register(function, array_type, checked_receiver)) return false;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ARRAY_REQUIRE_GENERATION;
    instruction.target = *checked_receiver; instruction.first = receiver;
    instruction.second = generation; instruction.third = MMAY_MIR_NO_REGISTER;
    instruction.array = array; instruction.span.start = start; instruction.span.end = end;
    return mmay_function_append_instruction(function, &instruction);
}

static const MarimayAstNode *mmay_array_mutation_child(
    const MarimayAstNode *node, MarimaySyntaxKind kind, size_t ordinal
) {
    size_t index = 0;
    size_t found = 0;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE
            && element->as.node != NULL && element->as.node->kind == kind) {
            if (found++ == ordinal) return element->as.node;
        }
    }
    return NULL;
}

static bool mmay_array_mutation_kind(
    const MarimayChecker *checker, const MarimaySyntaxToken *name,
    MarimayArrayMutation *result
) {
    if (checker == NULL || name == NULL || name->synthetic || result == NULL) return false;
    if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "push")) {
        *result = MMAY_ARRAY_MUTATION_PUSH; return true;
    }
    if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "pop")) {
        *result = MMAY_ARRAY_MUTATION_POP; return true;
    }
    if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "truncate")) {
        *result = MMAY_ARRAY_MUTATION_TRUNCATE; return true;
    }
    if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "clear")) {
        *result = MMAY_ARRAY_MUTATION_CLEAR; return true;
    }
    if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "reserve")) {
        *result = MMAY_ARRAY_MUTATION_RESERVE; return true;
    }
    return false;
}

static MarimayCheckEnumDeclaration *mmay_array_mutation_prelude_enum(
    MarimayFunctionChecker *function, const char *name
) {
    MarimaySymbolId symbol;
    size_t index = 0;
    if (function == NULL || !mmay_symbol_id_from_bytes(
            "std", 3, "prelude", 7, (const unsigned char *)name,
            strlen(name), 0, &symbol)) return NULL;
    for (index = 0; index < function->checker->prelude_enums.length; index += 1) {
        MarimayCheckEnumDeclaration *candidate = mmay_vector_at(
            &function->checker->prelude_enums, index
        );
        if (candidate != NULL && mmay_symbol_id_equal(candidate->symbol, symbol)) return candidate;
    }
    return NULL;
}

static bool mmay_array_mutation_result_type(
    MarimayFunctionChecker *function, const char *base_name,
    MarimayTypeId first, MarimayTypeId second, MarimayTypeId *result
) {
    MarimayCheckEnumDeclaration *base = mmay_array_mutation_prelude_enum(function, base_name);
    MarimayVector arguments;
    bool ok = false;
    bool initialized = false;
    memset(&arguments, 0, sizeof(arguments));
    if (base == NULL || !mmay_vector_init(&arguments, function->checker->result->allocator,
            sizeof(MarimayTypeId))) goto cleanup;
    initialized = true;
    if (!mmay_vector_push(&arguments, &first)
        || (second != MMAY_TYPE_INVALID && !mmay_vector_push(&arguments, &second))
        || !mmay_checker_private_intern_generic_nominal(
            function->checker, MMAY_TYPE_KIND_NOMINAL, base->symbol, &arguments, result)) goto cleanup;
    ok = true;
cleanup:
    if (initialized) mmay_vector_dispose(&arguments);
    if (!ok && !mmay_checker_has_failed(function->checker))
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
    return ok;
}

static bool mmay_array_mutation_arguments(
    const MarimayAstNode *arguments, const MarimayAstNode **first, size_t *count
) {
    size_t index = 0;
    *first = NULL; *count = 0;
    for (index = 0; arguments != NULL && index < arguments->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(arguments, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE) {
            if (*count == 0) *first = element->as.node;
            *count += 1;
        }
    }
    return arguments != NULL;
}

static bool mmay_array_mutation_emit_raw(
    MarimayFunctionChecker *function, MarimayMirInstructionKind kind,
    MarimayTypeId target_type, MarimayMirRegister receiver, MarimayMirRegister value,
    MarimayMirArrayId array, size_t start, size_t end, MarimayExpressionValue *result
) {
    MarimayMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    if (target_type == MMAY_TYPE_INVALID || !mmay_function_add_register(
            function, target_type, &instruction.target)) return false;
    instruction.kind = kind;
    instruction.first = receiver;
    instruction.second = value;
    instruction.third = MMAY_MIR_NO_REGISTER;
    instruction.array = array;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!mmay_function_append_instruction(function, &instruction)) return false;
    result->type = target_type;
    result->register_id = instruction.target;
    result->valid = true;
    return true;
}

static bool mmay_array_mutation_emit_enum(
    MarimayFunctionChecker *function, MarimayCheckEnumDeclaration *base,
    MarimayTypeId concrete_type, size_t variant, const MarimayMirEnumArgument *arguments,
    size_t argument_count, size_t start, size_t end, MarimayExpressionValue *result
) {
    MarimayCheckEnumDeclaration concrete;
    if (base == NULL) return false;
    concrete = *base;
    concrete.type = concrete_type;
    return mmay_checker_private_function_emit_enum_construction(
        function, &concrete, variant, arguments, argument_count, start, end, result
    );
}

static bool mmay_array_mutation_emit_length(
    MarimayFunctionChecker *function, MarimayExpressionValue receiver, MarimayMirArrayId array,
    size_t start, size_t end, MarimayExpressionValue *result
) {
    return mmay_array_mutation_emit_raw(function, MMAY_MIR_ARRAY_LENGTH,
        MMAY_TYPE_I64, receiver.register_id, MMAY_MIR_NO_REGISTER, array,
        start, end, result);
}

static bool mmay_array_mutation_emit_constant(
    MarimayFunctionChecker *function, int64_t value, size_t start, size_t end,
    MarimayExpressionValue *result
) {
    return mmay_function_emit_instruction(function, MMAY_MIR_CONSTANT_I64,
        MMAY_TYPE_I64, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, value,
        false, start, end, result);
}

static bool mmay_array_mutation_emit_error_result(
    MarimayFunctionChecker *function, const char *error_name,
    size_t error_variant, const MarimayMirEnumArgument *error_arguments, size_t error_argument_count,
    MarimayTypeId result_type, MarimayMirRegister target, size_t start, size_t end
) {
    MarimayCheckEnumDeclaration *error = mmay_array_mutation_prelude_enum(function, error_name);
    MarimayCheckEnumDeclaration *result_base = mmay_array_mutation_prelude_enum(function, "Result");
    MarimayExpressionValue error_value, wrapped;
    if (!mmay_array_mutation_emit_enum(function, error, error->type, error_variant,
            error_arguments, error_argument_count, start, end, &error_value)
        || !mmay_array_mutation_emit_enum(function, result_base, result_type, 1,
            &(MarimayMirEnumArgument){.register_id = error_value.register_id, .type = error_value.type},
            1, start, end, &wrapped)
        || !mmay_checker_private_function_emit_copy(function, target,
            wrapped.register_id, start, end)) return false;
    return true;
}

static bool mmay_array_mutation_emit_ok_result(
    MarimayFunctionChecker *function, MarimayTypeId result_type, MarimayMirRegister target,
    size_t start, size_t end
) {
    MarimayExpressionValue wrapped;
    return mmay_array_mutation_emit_enum(function,
        mmay_array_mutation_prelude_enum(function, "Result"), result_type, 0,
        NULL, 0, start, end, &wrapped)
        && mmay_checker_private_function_emit_copy(function, target,
            wrapped.register_id, start, end);
}

static bool mmay_array_mutation_blocks(
    MarimayFunctionChecker *function, MarimayMirBlockId *error_block,
    MarimayMirBlockId *success_block, MarimayMirBlockId *join
) {
    return mmay_checker_private_function_add_block(function, error_block)
        && mmay_checker_private_function_add_block(function, success_block)
        && mmay_checker_private_function_add_block(function, join);
}

bool mmay_checker_private_compile_array_mutation_call(
    MarimayFunctionChecker *function, const MarimayAstNode *node,
    const MarimayAstNode *precompiled_receiver_node,
    const MarimayExpressionValue *precompiled_receiver, bool *resolved,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *callee = mmay_array_mutation_child(node, MMAY_SYNTAX_NAME_EXPRESSION, 0);
    const MarimayAstNode *member = mmay_array_mutation_child(node, MMAY_SYNTAX_MEMBER_EXPRESSION, 0);
    const MarimayAstNode *arguments = mmay_array_mutation_child(node, MMAY_SYNTAX_ARGUMENTS, 0);
    const MarimayAstNode *receiver_node = NULL;
    const MarimayAstNode *value_node = NULL;
    const MarimaySyntaxToken *name = NULL;
    MarimayArrayMutation operation;
    MarimayExpressionValue receiver, value;
    MarimayTypeId element = MMAY_TYPE_INVALID, return_type = MMAY_TYPE_VOID;
    MarimayMirArrayId array = MMAY_MIR_NO_ARRAY;
    size_t argument_count = 0;
    bool dot = false;
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) return false;
    *resolved = false;
    if (callee != NULL) name = mmay_checker_private_check_child_token(callee, MMAY_TOKEN_IDENTIFIER, 0);
    else if (member != NULL && mmay_checker_private_member_parts(member, &receiver_node, &name)) dot = true;
    if (!mmay_array_mutation_kind(function->checker, name, &operation)) return true;
    if (mmay_array_mutation_child(node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0) != NULL) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "Array mutation operations do not accept type arguments", node->start, node->end);
        *resolved = true; return false;
    }
    if (!mmay_array_mutation_arguments(arguments, &value_node, &argument_count)) return true;
    if (!dot) {
        receiver_node = value_node;
        value_node = NULL;
        if (argument_count > 1) {
            size_t index = 0, found = 0;
            for (index = 0; index < arguments->child_count; index += 1) {
                const MarimayAstElement *item = mmay_ast_child(arguments, index);
                if (item != NULL && item->kind == MMAY_AST_ELEMENT_NODE && found++ == 1) {
                    value_node = item->as.node; break;
                }
            }
        }
    }
    if ((operation == MMAY_ARRAY_MUTATION_POP || operation == MMAY_ARRAY_MUTATION_CLEAR)
        ? (dot ? argument_count != 0 : argument_count != 1)
        : (dot ? argument_count != 1 : argument_count != 2)) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_CALL_ARITY,
            "Array mutation call has an invalid argument count", arguments->start, arguments->end);
        *resolved = true; return false;
    }
    memset(&receiver, 0, sizeof(receiver)); receiver.register_id = MMAY_MIR_NO_REGISTER;
    if (precompiled_receiver != NULL) {
        if (precompiled_receiver_node != receiver_node || !precompiled_receiver->valid) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT); return false;
        }
        receiver = *precompiled_receiver;
    } else if (!mmay_compile_expression(function, receiver_node, MMAY_TYPE_INVALID, &receiver)) return false;
    *resolved = true;
    if (mmay_expression_is_never(function, &receiver)) { *result = receiver; return true; }
    if (!mmay_type_array_element(&function->checker->result->types, receiver.type, &element)
        || !mmay_array_layout(function, receiver.type, &array)) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "array mutation requires a []T receiver", receiver_node->start, receiver_node->end);
        return false;
    }
    memset(&value, 0, sizeof(value)); value.register_id = MMAY_MIR_NO_REGISTER;
    if (value_node != NULL && !mmay_compile_expression(
            function, value_node,
            operation == MMAY_ARRAY_MUTATION_PUSH ? element : MMAY_TYPE_I64,
            &value
        )) return false;
    if (value_node != NULL && mmay_expression_is_never(function, &value)) { *result = value; return true; }
    if (value_node != NULL && value.type != (operation
            == MMAY_ARRAY_MUTATION_PUSH ? element : MMAY_TYPE_I64)) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            operation == MMAY_ARRAY_MUTATION_PUSH
                ? "push value must have the exact Array element type"
                : "Array mutation count must have exact type i64",
            value_node->start, value_node->end);
        return false;
    }
    if (operation == MMAY_ARRAY_MUTATION_POP) {
        if (!mmay_array_mutation_result_type(function, "Option", element, MMAY_TYPE_INVALID, &return_type)) return false;
    } else if (operation == MMAY_ARRAY_MUTATION_TRUNCATE) {
        MarimayCheckEnumDeclaration *error = mmay_array_mutation_prelude_enum(function, "ArrayTruncateError");
        if (error == NULL || !mmay_array_mutation_result_type(function, "Result", MMAY_TYPE_VOID, error->type, &return_type)) return false;
    } else if (operation == MMAY_ARRAY_MUTATION_RESERVE
        || operation == MMAY_ARRAY_MUTATION_PUSH) {
        MarimayCheckEnumDeclaration *error = mmay_array_mutation_prelude_enum(function, "ArrayGrowthError");
        if (error == NULL || !mmay_array_mutation_result_type(function, "Result", MMAY_TYPE_VOID, error->type, &return_type)) return false;
    }
    if (operation == MMAY_ARRAY_MUTATION_CLEAR) {
        MarimayExpressionValue ignored;
        if (!mmay_array_mutation_emit_raw(function, MMAY_MIR_ARRAY_CLEAR,
                receiver.type, receiver.register_id, MMAY_MIR_NO_REGISTER,
                array, node->start, node->end, &ignored)) return false;
        result->type = MMAY_TYPE_VOID;
        result->register_id = MMAY_MIR_NO_REGISTER;
        result->valid = true;
        return true;
    }
    if (operation == MMAY_ARRAY_MUTATION_PUSH) {
        MarimayExpressionValue length, maximum, at_limit, raw;
        MarimayMirBlockId error_block, success_block, join;
        MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
        MarimayMirBlockId entry = function->block;
        if (!mmay_function_add_register(function, return_type, &target)
            || !mmay_array_mutation_emit_length(function, receiver, array,
                node->start, node->end, &length)
            || !mmay_array_mutation_emit_constant(function, INT64_MAX,
                node->start, node->end, &maximum)
            || !mmay_function_emit_instruction(function, MMAY_MIR_EQUAL_I64,
                MMAY_TYPE_BOOL, length.register_id, maximum.register_id, 0,
                false, node->start, node->end, &at_limit)
            || !mmay_array_mutation_blocks(
                function, &error_block, &success_block, &join
            )
            || !mmay_checker_private_function_set_branch(
                function, entry, at_limit.register_id, error_block,
                success_block, node->start, node->end
            )) return false;
        function->block = error_block;
        function->terminated = false;
        if (!mmay_array_mutation_emit_error_result(
                function, "ArrayGrowthError", 0,
                &(MarimayMirEnumArgument){
                    .register_id = maximum.register_id,
                    .type = maximum.type
                },
                1, return_type, target, node->start, node->end
            ) || !mmay_checker_private_function_set_goto(
                function, function->block, join, node->end, node->end
            )) return false;
        function->block = success_block;
        function->terminated = false;
        if (!mmay_array_mutation_emit_raw(
                function, MMAY_MIR_ARRAY_PUSH, receiver.type,
                receiver.register_id, value.register_id, array,
                node->start, node->end, &raw
            ) || !mmay_array_mutation_emit_ok_result(
                function, return_type, target, node->start, node->end
            ) || !mmay_checker_private_function_set_goto(
                function, function->block, join, node->end, node->end
            )) return false;
        function->block = join;
        function->terminated = false;
        result->type = return_type;
        result->register_id = target;
        result->valid = true;
        return true;
    }
    if (operation == MMAY_ARRAY_MUTATION_POP) {
        MarimayCheckEnumDeclaration *option = mmay_array_mutation_prelude_enum(function, "Option");
        MarimayExpressionValue length, zero, has_value, raw, wrapped;
        MarimayMirBlockId none_block, some_block, join;
        MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
        MarimayMirBlockId entry = function->block;
        if (option == NULL || !mmay_function_add_register(function, return_type, &target)
            || !mmay_array_mutation_emit_length(function, receiver, array,
                node->start, node->end, &length)
            || !mmay_array_mutation_emit_constant(function, 0, node->start, node->end, &zero)
            || !mmay_function_emit_instruction(function, MMAY_MIR_LESS_I64,
                MMAY_TYPE_BOOL, zero.register_id, length.register_id, 0, false,
                node->start, node->end, &has_value)
            || !mmay_array_mutation_blocks(function, &none_block, &some_block, &join)
            || !mmay_checker_private_function_set_branch(function, entry,
                has_value.register_id, some_block, none_block, node->start, node->end)) return false;
        function->block = none_block;
        function->terminated = false;
        if (!mmay_array_mutation_emit_enum(function, option, return_type, 1,
                NULL, 0, node->start, node->end, &wrapped)
            || !mmay_checker_private_function_emit_copy(function, target,
                wrapped.register_id, node->start, node->end)
            || !mmay_checker_private_function_set_goto(function, function->block,
                join, node->end, node->end)) return false;
        function->block = some_block;
        function->terminated = false;
        if (!mmay_array_mutation_emit_raw(function, MMAY_MIR_ARRAY_POP, element,
                receiver.register_id, MMAY_MIR_NO_REGISTER, array,
                node->start, node->end, &raw)
            || !mmay_array_mutation_emit_enum(function, option, return_type, 0,
                &(MarimayMirEnumArgument){.register_id = raw.register_id, .type = raw.type},
                1, node->start, node->end, &wrapped)
            || !mmay_checker_private_function_emit_copy(function, target,
                wrapped.register_id, node->start, node->end)
            || !mmay_checker_private_function_set_goto(function, function->block,
                join, node->end, node->end)) return false;
        function->block = join;
        function->terminated = false;
        result->type = return_type;
        result->register_id = target;
        result->valid = true;
        return true;
    }
    {
        MarimayExpressionValue length, zero, invalid, raw;
        MarimayMirBlockId error_block, success_block, join;
        MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
        MarimayMirBlockId entry = function->block;
        MarimayCheckEnumDeclaration *error = mmay_array_mutation_prelude_enum(
            function, operation == MMAY_ARRAY_MUTATION_TRUNCATE
                ? "ArrayTruncateError" : "ArrayGrowthError"
        );
        if (error == NULL || !mmay_function_add_register(function, return_type, &target)
            || !mmay_array_mutation_emit_length(function, receiver, array,
                node->start, node->end, &length)
            || !mmay_array_mutation_emit_constant(function, 0, node->start, node->end, &zero)
            || !mmay_function_emit_instruction(function, MMAY_MIR_LESS_I64,
                MMAY_TYPE_BOOL, value.register_id, zero.register_id, 0, false,
                node->start, node->end, &invalid)
            || !mmay_array_mutation_blocks(function, &error_block, &success_block, &join)
            || !mmay_checker_private_function_set_branch(function, entry,
                invalid.register_id, error_block, success_block, node->start, node->end)) return false;
        function->block = error_block;
        function->terminated = false;
        if (operation == MMAY_ARRAY_MUTATION_TRUNCATE) {
            MarimayMirEnumArgument fields[2] = {
                {.register_id = value.register_id, .type = value.type},
                {.register_id = length.register_id, .type = length.type}
            };
            if (!mmay_array_mutation_emit_error_result(function, "ArrayTruncateError",
                    0, fields, 2, return_type, target, node->start, node->end)) return false;
        } else if (!mmay_array_mutation_emit_error_result(function, "ArrayGrowthError",
                1, &(MarimayMirEnumArgument){.register_id = value.register_id, .type = value.type},
                1, return_type, target, node->start, node->end)) return false;
        if (!mmay_checker_private_function_set_goto(function, function->block,
                join, node->end, node->end)) return false;
        function->block = success_block;
        function->terminated = false;
        if (operation == MMAY_ARRAY_MUTATION_TRUNCATE) {
            MarimayExpressionValue expands;
            MarimayMirBlockId raw_block = MMAY_MIR_NO_BLOCK;
            if (!mmay_function_emit_instruction(function, MMAY_MIR_LESS_I64,
                    MMAY_TYPE_BOOL, length.register_id, value.register_id, 0,
                    false, node->start, node->end, &expands)
                || !mmay_checker_private_function_add_block(function, &raw_block)
                || !mmay_checker_private_function_set_branch(function,
                    success_block, expands.register_id, error_block, raw_block,
                    node->start, node->end)) return false;
            function->block = raw_block;
            function->terminated = false;
        } else {
            MarimayExpressionValue maximum, available, too_large;
            MarimayMirBlockId limit_block = MMAY_MIR_NO_BLOCK;
            MarimayMirBlockId raw_block = MMAY_MIR_NO_BLOCK;
            if (!mmay_array_mutation_emit_constant(function, INT64_MAX,
                    node->start, node->end, &maximum)
                || !mmay_function_emit_instruction(function,
                    MMAY_MIR_SUBTRACT_CHECKED_I64, MMAY_TYPE_I64,
                    maximum.register_id, length.register_id, 0, false,
                    node->start, node->end, &available)
                || !mmay_function_emit_instruction(function, MMAY_MIR_LESS_I64,
                    MMAY_TYPE_BOOL, available.register_id, value.register_id,
                    0, false, node->start, node->end, &too_large)
                || !mmay_checker_private_function_add_block(function, &limit_block)
                || !mmay_checker_private_function_add_block(function, &raw_block)
                || !mmay_checker_private_function_set_branch(function,
                    success_block, too_large.register_id, limit_block, raw_block,
                    node->start, node->end)) return false;
            function->block = limit_block;
            function->terminated = false;
            if (!mmay_array_mutation_emit_error_result(function, "ArrayGrowthError",
                    0, &(MarimayMirEnumArgument){.register_id = maximum.register_id,
                        .type = maximum.type}, 1, return_type, target,
                    node->start, node->end)
                || !mmay_checker_private_function_set_goto(function, function->block,
                    join, node->end, node->end)) return false;
            function->block = raw_block;
            function->terminated = false;
        }
        if (!mmay_array_mutation_emit_raw(function,
                operation == MMAY_ARRAY_MUTATION_TRUNCATE ? MMAY_MIR_ARRAY_TRUNCATE : MMAY_MIR_ARRAY_RESERVE,
                receiver.type, receiver.register_id, value.register_id, array,
                node->start, node->end, &raw)
            || !mmay_array_mutation_emit_ok_result(function, return_type, target,
                node->start, node->end)
            || !mmay_checker_private_function_set_goto(function, function->block,
                join, node->end, node->end)) return false;
        function->block = join;
        function->terminated = false;
        result->type = return_type;
        result->register_id = target;
        result->valid = true;
        return true;
    }
}
