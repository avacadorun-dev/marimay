#include "checker_array_mutation_api.h"

#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_generic_types_api.h"
#include "checker_lookup_api.h"

#include <limits.h>
#include <string.h>

typedef enum RudaArrayMutation {
    RUDA_ARRAY_MUTATION_PUSH,
    RUDA_ARRAY_MUTATION_POP,
    RUDA_ARRAY_MUTATION_TRUNCATE,
    RUDA_ARRAY_MUTATION_CLEAR,
    RUDA_ARRAY_MUTATION_RESERVE
} RudaArrayMutation;

bool ruda_checker_private_array_capture_generation(
    RudaFunctionChecker *function, RudaMirArrayId array, RudaMirRegister receiver,
    size_t start, size_t end, RudaMirRegister *generation
) {
    RudaMirInstruction instruction;
    if (function == NULL || generation == NULL
        || !ruda_function_add_register(function, RUDA_TYPE_U64, generation)) return false;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_GENERATION;
    instruction.target = *generation; instruction.first = receiver;
    instruction.second = RUDA_MIR_NO_REGISTER; instruction.third = RUDA_MIR_NO_REGISTER;
    instruction.array = array; instruction.span.start = start; instruction.span.end = end;
    return ruda_function_append_instruction(function, &instruction);
}

bool ruda_checker_private_array_require_generation(
    RudaFunctionChecker *function, RudaMirArrayId array, RudaTypeId array_type,
    RudaMirRegister receiver, RudaMirRegister generation, size_t start, size_t end,
    RudaMirRegister *checked_receiver
) {
    RudaMirInstruction instruction;
    if (function == NULL || checked_receiver == NULL
        || !ruda_function_add_register(function, array_type, checked_receiver)) return false;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_REQUIRE_GENERATION;
    instruction.target = *checked_receiver; instruction.first = receiver;
    instruction.second = generation; instruction.third = RUDA_MIR_NO_REGISTER;
    instruction.array = array; instruction.span.start = start; instruction.span.end = end;
    return ruda_function_append_instruction(function, &instruction);
}

static const RudaAstNode *ruda_array_mutation_child(
    const RudaAstNode *node, RudaSyntaxKind kind, size_t ordinal
) {
    size_t index = 0;
    size_t found = 0;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node != NULL && element->as.node->kind == kind) {
            if (found++ == ordinal) return element->as.node;
        }
    }
    return NULL;
}

static bool ruda_array_mutation_kind(
    const RudaChecker *checker, const RudaSyntaxToken *name,
    RudaArrayMutation *result
) {
    if (checker == NULL || name == NULL || name->synthetic || result == NULL) return false;
    if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "push")) {
        *result = RUDA_ARRAY_MUTATION_PUSH; return true;
    }
    if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "pop")) {
        *result = RUDA_ARRAY_MUTATION_POP; return true;
    }
    if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "truncate")) {
        *result = RUDA_ARRAY_MUTATION_TRUNCATE; return true;
    }
    if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "clear")) {
        *result = RUDA_ARRAY_MUTATION_CLEAR; return true;
    }
    if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "reserve")) {
        *result = RUDA_ARRAY_MUTATION_RESERVE; return true;
    }
    return false;
}

static RudaCheckEnumDeclaration *ruda_array_mutation_prelude_enum(
    RudaFunctionChecker *function, const char *name
) {
    RudaSymbolId symbol;
    size_t index = 0;
    if (function == NULL || !ruda_symbol_id_from_bytes(
            "std", 3, "prelude", 7, (const unsigned char *)name,
            strlen(name), 0, &symbol)) return NULL;
    for (index = 0; index < function->checker->prelude_enums.length; index += 1) {
        RudaCheckEnumDeclaration *candidate = ruda_vector_at(
            &function->checker->prelude_enums, index
        );
        if (candidate != NULL && ruda_symbol_id_equal(candidate->symbol, symbol)) return candidate;
    }
    return NULL;
}

