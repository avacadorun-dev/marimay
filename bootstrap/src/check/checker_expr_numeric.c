#include "checker_expr_numeric_api.h"

#include "checker_core_api.h"
#include "checker_generic_types_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

#include <string.h>

typedef struct MarimayCheckerNumericBuiltin {
    const char *name;
    MarimayMirIntegerPolicy policy;
    MarimayMirIntegerOperation operation;
    bool unary;
} MarimayCheckerNumericBuiltin;

static bool mmay_checker_numeric_integer(MarimayTypeId type) {
    return type >= MMAY_TYPE_I8 && type <= MMAY_TYPE_U64;
}

static bool mmay_checker_numeric_signed(MarimayTypeId type) {
    return type >= MMAY_TYPE_I8 && type <= MMAY_TYPE_I64;
}

static unsigned int mmay_checker_numeric_width(MarimayTypeId type) {
    switch (type) {
        case MMAY_TYPE_I8: case MMAY_TYPE_U8: return 8;
        case MMAY_TYPE_I16: case MMAY_TYPE_U16: return 16;
        case MMAY_TYPE_I32: case MMAY_TYPE_U32: return 32;
        case MMAY_TYPE_I64: case MMAY_TYPE_U64: return 64;
        default: return 0;
    }
}

static bool mmay_checker_numeric_range_preserving(
    MarimayTypeId source,
    MarimayTypeId target
) {
    unsigned int source_width = mmay_checker_numeric_width(source);
    unsigned int target_width = mmay_checker_numeric_width(target);
    bool source_signed = mmay_checker_numeric_signed(source);
    bool target_signed = mmay_checker_numeric_signed(target);
    if (source_width == 0 || target_width == 0) {
        return false;
    }
    if (source_signed == target_signed) {
        return source_width <= target_width;
    }
    return !source_signed && target_signed && source_width < target_width;
}

static const MarimayCheckerNumericBuiltin *mmay_checker_numeric_builtin(
    MarimayChecker *checker,
    const MarimaySyntaxToken *token
) {
#define MMAY_NUMERIC_POLICY(prefix, policy) \
    {prefix "Add", policy, MMAY_MIR_POLICY_ADD, false}, \
    {prefix "Subtract", policy, MMAY_MIR_POLICY_SUBTRACT, false}, \
    {prefix "Multiply", policy, MMAY_MIR_POLICY_MULTIPLY, false}, \
    {prefix "Divide", policy, MMAY_MIR_POLICY_DIVIDE, false}, \
    {prefix "Remainder", policy, MMAY_MIR_POLICY_REMAINDER, false}, \
    {prefix "Negate", policy, MMAY_MIR_POLICY_NEGATE, true}, \
    {prefix "ShiftLeft", policy, MMAY_MIR_POLICY_SHIFT_LEFT, false}, \
    {prefix "ShiftRight", policy, MMAY_MIR_POLICY_SHIFT_RIGHT, false}
    static const MarimayCheckerNumericBuiltin builtins[] = {
        MMAY_NUMERIC_POLICY("checked", MMAY_MIR_INTEGER_CHECKED_OPTION),
        MMAY_NUMERIC_POLICY("wrapping", MMAY_MIR_INTEGER_WRAPPING),
        MMAY_NUMERIC_POLICY("saturating", MMAY_MIR_INTEGER_SATURATING)
    };
#undef MMAY_NUMERIC_POLICY
    size_t index = 0;
    if (checker == NULL || token == NULL || token->synthetic) {
        return NULL;
    }
    for (index = 0; index < sizeof(builtins) / sizeof(builtins[0]); index += 1) {
        if (mmay_checker_private_check_text_equal(
                checker, token->start, token->end, builtins[index].name
            )) {
            return &builtins[index];
        }
    }
    return NULL;
}

