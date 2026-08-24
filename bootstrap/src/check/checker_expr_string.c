#include "checker_expr_string_api.h"

#include "checker_core_api.h"
#include "checker_expr_calls_api.h"
#include "checker_expression_api.h"
#include "checker_imported_nominal_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

#include "mmay/base/string_literal.h"

#include <string.h>

bool mmay_checker_private_emit_string_constant(
    MarimayFunctionChecker *function,
    const unsigned char *bytes,
    size_t length,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
) {
    MarimayMirStringId string_constant = MMAY_MIR_NO_STRING;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    size_t new_string_bytes = 0;
    if (function == NULL || result == NULL || (bytes == NULL && length != 0)) {
        return false;
    }
    if (function->mir.string_constants.length
            >= function->mir.options.max_string_constants
        || !mmay_size_add(function->mir.string_bytes, length, &new_string_bytes)
        || new_string_bytes > function->mir.options.max_string_bytes) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!mmay_function_add_register(function, MMAY_TYPE_STRING, &target)
        || !mmay_mir_add_string_constant(
            &function->mir, bytes, length, &string_constant
        )) {
        if (!mmay_checker_has_failed(function->checker)) {
            mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        }
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_STRING;
    instruction.target = target;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.string_constant = string_constant;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!mmay_function_append_instruction(function, &instruction)) return false;
    result->type = MMAY_TYPE_STRING;
    result->register_id = target;
    result->valid = true;
    return true;
}

static bool mmay_checker_string_format_value(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayExpressionValue value,
    MarimayExpressionValue *result
) {
    MarimayMirInstructionKind kind;
    MarimayMirInstruction instruction;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    if (value.type == MMAY_TYPE_STRING) {
        *result = value;
        return true;
    }
    if (value.type == MMAY_TYPE_BOOL) kind = MMAY_MIR_FORMAT_BOOL;
    else if (value.type >= MMAY_TYPE_I8 && value.type <= MMAY_TYPE_U64) {
        kind = MMAY_MIR_FORMAT_INTEGER;
    } else if (value.type == MMAY_TYPE_F64) kind = MMAY_MIR_FORMAT_F64;
    else {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "interpolation accepts only string, bool, integer, or f64 values",
            node->start, node->end
        );
        return false;
    }
    if (!mmay_function_add_register(function, MMAY_TYPE_STRING, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = value.register_id;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!mmay_function_append_instruction(function, &instruction)) return false;
    result->type = MMAY_TYPE_STRING;
    result->register_id = target;
    result->valid = true;
    return true;
}

