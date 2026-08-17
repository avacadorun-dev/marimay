#include "checker_expr_numeric_api.h"

#include "checker_core_api.h"
#include "checker_generic_types_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

#include <string.h>

typedef struct RudaCheckerNumericBuiltin {
    const char *name;
    RudaMirIntegerPolicy policy;
    RudaMirIntegerOperation operation;
    bool unary;
} RudaCheckerNumericBuiltin;

static bool ruda_checker_numeric_integer(RudaTypeId type) {
    return type >= RUDA_TYPE_I8 && type <= RUDA_TYPE_U64;
}

static bool ruda_checker_numeric_signed(RudaTypeId type) {
    return type >= RUDA_TYPE_I8 && type <= RUDA_TYPE_I64;
}

static unsigned int ruda_checker_numeric_width(RudaTypeId type) {
    switch (type) {
        case RUDA_TYPE_I8: case RUDA_TYPE_U8: return 8;
        case RUDA_TYPE_I16: case RUDA_TYPE_U16: return 16;
        case RUDA_TYPE_I32: case RUDA_TYPE_U32: return 32;
        case RUDA_TYPE_I64: case RUDA_TYPE_U64: return 64;
        default: return 0;
    }
}

static bool ruda_checker_numeric_range_preserving(
    RudaTypeId source,
    RudaTypeId target
) {
    unsigned int source_width = ruda_checker_numeric_width(source);
    unsigned int target_width = ruda_checker_numeric_width(target);
    bool source_signed = ruda_checker_numeric_signed(source);
    bool target_signed = ruda_checker_numeric_signed(target);
    if (source_width == 0 || target_width == 0) {
        return false;
    }
    if (source_signed == target_signed) {
        return source_width <= target_width;
    }
    return !source_signed && target_signed && source_width < target_width;
}

static const RudaCheckerNumericBuiltin *ruda_checker_numeric_builtin(
    RudaChecker *checker,
    const RudaSyntaxToken *token
) {
#define RUDA_NUMERIC_POLICY(prefix, policy) \
    {prefix "Add", policy, RUDA_MIR_POLICY_ADD, false}, \
    {prefix "Subtract", policy, RUDA_MIR_POLICY_SUBTRACT, false}, \
    {prefix "Multiply", policy, RUDA_MIR_POLICY_MULTIPLY, false}, \
    {prefix "Divide", policy, RUDA_MIR_POLICY_DIVIDE, false}, \
    {prefix "Remainder", policy, RUDA_MIR_POLICY_REMAINDER, false}, \
    {prefix "Negate", policy, RUDA_MIR_POLICY_NEGATE, true}, \
    {prefix "ShiftLeft", policy, RUDA_MIR_POLICY_SHIFT_LEFT, false}, \
    {prefix "ShiftRight", policy, RUDA_MIR_POLICY_SHIFT_RIGHT, false}
    static const RudaCheckerNumericBuiltin builtins[] = {
        RUDA_NUMERIC_POLICY("checked", RUDA_MIR_INTEGER_CHECKED_OPTION),
        RUDA_NUMERIC_POLICY("wrapping", RUDA_MIR_INTEGER_WRAPPING),
        RUDA_NUMERIC_POLICY("saturating", RUDA_MIR_INTEGER_SATURATING)
    };
#undef RUDA_NUMERIC_POLICY
    size_t index = 0;
    if (checker == NULL || token == NULL || token->synthetic) {
        return NULL;
    }
    for (index = 0; index < sizeof(builtins) / sizeof(builtins[0]); index += 1) {
        if (ruda_checker_private_check_text_equal(
                checker, token->start, token->end, builtins[index].name
            )) {
            return &builtins[index];
        }
    }
    return NULL;
}

static bool ruda_checker_numeric_option(
    RudaFunctionChecker *function,
    RudaTypeId payload_type,
    RudaTypeId *option_type,
    RudaMirEnumId *layout
) {
    RudaCheckEnumDeclaration *base = function != NULL
        ? ruda_vector_at(&function->checker->prelude_enums, 0) : NULL;
    RudaCheckEnumDeclaration concrete;
    RudaTypeId arguments[1] = {payload_type};
    RudaVector argument_view = {
        .data = arguments,
        .length = 1,
        .capacity = 1,
        .element_size = sizeof(RudaTypeId)
    };
    if (base == NULL || option_type == NULL || layout == NULL
        || !ruda_checker_private_intern_generic_nominal(
            function->checker, RUDA_TYPE_KIND_NOMINAL,
            base->symbol, &argument_view, option_type
        )) {
        return false;
    }
    concrete = *base;
    concrete.type = *option_type;
    return ruda_checker_private_function_enum_layout(
        function, &concrete, layout
    );
}

