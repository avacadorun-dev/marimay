#include "checker_expr_scalar_api.h"
#include "checker_core_api.h"
#include "checker_lookup_api.h"

#include <limits.h>
#include <string.h>





 bool ruda_checker_private_compile_literal(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
) {
    const RudaSyntaxToken *token = ruda_checker_private_check_first_token(node);
    RudaChecker *checker = function->checker;
    if (token == NULL || token->synthetic) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (token->kind == RUDA_TOKEN_KW_TRUE
        || token->kind == RUDA_TOKEN_KW_FALSE) {
        return ruda_function_emit_instruction(
            function,
            RUDA_MIR_CONSTANT_BOOL,
            RUDA_TYPE_BOOL,
            RUDA_MIR_NO_REGISTER,
            RUDA_MIR_NO_REGISTER,
            0,
            token->kind == RUDA_TOKEN_KW_TRUE,
            token->start,
            token->end,
            result
        );
    }
    if (token->kind == RUDA_TOKEN_INTEGER) {
        uint64_t magnitude = 0;
        RudaTypeId integer_type = expected == RUDA_TYPE_INVALID
            ? RUDA_TYPE_I64 : expected;
        uint64_t limit = UINT64_MAX;
        bool signed_integer = integer_type >= RUDA_TYPE_I8
            && integer_type <= RUDA_TYPE_I64;
        bool integer = integer_type >= RUDA_TYPE_I8
            && integer_type <= RUDA_TYPE_U64;
        if (expected == RUDA_TYPE_STRING) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "integer literal does not match the required string type",
                token->start,
                token->end
            );
            return false;
        }
        if (!integer) {
            (void)ruda_checker_add_diagnostic(
                checker, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "integer literal requires an integer type",
                token->start, token->end
            );
            return false;
        }
        if (signed_integer) {
            unsigned int width = integer_type == RUDA_TYPE_I8 ? 8U
                : integer_type == RUDA_TYPE_I16 ? 16U
                : integer_type == RUDA_TYPE_I32 ? 32U : 64U;
            limit = width == 64U ? (uint64_t)INT64_MAX
                : (UINT64_C(1) << (width - 1U)) - UINT64_C(1);
        } else {
            unsigned int width = integer_type == RUDA_TYPE_U8 ? 8U
                : integer_type == RUDA_TYPE_U16 ? 16U
                : integer_type == RUDA_TYPE_U32 ? 32U : 64U;
            limit = width == 64U ? UINT64_MAX
                : (UINT64_C(1) << width) - UINT64_C(1);
        }
        if (!ruda_checker_private_integer_magnitude(
                checker, token, limit,
                &magnitude
            )) {
            return false;
        }
        if (integer_type == RUDA_TYPE_I64) {
            return ruda_function_emit_instruction(
                function, RUDA_MIR_CONSTANT_I64, RUDA_TYPE_I64,
                RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER,
                (int64_t)magnitude, false, token->start, token->end, result
            );
        }
        {
            RudaMirInstruction instruction;
            RudaMirRegister target = RUDA_MIR_NO_REGISTER;
            if (!ruda_function_add_register(function, integer_type, &target)) {
                return false;
            }
            memset(&instruction, 0, sizeof(instruction));
            instruction.kind = RUDA_MIR_CONSTANT_INTEGER;
            instruction.target = target;
            instruction.first = RUDA_MIR_NO_REGISTER;
            instruction.second = RUDA_MIR_NO_REGISTER;
            instruction.integer_bits = magnitude;
            instruction.span.start = token->start;
            instruction.span.end = token->end;
            if (!ruda_function_append_instruction(function, &instruction)) {
                return false;
            }
            result->type = integer_type;
            result->register_id = target;
            result->valid = true;
            return true;
        }
    }
    if (token->kind == RUDA_TOKEN_FLOAT) {
        uint64_t bits = 0;
        RudaF64ParseStatus parse_status;
        if (expected != RUDA_TYPE_INVALID && expected != RUDA_TYPE_F64) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "f64 literal does not match the required type",
                token->start,
                token->end
            );
            return false;
        }
        parse_status = ruda_f64_parse_decimal(
            checker->source + token->start,
            token->end - token->start,
            checker->result->allocator,
            checker->options.max_float_literal_digits,
            &bits
        );
        if (parse_status == RUDA_F64_PARSE_ALLOCATION_FAILED) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        if (parse_status == RUDA_F64_PARSE_LIMIT_REACHED) {
            ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
            return false;
        }
        if (parse_status != RUDA_F64_PARSE_OK) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        return ruda_checker_private_function_emit_f64_constant(
            function,
            bits,
            token->start,
            token->end,
            result
        );
    }
    if (token->kind == RUDA_TOKEN_STRING
        || token->kind == RUDA_TOKEN_RAW_STRING) {
        RudaOwnedBytes decoded;
        RudaStringLiteralStatus decode_status;
        RudaMirStringId string_constant = RUDA_MIR_NO_STRING;
        RudaMirRegister target = RUDA_MIR_NO_REGISTER;
        RudaMirInstruction instruction;
        size_t new_string_bytes = 0;
        if (expected != RUDA_TYPE_INVALID && expected != RUDA_TYPE_STRING) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "string literal does not match the required type",
                token->start,
                token->end
            );
            return false;
        }
        decode_status = ruda_string_literal_decode(
            checker->source + token->start,
            token->end - token->start,
            token->kind == RUDA_TOKEN_RAW_STRING,
            checker->result->allocator,
            &decoded
        );
        if (decode_status != RUDA_STRING_LITERAL_OK) {
            ruda_checker_fail(
                checker,
                decode_status == RUDA_STRING_LITERAL_ALLOCATION_FAILED
                    ? RUDA_CHECK_ALLOCATION_FAILED
                    : RUDA_CHECK_INTERNAL_ERROR
            );
            return false;
        }
        if (function->mir.string_constants.length
                >= function->mir.options.max_string_constants
            || !ruda_size_add(
                function->mir.string_bytes,
                decoded.length,
                &new_string_bytes
            ) || new_string_bytes > function->mir.options.max_string_bytes) {
            ruda_owned_bytes_dispose(&decoded);
            ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
            return false;
        }
        if (!ruda_function_add_register(function, RUDA_TYPE_STRING, &target)
            || !ruda_mir_add_string_constant(
                &function->mir,
                decoded.data,
                decoded.length,
                &string_constant
            )) {
            ruda_owned_bytes_dispose(&decoded);
            if (!ruda_checker_has_failed(checker)) {
                ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            }
            return false;
        }
        ruda_owned_bytes_dispose(&decoded);
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_CONSTANT_STRING;
        instruction.target = target;
        instruction.first = RUDA_MIR_NO_REGISTER;
        instruction.second = RUDA_MIR_NO_REGISTER;
        instruction.string_constant = string_constant;
        instruction.span.start = token->start;
        instruction.span.end = token->end;
        if (!ruda_function_append_instruction(function, &instruction)) {
            return false;
        }
        result->type = RUDA_TYPE_STRING;
        result->register_id = target;
        result->valid = true;
        return true;
    }
    ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
    return false;
}

 bool ruda_checker_private_compile_name(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *result
) {
    const RudaSyntaxToken *token = ruda_checker_private_check_child_token(
        node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    RudaCheckName name;
    const RudaCheckLocal *local = NULL;
    RudaCheckConstantDeclaration *constant = NULL;
    if (token == NULL || token->synthetic) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    name.snapshot = function->checker->ast->snapshot;
    name.start = token->start;
    name.end = token->end;
    local = ruda_checker_private_function_find_local(function, name);
    if (local == NULL) {
        constant = ruda_checker_private_checker_find_constant(function->checker, name);
        if (constant != NULL) {
            if (!ruda_checker_private_evaluate_constant(
                    function->checker,
                    constant,
                    token->start,
                    token->end
                )) {
                return false;
            }
            if (!ruda_check_record_occurrence(
                    function->checker,
                    name,
                    constant->symbol
                )) {
                return false;
            }
            if (constant->value.type == RUDA_TYPE_I64) {
                return ruda_function_emit_instruction(
                    function,
                    RUDA_MIR_CONSTANT_I64,
                    RUDA_TYPE_I64,
                    RUDA_MIR_NO_REGISTER,
                    RUDA_MIR_NO_REGISTER,
                    constant->value.as.i64,
                    false,
                    token->start,
                    token->end,
                    result
                );
            }
            if (ruda_checker_private_type_is_integer(constant->value.type)) {
                RudaMirInstruction instruction;
                RudaMirRegister target = RUDA_MIR_NO_REGISTER;
                if (!ruda_function_add_register(
                        function, constant->value.type, &target
                    )) {
                    return false;
                }
                memset(&instruction, 0, sizeof(instruction));
                instruction.kind = RUDA_MIR_CONSTANT_INTEGER;
                instruction.target = target;
                instruction.first = RUDA_MIR_NO_REGISTER;
                instruction.second = RUDA_MIR_NO_REGISTER;
                instruction.integer_bits = constant->value.as.integer_bits;
                instruction.span.start = token->start;
                instruction.span.end = token->end;
                if (!ruda_function_append_instruction(function, &instruction)) {
                    return false;
                }
                result->type = constant->value.type;
                result->register_id = target;
                result->valid = true;
                return true;
            }
            if (constant->value.type == RUDA_TYPE_BOOL) {
                return ruda_function_emit_instruction(
                    function,
                    RUDA_MIR_CONSTANT_BOOL,
                    RUDA_TYPE_BOOL,
                    RUDA_MIR_NO_REGISTER,
                    RUDA_MIR_NO_REGISTER,
                    0,
                    constant->value.as.boolean,
                    token->start,
                    token->end,
                    result
                );
            }
            if (constant->value.type == RUDA_TYPE_F64) {
                return ruda_checker_private_function_emit_f64_constant(
                    function,
                    constant->value.as.f64_bits,
                    token->start,
                    token->end,
                    result
                );
            }
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        if (ruda_checker_private_checker_find_top_level(function->checker, name) != NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        if (ruda_checker_private_check_text_equal(
                function->checker,
                token->start,
                token->end,
                "panic"
            )) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_CALL_ARITY,
                "panic requires exactly one message argument",
                token->start,
                token->end
            );
            return false;
        }
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_UNKNOWN_NAME,
            "unknown name",
            token->start,
            token->end
        );
        return false;
    }
    if (!local->initialized) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_UNINITIALIZED_LOCAL,
            "local is used before it is initialized",
            token->start,
            token->end
        );
        return false;
    }
    result->type = local->type;
    result->register_id = local->register_id;
    result->valid = true;
    return true;
}

 const RudaAstNode *ruda_checker_private_expression_only_node(
    const RudaAstNode *node
) {
    const RudaAstNode *result = NULL;
    size_t index = 0;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE) {
            if (result != NULL) {
                return NULL;
            }
            result = element->as.node;
        }
    }
    return result;
}

 bool ruda_checker_private_compile_unary(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
) {
    const RudaSyntaxToken *operator_token = ruda_checker_private_check_first_token(node);
    const RudaAstNode *operand = ruda_checker_private_expression_only_node(node);
    RudaExpressionValue value;
    RudaMirInstructionKind kind;
    RudaTypeId operand_type = RUDA_TYPE_INVALID;
    memset(&value, 0, sizeof(value));
    value.register_id = RUDA_MIR_NO_REGISTER;
    if (operator_token == NULL || operand == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (operator_token->kind == RUDA_TOKEN_MINUS
        && operand->kind == RUDA_SYNTAX_LITERAL_EXPRESSION) {
        const RudaSyntaxToken *literal = ruda_checker_private_check_first_token(operand);
        uint64_t magnitude = 0;
        RudaTypeId negative_type = expected;
        unsigned int negative_width = 64U;
        uint64_t negative_limit = (uint64_t)INT64_MAX + UINT64_C(1);
        if (!(negative_type >= RUDA_TYPE_I8
                && negative_type <= RUDA_TYPE_I64)) {
            negative_type = RUDA_TYPE_I64;
        }
        negative_width = negative_type == RUDA_TYPE_I8 ? 8U
            : negative_type == RUDA_TYPE_I16 ? 16U
            : negative_type == RUDA_TYPE_I32 ? 32U : 64U;
        if (negative_width != 64U) {
            negative_limit = UINT64_C(1) << (negative_width - 1U);
        }
        if (literal != NULL && literal->kind == RUDA_TOKEN_INTEGER
            && ruda_checker_private_integer_magnitude(
                function->checker,
                literal,
                negative_limit,
                &magnitude
            )) {
            if (magnitude == negative_limit) {
                if (negative_type != RUDA_TYPE_I64) {
                    RudaMirInstruction instruction;
                    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
                    uint64_t mask = (UINT64_C(1) << negative_width) - 1U;
                    if (!ruda_function_add_register(
                            function, negative_type, &target
                        )) return false;
                    memset(&instruction, 0, sizeof(instruction));
                    instruction.kind = RUDA_MIR_CONSTANT_INTEGER;
                    instruction.target = target;
                    instruction.first = RUDA_MIR_NO_REGISTER;
                    instruction.second = RUDA_MIR_NO_REGISTER;
                    instruction.integer_bits = (UINT64_C(0) - magnitude) & mask;
                    instruction.span.start = node->start;
                    instruction.span.end = node->end;
                    if (!ruda_function_append_instruction(function, &instruction)) {
                        return false;
                    }
                    result->type = negative_type;
                    result->register_id = target;
                    result->valid = true;
                    return true;
                }
                return ruda_function_emit_instruction(
                    function,
                    RUDA_MIR_CONSTANT_I64,
                    RUDA_TYPE_I64,
                    RUDA_MIR_NO_REGISTER,
                    RUDA_MIR_NO_REGISTER,
                    INT64_MIN,
                    false,
                    node->start,
                    node->end,
                    result
                );
            }
        } else if (literal != NULL && literal->kind == RUDA_TOKEN_FLOAT) {
            uint64_t bits = 0;
            RudaF64ParseStatus parse_status = ruda_f64_parse_decimal(
                function->checker->source + literal->start,
                literal->end - literal->start,
                function->checker->result->allocator,
                function->checker->options.max_float_literal_digits,
                &bits
            );
            if (parse_status == RUDA_F64_PARSE_OK) {
                return ruda_checker_private_function_emit_f64_constant(
                    function,
                    ruda_f64_negate(bits),
                    node->start,
                    node->end,
                    result
                );
            }
            if (parse_status == RUDA_F64_PARSE_ALLOCATION_FAILED) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_ALLOCATION_FAILED
                );
            } else if (parse_status == RUDA_F64_PARSE_LIMIT_REACHED) {
                ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            } else {
                ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            }
            return false;
        } else if (ruda_checker_has_failed(function->checker)
            || function->checker->result->diagnostics.length
                != function->diagnostic_start) {
            return false;
        }
    }
    if (operator_token->kind == RUDA_TOKEN_BANG) {
        operand_type = RUDA_TYPE_BOOL;
        kind = RUDA_MIR_NOT_BOOL;
    } else if (operator_token->kind == RUDA_TOKEN_MINUS) {
        operand_type = expected;
        if (!(operand_type >= RUDA_TYPE_I8 && operand_type <= RUDA_TYPE_I64)
            && operand_type != RUDA_TYPE_F64) {
            operand_type = RUDA_TYPE_INVALID;
        }
        kind = RUDA_MIR_NEGATE_CHECKED_I64;
    } else if (operator_token->kind == RUDA_TOKEN_TILDE) {
        operand_type = (expected >= RUDA_TYPE_I8 && expected <= RUDA_TYPE_U64)
            ? expected : RUDA_TYPE_INVALID;
        kind = RUDA_MIR_BITWISE_NOT_I64;
    } else {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (!ruda_compile_expression(function, operand, operand_type, &value)) {
        return false;
    }
    if (ruda_expression_is_never(function, &value)) {
        *result = value;
        return true;
    }
    if (operator_token->kind == RUDA_TOKEN_MINUS
        && ((value.type >= RUDA_TYPE_I8 && value.type <= RUDA_TYPE_I64)
            || value.type == RUDA_TYPE_F64)) {
        operand_type = value.type;
        kind = value.type == RUDA_TYPE_F64
            ? RUDA_MIR_NEGATE_F64
            : RUDA_MIR_NEGATE_CHECKED_I64;
    }
    if (value.type != operand_type) {
        if ((operator_token->kind == RUDA_TOKEN_MINUS
                && (value.type == RUDA_TYPE_I8
                    || value.type == RUDA_TYPE_I16
                    || value.type == RUDA_TYPE_I32
                    || value.type == RUDA_TYPE_F64))
            || (operator_token->kind == RUDA_TOKEN_TILDE
                && ruda_checker_private_type_is_integer(value.type))) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_INVALID_OPERATOR,
            "operator is not defined for the operand type",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    return ruda_function_emit_instruction(
        function,
        kind,
        operand_type,
        value.register_id,
        RUDA_MIR_NO_REGISTER,
        0,
        false,
        node->start,
        node->end,
        result
    );
}

 bool ruda_checker_private_binary_parts(
    const RudaAstNode *node,
    const RudaAstNode **left,
    const RudaSyntaxToken **operator_token,
    const RudaAstNode **right
) {
    size_t index = 0;
    *left = NULL;
    *operator_token = NULL;
    *right = NULL;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element == NULL) {
            return false;
        }
        if (element->kind == RUDA_AST_ELEMENT_NODE) {
            if (*left == NULL) {
                *left = element->as.node;
            } else if (*right == NULL) {
                *right = element->as.node;
            } else {
                return false;
            }
        } else if (element->kind == RUDA_AST_ELEMENT_TOKEN) {
            if (*operator_token != NULL) {
                return false;
            }
            *operator_token = &element->as.token;
        }
    }
    return *left != NULL && *operator_token != NULL && *right != NULL;
}

 bool ruda_checker_private_expression_is_integer_literal(const RudaAstNode *node) {
    const RudaAstNode *inner = NULL;
    const RudaSyntaxToken *token = NULL;
    if (node == NULL) {
        return false;
    }
    if (node->kind == RUDA_SYNTAX_EXPRESSION
        || node->kind == RUDA_SYNTAX_PAREN_GROUP) {
        inner = ruda_checker_private_expression_only_node(node);
        return ruda_checker_private_expression_is_integer_literal(inner);
    }
    if (node->kind != RUDA_SYNTAX_LITERAL_EXPRESSION) {
        return false;
    }
    token = ruda_checker_private_check_first_token(node);
    return token != NULL && token->kind == RUDA_TOKEN_INTEGER;
}

 bool ruda_checker_private_compile_short_circuit(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaAstNode *left_node,
    const RudaSyntaxToken *operator_token,
    const RudaAstNode *right_node,
    RudaExpressionValue *result
) {
    RudaExpressionValue left;
    RudaExpressionValue right;
    RudaExpressionValue shortcut;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId branch_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId right_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId shortcut_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId join_block = RUDA_MIR_NO_BLOCK;
    bool is_and = operator_token->kind == RUDA_TOKEN_AMPERSAND_AMPERSAND;
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    memset(&shortcut, 0, sizeof(shortcut));
    if (!ruda_compile_expression(
            function,
            left_node,
            RUDA_TYPE_INVALID,
            &left
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &left)) {
        *result = left;
        return true;
    }
    if (left.type != RUDA_TYPE_BOOL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_INVALID_OPERATOR,
            "logical operator requires bool operands",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    branch_block = function->block;
    if (!ruda_function_add_register(function, RUDA_TYPE_BOOL, &target)
        || !ruda_checker_private_function_add_block(function, &right_block)
        || !ruda_checker_private_function_add_block(function, &shortcut_block)
        || !ruda_checker_private_function_add_block(function, &join_block)
        || !ruda_checker_private_function_set_branch(
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
    if (!ruda_function_emit_instruction(
            function,
            RUDA_MIR_CONSTANT_BOOL,
            RUDA_TYPE_BOOL,
            RUDA_MIR_NO_REGISTER,
            RUDA_MIR_NO_REGISTER,
            0,
            !is_and,
            operator_token->start,
            operator_token->end,
            &shortcut
        ) || !ruda_checker_private_function_emit_copy(
            function,
            target,
            shortcut.register_id,
            operator_token->start,
            operator_token->end
        ) || !ruda_checker_private_function_set_goto(
            function,
            shortcut_block,
            join_block,
            operator_token->start,
            operator_token->end
        )) {
        return false;
    }

    function->block = right_block;
    if (!ruda_compile_expression(
            function,
            right_node,
            RUDA_TYPE_INVALID,
            &right
        )) {
        return false;
    }
    if (!ruda_expression_is_never(function, &right)
        && right.type != RUDA_TYPE_BOOL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_INVALID_OPERATOR,
            "logical operator requires bool operands",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    if (!ruda_expression_is_never(function, &right)
        && (!ruda_checker_private_function_emit_copy(
            function,
            target,
            right.register_id,
            right_node->start,
            right_node->end
        ) || !ruda_checker_private_function_set_goto(
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
    result->type = RUDA_TYPE_BOOL;
    result->register_id = target;
    result->valid = true;
    (void)node;
    return true;
}

 bool ruda_checker_private_compile_binary(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
) {
    const RudaAstNode *left_node = NULL;
    const RudaAstNode *right_node = NULL;
    const RudaSyntaxToken *operator_token = NULL;
    RudaExpressionValue left;
    RudaExpressionValue right;
    RudaExpressionValue equality;
    RudaMirInstructionKind kind = RUDA_MIR_ADD_CHECKED_I64;
    RudaTypeId operand_type = RUDA_TYPE_I64;
    RudaTypeId result_type = RUDA_TYPE_I64;
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
    if (!ruda_checker_private_binary_parts(node, &left_node, &operator_token, &right_node)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (operator_token->kind == RUDA_TOKEN_AMPERSAND_AMPERSAND
        || operator_token->kind == RUDA_TOKEN_PIPE_PIPE) {
        return ruda_checker_private_compile_short_circuit(
            function,
            node,
            left_node,
            operator_token,
            right_node,
            result
        );
    }
    switch (operator_token->kind) {
        case RUDA_TOKEN_PLUS:
            kind = RUDA_MIR_ADD_CHECKED_I64;
            numeric_operator = true;
            break;
        case RUDA_TOKEN_MINUS:
            kind = RUDA_MIR_SUBTRACT_CHECKED_I64;
            numeric_operator = true;
            break;
        case RUDA_TOKEN_STAR:
            kind = RUDA_MIR_MULTIPLY_CHECKED_I64;
            numeric_operator = true;
            break;
        case RUDA_TOKEN_SLASH:
            kind = RUDA_MIR_DIVIDE_CHECKED_I64;
            numeric_operator = true;
            break;
        case RUDA_TOKEN_PERCENT:
            kind = RUDA_MIR_REMAINDER_CHECKED_I64;
            numeric_operator = true;
            break;
        case RUDA_TOKEN_AMPERSAND:
            kind = RUDA_MIR_BITWISE_AND_I64;
            bitwise_operator = true;
            break;
        case RUDA_TOKEN_CARET:
            kind = RUDA_MIR_BITWISE_XOR_I64;
            bitwise_operator = true;
            break;
        case RUDA_TOKEN_PIPE:
            kind = RUDA_MIR_BITWISE_OR_I64;
            bitwise_operator = true;
            break;
        case RUDA_TOKEN_SHIFT_LEFT:
            kind = RUDA_MIR_SHIFT_LEFT_CHECKED_I64;
            shift_operator = true;
            break;
        case RUDA_TOKEN_SHIFT_RIGHT:
            kind = RUDA_MIR_SHIFT_RIGHT_I64;
            shift_operator = true;
            break;
        case RUDA_TOKEN_LESS:
            kind = RUDA_MIR_LESS_I64;
            result_type = RUDA_TYPE_BOOL;
            comparison_operator = true;
            break;
        case RUDA_TOKEN_LESS_EQUAL:
            kind = RUDA_MIR_LESS_EQUAL_I64;
            result_type = RUDA_TYPE_BOOL;
            comparison_operator = true;
            break;
        case RUDA_TOKEN_GREATER:
            kind = RUDA_MIR_LESS_I64;
            result_type = RUDA_TYPE_BOOL;
            swap = true;
            comparison_operator = true;
            break;
        case RUDA_TOKEN_GREATER_EQUAL:
            kind = RUDA_MIR_LESS_EQUAL_I64;
            result_type = RUDA_TYPE_BOOL;
            swap = true;
            comparison_operator = true;
            break;
        case RUDA_TOKEN_EQUAL_EQUAL:
            equality_operator = true;
            result_type = RUDA_TYPE_BOOL;
            break;
        case RUDA_TOKEN_BANG_EQUAL:
            equality_operator = true;
            result_type = RUDA_TYPE_BOOL;
            negate = true;
            break;
        default:
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
    }
    if (!ruda_compile_expression(
            function, left_node,
            (expected >= RUDA_TYPE_I8 && expected <= RUDA_TYPE_U64)
                || expected == RUDA_TYPE_F64 ? expected : RUDA_TYPE_INVALID,
            &left
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &left)) {
        *result = left;
        return true;
    }
    operand_type = left.type;
    if (!ruda_compile_expression(
            function,
            right_node,
            operand_type,
            &right
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &right)) {
        *result = right;
        return true;
    }
    if (left.type != right.type) {
        if ((ruda_checker_private_expression_is_integer_literal(left_node)
                && ruda_checker_private_type_is_integer(right.type))
            || (ruda_checker_private_expression_is_integer_literal(right_node)
                && ruda_checker_private_type_is_integer(left.type))) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_INVALID_OPERATOR,
            "operator is not defined for the operand types",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    if (!(operand_type >= RUDA_TYPE_I8 && operand_type <= RUDA_TYPE_U64)
        && operand_type != RUDA_TYPE_F64
        && !(equality_operator && operand_type == RUDA_TYPE_BOOL)) {
        if ((numeric_operator || comparison_operator)
            && ruda_checker_private_type_is_numeric(operand_type)) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        if (bitwise_operator && ruda_checker_private_type_is_integer(operand_type)) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        if (shift_operator && ruda_checker_private_type_is_integer(operand_type)) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        if (equality_operator) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_INVALID_OPERATOR,
            "operator is not defined for the operand types",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    if (operand_type == RUDA_TYPE_F64) {
        if (bitwise_operator || shift_operator
            || operator_token->kind == RUDA_TOKEN_PERCENT) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_INVALID_OPERATOR,
                "operator is not defined for f64 operands",
                operator_token->start,
                operator_token->end
            );
            return false;
        }
        if (numeric_operator) {
            result_type = RUDA_TYPE_F64;
        }
        if (equality_operator) {
            kind = RUDA_MIR_EQUAL_F64;
        } else if (comparison_operator) {
            kind = operator_token->kind == RUDA_TOKEN_LESS
                    || operator_token->kind == RUDA_TOKEN_GREATER
                ? RUDA_MIR_LESS_F64
                : RUDA_MIR_LESS_EQUAL_F64;
        } else if (operator_token->kind == RUDA_TOKEN_PLUS) {
            kind = RUDA_MIR_ADD_F64;
        } else if (operator_token->kind == RUDA_TOKEN_MINUS) {
            kind = RUDA_MIR_SUBTRACT_F64;
        } else if (operator_token->kind == RUDA_TOKEN_STAR) {
            kind = RUDA_MIR_MULTIPLY_F64;
        } else if (operator_token->kind == RUDA_TOKEN_SLASH) {
            kind = RUDA_MIR_DIVIDE_F64;
        }
    }
    if (equality_operator) {
        if (operand_type == RUDA_TYPE_BOOL) {
            kind = RUDA_MIR_EQUAL_BOOL;
        } else if (operand_type >= RUDA_TYPE_I8
            && operand_type <= RUDA_TYPE_U64) {
            kind = RUDA_MIR_EQUAL_I64;
        }
    }
    if (!ruda_function_emit_instruction(
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
    return ruda_function_emit_instruction(
        function,
        RUDA_MIR_NOT_BOOL,
        RUDA_TYPE_BOOL,
        equality.register_id,
        RUDA_MIR_NO_REGISTER,
        0,
        false,
        node->start,
        node->end,
        result
    );
}