static bool mmay_checker_string_append(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayExpressionValue *accumulator,
    MarimayExpressionValue piece,
    bool *has_accumulator
) {
    MarimayMirInstruction instruction;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    if (!*has_accumulator) {
        *accumulator = piece;
        *has_accumulator = true;
        return true;
    }
    if (!mmay_function_add_register(function, MMAY_TYPE_STRING, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_STRING_CONCAT;
    instruction.target = target;
    instruction.first = accumulator->register_id;
    instruction.second = piece.register_id;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!mmay_function_append_instruction(function, &instruction)) return false;
    accumulator->type = MMAY_TYPE_STRING;
    accumulator->register_id = target;
    accumulator->valid = true;
    return true;
}

bool mmay_checker_private_compile_interpolation(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
) {
    MarimayExpressionValue accumulator;
    bool has_accumulator = false;
    size_t index;
    if (function == NULL || node == NULL || result == NULL) return false;
    if (expected != MMAY_TYPE_INVALID && expected != MMAY_TYPE_STRING) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "interpolated string does not match the required type",
            node->start, node->end
        );
        return false;
    }
    memset(&accumulator, 0, sizeof(accumulator));
    for (index = 0; index < node->child_count; index += 1U) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        MarimayExpressionValue piece;
        if (element == NULL) return false;
        memset(&piece, 0, sizeof(piece));
        if (element->kind == MMAY_AST_ELEMENT_TOKEN
            && element->as.token.kind == MMAY_TOKEN_INTERPOLATED_TEXT) {
            MarimayOwnedBytes decoded;
            MarimayStringLiteralStatus status = mmay_interpolated_text_decode(
                function->checker->source + element->as.token.start,
                element->as.token.end - element->as.token.start,
                function->checker->result->allocator, &decoded
            );
            if (status != MMAY_STRING_LITERAL_OK) {
                mmay_checker_fail(
                    function->checker,
                    status == MMAY_STRING_LITERAL_ALLOCATION_FAILED
                        ? MMAY_CHECK_ALLOCATION_FAILED : MMAY_CHECK_INTERNAL_ERROR
                );
                return false;
            }
            if (!mmay_checker_private_emit_string_constant(
                    function, decoded.data, decoded.length,
                    element->as.token.start, element->as.token.end, &piece
                )) {
                mmay_owned_bytes_dispose(&decoded);
                return false;
            }
            mmay_owned_bytes_dispose(&decoded);
        } else if (element->kind == MMAY_AST_ELEMENT_NODE
            && element->as.node != NULL
            && element->as.node->kind == MMAY_SYNTAX_EXPRESSION) {
            MarimayExpressionValue value;
            memset(&value, 0, sizeof(value));
            if (!mmay_compile_expression(
                    function, element->as.node, MMAY_TYPE_INVALID, &value
                )) return false;
            if (mmay_expression_is_never(function, &value)) {
                *result = value;
                return true;
            }
            if (!mmay_checker_string_format_value(
                    function, element->as.node, value, &piece
                )) return false;
        } else {
            continue;
        }
        if (!mmay_checker_string_append(
                function, node, &accumulator, piece, &has_accumulator
            )) return false;
    }
    if (!has_accumulator && !mmay_checker_private_emit_string_constant(
            function, NULL, 0, node->start, node->end, &accumulator
        )) return false;
    *result = accumulator;
    return true;
}

static bool mmay_checker_string_bytes_equal(
    const unsigned char *left, size_t left_length,
    const unsigned char *right, size_t right_length
) {
    return left_length == right_length
        && (left_length == 0 || memcmp(left, right, left_length) == 0);
}

static MarimayTypeId mmay_checker_string_format_type(
    const unsigned char *name,
    size_t length
) {
    static const struct { const char *name; MarimayTypeId type; } formats[] = {
        {"formatBool", MMAY_TYPE_BOOL}, {"formatI8", MMAY_TYPE_I8},
        {"formatI16", MMAY_TYPE_I16}, {"formatI32", MMAY_TYPE_I32},
        {"formatI64", MMAY_TYPE_I64}, {"formatU8", MMAY_TYPE_U8},
        {"formatU16", MMAY_TYPE_U16}, {"formatU32", MMAY_TYPE_U32},
        {"formatU64", MMAY_TYPE_U64}
    };
    size_t index;
    for (index = 0; index < sizeof(formats) / sizeof(formats[0]); index += 1U) {
        if (strlen(formats[index].name) == length
            && memcmp(name, formats[index].name, length) == 0) {
            return formats[index].type;
        }
    }
    return MMAY_TYPE_INVALID;
}