static size_t ruda_checker_numeric_arguments(
    const RudaAstNode *arguments,
    const RudaAstNode **nodes,
    size_t capacity
) {
    size_t count = 0;
    size_t index = 0;
    for (index = 0; arguments != NULL && index < arguments->child_count;
        index += 1) {
        const RudaAstElement *element = ruda_ast_child(arguments, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE) {
            if (count < capacity) {
                nodes[count] = element->as.node;
            }
            count += 1;
        }
    }
    return count;
}

static bool ruda_checker_numeric_expected(
    RudaFunctionChecker *function,
    RudaTypeId expected,
    RudaTypeId actual,
    const RudaAstNode *node
) {
    if (expected == RUDA_TYPE_INVALID || expected == actual) {
        return true;
    }
    (void)ruda_checker_add_diagnostic(
        function->checker,
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "integer policy result does not match the expected type",
        node->start,
        node->end
    );
    return false;
}

bool ruda_checker_private_compile_numeric_call(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    bool *resolved,
    RudaExpressionValue *result
) {
    const RudaAstNode *callee = ruda_checker_private_check_child_node(
        node, RUDA_SYNTAX_NAME_EXPRESSION, 0
    );
    const RudaAstNode *arguments = ruda_checker_private_check_child_node(
        node, RUDA_SYNTAX_ARGUMENTS, 0
    );
    const RudaSyntaxToken *token = ruda_checker_private_check_child_token(
        callee, RUDA_TOKEN_IDENTIFIER, 0
    );
    const RudaCheckerNumericBuiltin *builtin = ruda_checker_numeric_builtin(
        function != NULL ? function->checker : NULL, token
    );
    const RudaAstNode *argument_nodes[2] = {NULL, NULL};
    RudaExpressionValue first;
    RudaExpressionValue second;
    RudaMirInstruction instruction;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaTypeId result_type = RUDA_TYPE_INVALID;
    RudaMirEnumId option_layout = RUDA_MIR_NO_ENUM;
    size_t argument_count = 0;
    if (function == NULL || resolved == NULL || result == NULL) {
        return false;
    }
    *resolved = false;
    if (builtin == NULL) {
        return true;
    }
    {
        RudaCheckName name = {
            function->checker->ast->snapshot, token->start, token->end
        };
        if (ruda_checker_private_function_find_local(function, name) != NULL
            || ruda_checker_private_checker_find_top_level(
                function->checker, name
            ) != NULL) {
            return true;
        }
    }
    *resolved = true;
    if (ruda_checker_private_check_child_node(
            node, RUDA_SYNTAX_TYPE_ARGUMENTS, 0
        ) != NULL) {
        (void)ruda_checker_add_diagnostic(
            function->checker, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "integer policy operations do not accept type arguments",
            node->start, node->end
        );
        return false;
    }
    argument_count = ruda_checker_numeric_arguments(
        arguments, argument_nodes, 2
    );
    if (argument_count != (builtin->unary ? 1 : 2)) {
        (void)ruda_checker_add_diagnostic(
            function->checker, RUDA_DIAGNOSTIC_CALL_ARITY,
            builtin->unary
                ? "integer negate policy requires one argument"
                : "integer policy operation requires two arguments",
            node->start, node->end
        );
        return false;
    }
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    if (!ruda_compile_expression(
            function, argument_nodes[0], RUDA_TYPE_INVALID, &first
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &first)) {
        *result = first;
        return true;
    }
    if (!ruda_checker_numeric_integer(first.type)) {
        (void)ruda_checker_add_diagnostic(
            function->checker, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "integer policy operation requires primitive integer arguments",
            argument_nodes[0]->start, argument_nodes[0]->end
        );
        return false;
    }
    second = first;
    if (!builtin->unary && !ruda_compile_expression(
            function, argument_nodes[1], first.type, &second
        )) {
        return false;
    }
    if (!builtin->unary && ruda_expression_is_never(function, &second)) {
        *result = second;
        return true;
    }
    if (second.type != first.type) {
        (void)ruda_checker_add_diagnostic(
            function->checker, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "integer policy arguments must have the same exact type",
            argument_nodes[1]->start, argument_nodes[1]->end
        );
        return false;
    }
    result_type = first.type;
    if (builtin->policy == RUDA_MIR_INTEGER_CHECKED_OPTION
        && !ruda_checker_numeric_option(
            function, first.type, &result_type, &option_layout
        )) {
        return false;
    }
    if (!ruda_checker_numeric_expected(function, expected, result_type, node)
        || !ruda_function_add_register(function, result_type, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_INTEGER_POLICY;
    instruction.target = target;
    instruction.first = first.register_id;
    instruction.second = second.register_id;
    instruction.enum_id = option_layout;
    instruction.integer_policy = builtin->policy;
    instruction.integer_operation = builtin->operation;
    instruction.integer_target_type = RUDA_TYPE_INVALID;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}

static const RudaAstNode *ruda_checker_numeric_conversion_source(
    const RudaAstNode *node
) {
    size_t index = 0;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node != NULL
            && element->as.node->kind != RUDA_SYNTAX_TYPE) {
            return element->as.node;
        }
    }
    return NULL;
}