static bool ruda_array_mutation_result_type(
    RudaFunctionChecker *function, const char *base_name,
    RudaTypeId first, RudaTypeId second, RudaTypeId *result
) {
    RudaCheckEnumDeclaration *base = ruda_array_mutation_prelude_enum(function, base_name);
    RudaVector arguments;
    bool ok = false;
    bool initialized = false;
    memset(&arguments, 0, sizeof(arguments));
    if (base == NULL || !ruda_vector_init(&arguments, function->checker->result->allocator,
            sizeof(RudaTypeId))) goto cleanup;
    initialized = true;
    if (!ruda_vector_push(&arguments, &first)
        || (second != RUDA_TYPE_INVALID && !ruda_vector_push(&arguments, &second))
        || !ruda_checker_private_intern_generic_nominal(
            function->checker, RUDA_TYPE_KIND_NOMINAL, base->symbol, &arguments, result)) goto cleanup;
    ok = true;
cleanup:
    if (initialized) ruda_vector_dispose(&arguments);
    if (!ok && !ruda_checker_has_failed(function->checker))
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
    return ok;
}

static bool ruda_array_mutation_arguments(
    const RudaAstNode *arguments, const RudaAstNode **first, size_t *count
) {
    size_t index = 0;
    *first = NULL; *count = 0;
    for (index = 0; arguments != NULL && index < arguments->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(arguments, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE) {
            if (*count == 0) *first = element->as.node;
            *count += 1;
        }
    }
    return arguments != NULL;
}

static bool ruda_array_mutation_emit_raw(
    RudaFunctionChecker *function, RudaMirInstructionKind kind,
    RudaTypeId target_type, RudaMirRegister receiver, RudaMirRegister value,
    RudaMirArrayId array, size_t start, size_t end, RudaExpressionValue *result
) {
    RudaMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    if (target_type == RUDA_TYPE_INVALID || !ruda_function_add_register(
            function, target_type, &instruction.target)) return false;
    instruction.kind = kind;
    instruction.first = receiver;
    instruction.second = value;
    instruction.third = RUDA_MIR_NO_REGISTER;
    instruction.array = array;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!ruda_function_append_instruction(function, &instruction)) return false;
    result->type = target_type;
    result->register_id = instruction.target;
    result->valid = true;
    return true;
}

static bool ruda_array_mutation_emit_enum(
    RudaFunctionChecker *function, RudaCheckEnumDeclaration *base,
    RudaTypeId concrete_type, size_t variant, const RudaMirEnumArgument *arguments,
    size_t argument_count, size_t start, size_t end, RudaExpressionValue *result
) {
    RudaCheckEnumDeclaration concrete;
    if (base == NULL) return false;
    concrete = *base;
    concrete.type = concrete_type;
    return ruda_checker_private_function_emit_enum_construction(
        function, &concrete, variant, arguments, argument_count, start, end, result
    );
}

static bool ruda_array_mutation_emit_length(
    RudaFunctionChecker *function, RudaExpressionValue receiver, RudaMirArrayId array,
    size_t start, size_t end, RudaExpressionValue *result
) {
    return ruda_array_mutation_emit_raw(function, RUDA_MIR_ARRAY_LENGTH,
        RUDA_TYPE_I64, receiver.register_id, RUDA_MIR_NO_REGISTER, array,
        start, end, result);
}

static bool ruda_array_mutation_emit_constant(
    RudaFunctionChecker *function, int64_t value, size_t start, size_t end,
    RudaExpressionValue *result
) {
    return ruda_function_emit_instruction(function, RUDA_MIR_CONSTANT_I64,
        RUDA_TYPE_I64, RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, value,
        false, start, end, result);
}

static bool ruda_array_mutation_emit_error_result(
    RudaFunctionChecker *function, const char *error_name,
    size_t error_variant, const RudaMirEnumArgument *error_arguments, size_t error_argument_count,
    RudaTypeId result_type, RudaMirRegister target, size_t start, size_t end
) {
    RudaCheckEnumDeclaration *error = ruda_array_mutation_prelude_enum(function, error_name);
    RudaCheckEnumDeclaration *result_base = ruda_array_mutation_prelude_enum(function, "Result");
    RudaExpressionValue error_value, wrapped;
    if (!ruda_array_mutation_emit_enum(function, error, error->type, error_variant,
            error_arguments, error_argument_count, start, end, &error_value)
        || !ruda_array_mutation_emit_enum(function, result_base, result_type, 1,
            &(RudaMirEnumArgument){.register_id = error_value.register_id, .type = error_value.type},
            1, start, end, &wrapped)
        || !ruda_checker_private_function_emit_copy(function, target,
            wrapped.register_id, start, end)) return false;
    return true;
}