static MarimayTypeId mmay_checker_string_imported_format(
    MarimayFunctionChecker *function,
    const MarimaySyntaxToken *namespace_token,
    const MarimaySyntaxToken *name_token
) {
    const MarimayCheckImportEnvironment *imports = function->checker->imports;
    size_t index;
    for (index = 0; imports != NULL && index < imports->binding_count; index += 1U) {
        const MarimayCheckImportBinding *binding = &imports->bindings[index];
        const MarimayCheckPackageSurface *surface = binding->surface_index
                < imports->surface_count
            ? imports->surfaces[binding->surface_index] : NULL;
        const unsigned char *local = function->checker->source
            + (namespace_token != NULL ? namespace_token->start : name_token->start);
        size_t local_length = namespace_token != NULL
            ? namespace_token->end - namespace_token->start
            : name_token->end - name_token->start;
        const unsigned char *public_name = namespace_token != NULL
            ? function->checker->source + name_token->start : binding->original;
        size_t public_length = namespace_token != NULL
            ? name_token->end - name_token->start : binding->original_length;
        if (binding->ast == function->checker->ast
            && binding->is_namespace == (namespace_token != NULL)
            && surface != NULL
            && mmay_checker_string_bytes_equal(
                surface->path, surface->path_length,
                (const unsigned char *)"std/convert", 11
            ) && mmay_checker_string_bytes_equal(
                binding->local, binding->local_length, local, local_length
            )) {
            MarimayTypeId type = mmay_checker_string_format_type(
                public_name, public_length
            );
            if (type != MMAY_TYPE_INVALID) return type;
        }
    }
    return MMAY_TYPE_INVALID;
}

bool mmay_checker_private_compile_convert_string_call(
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
    const MarimayAstNode *argument = NULL;
    MarimayTypeId source_type;
    MarimayExpressionValue source;
    size_t count = 0;
    size_t index;
    if (function == NULL || resolved == NULL || result == NULL) return false;
    *resolved = false;
    if (name_token == NULL && member != NULL
        && mmay_checker_private_member_parts(member, &receiver, &name_token)
        && receiver != NULL && receiver->kind == MMAY_SYNTAX_NAME_EXPRESSION) {
        namespace_token = mmay_checker_private_check_child_token(
            receiver, MMAY_TOKEN_IDENTIFIER, 0
        );
    }
    if (name_token == NULL || name_token->synthetic) return true;
    source_type = mmay_checker_string_imported_format(
        function, namespace_token, name_token
    );
    if (source_type == MMAY_TYPE_INVALID) return true;
    {
        const MarimaySyntaxToken *shadow = namespace_token != NULL
            ? namespace_token : name_token;
        MarimayCheckName local_name = {
            function->checker->ast->snapshot, shadow->start, shadow->end
        };
        if (mmay_checker_private_function_find_local(
                function, local_name
            ) != NULL) {
            return true;
        }
    }
    *resolved = true;
    if (mmay_checker_private_check_child_node(
            node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0
        ) != NULL) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "std/convert primitive formatter does not accept type arguments",
            node->start, node->end
        );
        return false;
    }
    for (index = 0; arguments != NULL && index < arguments->child_count; index += 1U) {
        const MarimayAstElement *element = mmay_ast_child(arguments, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE) {
            argument = element->as.node;
            count += 1U;
        }
    }
    if (count != 1U || argument == NULL) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_CALL_ARITY,
            "std/convert primitive formatter requires exactly one argument",
            node->start, node->end
        );
        return false;
    }
    memset(&source, 0, sizeof(source));
    if (!mmay_compile_expression(function, argument, source_type, &source)) {
        return false;
    }
    if (mmay_expression_is_never(function, &source)) {
        *result = source;
        return true;
    }
    if (source.type != source_type
        || (expected != MMAY_TYPE_INVALID && expected != MMAY_TYPE_STRING)) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "std/convert primitive formatter type does not match",
            node->start, node->end
        );
        return false;
    }
    return mmay_checker_string_format_value(function, node, source, result);
}