static bool mmay_checker_numeric_option(
    MarimayFunctionChecker *function,
    MarimayTypeId payload_type,
    MarimayTypeId *option_type,
    MarimayMirEnumId *layout
) {
    MarimayCheckEnumDeclaration *base = function != NULL
        ? mmay_vector_at(&function->checker->prelude_enums, 0) : NULL;
    MarimayCheckEnumDeclaration concrete;
    MarimayTypeId arguments[1] = {payload_type};
    MarimayVector argument_view = {
        .data = arguments,
        .length = 1,
        .capacity = 1,
        .element_size = sizeof(MarimayTypeId)
    };
    if (base == NULL || option_type == NULL || layout == NULL
        || !mmay_checker_private_intern_generic_nominal(
            function->checker, MMAY_TYPE_KIND_NOMINAL,
            base->symbol, &argument_view, option_type
        )) {
        return false;
    }
    concrete = *base;
    concrete.type = *option_type;
    return mmay_checker_private_function_enum_layout(
        function, &concrete, layout
    );
}

static size_t mmay_checker_numeric_arguments(
    const MarimayAstNode *arguments,
    const MarimayAstNode **nodes,
    size_t capacity
) {
    size_t count = 0;
    size_t index = 0;
    for (index = 0; arguments != NULL && index < arguments->child_count;
        index += 1) {
        const MarimayAstElement *element = mmay_ast_child(arguments, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE) {
            if (count < capacity) {
                nodes[count] = element->as.node;
            }
            count += 1;
        }
    }
    return count;
}

static bool mmay_checker_numeric_expected(
    MarimayFunctionChecker *function,
    MarimayTypeId expected,
    MarimayTypeId actual,
    const MarimayAstNode *node
);