static bool ruda_array_mutation_emit_ok_result(
    RudaFunctionChecker *function, RudaTypeId result_type, RudaMirRegister target,
    size_t start, size_t end
) {
    RudaExpressionValue wrapped;
    return ruda_array_mutation_emit_enum(function,
        ruda_array_mutation_prelude_enum(function, "Result"), result_type, 0,
        NULL, 0, start, end, &wrapped)
        && ruda_checker_private_function_emit_copy(function, target,
            wrapped.register_id, start, end);
}

static bool ruda_array_mutation_blocks(
    RudaFunctionChecker *function, RudaMirBlockId *error_block,
    RudaMirBlockId *success_block, RudaMirBlockId *join
) {
    return ruda_checker_private_function_add_block(function, error_block)
        && ruda_checker_private_function_add_block(function, success_block)
        && ruda_checker_private_function_add_block(function, join);
}

bool ruda_checker_private_compile_array_mutation_call(
    RudaFunctionChecker *function, const RudaAstNode *node,
    const RudaAstNode *precompiled_receiver_node,
    const RudaExpressionValue *precompiled_receiver, bool *resolved,
    RudaExpressionValue *result
) {
    const RudaAstNode *callee = ruda_array_mutation_child(node, RUDA_SYNTAX_NAME_EXPRESSION, 0);
    const RudaAstNode *member = ruda_array_mutation_child(node, RUDA_SYNTAX_MEMBER_EXPRESSION, 0);
    const RudaAstNode *arguments = ruda_array_mutation_child(node, RUDA_SYNTAX_ARGUMENTS, 0);
    const RudaAstNode *receiver_node = NULL;
    const RudaAstNode *value_node = NULL;
    const RudaSyntaxToken *name = NULL;
    RudaArrayMutation operation;
    RudaExpressionValue receiver, value;
    RudaTypeId element = RUDA_TYPE_INVALID, return_type = RUDA_TYPE_VOID;
    RudaMirArrayId array = RUDA_MIR_NO_ARRAY;
    size_t argument_count = 0;
    bool dot = false;
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) return false;
    *resolved = false;
    if (callee != NULL) name = ruda_checker_private_check_child_token(callee, RUDA_TOKEN_IDENTIFIER, 0);
    else if (member != NULL && ruda_checker_private_member_parts(member, &receiver_node, &name)) dot = true;
    if (!ruda_array_mutation_kind(function->checker, name, &operation)) return true;
    if (ruda_array_mutation_child(node, RUDA_SYNTAX_TYPE_ARGUMENTS, 0) != NULL) {
        (void)ruda_checker_add_diagnostic(function->checker, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "Array mutation operations do not accept type arguments", node->start, node->end);
        *resolved = true; return false;
    }
    if (!ruda_array_mutation_arguments(arguments, &value_node, &argument_count)) return true;
    if (!dot) {
        receiver_node = value_node;
        value_node = NULL;
        if (argument_count > 1) {
            size_t index = 0, found = 0;
            for (index = 0; index < arguments->child_count; index += 1) {
                const RudaAstElement *item = ruda_ast_child(arguments, index);
                if (item != NULL && item->kind == RUDA_AST_ELEMENT_NODE && found++ == 1) {
                    value_node = item->as.node; break;
                }
            }
        }
    }
    if ((operation == RUDA_ARRAY_MUTATION_POP || operation == RUDA_ARRAY_MUTATION_CLEAR)
        ? (dot ? argument_count != 0 : argument_count != 1)
        : (dot ? argument_count != 1 : argument_count != 2)) {
        (void)ruda_checker_add_diagnostic(function->checker, RUDA_DIAGNOSTIC_CALL_ARITY,
            "Array mutation call has an invalid argument count", arguments->start, arguments->end);
        *resolved = true; return false;
    }
    memset(&receiver, 0, sizeof(receiver)); receiver.register_id = RUDA_MIR_NO_REGISTER;
    if (precompiled_receiver != NULL) {
        if (precompiled_receiver_node != receiver_node || !precompiled_receiver->valid) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT); return false;
        }
        receiver = *precompiled_receiver;
    } else if (!ruda_compile_expression(function, receiver_node, RUDA_TYPE_INVALID, &receiver)) return false;
    *resolved = true;
    if (ruda_expression_is_never(function, &receiver)) { *result = receiver; return true; }
    if (!ruda_type_array_element(&function->checker->result->types, receiver.type, &element)
        || !ruda_array_layout(function, receiver.type, &array)) {
        (void)ruda_checker_add_diagnostic(function->checker, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "array mutation requires a []T receiver", receiver_node->start, receiver_node->end);
        return false;
    }
    memset(&value, 0, sizeof(value)); value.register_id = RUDA_MIR_NO_REGISTER;
    if (value_node != NULL && !ruda_compile_expression(
            function, value_node,
            operation == RUDA_ARRAY_MUTATION_PUSH ? element : RUDA_TYPE_I64,
            &value
        )) return false;
    if (value_node != NULL && ruda_expression_is_never(function, &value)) { *result = value; return true; }
    if (value_node != NULL && value.type != (operation
            == RUDA_ARRAY_MUTATION_PUSH ? element : RUDA_TYPE_I64)) {
        (void)ruda_checker_add_diagnostic(function->checker, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            operation == RUDA_ARRAY_MUTATION_PUSH
                ? "push value must have the exact Array element type"
                : "Array mutation count must have exact type i64",
            value_node->start, value_node->end);
        return false;
    }
    if (operation == RUDA_ARRAY_MUTATION_POP) {
        if (!ruda_array_mutation_result_type(function, "Option", element, RUDA_TYPE_INVALID, &return_type)) return false;
    } else if (operation == RUDA_ARRAY_MUTATION_TRUNCATE) {
        RudaCheckEnumDeclaration *error = ruda_array_mutation_prelude_enum(function, "ArrayTruncateError");
        if (error == NULL || !ruda_array_mutation_result_type(function, "Result", RUDA_TYPE_VOID, error->type, &return_type)) return false;
    } else if (operation == RUDA_ARRAY_MUTATION_RESERVE
        || operation == RUDA_ARRAY_MUTATION_PUSH) {
        RudaCheckEnumDeclaration *error = ruda_array_mutation_prelude_enum(function, "ArrayGrowthError");
        if (error == NULL || !ruda_array_mutation_result_type(function, "Result", RUDA_TYPE_VOID, error->type, &return_type)) return false;
    }
    if (operation == RUDA_ARRAY_MUTATION_CLEAR) {
        RudaExpressionValue ignored;
        if (!ruda_array_mutation_emit_raw(function, RUDA_MIR_ARRAY_CLEAR,
                receiver.type, receiver.register_id, RUDA_MIR_NO_REGISTER,
                array, node->start, node->end, &ignored)) return false;
        result->type = RUDA_TYPE_VOID;
        result->register_id = RUDA_MIR_NO_REGISTER;
        result->valid = true;
        return true;
    }
    if (operation == RUDA_ARRAY_MUTATION_PUSH) {
        RudaExpressionValue length, maximum, at_limit, raw;
        RudaMirBlockId error_block, success_block, join;
        RudaMirRegister target = RUDA_MIR_NO_REGISTER;
        RudaMirBlockId entry = function->block;
        if (!ruda_function_add_register(function, return_type, &target)
            || !ruda_array_mutation_emit_length(function, receiver, array,
                node->start, node->end, &length)
            || !ruda_array_mutation_emit_constant(function, INT64_MAX,
                node->start, node->end, &maximum)
            || !ruda_function_emit_instruction(function, RUDA_MIR_EQUAL_I64,
                RUDA_TYPE_BOOL, length.register_id, maximum.register_id, 0,
                false, node->start, node->end, &at_limit)
            || !ruda_array_mutation_blocks(
                function, &error_block, &success_block, &join
            )
            || !ruda_checker_private_function_set_branch(
                function, entry, at_limit.register_id, error_block,
                success_block, node->start, node->end
            )) return false;
        function->block = error_block;
        function->terminated = false;
        if (!ruda_array_mutation_emit_error_result(
                function, "ArrayGrowthError", 0,
                &(RudaMirEnumArgument){
                    .register_id = maximum.register_id,
                    .type = maximum.type
                },
                1, return_type, target, node->start, node->end
            ) || !ruda_checker_private_function_set_goto(
                function, function->block, join, node->end, node->end
            )) return false;
        function->block = success_block;
        function->terminated = false;
        if (!ruda_array_mutation_emit_raw(
                function, RUDA_MIR_ARRAY_PUSH, receiver.type,
                receiver.register_id, value.register_id, array,
                node->start, node->end, &raw
            ) || !ruda_array_mutation_emit_ok_result(
                function, return_type, target, node->start, node->end
            ) || !ruda_checker_private_function_set_goto(
                function, function->block, join, node->end, node->end
            )) return false;
        function->block = join;
        function->terminated = false;
        result->type = return_type;
        result->register_id = target;
        result->valid = true;
        return true;
    }
    if (operation == RUDA_ARRAY_MUTATION_POP) {
        RudaCheckEnumDeclaration *option = ruda_array_mutation_prelude_enum(function, "Option");
        RudaExpressionValue length, zero, has_value, raw, wrapped;
        RudaMirBlockId none_block, some_block, join;
        RudaMirRegister target = RUDA_MIR_NO_REGISTER;
        RudaMirBlockId entry = function->block;
        if (option == NULL || !ruda_function_add_register(function, return_type, &target)
            || !ruda_array_mutation_emit_length(function, receiver, array,
                node->start, node->end, &length)
            || !ruda_array_mutation_emit_constant(function, 0, node->start, node->end, &zero)
            || !ruda_function_emit_instruction(function, RUDA_MIR_LESS_I64,
                RUDA_TYPE_BOOL, zero.register_id, length.register_id, 0, false,
                node->start, node->end, &has_value)
            || !ruda_array_mutation_blocks(function, &none_block, &some_block, &join)
            || !ruda_checker_private_function_set_branch(function, entry,
                has_value.register_id, some_block, none_block, node->start, node->end)) return false;
        function->block = none_block;
        function->terminated = false;
        if (!ruda_array_mutation_emit_enum(function, option, return_type, 1,
                NULL, 0, node->start, node->end, &wrapped)
            || !ruda_checker_private_function_emit_copy(function, target,
                wrapped.register_id, node->start, node->end)
            || !ruda_checker_private_function_set_goto(function, function->block,
                join, node->end, node->end)) return false;
        function->block = some_block;
        function->terminated = false;
        if (!ruda_array_mutation_emit_raw(function, RUDA_MIR_ARRAY_POP, element,
                receiver.register_id, RUDA_MIR_NO_REGISTER, array,
                node->start, node->end, &raw)
            || !ruda_array_mutation_emit_enum(function, option, return_type, 0,
                &(RudaMirEnumArgument){.register_id = raw.register_id, .type = raw.type},
                1, node->start, node->end, &wrapped)
            || !ruda_checker_private_function_emit_copy(function, target,
                wrapped.register_id, node->start, node->end)
            || !ruda_checker_private_function_set_goto(function, function->block,
                join, node->end, node->end)) return false;
        function->block = join;
        function->terminated = false;
        result->type = return_type;
        result->register_id = target;
        result->valid = true;
        return true;
    }
    {
        RudaExpressionValue length, zero, invalid, raw;
        RudaMirBlockId error_block, success_block, join;
        RudaMirRegister target = RUDA_MIR_NO_REGISTER;
        RudaMirBlockId entry = function->block;
        RudaCheckEnumDeclaration *error = ruda_array_mutation_prelude_enum(
            function, operation == RUDA_ARRAY_MUTATION_TRUNCATE
                ? "ArrayTruncateError" : "ArrayGrowthError"
        );
        if (error == NULL || !ruda_function_add_register(function, return_type, &target)
            || !ruda_array_mutation_emit_length(function, receiver, array,
                node->start, node->end, &length)
            || !ruda_array_mutation_emit_constant(function, 0, node->start, node->end, &zero)
            || !ruda_function_emit_instruction(function, RUDA_MIR_LESS_I64,
                RUDA_TYPE_BOOL, value.register_id, zero.register_id, 0, false,
                node->start, node->end, &invalid)
            || !ruda_array_mutation_blocks(function, &error_block, &success_block, &join)
            || !ruda_checker_private_function_set_branch(function, entry,
                invalid.register_id, error_block, success_block, node->start, node->end)) return false;
        function->block = error_block;
        function->terminated = false;
        if (operation == RUDA_ARRAY_MUTATION_TRUNCATE) {
            RudaMirEnumArgument fields[2] = {
                {.register_id = value.register_id, .type = value.type},
                {.register_id = length.register_id, .type = length.type}
            };
            if (!ruda_array_mutation_emit_error_result(function, "ArrayTruncateError",
                    0, fields, 2, return_type, target, node->start, node->end)) return false;
        } else if (!ruda_array_mutation_emit_error_result(function, "ArrayGrowthError",
                1, &(RudaMirEnumArgument){.register_id = value.register_id, .type = value.type},
                1, return_type, target, node->start, node->end)) return false;
        if (!ruda_checker_private_function_set_goto(function, function->block,
                join, node->end, node->end)) return false;
        function->block = success_block;
        function->terminated = false;
        if (operation == RUDA_ARRAY_MUTATION_TRUNCATE) {
            RudaExpressionValue expands;
            RudaMirBlockId raw_block = RUDA_MIR_NO_BLOCK;
            if (!ruda_function_emit_instruction(function, RUDA_MIR_LESS_I64,
                    RUDA_TYPE_BOOL, length.register_id, value.register_id, 0,
                    false, node->start, node->end, &expands)
                || !ruda_checker_private_function_add_block(function, &raw_block)
                || !ruda_checker_private_function_set_branch(function,
                    success_block, expands.register_id, error_block, raw_block,
                    node->start, node->end)) return false;
            function->block = raw_block;
            function->terminated = false;
        } else {
            RudaExpressionValue maximum, available, too_large;
            RudaMirBlockId limit_block = RUDA_MIR_NO_BLOCK;
            RudaMirBlockId raw_block = RUDA_MIR_NO_BLOCK;
            if (!ruda_array_mutation_emit_constant(function, INT64_MAX,
                    node->start, node->end, &maximum)
                || !ruda_function_emit_instruction(function,
                    RUDA_MIR_SUBTRACT_CHECKED_I64, RUDA_TYPE_I64,
                    maximum.register_id, length.register_id, 0, false,
                    node->start, node->end, &available)
                || !ruda_function_emit_instruction(function, RUDA_MIR_LESS_I64,
                    RUDA_TYPE_BOOL, available.register_id, value.register_id,
                    0, false, node->start, node->end, &too_large)
                || !ruda_checker_private_function_add_block(function, &limit_block)
                || !ruda_checker_private_function_add_block(function, &raw_block)
                || !ruda_checker_private_function_set_branch(function,
                    success_block, too_large.register_id, limit_block, raw_block,
                    node->start, node->end)) return false;
            function->block = limit_block;
            function->terminated = false;
            if (!ruda_array_mutation_emit_error_result(function, "ArrayGrowthError",
                    0, &(RudaMirEnumArgument){.register_id = maximum.register_id,
                        .type = maximum.type}, 1, return_type, target,
                    node->start, node->end)
                || !ruda_checker_private_function_set_goto(function, function->block,
                    join, node->end, node->end)) return false;
            function->block = raw_block;
            function->terminated = false;
        }
        if (!ruda_array_mutation_emit_raw(function,
                operation == RUDA_ARRAY_MUTATION_TRUNCATE ? RUDA_MIR_ARRAY_TRUNCATE : RUDA_MIR_ARRAY_RESERVE,
                receiver.type, receiver.register_id, value.register_id, array,
                node->start, node->end, &raw)
            || !ruda_array_mutation_emit_ok_result(function, return_type, target,
                node->start, node->end)
            || !ruda_checker_private_function_set_goto(function, function->block,
                join, node->end, node->end)) return false;
        function->block = join;
        function->terminated = false;
        result->type = return_type;
        result->register_id = target;
        result->valid = true;
        return true;
    }
}