bool ruda_checker_private_compile_integer_conversion(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
) {
    const RudaAstNode *source_node = ruda_checker_numeric_conversion_source(node);
    const RudaAstNode *type_node = ruda_checker_private_check_child_node(
        node, RUDA_SYNTAX_TYPE, 0
    );
    RudaExpressionValue source;
    RudaTypeId target_type = RUDA_TYPE_INVALID;
    RudaTypeId result_type = RUDA_TYPE_INVALID;
    RudaMirEnumId option_layout = RUDA_MIR_NO_ENUM;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    if (function == NULL || node == NULL || result == NULL
        || source_node == NULL || type_node == NULL
        || !ruda_checker_private_check_exact_type(
            function->checker, type_node, &target_type
        )) {
        return false;
    }
    memset(&source, 0, sizeof(source));
    if (!ruda_compile_expression(
            function, source_node, RUDA_TYPE_INVALID, &source
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &source)) {
        *result = source;
        return true;
    }
    if (!ruda_checker_numeric_integer(source.type)
        || !ruda_checker_numeric_integer(target_type)) {
        (void)ruda_checker_add_diagnostic(
            function->checker, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "'as' conversion requires primitive integer source and target types",
            node->start, node->end
        );
        return false;
    }
    result_type = target_type;
    if (!ruda_checker_numeric_range_preserving(source.type, target_type)
        && !ruda_checker_numeric_option(
            function, target_type, &result_type, &option_layout
        )) {
        return false;
    }
    if (!ruda_checker_numeric_expected(function, expected, result_type, node)
        || !ruda_function_add_register(function, result_type, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_INTEGER_CONVERT;
    instruction.target = target;
    instruction.first = source.register_id;
    instruction.second = source.register_id;
    instruction.enum_id = option_layout;
    instruction.integer_target_type = target_type;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}

static bool ruda_checker_numeric_bytes_equal(
    const unsigned char *left,
    size_t left_length,
    const unsigned char *right,
    size_t right_length
) {
    return left_length == right_length
        && (left_length == 0 || memcmp(left, right, left_length) == 0);
}

static const RudaCheckImportBinding *ruda_checker_numeric_format_binding(
    RudaFunctionChecker *function,
    const RudaSyntaxToken *namespace_token,
    const RudaSyntaxToken *name_token,
    const char **format_name
) {
    const RudaCheckImportEnvironment *imports = function != NULL
        ? function->checker->imports : NULL;
    size_t index = 0;
    for (index = 0; imports != NULL && index < imports->binding_count;
        index += 1) {
        const RudaCheckImportBinding *binding = &imports->bindings[index];
        const RudaCheckPackageSurface *surface = binding->surface_index
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
            || !ruda_checker_numeric_bytes_equal(
                surface->path, surface->path_length,
                (const unsigned char *)"std/convert", 11
            ) || !ruda_checker_numeric_bytes_equal(
                binding->local, binding->local_length,
                local_name, local_length
            )) {
            continue;
        }
        if (ruda_checker_numeric_bytes_equal(
                public_name, public_length,
                (const unsigned char *)"formatF32", 9
            )) {
            *format_name = "formatF32";
            return binding;
        }
        if (ruda_checker_numeric_bytes_equal(
                public_name, public_length,
                (const unsigned char *)"formatF64", 9
            )) {
            *format_name = "formatF64";
            return binding;
        }
    }
    return NULL;
}

bool ruda_checker_private_compile_convert_format_call(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    bool *resolved,
    RudaExpressionValue *result
) {
    const RudaAstNode *callee = ruda_checker_private_check_child_node(
        node, RUDA_SYNTAX_NAME_EXPRESSION, 0
    );
    const RudaAstNode *member = ruda_checker_private_check_child_node(
        node, RUDA_SYNTAX_MEMBER_EXPRESSION, 0
    );
    const RudaAstNode *receiver = NULL;
    const RudaAstNode *arguments = ruda_checker_private_check_child_node(
        node, RUDA_SYNTAX_ARGUMENTS, 0
    );
    const RudaSyntaxToken *namespace_token = NULL;
    const RudaSyntaxToken *name_token = ruda_checker_private_check_child_token(
        callee, RUDA_TOKEN_IDENTIFIER, 0
    );
    const RudaCheckImportBinding *binding;
    const RudaAstNode *argument_nodes[1] = {NULL};
    const char *format_name = NULL;
    RudaExpressionValue source;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    RudaTypeId source_type;
    if (function == NULL || resolved == NULL || result == NULL) {
        return false;
    }
    *resolved = false;
    if (name_token == NULL && member != NULL
        && ruda_checker_private_member_parts(
            member, &receiver, &name_token
        ) && receiver != NULL
        && receiver->kind == RUDA_SYNTAX_NAME_EXPRESSION) {
        namespace_token = ruda_checker_private_check_child_token(
            receiver, RUDA_TOKEN_IDENTIFIER, 0
        );
    }
    if (name_token == NULL || name_token->synthetic
        || (namespace_token != NULL && namespace_token->synthetic)) {
        return true;
    }
    binding = ruda_checker_numeric_format_binding(
        function, namespace_token, name_token, &format_name
    );
    if (binding == NULL) {
        return true;
    }
    if (namespace_token == NULL) {
        RudaCheckName local_name = {
            function->checker->ast->snapshot,
            name_token->start,
            name_token->end
        };
        if (ruda_checker_private_function_find_local(
                function, local_name
            ) != NULL) {
            return true;
        }
    } else {
        RudaCheckName namespace_name = {
            function->checker->ast->snapshot,
            namespace_token->start,
            namespace_token->end
        };
        if (ruda_checker_private_function_find_local(
                function, namespace_name
            ) != NULL) {
            return true;
        }
    }
    (void)binding;
    *resolved = true;
    if (ruda_checker_private_check_child_node(
            node, RUDA_SYNTAX_TYPE_ARGUMENTS, 0
        ) != NULL
        || ruda_checker_numeric_arguments(arguments, argument_nodes, 1) != 1) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            ruda_checker_private_check_child_node(
                    node, RUDA_SYNTAX_TYPE_ARGUMENTS, 0
                ) != NULL
                ? RUDA_DIAGNOSTIC_TYPE_MISMATCH
                : RUDA_DIAGNOSTIC_CALL_ARITY,
            "std/convert formatter requires exactly one argument and no type arguments",
            node->start,
            node->end
        );
        return false;
    }
    source_type = strcmp(format_name, "formatF32") == 0
        ? RUDA_TYPE_F32 : RUDA_TYPE_F64;
    memset(&source, 0, sizeof(source));
    if (!ruda_compile_expression(
            function, argument_nodes[0], source_type, &source
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &source)) {
        *result = source;
        return true;
    }
    if (source.type != source_type || (expected != RUDA_TYPE_INVALID
            && expected != RUDA_TYPE_STRING)) {
        (void)ruda_checker_add_diagnostic(
            function->checker, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "std/convert formatter argument or result type does not match",
            node->start, node->end
        );
        return false;
    }
    if (!ruda_function_add_register(
            function, RUDA_TYPE_STRING, &target
        )) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = source_type == RUDA_TYPE_F32
        ? RUDA_MIR_FORMAT_F32 : RUDA_MIR_FORMAT_F64;
    instruction.target = target;
    instruction.first = source.register_id;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = RUDA_TYPE_STRING;
    result->register_id = target;
    result->valid = true;
    return true;
}