static const MarimayCheckImportBinding *mmay_checker_parse_binding(
    MarimayFunctionChecker *function,
    const MarimaySyntaxToken *namespace_token,
    const MarimaySyntaxToken *name_token,
    bool *explicit_radix
) {
    const MarimayCheckImportEnvironment *imports = function->checker->imports;
    size_t index;
    for (index = 0; imports != NULL && index < imports->binding_count;
        index += 1U) {
        const MarimayCheckImportBinding *binding = &imports->bindings[index];
        const MarimayCheckPackageSurface *surface = binding->surface_index
                < imports->surface_count
            ? imports->surfaces[binding->surface_index] : NULL;
        const unsigned char *local = function->checker->source
            + (namespace_token != NULL
                ? namespace_token->start : name_token->start);
        size_t local_length = namespace_token != NULL
            ? namespace_token->end - namespace_token->start
            : name_token->end - name_token->start;
        const unsigned char *original = namespace_token != NULL
            ? function->checker->source + name_token->start : binding->original;
        size_t original_length = namespace_token != NULL
            ? name_token->end - name_token->start : binding->original_length;
        bool plain = mmay_checker_string_bytes_equal(
            original, original_length,
            (const unsigned char *)"parseInt", 8
        );
        bool with_radix = mmay_checker_string_bytes_equal(
            original, original_length,
            (const unsigned char *)"parseIntRadix", 13
        );
        if (binding->ast == function->checker->ast
            && binding->is_namespace == (namespace_token != NULL)
            && surface != NULL && (plain || with_radix)
            && mmay_checker_string_bytes_equal(
                surface->path, surface->path_length,
                (const unsigned char *)"std/convert", 11
            ) && mmay_checker_string_bytes_equal(
                binding->local, binding->local_length, local, local_length
            )) {
            *explicit_radix = with_radix;
            return binding;
        }
    }
    return NULL;
}

static size_t mmay_checker_parse_arguments(
    const MarimayAstNode *arguments,
    const MarimayAstNode **nodes,
    size_t capacity
) {
    size_t count = 0;
    size_t index;
    for (index = 0; arguments != NULL && index < arguments->child_count;
        index += 1U) {
        const MarimayAstElement *element = mmay_ast_child(arguments, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE) {
            if (count < capacity) nodes[count] = element->as.node;
            count += 1U;
        }
    }
    return count;
}