static bool mmay_checker_compile_try_convert(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayAstNode *arguments,
    MarimayTypeId expected,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *type_arguments =
        mmay_checker_private_check_child_node(
            node,
            MMAY_SYNTAX_TYPE_ARGUMENTS,
            0
        );
    const MarimayAstNode *argument_nodes[1] = {NULL};
    const MarimayAstNode *target_node = NULL;
    MarimayExpressionValue source;
    MarimayTypeId target_type = MMAY_TYPE_INVALID;
    MarimayTypeId result_type = MMAY_TYPE_INVALID;
    MarimayMirEnumId option_layout = MMAY_MIR_NO_ENUM;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    size_t type_count = 0;
    size_t index = 0;
    for (index = 0; type_arguments != NULL
        && index < type_arguments->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(
            type_arguments,
            index
        );
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE
            && element->as.node != NULL
            && element->as.node->kind == MMAY_SYNTAX_TYPE) {
            if (type_count == 0) {
                target_node = element->as.node;
            }
            type_count += 1;
        }
    }
    if (type_count != 1 || target_node == NULL) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "tryConvert requires exactly one target type argument",
            node->start,
            node->end
        );
        return false;
    }
    if (mmay_checker_numeric_arguments(arguments, argument_nodes, 1) != 1) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_CALL_ARITY,
            "tryConvert requires exactly one value argument",
            node->start,
            node->end
        );
        return false;
    }
    if (!mmay_checker_private_check_exact_type(
            function->checker,
            target_node,
            &target_type
        )) {
        return false;
    }
    memset(&source, 0, sizeof(source));
    if (!mmay_compile_expression(
            function,
            argument_nodes[0],
            MMAY_TYPE_INVALID,
            &source
        )) {
        return false;
    }
    if (mmay_expression_is_never(function, &source)) {
        *result = source;
        return true;
    }
    if (!mmay_checker_numeric_integer(source.type)
        || !mmay_checker_numeric_integer(target_type)) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "tryConvert requires primitive integer source and target types",
            node->start,
            node->end
        );
        return false;
    }
    if (!mmay_checker_numeric_option(
            function,
            target_type,
            &result_type,
            &option_layout
        ) || !mmay_checker_numeric_expected(
            function,
            expected,
            result_type,
            node
        ) || !mmay_function_add_register(function, result_type, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_INTEGER_CONVERT;
    instruction.target = target;
    instruction.first = source.register_id;
    instruction.second = source.register_id;
    instruction.enum_id = option_layout;
    instruction.integer_target_type = target_type;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!mmay_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}

static bool mmay_checker_numeric_expected(
    MarimayFunctionChecker *function,
    MarimayTypeId expected,
    MarimayTypeId actual,
    const MarimayAstNode *node
) {
    if (expected == MMAY_TYPE_INVALID || expected == actual) {
        return true;
    }
    (void)mmay_checker_add_diagnostic(
        function->checker,
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "integer policy result does not match the expected type",
        node->start,
        node->end
    );
    return false;
}

bool mmay_checker_private_compile_numeric_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *callee = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_NAME_EXPRESSION, 0
    );
    const MarimayAstNode *arguments = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_ARGUMENTS, 0
    );
    const MarimaySyntaxToken *token = mmay_checker_private_check_child_token(
        callee, MMAY_TOKEN_IDENTIFIER, 0
    );
    const MarimayCheckerNumericBuiltin *builtin = mmay_checker_numeric_builtin(
        function != NULL ? function->checker : NULL, token
    );
    bool try_convert = token != NULL && !token->synthetic
        && mmay_checker_private_check_text_equal(
            function->checker,
            token->start,
            token->end,
            "tryConvert"
        );
    const MarimayAstNode *argument_nodes[2] = {NULL, NULL};
    MarimayExpressionValue first;
    MarimayExpressionValue second;
    MarimayMirInstruction instruction;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayTypeId result_type = MMAY_TYPE_INVALID;
    MarimayMirEnumId option_layout = MMAY_MIR_NO_ENUM;
    size_t argument_count = 0;
    if (function == NULL || resolved == NULL || result == NULL) {
        return false;
    }
    *resolved = false;
    if (builtin == NULL && !try_convert) {
        return true;
    }
    {
        MarimayCheckName name = {
            function->checker->ast->snapshot, token->start, token->end
        };
        if (mmay_checker_private_function_find_local(function, name) != NULL
            || mmay_checker_private_checker_find_top_level(
                function->checker, name
            ) != NULL) {
            return true;
        }
    }
    *resolved = true;
    if (try_convert) {
        return mmay_checker_compile_try_convert(
            function,
            node,
            arguments,
            expected,
            result
        );
    }
    if (mmay_checker_private_check_child_node(
            node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0
        ) != NULL) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "integer policy operations do not accept type arguments",
            node->start, node->end
        );
        return false;
    }
    argument_count = mmay_checker_numeric_arguments(
        arguments, argument_nodes, 2
    );
    if (argument_count != (builtin->unary ? 1 : 2)) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_CALL_ARITY,
            builtin->unary
                ? "integer negate policy requires one argument"
                : "integer policy operation requires two arguments",
            node->start, node->end
        );
        return false;
    }
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    if (!mmay_compile_expression(
            function, argument_nodes[0], MMAY_TYPE_INVALID, &first
        )) {
        return false;
    }
    if (mmay_expression_is_never(function, &first)) {
        *result = first;
        return true;
    }
    if (!mmay_checker_numeric_integer(first.type)) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "integer policy operation requires primitive integer arguments",
            argument_nodes[0]->start, argument_nodes[0]->end
        );
        return false;
    }
    second = first;
    if (!builtin->unary && !mmay_compile_expression(
            function, argument_nodes[1], first.type, &second
        )) {
        return false;
    }
    if (!builtin->unary && mmay_expression_is_never(function, &second)) {
        *result = second;
        return true;
    }
    if (second.type != first.type) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "integer policy arguments must have the same exact type",
            argument_nodes[1]->start, argument_nodes[1]->end
        );
        return false;
    }
    result_type = first.type;
    if (builtin->policy == MMAY_MIR_INTEGER_CHECKED_OPTION
        && !mmay_checker_numeric_option(
            function, first.type, &result_type, &option_layout
        )) {
        return false;
    }
    if (!mmay_checker_numeric_expected(function, expected, result_type, node)
        || !mmay_function_add_register(function, result_type, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_INTEGER_POLICY;
    instruction.target = target;
    instruction.first = first.register_id;
    instruction.second = second.register_id;
    instruction.enum_id = option_layout;
    instruction.integer_policy = builtin->policy;
    instruction.integer_operation = builtin->operation;
    instruction.integer_target_type = MMAY_TYPE_INVALID;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!mmay_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}

static const MarimayAstNode *mmay_checker_numeric_conversion_source(
    const MarimayAstNode *node
) {
    size_t index = 0;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE
            && element->as.node != NULL
            && element->as.node->kind != MMAY_SYNTAX_TYPE) {
            return element->as.node;
        }
    }
    return NULL;
}

