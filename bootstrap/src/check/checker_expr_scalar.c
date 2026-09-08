#include "checker_closure_api.h"
#include "checker_function_values_api.h"
#include "checker_expr_scalar_api.h"
#include "checker_equality_api.h"
#include "checker_core_api.h"
#include "checker_lookup_api.h"
#include "checker_expr_prelude_api.h"
#include "checker_expr_string_api.h"

#include <limits.h>
#include <string.h>





 bool mmay_checker_private_compile_literal(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
) {
    const MarimaySyntaxToken *token = mmay_checker_private_check_first_token(node);
    MarimayChecker *checker = function->checker;
    if (token == NULL || token->synthetic) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (token->kind == MMAY_TOKEN_KW_TRUE
        || token->kind == MMAY_TOKEN_KW_FALSE) {
        return mmay_function_emit_instruction(
            function,
            MMAY_MIR_CONSTANT_BOOL,
            MMAY_TYPE_BOOL,
            MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_REGISTER,
            0,
            token->kind == MMAY_TOKEN_KW_TRUE,
            token->start,
            token->end,
            result
        );
    }
    if (token->kind == MMAY_TOKEN_INTEGER) {
        uint64_t magnitude = 0;
        MarimayTypeId integer_type = expected == MMAY_TYPE_INVALID
            ? MMAY_TYPE_I64 : expected;
        uint64_t limit = UINT64_MAX;
        bool signed_integer = integer_type >= MMAY_TYPE_I8
            && integer_type <= MMAY_TYPE_I64;
        bool integer = integer_type >= MMAY_TYPE_I8
            && integer_type <= MMAY_TYPE_U64;
        if (expected == MMAY_TYPE_STRING) {
            (void)mmay_checker_add_diagnostic(
                checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "integer literal does not match the required string type",
                token->start,
                token->end
            );
            return false;
        }
        if (!integer) {
            (void)mmay_checker_add_diagnostic(
                checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "integer literal requires an integer type",
                token->start, token->end
            );
            return false;
        }
        if (signed_integer) {
            unsigned int width = integer_type == MMAY_TYPE_I8 ? 8U
                : integer_type == MMAY_TYPE_I16 ? 16U
                : integer_type == MMAY_TYPE_I32 ? 32U : 64U;
            limit = width == 64U ? (uint64_t)INT64_MAX
                : (UINT64_C(1) << (width - 1U)) - UINT64_C(1);
        } else {
            unsigned int width = integer_type == MMAY_TYPE_U8 ? 8U
                : integer_type == MMAY_TYPE_U16 ? 16U
                : integer_type == MMAY_TYPE_U32 ? 32U : 64U;
            limit = width == 64U ? UINT64_MAX
                : (UINT64_C(1) << width) - UINT64_C(1);
        }
        if (!mmay_checker_private_integer_magnitude(
                checker, token, limit,
                &magnitude
            )) {
            return false;
        }
        if (integer_type == MMAY_TYPE_I64) {
            return mmay_function_emit_instruction(
                function, MMAY_MIR_CONSTANT_I64, MMAY_TYPE_I64,
                MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER,
                (int64_t)magnitude, false, token->start, token->end, result
            );
        }
        {
            MarimayMirInstruction instruction;
            MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
            if (!mmay_function_add_register(function, integer_type, &target)) {
                return false;
            }
            memset(&instruction, 0, sizeof(instruction));
            instruction.kind = MMAY_MIR_CONSTANT_INTEGER;
            instruction.target = target;
            instruction.first = MMAY_MIR_NO_REGISTER;
            instruction.second = MMAY_MIR_NO_REGISTER;
            instruction.integer_bits = magnitude;
            instruction.span.start = token->start;
            instruction.span.end = token->end;
            if (!mmay_function_append_instruction(function, &instruction)) {
                return false;
            }
            result->type = integer_type;
            result->register_id = target;
            result->valid = true;
            return true;
        }
    }
    if (token->kind == MMAY_TOKEN_FLOAT) {
        uint64_t bits = 0;
        MarimayF64ParseStatus parse_status;
        if (expected != MMAY_TYPE_INVALID && expected != MMAY_TYPE_F64) {
            (void)mmay_checker_add_diagnostic(
                checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "f64 literal does not match the required type",
                token->start,
                token->end
            );
            return false;
        }
        parse_status = mmay_f64_parse_decimal(
            checker->source + token->start,
            token->end - token->start,
            checker->result->allocator,
            checker->options.max_float_literal_digits,
            &bits
        );
        if (parse_status == MMAY_F64_PARSE_ALLOCATION_FAILED) {
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
        if (parse_status == MMAY_F64_PARSE_LIMIT_REACHED) {
            mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
            return false;
        }
        if (parse_status != MMAY_F64_PARSE_OK) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        return mmay_checker_private_function_emit_f64_constant(
            function,
            bits,
            token->start,
            token->end,
            result
        );
    }
    if (token->kind == MMAY_TOKEN_STRING
        || token->kind == MMAY_TOKEN_RAW_STRING) {
        MarimayOwnedBytes decoded;
        MarimayStringLiteralStatus decode_status;
        if (expected != MMAY_TYPE_INVALID && expected != MMAY_TYPE_STRING) {
            (void)mmay_checker_add_diagnostic(
                checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "string literal does not match the required type",
                token->start,
                token->end
            );
            return false;
        }
        decode_status = mmay_string_literal_decode(
            checker->source + token->start,
            token->end - token->start,
            token->kind == MMAY_TOKEN_RAW_STRING,
            checker->result->allocator,
            &decoded
        );
        if (decode_status != MMAY_STRING_LITERAL_OK) {
            mmay_checker_fail(
                checker,
                decode_status == MMAY_STRING_LITERAL_ALLOCATION_FAILED
                    ? MMAY_CHECK_ALLOCATION_FAILED
                    : MMAY_CHECK_INTERNAL_ERROR
            );
            return false;
        }
        {
            bool emitted = mmay_checker_private_emit_string_constant(
                function, decoded.data, decoded.length,
                token->start, token->end, result
            );
            mmay_owned_bytes_dispose(&decoded);
            return emitted;
        }
    }
    mmay_checker_fail(checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
    return false;
}

 bool mmay_checker_private_compile_name(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
) {
    const MarimaySyntaxToken *token = mmay_checker_private_check_child_token(
        node,
        MMAY_TOKEN_IDENTIFIER,
        0
    );
    MarimayCheckName name;
    const MarimayCheckLocal *local = NULL;
    MarimayCheckConstantDeclaration *constant = NULL;
    if (token == NULL || token->synthetic) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    name.snapshot = function->checker->ast->snapshot;
    name.start = token->start;
    name.end = token->end;
    local = mmay_closure_find_local(function, name);
    if (local == NULL) {
        bool callable = false;
        if (!mmay_function_value_named(function, node, expected, &callable, result)) return false;
        if (callable) return true;
        constant = mmay_checker_private_checker_find_constant(function->checker, name);
        if (constant != NULL) {
            if (!mmay_checker_private_evaluate_constant(
                    function->checker,
                    constant,
                    token->start,
                    token->end
                )) {
                return false;
            }
            if (!mmay_check_record_occurrence(
                    function->checker,
                    name,
                    constant->symbol
                )) {
                return false;
            }
            if (constant->value.type == MMAY_TYPE_I64) {
                return mmay_function_emit_instruction(
                    function,
                    MMAY_MIR_CONSTANT_I64,
                    MMAY_TYPE_I64,
                    MMAY_MIR_NO_REGISTER,
                    MMAY_MIR_NO_REGISTER,
                    constant->value.as.i64,
                    false,
                    token->start,
                    token->end,
                    result
                );
            }
            if (mmay_checker_private_type_is_integer(constant->value.type)) {
                MarimayMirInstruction instruction;
                MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
                if (!mmay_function_add_register(
                        function, constant->value.type, &target
                    )) {
                    return false;
                }
                memset(&instruction, 0, sizeof(instruction));
                instruction.kind = MMAY_MIR_CONSTANT_INTEGER;
                instruction.target = target;
                instruction.first = MMAY_MIR_NO_REGISTER;
                instruction.second = MMAY_MIR_NO_REGISTER;
                instruction.integer_bits = constant->value.as.integer_bits;
                instruction.span.start = token->start;
                instruction.span.end = token->end;
                if (!mmay_function_append_instruction(function, &instruction)) {
                    return false;
                }
                result->type = constant->value.type;
                result->register_id = target;
                result->valid = true;
                return true;
            }
            if (constant->value.type == MMAY_TYPE_BOOL) {
                return mmay_function_emit_instruction(
                    function,
                    MMAY_MIR_CONSTANT_BOOL,
                    MMAY_TYPE_BOOL,
                    MMAY_MIR_NO_REGISTER,
                    MMAY_MIR_NO_REGISTER,
                    0,
                    constant->value.as.boolean,
                    token->start,
                    token->end,
                    result
                );
            }
            if (constant->value.type == MMAY_TYPE_F64) {
                return mmay_checker_private_function_emit_f64_constant(
                    function,
                    constant->value.as.f64_bits,
                    token->start,
                    token->end,
                    result
                );
            }
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        if (mmay_checker_private_checker_find_top_level(function->checker, name) != NULL) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        {
            bool prelude_resolved = false;
            if (!mmay_checker_private_compile_prelude_unit_value(
                    function,
                    node,
                    expected,
                    &prelude_resolved,
                    result
                )) {
                return false;
            }
            if (prelude_resolved) {
                return true;
            }
        }
        if (mmay_checker_private_check_text_equal(
                function->checker,
                token->start,
                token->end,
                "panic"
            )) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_CALL_ARITY,
                "panic requires exactly one message argument",
                token->start,
                token->end
            );
            return false;
        }
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_UNKNOWN_NAME,
            "unknown name",
            token->start,
            token->end
        );
        return false;
    }
    if (!local->initialized) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_UNINITIALIZED_LOCAL,
            "local is used before it is initialized",
            token->start,
            token->end
        );
        return false;
    }
    return mmay_closure_read_local(function, local, token->start, token->end, result);
}

 const MarimayAstNode *mmay_checker_private_expression_only_node(
    const MarimayAstNode *node
) {
    const MarimayAstNode *result = NULL;
    size_t index = 0;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE) {
            if (result != NULL) {
                return NULL;
            }
            result = element->as.node;
        }
    }
    return result;
}

 bool mmay_checker_private_compile_unary(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
) {
    const MarimaySyntaxToken *operator_token = mmay_checker_private_check_first_token(node);
    const MarimayAstNode *operand = mmay_checker_private_expression_only_node(node);
    MarimayExpressionValue value;
    MarimayMirInstructionKind kind;
    MarimayTypeId operand_type = MMAY_TYPE_INVALID;
    memset(&value, 0, sizeof(value));
    value.register_id = MMAY_MIR_NO_REGISTER;
    if (operator_token == NULL || operand == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (operator_token->kind == MMAY_TOKEN_MINUS
        && operand->kind == MMAY_SYNTAX_LITERAL_EXPRESSION) {
        const MarimaySyntaxToken *literal = mmay_checker_private_check_first_token(operand);
        uint64_t magnitude = 0;
        MarimayTypeId negative_type = expected;
        unsigned int negative_width = 64U;
        uint64_t negative_limit = (uint64_t)INT64_MAX + UINT64_C(1);
        if (!(negative_type >= MMAY_TYPE_I8
                && negative_type <= MMAY_TYPE_I64)) {
            negative_type = MMAY_TYPE_I64;
        }
        negative_width = negative_type == MMAY_TYPE_I8 ? 8U
            : negative_type == MMAY_TYPE_I16 ? 16U
            : negative_type == MMAY_TYPE_I32 ? 32U : 64U;
        if (negative_width != 64U) {
            negative_limit = UINT64_C(1) << (negative_width - 1U);
        }
        if (literal != NULL && literal->kind == MMAY_TOKEN_INTEGER
            && mmay_checker_private_integer_magnitude(
                function->checker,
                literal,
                negative_limit,
                &magnitude
            )) {
            if (magnitude == negative_limit) {
                if (negative_type != MMAY_TYPE_I64) {
                    MarimayMirInstruction instruction;
                    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
                    uint64_t mask = (UINT64_C(1) << negative_width) - 1U;
                    if (!mmay_function_add_register(
                            function, negative_type, &target
                        )) return false;
                    memset(&instruction, 0, sizeof(instruction));
                    instruction.kind = MMAY_MIR_CONSTANT_INTEGER;
                    instruction.target = target;
                    instruction.first = MMAY_MIR_NO_REGISTER;
                    instruction.second = MMAY_MIR_NO_REGISTER;
                    instruction.integer_bits = (UINT64_C(0) - magnitude) & mask;
                    instruction.span.start = node->start;
                    instruction.span.end = node->end;
                    if (!mmay_function_append_instruction(function, &instruction)) {
                        return false;
                    }
                    result->type = negative_type;
                    result->register_id = target;
                    result->valid = true;
                    return true;
                }
                return mmay_function_emit_instruction(
                    function,
                    MMAY_MIR_CONSTANT_I64,
                    MMAY_TYPE_I64,
                    MMAY_MIR_NO_REGISTER,
                    MMAY_MIR_NO_REGISTER,
                    INT64_MIN,
                    false,
                    node->start,
                    node->end,
                    result
                );
            }
        } else if (literal != NULL && literal->kind == MMAY_TOKEN_FLOAT) {
            uint64_t bits = 0;
            MarimayF64ParseStatus parse_status = mmay_f64_parse_decimal(
                function->checker->source + literal->start,
                literal->end - literal->start,
                function->checker->result->allocator,
                function->checker->options.max_float_literal_digits,
                &bits
            );
            if (parse_status == MMAY_F64_PARSE_OK) {
                return mmay_checker_private_function_emit_f64_constant(
                    function,
                    mmay_f64_negate(bits),
                    node->start,
                    node->end,
                    result
                );
            }
            if (parse_status == MMAY_F64_PARSE_ALLOCATION_FAILED) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_ALLOCATION_FAILED
                );
            } else if (parse_status == MMAY_F64_PARSE_LIMIT_REACHED) {
                mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
            } else {
                mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            }
            return false;
        } else if (mmay_checker_has_failed(function->checker)
            || function->checker->result->error_count
                != function->diagnostic_start) {
            return false;
        }
    }
    if (operator_token->kind == MMAY_TOKEN_BANG) {
        operand_type = MMAY_TYPE_BOOL;
        kind = MMAY_MIR_NOT_BOOL;
    } else if (operator_token->kind == MMAY_TOKEN_MINUS) {
        operand_type = expected;
        if (!(operand_type >= MMAY_TYPE_I8 && operand_type <= MMAY_TYPE_I64)
            && operand_type != MMAY_TYPE_F64) {
            operand_type = MMAY_TYPE_INVALID;
        }
        kind = MMAY_MIR_NEGATE_CHECKED_I64;
    } else if (operator_token->kind == MMAY_TOKEN_TILDE) {
        operand_type = (expected >= MMAY_TYPE_I8 && expected <= MMAY_TYPE_U64)
            ? expected : MMAY_TYPE_INVALID;
        kind = MMAY_MIR_BITWISE_NOT_I64;
    } else {
        mmay_checker_fail(function->checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (!mmay_compile_expression(function, operand, operand_type, &value)) {
        return false;
    }
    if (mmay_expression_is_never(function, &value)) {
        *result = value;
        return true;
    }
    if (operator_token->kind == MMAY_TOKEN_MINUS
        && ((value.type >= MMAY_TYPE_I8 && value.type <= MMAY_TYPE_I64)
            || value.type == MMAY_TYPE_F64)) {
        operand_type = value.type;
        kind = value.type == MMAY_TYPE_F64
            ? MMAY_MIR_NEGATE_F64
            : MMAY_MIR_NEGATE_CHECKED_I64;
    }
    if (value.type != operand_type) {
        if ((operator_token->kind == MMAY_TOKEN_MINUS
                && (value.type == MMAY_TYPE_I8
                    || value.type == MMAY_TYPE_I16
                    || value.type == MMAY_TYPE_I32
                    || value.type == MMAY_TYPE_F64))
            || (operator_token->kind == MMAY_TOKEN_TILDE
                && mmay_checker_private_type_is_integer(value.type))) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_INVALID_OPERATOR,
            "operator is not defined for the operand type",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    return mmay_function_emit_instruction(
        function,
        kind,
        operand_type,
        value.register_id,
        MMAY_MIR_NO_REGISTER,
        0,
        false,
        node->start,
        node->end,
        result
    );
}

 bool mmay_checker_private_binary_parts(
    const MarimayAstNode *node,
    const MarimayAstNode **left,
    const MarimaySyntaxToken **operator_token,
    const MarimayAstNode **right
) {
    size_t index = 0;
    *left = NULL;
    *operator_token = NULL;
    *right = NULL;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element == NULL) {
            return false;
        }
        if (element->kind == MMAY_AST_ELEMENT_NODE) {
            if (*left == NULL) {
                *left = element->as.node;
            } else if (*right == NULL) {
                *right = element->as.node;
            } else {
                return false;
            }
        } else if (element->kind == MMAY_AST_ELEMENT_TOKEN) {
            if (*operator_token != NULL) {
                return false;
            }
            *operator_token = &element->as.token;
        }
    }
    return *left != NULL && *operator_token != NULL && *right != NULL;
}

 bool mmay_checker_private_expression_is_integer_literal(const MarimayAstNode *node) {
    const MarimayAstNode *inner = NULL;
    const MarimaySyntaxToken *token = NULL;
    if (node == NULL) {
        return false;
    }
    if (node->kind == MMAY_SYNTAX_EXPRESSION
        || node->kind == MMAY_SYNTAX_PAREN_GROUP) {
        inner = mmay_checker_private_expression_only_node(node);
        return mmay_checker_private_expression_is_integer_literal(inner);
    }
    if (node->kind != MMAY_SYNTAX_LITERAL_EXPRESSION) {
        return false;
    }
    token = mmay_checker_private_check_first_token(node);
    return token != NULL && token->kind == MMAY_TOKEN_INTEGER;
}

 bool mmay_checker_private_compile_short_circuit(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayAstNode *left_node,
    const MarimaySyntaxToken *operator_token,
    const MarimayAstNode *right_node,
    MarimayExpressionValue *result
) {
    MarimayExpressionValue left;
    MarimayExpressionValue right;
    MarimayExpressionValue shortcut;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId branch_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId right_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId shortcut_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId join_block = MMAY_MIR_NO_BLOCK;
    bool is_and = operator_token->kind == MMAY_TOKEN_AMPERSAND_AMPERSAND;
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    memset(&shortcut, 0, sizeof(shortcut));
    if (!mmay_compile_expression(
            function,
            left_node,
            MMAY_TYPE_INVALID,
            &left
        )) {
        return false;
    }
    if (mmay_expression_is_never(function, &left)) {
        *result = left;
        return true;
    }
    if (left.type != MMAY_TYPE_BOOL) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_INVALID_OPERATOR,
            "logical operator requires bool operands",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    branch_block = function->block;
    if (!mmay_function_add_register(function, MMAY_TYPE_BOOL, &target)
        || !mmay_checker_private_function_add_block(function, &right_block)
        || !mmay_checker_private_function_add_block(function, &shortcut_block)
        || !mmay_checker_private_function_add_block(function, &join_block)
        || !mmay_checker_private_function_set_branch(
            function,
            branch_block,
            left.register_id,
            is_and ? right_block : shortcut_block,
            is_and ? shortcut_block : right_block,
            operator_token->start,
            operator_token->end
        )) {
        return false;
    }

    function->block = shortcut_block;
    if (!mmay_function_emit_instruction(
            function,
            MMAY_MIR_CONSTANT_BOOL,
            MMAY_TYPE_BOOL,
            MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_REGISTER,
            0,
            !is_and,
            operator_token->start,
            operator_token->end,
            &shortcut
        ) || !mmay_checker_private_function_emit_copy(
            function,
            target,
            shortcut.register_id,
            operator_token->start,
            operator_token->end
        ) || !mmay_checker_private_function_set_goto(
            function,
            shortcut_block,
            join_block,
            operator_token->start,
            operator_token->end
        )) {
        return false;
    }

    function->block = right_block;
    if (!mmay_compile_expression(
            function,
            right_node,
            MMAY_TYPE_INVALID,
            &right
        )) {
        return false;
    }
    if (!mmay_expression_is_never(function, &right)
        && right.type != MMAY_TYPE_BOOL) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_INVALID_OPERATOR,
            "logical operator requires bool operands",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    if (!mmay_expression_is_never(function, &right)
        && (!mmay_checker_private_function_emit_copy(
            function,
            target,
            right.register_id,
            right_node->start,
            right_node->end
        ) || !mmay_checker_private_function_set_goto(
            function,
            function->block,
            join_block,
            right_node->start,
            right_node->end
        ))) {
        return false;
    }
    function->block = join_block;
    function->terminated = false;
    result->type = MMAY_TYPE_BOOL;
    result->register_id = target;
    result->valid = true;
    (void)node;
    return true;
}

 bool mmay_checker_private_compile_binary(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *left_node = NULL;
    const MarimayAstNode *right_node = NULL;
    const MarimaySyntaxToken *operator_token = NULL;
    MarimayExpressionValue left;
    MarimayExpressionValue right;
    MarimayExpressionValue equality;
    MarimayMirInstructionKind kind = MMAY_MIR_ADD_CHECKED_I64;
    MarimayTypeId operand_type = MMAY_TYPE_I64;
    MarimayTypeId result_type = MMAY_TYPE_I64;
    bool swap = false;
    bool negate = false;
    bool equality_operator = false;
    bool numeric_operator = false;
    bool comparison_operator = false;
    bool bitwise_operator = false;
    bool shift_operator = false;
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    memset(&equality, 0, sizeof(equality));
    if (!mmay_checker_private_binary_parts(node, &left_node, &operator_token, &right_node)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (operator_token->kind == MMAY_TOKEN_AMPERSAND_AMPERSAND
        || operator_token->kind == MMAY_TOKEN_PIPE_PIPE) {
        return mmay_checker_private_compile_short_circuit(
            function,
            node,
            left_node,
            operator_token,
            right_node,
            result
        );
    }
    switch (operator_token->kind) {
        case MMAY_TOKEN_PLUS:
            kind = MMAY_MIR_ADD_CHECKED_I64;
            numeric_operator = true;
            break;
        case MMAY_TOKEN_MINUS:
            kind = MMAY_MIR_SUBTRACT_CHECKED_I64;
            numeric_operator = true;
            break;
        case MMAY_TOKEN_STAR:
            kind = MMAY_MIR_MULTIPLY_CHECKED_I64;
            numeric_operator = true;
            break;
        case MMAY_TOKEN_SLASH:
            kind = MMAY_MIR_DIVIDE_CHECKED_I64;
            numeric_operator = true;
            break;
        case MMAY_TOKEN_PERCENT:
            kind = MMAY_MIR_REMAINDER_CHECKED_I64;
            numeric_operator = true;
            break;
        case MMAY_TOKEN_AMPERSAND:
            kind = MMAY_MIR_BITWISE_AND_I64;
            bitwise_operator = true;
            break;
        case MMAY_TOKEN_CARET:
            kind = MMAY_MIR_BITWISE_XOR_I64;
            bitwise_operator = true;
            break;
        case MMAY_TOKEN_PIPE:
            kind = MMAY_MIR_BITWISE_OR_I64;
            bitwise_operator = true;
            break;
        case MMAY_TOKEN_SHIFT_LEFT:
            kind = MMAY_MIR_SHIFT_LEFT_CHECKED_I64;
            shift_operator = true;
            break;
        case MMAY_TOKEN_SHIFT_RIGHT:
            kind = MMAY_MIR_SHIFT_RIGHT_I64;
            shift_operator = true;
            break;
        case MMAY_TOKEN_LESS:
            kind = MMAY_MIR_LESS_I64;
            result_type = MMAY_TYPE_BOOL;
            comparison_operator = true;
            break;
        case MMAY_TOKEN_LESS_EQUAL:
            kind = MMAY_MIR_LESS_EQUAL_I64;
            result_type = MMAY_TYPE_BOOL;
            comparison_operator = true;
            break;
        case MMAY_TOKEN_GREATER:
            kind = MMAY_MIR_LESS_I64;
            result_type = MMAY_TYPE_BOOL;
            swap = true;
            comparison_operator = true;
            break;
        case MMAY_TOKEN_GREATER_EQUAL:
            kind = MMAY_MIR_LESS_EQUAL_I64;
            result_type = MMAY_TYPE_BOOL;
            swap = true;
            comparison_operator = true;
            break;
        case MMAY_TOKEN_EQUAL_EQUAL:
            equality_operator = true;
            result_type = MMAY_TYPE_BOOL;
            break;
        case MMAY_TOKEN_BANG_EQUAL:
            equality_operator = true;
            result_type = MMAY_TYPE_BOOL;
            negate = true;
            break;
        default:
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
    }
    if (!mmay_compile_expression(
            function, left_node,
            (expected >= MMAY_TYPE_I8 && expected <= MMAY_TYPE_U64)
                || expected == MMAY_TYPE_F64 ? expected : MMAY_TYPE_INVALID,
            &left
        )) {
        return false;
    }
    if (mmay_expression_is_never(function, &left)) {
        *result = left;
        return true;
    }
    operand_type = left.type;
    if (!mmay_compile_expression(
            function,
            right_node,
            operand_type,
            &right
        )) {
        return false;
    }
    if (mmay_expression_is_never(function, &right)) {
        *result = right;
        return true;
    }
    if (left.type != right.type) {
        if ((mmay_checker_private_expression_is_integer_literal(left_node)
                && mmay_checker_private_type_is_integer(right.type))
            || (mmay_checker_private_expression_is_integer_literal(right_node)
                && mmay_checker_private_type_is_integer(left.type))) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_INVALID_OPERATOR,
            "operator is not defined for the operand types",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    if (!(operand_type >= MMAY_TYPE_I8 && operand_type <= MMAY_TYPE_U64)
        && operand_type != MMAY_TYPE_F64
        && !(equality_operator && operand_type == MMAY_TYPE_BOOL)
        && !(equality_operator && operand_type == MMAY_TYPE_STRING)
        && !(equality_operator && operand_type == MMAY_TYPE_RUNE)
        && !(equality_operator && operand_type >= MMAY_TYPE_FIRST_DYNAMIC)) {
        if ((numeric_operator || comparison_operator)
            && mmay_checker_private_type_is_numeric(operand_type)) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        if (bitwise_operator && mmay_checker_private_type_is_integer(operand_type)) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        if (shift_operator && mmay_checker_private_type_is_integer(operand_type)) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        if (equality_operator) return mmay_checker_private_compile_equality(
            function, node, left, right, negate, result
        );
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_INVALID_OPERATOR,
            "operator is not defined for the operand types",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    if ((numeric_operator || bitwise_operator || shift_operator)
        && mmay_checker_private_type_is_integer(operand_type)) {
        result_type = operand_type;
    }
    if (operand_type == MMAY_TYPE_F64) {
        if (bitwise_operator || shift_operator
            || operator_token->kind == MMAY_TOKEN_PERCENT) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_INVALID_OPERATOR,
                "operator is not defined for f64 operands",
                operator_token->start,
                operator_token->end
            );
            return false;
        }
        if (numeric_operator) {
            result_type = MMAY_TYPE_F64;
        }
        if (equality_operator) {
            kind = MMAY_MIR_EQUAL_F64;
        } else if (comparison_operator) {
            kind = operator_token->kind == MMAY_TOKEN_LESS
                    || operator_token->kind == MMAY_TOKEN_GREATER
                ? MMAY_MIR_LESS_F64
                : MMAY_MIR_LESS_EQUAL_F64;
        } else if (operator_token->kind == MMAY_TOKEN_PLUS) {
            kind = MMAY_MIR_ADD_F64;
        } else if (operator_token->kind == MMAY_TOKEN_MINUS) {
            kind = MMAY_MIR_SUBTRACT_F64;
        } else if (operator_token->kind == MMAY_TOKEN_STAR) {
            kind = MMAY_MIR_MULTIPLY_F64;
        } else if (operator_token->kind == MMAY_TOKEN_SLASH) {
            kind = MMAY_MIR_DIVIDE_F64;
        }
    }
    if (equality_operator) {
        if (operand_type >= MMAY_TYPE_FIRST_DYNAMIC) {
            return mmay_checker_private_compile_equality(
                function, node, left, right, negate, result
            );
        }
        if (operand_type == MMAY_TYPE_BOOL) {
            kind = MMAY_MIR_EQUAL_BOOL;
        } else if (operand_type == MMAY_TYPE_STRING) {
            kind = MMAY_MIR_EQUAL_STRING;
        } else if ((operand_type >= MMAY_TYPE_I8
            && operand_type <= MMAY_TYPE_U64)
            || operand_type == MMAY_TYPE_RUNE) {
            kind = MMAY_MIR_EQUAL_I64;
        }
    }
    if (!mmay_function_emit_instruction(
            function,
            kind,
            result_type,
            swap ? right.register_id : left.register_id,
            swap ? left.register_id : right.register_id,
            0,
            false,
            node->start,
            node->end,
            negate ? &equality : result
        )) {
        return false;
    }
    if (!negate) {
        return true;
    }
    return mmay_function_emit_instruction(
        function,
        MMAY_MIR_NOT_BOOL,
        MMAY_TYPE_BOOL,
        equality.register_id,
        MMAY_MIR_NO_REGISTER,
        0,
        false,
        node->start,
        node->end,
        result
    );
}