bool mmay_checker_private_compile_convert_parse_call(
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
    const MarimayAstNode *argument_nodes[2] = {NULL, NULL};
    const MarimaySyntaxToken *namespace_token = NULL;
    const MarimaySyntaxToken *name_token = mmay_checker_private_check_child_token(
        callee, MMAY_TOKEN_IDENTIFIER, 0
    );
    const MarimayCheckImportBinding *binding;
    MarimayCheckName namespace_name = {0};
    MarimayCheckName function_name;
    MarimayCheckCallable callable;
    MarimayExpressionValue text;
    MarimayExpressionValue radix;
    const MarimayTypeDescriptor *result_descriptor;
    const MarimayCheckedEnum *error_enum;
    MarimayCheckEnumDeclaration *result_enum;
    MarimayCheckEnumDeclaration concrete_result;
    MarimayMirEnumId error_layout = MMAY_MIR_NO_ENUM;
    MarimayMirEnumId result_layout = MMAY_MIR_NO_ENUM;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    bool explicit_radix = false;
    size_t argument_count;
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) {
        return false;
    }
    *resolved = false;
    if (name_token == NULL && member != NULL
        && mmay_checker_private_member_parts(member, &receiver, &name_token)
        && receiver != NULL && receiver->kind == MMAY_SYNTAX_NAME_EXPRESSION) {
        namespace_token = mmay_checker_private_check_child_token(
            receiver, MMAY_TOKEN_IDENTIFIER, 0
        );
    }
    if (name_token == NULL || name_token->synthetic
        || (namespace_token != NULL && namespace_token->synthetic)) {
        return true;
    }
    binding = mmay_checker_parse_binding(
        function, namespace_token, name_token, &explicit_radix
    );
    if (binding == NULL) return true;
    {
        const MarimaySyntaxToken *shadow = namespace_token != NULL
            ? namespace_token : name_token;
        MarimayCheckName local_name = {
            function->checker->ast->snapshot, shadow->start, shadow->end
        };
        if (mmay_checker_private_function_find_local(
                function, local_name
            ) != NULL) {
            return true;
        }
    }
    *resolved = true;
    argument_count = mmay_checker_parse_arguments(
        arguments, argument_nodes, 2
    );
    if (mmay_checker_private_check_child_node(
            node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0
        ) != NULL || argument_count != (explicit_radix ? 2U : 1U)) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            argument_count != (explicit_radix ? 2U : 1U)
                ? MMAY_DIAGNOSTIC_CALL_ARITY : MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            explicit_radix
                ? "std/convert.parseIntRadix requires string and int arguments"
                : "std/convert.parseInt requires one string argument",
            node->start, node->end
        );
        return false;
    }
    memset(&callable, 0, sizeof(callable));
    function_name = (MarimayCheckName){
        function->checker->ast->snapshot, name_token->start, name_token->end
    };
    if (namespace_token != NULL) {
        namespace_name = (MarimayCheckName){
            function->checker->ast->snapshot,
            namespace_token->start, namespace_token->end
        };
    }
    if (mmay_check_resolve_imported_call(
            function->checker, namespace_name, namespace_token != NULL,
            function_name, false, MMAY_TYPE_INVALID, &callable
        ) != MMAY_CHECK_CALL_FOUND
        || callable.parameter_count != argument_count
        || callable.parameter_types == NULL || !callable.is_external
        || callable.parameter_types[0] != MMAY_TYPE_STRING
        || (explicit_radix && callable.parameter_types[1] != MMAY_TYPE_I64)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    memset(&text, 0, sizeof(text));
    memset(&radix, 0, sizeof(radix));
    if (!mmay_compile_expression(
            function, argument_nodes[0], MMAY_TYPE_STRING, &text
        )) return false;
    if (mmay_expression_is_never(function, &text)) {
        *result = text;
        return true;
    }
    if (explicit_radix) {
        if (!mmay_compile_expression(
                function, argument_nodes[1], MMAY_TYPE_I64, &radix
            )) return false;
        if (mmay_expression_is_never(function, &radix)) {
            *result = radix;
            return true;
        }
    } else if (!mmay_function_emit_instruction(
            function, MMAY_MIR_CONSTANT_I64, MMAY_TYPE_I64,
            MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER,
            10, false, node->start, node->end, &radix
        )) return false;
    result_descriptor = mmay_type_descriptor(
        &function->checker->result->types, callable.result_type
    );
    if (text.type != MMAY_TYPE_STRING || radix.type != MMAY_TYPE_I64
        || (expected != MMAY_TYPE_INVALID && expected != callable.result_type)
        || result_descriptor == NULL
        || result_descriptor->kind != MMAY_TYPE_KIND_NOMINAL
        || result_descriptor->argument_count != 2
        || result_descriptor->arguments[0] != MMAY_TYPE_I64) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "std/convert integer parser type does not match",
            node->start, node->end
        );
        return false;
    }
    error_enum = mmay_check_imported_enum_by_type(
        function->checker, result_descriptor->arguments[1]
    );
    result_enum = mmay_checker_private_checker_find_enum_by_type(
        function->checker, callable.result_type
    );
    if (error_enum == NULL || result_enum == NULL
        || !mmay_checker_private_imported_enum_layout(
            function, error_enum, result_descriptor->arguments[1], &error_layout
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    (void)error_layout;
    concrete_result = *result_enum;
    concrete_result.type = callable.result_type;
    if (!mmay_checker_private_function_enum_layout(
            function, &concrete_result, &result_layout
        ) || !mmay_function_add_register(
            function, callable.result_type, &target
        )) return false;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_PARSE_INTEGER;
    instruction.target = target;
    instruction.first = text.register_id;
    instruction.second = radix.register_id;
    instruction.enum_id = result_layout;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!mmay_function_append_instruction(function, &instruction)) return false;
    result->type = callable.result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}