bool mmay_checker_private_compile_integer_conversion(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *source_node = mmay_checker_numeric_conversion_source(node);
    const MarimayAstNode *type_node = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_TYPE, 0
    );
    MarimayExpressionValue source;
    MarimayTypeId target_type = MMAY_TYPE_INVALID;
    MarimayTypeId result_type = MMAY_TYPE_INVALID;
    MarimayMirEnumId option_layout = MMAY_MIR_NO_ENUM;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    if (function == NULL || node == NULL || result == NULL
        || source_node == NULL || type_node == NULL
        || !mmay_checker_private_check_exact_type(
            function->checker, type_node, &target_type
        )) {
        return false;
    }
    memset(&source, 0, sizeof(source));
    if (!mmay_compile_expression(
            function, source_node, MMAY_TYPE_INVALID, &source
        )) {
        return false;
    }
    if (mmay_expression_is_never(function, &source)) {
        *result = source;
        return true;
    }
    if (!mmay_checker_numeric_integer(source.type)
        || !mmay_checker_numeric_integer(target_type)) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "'as' conversion requires primitive integer source and target types",
            node->start, node->end
        );
        return false;
    }
    result_type = target_type;
    if (!mmay_checker_numeric_range_preserving(source.type, target_type)) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "'as' requires a lossless integer conversion; use tryConvert<Target>(value) for a fallible conversion",
            node->start,
            node->end
        );
        return false;
    }
    if (!mmay_checker_numeric_expected(function, expected, result_type, node)
        || !mmay_function_add_register(function, result_type, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_INTEGER_CONVERT;
    instruction.target = target;
    instruction.first = source.register_id;
    instruction.second = source.register_id;
    instruction.enum_id = option_layout;
    instruction.integer_target_type = target_type;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!mmay_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}

static bool mmay_checker_numeric_bytes_equal(
    const unsigned char *left,
    size_t left_length,
    const unsigned char *right,
    size_t right_length
) {
    return left_length == right_length
        && (left_length == 0 || memcmp(left, right, left_length) == 0);
}

static const MarimayCheckImportBinding *mmay_checker_numeric_format_binding(
    MarimayFunctionChecker *function,
    const MarimaySyntaxToken *namespace_token,
    const MarimaySyntaxToken *name_token,
    const char **format_name
) {
    const MarimayCheckImportEnvironment *imports = function != NULL
        ? function->checker->imports : NULL;
    size_t index = 0;
    for (index = 0; imports != NULL && index < imports->binding_count;
        index += 1) {
        const MarimayCheckImportBinding *binding = &imports->bindings[index];
        const MarimayCheckPackageSurface *surface = binding->surface_index
                < imports->surface_count
            ? imports->surfaces[binding->surface_index] : NULL;
        const unsigned char *local_name = namespace_token != NULL
            ? function->checker->source + namespace_token->start
            : function->checker->source + name_token->start;
        size_t local_length = namespace_token != NULL
            ? namespace_token->end - namespace_token->start
            : name_token->end - name_token->start;
        const unsigned char *public_name = namespace_token != NULL
            ? function->checker->source + name_token->start
            : binding->original;
        size_t public_length = namespace_token != NULL
            ? name_token->end - name_token->start
            : binding->original_length;
        if (binding->ast != function->checker->ast
            || binding->is_namespace != (namespace_token != NULL)
            || surface == NULL
            || !mmay_checker_numeric_bytes_equal(
                surface->path, surface->path_length,
                (const unsigned char *)"std/convert", 11
            ) || !mmay_checker_numeric_bytes_equal(
                binding->local, binding->local_length,
                local_name, local_length
            )) {
            continue;
        }
        if (mmay_checker_numeric_bytes_equal(
                public_name, public_length,
                (const unsigned char *)"formatF32", 9
            )) {
            *format_name = "formatF32";
            return binding;
        }
        if (mmay_checker_numeric_bytes_equal(
                public_name, public_length,
                (const unsigned char *)"formatF64", 9
            )) {
            *format_name = "formatF64";
            return binding;
        }
    }
    return NULL;
}

bool mmay_checker_private_compile_convert_format_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *callee = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_NAME_EXPRESSION, 0
    );
    const MarimayAstNode *member = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_MEMBER_EXPRESSION, 0
    );
    const MarimayAstNode *receiver = NULL;
    const MarimayAstNode *arguments = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_ARGUMENTS, 0
    );
    const MarimaySyntaxToken *namespace_token = NULL;
    const MarimaySyntaxToken *name_token = mmay_checker_private_check_child_token(
        callee, MMAY_TOKEN_IDENTIFIER, 0
    );
    const MarimayCheckImportBinding *binding;
    const MarimayAstNode *argument_nodes[1] = {NULL};
    const char *format_name = NULL;
    MarimayExpressionValue source;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    MarimayTypeId source_type;
    if (function == NULL || resolved == NULL || result == NULL) {
        return false;
    }
    *resolved = false;
    if (name_token == NULL && member != NULL
        && mmay_checker_private_member_parts(
            member, &receiver, &name_token
        ) && receiver != NULL
        && receiver->kind == MMAY_SYNTAX_NAME_EXPRESSION) {
        namespace_token = mmay_checker_private_check_child_token(
            receiver, MMAY_TOKEN_IDENTIFIER, 0
        );
    }
    if (name_token == NULL || name_token->synthetic
        || (namespace_token != NULL && namespace_token->synthetic)) {
        return true;
    }
    binding = mmay_checker_numeric_format_binding(
        function, namespace_token, name_token, &format_name
    );
    if (binding == NULL) {
        return true;
    }
    if (namespace_token == NULL) {
        MarimayCheckName local_name = {
            function->checker->ast->snapshot,
            name_token->start,
            name_token->end
        };
        if (mmay_checker_private_function_find_local(
                function, local_name
            ) != NULL) {
            return true;
        }
    } else {
        MarimayCheckName namespace_name = {
            function->checker->ast->snapshot,
            namespace_token->start,
            namespace_token->end
        };
        if (mmay_checker_private_function_find_local(
                function, namespace_name
            ) != NULL) {
            return true;
        }
    }
    (void)binding;
    *resolved = true;
    if (mmay_checker_private_check_child_node(
            node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0
        ) != NULL
        || mmay_checker_numeric_arguments(arguments, argument_nodes, 1) != 1) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            mmay_checker_private_check_child_node(
                    node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0
                ) != NULL
                ? MMAY_DIAGNOSTIC_TYPE_MISMATCH
                : MMAY_DIAGNOSTIC_CALL_ARITY,
            "std/convert formatter requires exactly one argument and no type arguments",
            node->start,
            node->end
        );
        return false;
    }
    source_type = strcmp(format_name, "formatF32") == 0
        ? MMAY_TYPE_F32 : MMAY_TYPE_F64;
    memset(&source, 0, sizeof(source));
    if (!mmay_compile_expression(
            function, argument_nodes[0], source_type, &source
        )) {
        return false;
    }
    if (mmay_expression_is_never(function, &source)) {
        *result = source;
        return true;
    }
    if (source.type != source_type || (expected != MMAY_TYPE_INVALID
            && expected != MMAY_TYPE_STRING)) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "std/convert formatter argument or result type does not match",
            node->start, node->end
        );
        return false;
    }
    if (!mmay_function_add_register(
            function, MMAY_TYPE_STRING, &target
        )) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = source_type == MMAY_TYPE_F32
        ? MMAY_MIR_FORMAT_F32 : MMAY_MIR_FORMAT_F64;
    instruction.target = target;
    instruction.first = source.register_id;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!mmay_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = MMAY_TYPE_STRING;
    result->register_id = target;
    result->valid = true;
    return true;
}
