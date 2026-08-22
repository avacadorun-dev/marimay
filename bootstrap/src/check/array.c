#include "checker_internal.h"
#include "checker_function_context.h"
#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_generic_types_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ruda/generated/diagnostic_codes.h"

static bool ruda_array_member_parts(
    const RudaAstNode *node,
    const RudaAstNode **receiver,
    const RudaSyntaxToken **member
);

static const RudaAstNode *ruda_array_child_node(
    const RudaAstNode *node,
    RudaSyntaxKind kind,
    size_t ordinal
) {
    size_t index = 0;
    size_t found = 0;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node != NULL && element->as.node->kind == kind) {
            if (found == ordinal) {
                return element->as.node;
            }
            found += 1;
        }
    }
    return NULL;
}

static const RudaSyntaxToken *ruda_array_child_token(
    const RudaAstNode *node,
    RudaTokenKind kind,
    size_t ordinal
) {
    size_t index = 0;
    size_t found = 0;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_TOKEN
            && element->as.token.kind == kind) {
            if (found == ordinal) {
                return &element->as.token;
            }
            found += 1;
        }
    }
    return NULL;
}

static bool ruda_array_text_equal(
    const RudaChecker *checker,
    const RudaSyntaxToken *token,
    const char *text
) {
    size_t length = text != NULL ? strlen(text) : 0;
    return checker != NULL && token != NULL && !token->synthetic
        && token->start <= token->end && token->end <= checker->source_length
        && token->end - token->start == length
        && memcmp(checker->source + token->start, text, length) == 0;
}

static bool ruda_array_report(
    RudaFunctionChecker *function,
    const char *code,
    const char *message,
    size_t start,
    size_t end
) {
    return ruda_checker_add_diagnostic(
        function->checker,
        code,
        message,
        start,
        end
    );
}

static RudaCheckEnumDeclaration *ruda_array_prelude_enum(
    RudaFunctionChecker *function,
    const char *name
) {
    RudaSymbolId symbol;
    size_t index = 0;
    if (function == NULL || name == NULL
        || !ruda_symbol_id_from_bytes(
            "std", 3, "prelude", 7,
            (const unsigned char *)name, strlen(name), 0, &symbol
        )) {
        return NULL;
    }
    for (index = 0; index < function->checker->prelude_enums.length; index += 1) {
        RudaCheckEnumDeclaration *declaration = ruda_vector_at(
            &function->checker->prelude_enums,
            index
        );
        if (declaration != NULL
            && ruda_symbol_id_equal(declaration->symbol, symbol)) {
            return declaration;
        }
    }
    return NULL;
}

static bool ruda_array_push_result(
    RudaFunctionChecker *function,
    size_t start,
    size_t end,
    RudaExpressionValue *result
) {
    RudaCheckEnumDeclaration *result_base = NULL;
    RudaCheckEnumDeclaration *error_base = NULL;
    RudaCheckEnumDeclaration concrete_result;
    RudaVector arguments;
    RudaTypeId result_type = RUDA_TYPE_INVALID;
    RudaTypeId generic_arguments[2];
    RudaExpressionValue emitted;
    bool initialized = false;
    if (function == NULL || result == NULL) {
        return false;
    }
    result_base = ruda_array_prelude_enum(function, "Result");
    error_base = ruda_array_prelude_enum(function, "ArrayGrowthError");
    if (result_base == NULL || error_base == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    memset(&concrete_result, 0, sizeof(concrete_result));
    memset(&arguments, 0, sizeof(arguments));
    memset(&emitted, 0, sizeof(emitted));
    generic_arguments[0] = RUDA_TYPE_VOID;
    generic_arguments[1] = error_base->type;
    if (!ruda_vector_init(
            &arguments,
            function->checker->result->allocator,
            sizeof(RudaTypeId)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    initialized = true;
    if (!ruda_vector_push(&arguments, &generic_arguments[0])
        || !ruda_vector_push(&arguments, &generic_arguments[1])
        || !ruda_checker_private_intern_generic_nominal(
            function->checker,
            RUDA_TYPE_KIND_NOMINAL,
            result_base->symbol,
            &arguments,
            &result_type
        )) {
        if (!ruda_checker_has_failed(function->checker)) {
            ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        }
        goto cleanup;
    }
    concrete_result = *result_base;
    concrete_result.type = result_type;
    if (concrete_result.variants.length == 0
        || !ruda_checker_private_function_emit_enum_construction(
            function,
            &concrete_result,
            0,
            NULL,
            0,
            start,
            end,
            &emitted
        )) {
        goto cleanup;
    }
    *result = emitted;
cleanup:
    if (initialized) {
        ruda_vector_dispose(&arguments);
    }
    return result->valid && !ruda_checker_has_failed(function->checker);
}

bool ruda_array_layout(
    RudaFunctionChecker *function,
    RudaTypeId type,
    RudaMirArrayId *result
) {
    RudaTypeId element_type = RUDA_TYPE_INVALID;
    RudaTypeId nested_element_type = RUDA_TYPE_INVALID;
    RudaMirArrayId nested = RUDA_MIR_NO_ARRAY;
    size_t index = 0;
    if (function == NULL || result == NULL || !function->mir.initialized
        || !ruda_type_array_element(
            &function->checker->result->types,
            type,
            &element_type
        )) {
        if (function != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    if (ruda_type_array_element(
            &function->checker->result->types,
            element_type,
            &nested_element_type
        ) && !ruda_array_layout(function, element_type, &nested)) {
        return false;
    }
    for (index = 0; index < function->mir.arrays.length; index += 1) {
        const RudaMirArray *existing = ruda_vector_at_const(
            &function->mir.arrays,
            index
        );
        if (existing != NULL && existing->type == type) {
            if (existing->element_type != element_type || index > UINT32_MAX) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                return false;
            }
            *result = (RudaMirArrayId)index;
            return true;
        }
    }
    if (function->mir.arrays.length >= function->mir.options.max_array_layouts
        || function->mir.arrays.length >= UINT32_MAX) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!ruda_mir_add_array(&function->mir, type, element_type, result)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    (void)nested_element_type;
    (void)nested;
    return true;
}

bool ruda_function_ensure_array_layout(
    RudaFunctionChecker *function,
    RudaTypeId type
) {
    RudaTypeId element_type = RUDA_TYPE_INVALID;
    RudaMirArrayId array = RUDA_MIR_NO_ARRAY;
    if (function == NULL) {
        return false;
    }
    if (!ruda_type_array_element(
            &function->checker->result->types,
            type,
            &element_type
        )) {
        return true;
    }
    (void)element_type;
    return ruda_array_layout(function, type, &array);
}

static bool ruda_array_intern_type(
    RudaFunctionChecker *function,
    RudaTypeId element_type,
    RudaTypeId *result
) {
    RudaTypeTable *types = &function->checker->result->types;
    if (!ruda_type_intern_array(types, element_type, result)) {
        ruda_checker_fail(
            function->checker,
            types->descriptors.length
                    >= function->checker->options.type_options.max_types
                        - RUDA_TYPE_FIRST_DYNAMIC
                ? RUDA_CHECK_LIMIT_REACHED
                : RUDA_CHECK_ALLOCATION_FAILED
        );
        return false;
    }
    return true;
}

bool ruda_compile_array_literal(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
) {
    RudaMirArrayArgument *arguments = NULL;
    RudaTypeId array_type = expected;
    RudaTypeId element_type = RUDA_TYPE_INVALID;
    RudaMirArrayId array = RUDA_MIR_NO_ARRAY;
    RudaMirArrayConstructionId construction = RUDA_MIR_NO_ARRAY_CONSTRUCTION;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    size_t argument_count = 0;
    size_t argument_index = 0;
    size_t argument_bytes = 0;
    size_t child_index = 0;
    size_t new_argument_count = 0;
    bool accepted = false;
    if (function == NULL || node == NULL || result == NULL
        || node->kind != RUDA_SYNTAX_ARRAY_EXPRESSION) {
        if (function != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    if (function->constant_evaluation) {
        (void)ruda_array_report(
            function,
            RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
            "managed arrays are not available in constant expressions",
            node->start,
            node->end
        );
        return false;
    }
    if (expected != RUDA_TYPE_INVALID && !ruda_type_array_element(
            &function->checker->result->types,
            expected,
            &element_type
        )) {
        (void)ruda_array_report(
            function,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "array literal does not match the required type",
            node->start,
            node->end
        );
        return false;
    }
    for (child_index = 0; child_index < node->child_count; child_index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, child_index);
        if (element == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        if (element->kind == RUDA_AST_ELEMENT_NODE) {
            if (element->as.node == NULL
                || element->as.node->kind != RUDA_SYNTAX_EXPRESSION
                || argument_count == SIZE_MAX) {
                ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
                goto cleanup;
            }
            argument_count += 1;
        } else if (element->as.token.synthetic) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
    }
    if (argument_count == 0 && expected == RUDA_TYPE_INVALID) {
        (void)ruda_array_report(
            function,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "empty array literal requires an expected []T type",
            node->start,
            node->end
        );
        goto cleanup;
    }
    if (function->mir.array_constructions.length
            >= function->mir.options.max_array_constructions
        || !ruda_size_add(
            function->mir.array_arguments.length,
            argument_count,
            &new_argument_count
        ) || new_argument_count > function->mir.options.max_array_arguments) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (!ruda_size_multiply(
            argument_count,
            sizeof(RudaMirArrayArgument),
            &argument_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (argument_bytes != 0) {
        arguments = ruda_allocate(
            &function->checker->result->allocator,
            argument_bytes
        );
        if (arguments == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    for (child_index = 0; child_index < node->child_count; child_index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, child_index);
        RudaExpressionValue value;
        if (element == NULL || element->kind != RUDA_AST_ELEMENT_NODE) {
            continue;
        }
        memset(&value, 0, sizeof(value));
        value.register_id = RUDA_MIR_NO_REGISTER;
        if (!ruda_compile_expression(
                function,
                element->as.node,
                element_type,
                &value
            )) {
            goto cleanup;
        }
        if (ruda_expression_is_never(function, &value)) {
            *result = value;
            accepted = true;
            goto cleanup;
        }
        if (element_type == RUDA_TYPE_INVALID) {
            element_type = value.type;
            if (!ruda_array_intern_type(
                    function,
                    element_type,
                    &array_type
                )) {
                goto cleanup;
            }
        }
        if (value.type != element_type) {
            (void)ruda_array_report(
                function,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "array literal elements must have one exact type",
                element->as.node->start,
                element->as.node->end
            );
            goto cleanup;
        }
        if (arguments == NULL || argument_index >= argument_count) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        arguments[argument_index].register_id = value.register_id;
        arguments[argument_index].type = value.type;
        argument_index += 1;
    }
    if (expected != RUDA_TYPE_INVALID) {
        array_type = expected;
    } else if (array_type == RUDA_TYPE_INVALID) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (argument_index != argument_count
        || !ruda_array_layout(function, array_type, &array)) {
        if (argument_index != argument_count) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        }
        goto cleanup;
    }
    if (function->mir.array_constructions.length
            >= function->mir.options.max_array_constructions
        || !ruda_size_add(
            function->mir.array_arguments.length,
            argument_count,
            &new_argument_count
        ) || new_argument_count > function->mir.options.max_array_arguments) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (!ruda_function_add_register(function, array_type, &target)
        || !ruda_mir_add_array_construction(
            &function->mir,
            array,
            arguments,
            argument_count,
            &construction
        )) {
        if (!ruda_checker_has_failed(function->checker)) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
        }
        goto cleanup;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_NEW;
    instruction.target = target;
    instruction.first = RUDA_MIR_NO_REGISTER;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.third = RUDA_MIR_NO_REGISTER;
    instruction.array = array;
    instruction.array_construction = construction;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        goto cleanup;
    }
    result->type = array_type;
    result->register_id = target;
    result->valid = true;
    accepted = true;

cleanup:
    ruda_deallocate(
        &function->checker->result->allocator,
        arguments,
        argument_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}

bool ruda_compile_array_builtin_call(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaAstNode *precompiled_receiver_node,
    const RudaExpressionValue *precompiled_receiver,
    bool *resolved,
    RudaExpressionValue *result
) {
    const RudaAstNode *callee = NULL;
    const RudaAstNode *arguments = NULL;
    const RudaAstNode *type_arguments = NULL;
    const RudaSyntaxToken *name = NULL;
    const RudaAstNode *receiver_node = NULL;
    RudaVector explicit_types;
    RudaTypeId array_type = RUDA_TYPE_INVALID;
    RudaTypeId element_type = RUDA_TYPE_INVALID;
    RudaAstNode *array_node = NULL;
    size_t index = 0;
    size_t array_node_child_count = 0;
    size_t array_node_bytes = 0;
    bool initialized = false;
    bool dot_call = false;
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) {
        if (function != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    *resolved = false;
    callee = ruda_array_child_node(node, RUDA_SYNTAX_NAME_EXPRESSION, 0);
    arguments = ruda_array_child_node(node, RUDA_SYNTAX_ARGUMENTS, 0);
    type_arguments = ruda_array_child_node(node, RUDA_SYNTAX_TYPE_ARGUMENTS, 0);
    name = ruda_array_child_token(callee, RUDA_TOKEN_IDENTIFIER, 0);
    if (callee == NULL) {
        callee = ruda_array_child_node(node, RUDA_SYNTAX_MEMBER_EXPRESSION, 0);
        if (callee != NULL && ruda_array_member_parts(
                callee, &receiver_node, &name
            )) {
            dot_call = true;
            if (receiver_node->kind == RUDA_SYNTAX_NAME_EXPRESSION) {
                const RudaSyntaxToken *namespace_token =
                    ruda_array_child_token(
                        receiver_node, RUDA_TOKEN_IDENTIFIER, 0
                    );
                if (namespace_token != NULL && !namespace_token->synthetic
                    && ruda_check_import_namespace_exists(
                        function->checker,
                        function->checker->source + namespace_token->start,
                        namespace_token->end - namespace_token->start
                    )) {
                    return true;
                }
            }
        }
    }
    if (!ruda_array_text_equal(function->checker, name, "arrayOf")
        && !ruda_array_text_equal(function->checker, name, "push")) {
        return true;
    }
    if (arguments == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (precompiled_receiver == NULL
        && (ruda_checker_private_function_find_local(function, (RudaCheckName){
            function->checker->ast->snapshot, name->start, name->end
        }) != NULL || ruda_checker_private_checker_find_top_level(
            function->checker,
            (RudaCheckName){function->checker->ast->snapshot, name->start, name->end}
        ) != NULL)) {
        return true;
    }
    if (ruda_array_text_equal(function->checker, name, "push")) {
        RudaExpressionValue receiver;
        RudaExpressionValue value;
        RudaTypeId element = RUDA_TYPE_INVALID;
        RudaMirArrayId array = RUDA_MIR_NO_ARRAY;
        RudaMirRegister target = RUDA_MIR_NO_REGISTER;
        RudaMirInstruction instruction;
        size_t argument_count = 0;
        for (index = 0; index < arguments->child_count; index += 1) {
            const RudaAstElement *element_node = ruda_ast_child(arguments, index);
            if (element_node != NULL
                && element_node->kind == RUDA_AST_ELEMENT_NODE) {
                argument_count += 1;
            }
        }
        if ((dot_call && argument_count != 1)
            || (!dot_call && argument_count != 2)
            || (receiver_node == NULL && dot_call)) {
            (void)ruda_array_report(
                function, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "push expects an array and one exact element",
                node->start, node->end
            );
            *resolved = true;
            return false;
        }
        memset(&receiver, 0, sizeof(receiver));
        memset(&value, 0, sizeof(value));
        if (dot_call) {
            const RudaAstNode *argument = NULL;
            for (index = 0; index < arguments->child_count; index += 1) {
                const RudaAstElement *item = ruda_ast_child(arguments, index);
                if (item != NULL && item->kind == RUDA_AST_ELEMENT_NODE) {
                    argument = item->as.node;
                    break;
                }
            }
            if (precompiled_receiver != NULL) {
                if (precompiled_receiver_node != receiver_node
                    || !precompiled_receiver->valid) {
                    ruda_checker_fail(
                        function->checker, RUDA_CHECK_INVALID_ARGUMENT
                    );
                    return false;
                }
                receiver = *precompiled_receiver;
            } else if (!ruda_compile_expression(
                    function, receiver_node, RUDA_TYPE_INVALID, &receiver
                )) {
                return false;
            }
            if (!ruda_type_array_element(
                    &function->checker->result->types, receiver.type, &element
                ) || !ruda_compile_expression(function, argument, element, &value)) {
                return false;
            }
        } else {
            const RudaAstNode *first = NULL;
            const RudaAstNode *second = NULL;
            size_t found = 0;
            for (index = 0; index < arguments->child_count; index += 1) {
                const RudaAstElement *item = ruda_ast_child(arguments, index);
                if (item != NULL && item->kind == RUDA_AST_ELEMENT_NODE) {
                    if (found++ == 0) first = item->as.node;
                    else second = item->as.node;
                }
            }
            if (precompiled_receiver != NULL) {
                if (precompiled_receiver_node != first
                    || !precompiled_receiver->valid) {
                    ruda_checker_fail(
                        function->checker, RUDA_CHECK_INVALID_ARGUMENT
                    );
                    return false;
                }
                receiver = *precompiled_receiver;
            } else if (!ruda_compile_expression(
                    function, first, RUDA_TYPE_INVALID, &receiver
                )) {
                return false;
            }
            if (!ruda_type_array_element(
                    &function->checker->result->types, receiver.type, &element
                ) || !ruda_compile_expression(function, second, element, &value)) {
                return false;
            }
        }
        if (receiver.type < RUDA_TYPE_FIRST_DYNAMIC || value.type != element
            || !ruda_array_layout(function, receiver.type, &array)
            || !ruda_function_add_register(function, receiver.type, &target)) {
            (void)ruda_array_report(
                function, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "push requires []T and an exact T value",
                node->start, node->end
            );
            return false;
        }
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_ARRAY_PUSH;
        instruction.target = target;
        instruction.first = receiver.register_id;
        instruction.second = value.register_id;
        instruction.third = RUDA_MIR_NO_REGISTER;
        instruction.array = array;
        instruction.span.start = node->start;
        instruction.span.end = node->end;
        if (!ruda_function_append_instruction(function, &instruction)) {
            return false;
        }
        if (!ruda_array_push_result(function, node->start, node->end, result)) {
            return false;
        }
        *resolved = true;
        return true;
    }
    if (dot_call) {
        return true;
    }
    if (ruda_checker_private_function_find_local(function, (RudaCheckName){
            function->checker->ast->snapshot, name->start, name->end
        }) != NULL || ruda_checker_private_checker_find_top_level(
            function->checker,
            (RudaCheckName){function->checker->ast->snapshot, name->start, name->end}
        ) != NULL) {
        return true;
    }
    *resolved = true;
    if (arguments == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    memset(&explicit_types, 0, sizeof(explicit_types));
    if (!ruda_vector_init(
            &explicit_types, function->checker->result->allocator,
            sizeof(RudaTypeId)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    initialized = true;
    if (type_arguments != NULL
        && !ruda_checker_private_parse_type_arguments(
            function->checker, type_arguments, &explicit_types
        )) {
        goto cleanup;
    }
    if (explicit_types.length > 1) {
        (void)ruda_array_report(
            function, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "arrayOf accepts exactly one element type", node->start, node->end
        );
        goto cleanup;
    }
    for (index = 0; index < arguments->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(arguments, index);
        if (element == NULL || (element->kind == RUDA_AST_ELEMENT_NODE
                && element->as.node == NULL)) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        if (element->kind == RUDA_AST_ELEMENT_TOKEN
            && element->as.token.synthetic) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
    }
    if (explicit_types.length == 1) {
        const RudaTypeId *type = ruda_vector_at_const(&explicit_types, 0);
        element_type = type != NULL ? *type : RUDA_TYPE_INVALID;
        if (element_type == RUDA_TYPE_INVALID
            || !ruda_array_intern_type(function, element_type, &array_type)) {
            goto cleanup;
        }
    } else {
        for (index = 0; index < arguments->child_count; index += 1) {
            const RudaAstElement *element = ruda_ast_child(arguments, index);
            if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE) {
                array_node_child_count += 1;
            }
        }
        if (array_node_child_count == 0) {
            (void)ruda_array_report(
                function, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "empty arrayOf requires an explicit element type",
                node->start, node->end
            );
            goto cleanup;
        }
    }
    array_node_child_count = 0;
    for (index = 0; index < arguments->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(arguments, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE) {
            if (array_node_child_count == SIZE_MAX) {
                ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
                goto cleanup;
            }
            array_node_child_count += 1;
        }
    }
    if (!ruda_size_multiply(
            array_node_child_count, sizeof(RudaAstElement), &array_node_bytes
        ) || !ruda_size_add(
            offsetof(RudaAstNode, children), array_node_bytes, &array_node_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    array_node = ruda_allocate(
        &function->checker->result->allocator,
        array_node_bytes
    );
    if (array_node == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    array_node->kind = RUDA_SYNTAX_ARRAY_EXPRESSION;
    array_node->start = arguments->start;
    array_node->end = arguments->end;
    array_node->child_count = array_node_child_count;
    array_node->has_span = arguments->has_span;
    array_node_child_count = 0;
    for (index = 0; index < arguments->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(arguments, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE) {
            array_node->children[array_node_child_count] = *element;
            array_node_child_count += 1;
        }
    }
    if (!ruda_compile_array_literal(
        function, array_node, array_type, result
    )) {
        goto cleanup;
    }
    if (element_type != RUDA_TYPE_INVALID && result->type != array_type) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
cleanup:
    ruda_deallocate(
        &function->checker->result->allocator,
        array_node,
        array_node_bytes
    );
    if (initialized) {
        ruda_vector_dispose(&explicit_types);
    }
    return *resolved && result->valid && !ruda_checker_has_failed(function->checker);
}

static bool ruda_array_index_parts(
    const RudaAstNode *node,
    const RudaAstNode **receiver,
    const RudaAstNode **index
) {
    const RudaAstNode *group = NULL;
    size_t child_index = 0;
    *receiver = NULL;
    *index = NULL;
    for (child_index = 0; node != NULL && child_index < node->child_count;
        child_index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, child_index);
        if (element == NULL) {
            return false;
        }
        if (element->kind != RUDA_AST_ELEMENT_NODE) {
            if (element->as.token.synthetic) {
                return false;
            }
            continue;
        }
        if (element->as.node != NULL
            && element->as.node->kind == RUDA_SYNTAX_BRACKET_GROUP) {
            if (group != NULL) {
                return false;
            }
            group = element->as.node;
        } else if (*receiver == NULL) {
            *receiver = element->as.node;
        } else {
            return false;
        }
    }
    if (group != NULL) {
        *index = ruda_array_child_node(group, RUDA_SYNTAX_EXPRESSION, 0);
    }
    return *receiver != NULL && *index != NULL
        && ruda_array_child_node(group, RUDA_SYNTAX_EXPRESSION, 1) == NULL;
}

static bool ruda_array_resolve_access(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *receiver,
    RudaExpressionValue *index,
    RudaTypeId *element_type,
    RudaMirArrayId *array
) {
    const RudaAstNode *receiver_node = NULL;
    const RudaAstNode *index_node = NULL;
    if (!ruda_array_index_parts(node, &receiver_node, &index_node)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    memset(receiver, 0, sizeof(*receiver));
    memset(index, 0, sizeof(*index));
    receiver->register_id = RUDA_MIR_NO_REGISTER;
    index->register_id = RUDA_MIR_NO_REGISTER;
    if (!ruda_compile_expression(
            function,
            receiver_node,
            RUDA_TYPE_INVALID,
            receiver
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, receiver)) {
        return true;
    }
    if (!ruda_type_array_element(
            &function->checker->result->types,
            receiver->type,
            element_type
        )) {
        (void)ruda_array_report(
            function,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "indexing requires a []T receiver",
            receiver_node->start,
            receiver_node->end
        );
        return false;
    }
    if (!ruda_compile_expression(
            function,
            index_node,
            RUDA_TYPE_I64,
            index
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, index)) {
        return true;
    }
    if (index->type != RUDA_TYPE_I64) {
        (void)ruda_array_report(
            function,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "array index must have exact type i64",
            index_node->start,
            index_node->end
        );
        return false;
    }
    return ruda_array_layout(function, receiver->type, array);
}

bool ruda_compile_array_index(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *result
) {
    RudaExpressionValue receiver;
    RudaExpressionValue index;
    RudaTypeId element_type = RUDA_TYPE_INVALID;
    RudaMirArrayId array = RUDA_MIR_NO_ARRAY;
    RudaMirRegister checked = RUDA_MIR_NO_REGISTER;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    if (function == NULL || node == NULL || result == NULL
        || node->kind != RUDA_SYNTAX_INDEX_EXPRESSION) {
        if (function != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    if (function->constant_evaluation) {
        (void)ruda_array_report(
            function,
            RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
            "managed arrays are not available in constant expressions",
            node->start,
            node->end
        );
        return false;
    }
    if (!ruda_array_resolve_access(
            function,
            node,
            &receiver,
            &index,
            &element_type,
            &array
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &receiver)) {
        *result = receiver;
        return true;
    }
    if (ruda_expression_is_never(function, &index)) {
        *result = index;
        return true;
    }
    if (!ruda_function_add_register(function, receiver.type, &checked)
        || !ruda_function_add_register(function, element_type, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_REQUIRE_INDEX;
    instruction.target = checked;
    instruction.first = receiver.register_id;
    instruction.second = index.register_id;
    instruction.third = RUDA_MIR_NO_REGISTER;
    instruction.array = array;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_GET;
    instruction.target = target;
    instruction.first = checked;
    instruction.second = index.register_id;
    instruction.third = RUDA_MIR_NO_REGISTER;
    instruction.array = array;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = element_type;
    result->register_id = target;
    result->valid = true;
    return true;
}

static bool ruda_array_member_parts(
    const RudaAstNode *node,
    const RudaAstNode **receiver,
    const RudaSyntaxToken **member
) {
    size_t index = 0;
    *receiver = NULL;
    *member = NULL;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element == NULL) {
            return false;
        }
        if (element->kind == RUDA_AST_ELEMENT_NODE) {
            if (*receiver != NULL) {
                return false;
            }
            *receiver = element->as.node;
        } else if (element->as.token.kind == RUDA_TOKEN_IDENTIFIER) {
            if (*member != NULL) {
                return false;
            }
            *member = &element->as.token;
        }
    }
    return *receiver != NULL && *member != NULL && !(*member)->synthetic;
}

bool ruda_compile_array_length_call(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    bool builtin_available,
    const RudaAstNode *precompiled_receiver_node,
    const RudaExpressionValue *precompiled_receiver,
    bool *resolved,
    RudaExpressionValue *result
) {
    const RudaAstNode *callee = NULL;
    const RudaAstNode *arguments_node = NULL;
    const RudaAstNode *receiver_node = NULL;
    const RudaSyntaxToken *name = NULL;
    const RudaAstNode *argument_node = NULL;
    RudaExpressionValue receiver;
    RudaTypeId element_type = RUDA_TYPE_INVALID;
    RudaMirArrayId array = RUDA_MIR_NO_ARRAY;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    size_t index = 0;
    size_t argument_count = 0;
    bool dot_call = false;
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) {
        if (function != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    *resolved = false;
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node != NULL) {
            if (element->as.node->kind == RUDA_SYNTAX_ARGUMENTS) {
                arguments_node = element->as.node;
            } else if (element->as.node->kind != RUDA_SYNTAX_TYPE_ARGUMENTS
                && callee == NULL) {
                callee = element->as.node;
            }
        }
    }
    if (callee != NULL && callee->kind == RUDA_SYNTAX_NAME_EXPRESSION) {
        name = ruda_array_child_token(callee, RUDA_TOKEN_IDENTIFIER, 0);
    } else if (callee != NULL
        && callee->kind == RUDA_SYNTAX_MEMBER_EXPRESSION
        && ruda_array_member_parts(callee, &receiver_node, &name)) {
        dot_call = true;
        if (receiver_node->kind == RUDA_SYNTAX_NAME_EXPRESSION) {
            const RudaSyntaxToken *namespace_token = ruda_array_child_token(
                receiver_node, RUDA_TOKEN_IDENTIFIER, 0
            );
            if (namespace_token != NULL && !namespace_token->synthetic
                && ruda_check_import_namespace_exists(
                    function->checker,
                    function->checker->source + namespace_token->start,
                    namespace_token->end - namespace_token->start
                )) {
                return true;
            }
        }
    }
    if (!ruda_array_text_equal(function->checker, name, "length")) {
        return true;
    }
    if (!builtin_available) {
        return true;
    }
    *resolved = true;
    if (ruda_array_child_node(node, RUDA_SYNTAX_TYPE_ARGUMENTS, 0) != NULL) {
        (void)ruda_array_report(
            function,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "length does not accept type arguments",
            node->start,
            node->end
        );
        return false;
    }
    if (arguments_node == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    for (index = 0; index < arguments_node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(arguments_node, index);
        if (element == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (element->kind == RUDA_AST_ELEMENT_NODE) {
            if (element->as.node == NULL
                || element->as.node->kind != RUDA_SYNTAX_EXPRESSION
                || argument_count == SIZE_MAX) {
                ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
                return false;
            }
            argument_node = element->as.node;
            argument_count += 1;
        } else if (element->as.token.synthetic) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
    }
    if ((dot_call && argument_count != 0)
        || (!dot_call && argument_count != 1)) {
        (void)ruda_array_report(
            function,
            RUDA_DIAGNOSTIC_CALL_ARITY,
            "length requires exactly one array value",
            arguments_node->start,
            arguments_node->end
        );
        return false;
    }
    if (!dot_call) {
        receiver_node = argument_node;
    }
    memset(&receiver, 0, sizeof(receiver));
    receiver.register_id = RUDA_MIR_NO_REGISTER;
    if (precompiled_receiver != NULL) {
        if (precompiled_receiver_node != receiver_node
            || !precompiled_receiver->valid) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        receiver = *precompiled_receiver;
    } else {
        if (!ruda_compile_expression(
                function,
                receiver_node,
                RUDA_TYPE_INVALID,
                &receiver
            )) {
            return false;
        }
    }
    if (ruda_expression_is_never(function, &receiver)) {
        *result = receiver;
        return true;
    }
    if (!ruda_type_array_element(
            &function->checker->result->types,
            receiver.type,
            &element_type
        )) {
        (void)ruda_array_report(
            function,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "length requires a []T value",
            receiver_node->start,
            receiver_node->end
        );
        return false;
    }
    if (!ruda_array_layout(function, receiver.type, &array)
        || !ruda_function_add_register(function, RUDA_TYPE_I64, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_LENGTH;
    instruction.target = target;
    instruction.first = receiver.register_id;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.third = RUDA_MIR_NO_REGISTER;
    instruction.array = array;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = RUDA_TYPE_I64;
    result->register_id = target;
    result->valid = true;
    (void)element_type;
    return true;
}

bool ruda_compile_array_assignment(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaAstNode *left,
    const RudaAstNode *right,
    const RudaSyntaxToken *operator_token,
    bool compound
) {
    RudaExpressionValue receiver;
    RudaExpressionValue index;
    RudaExpressionValue value;
    RudaExpressionValue combined;
    RudaTypeId element_type = RUDA_TYPE_INVALID;
    RudaMirArrayId array = RUDA_MIR_NO_ARRAY;
    RudaMirRegister frozen_index = RUDA_MIR_NO_REGISTER;
    RudaMirRegister checked = RUDA_MIR_NO_REGISTER;
    RudaMirRegister previous = RUDA_MIR_NO_REGISTER;
    RudaMirRegister stored = RUDA_MIR_NO_REGISTER;
    RudaMirInstructionKind compound_kind = RUDA_MIR_COPY;
    RudaMirInstruction instruction;
    RudaMirRegister stored_value = RUDA_MIR_NO_REGISTER;
    if (function == NULL || node == NULL || left == NULL || right == NULL
        || operator_token == NULL
        || left->kind != RUDA_SYNTAX_INDEX_EXPRESSION) {
        if (function != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    if (function->constant_evaluation) {
        (void)ruda_array_report(
            function,
            RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
            "managed arrays are not available in constant expressions",
            node->start,
            node->end
        );
        return false;
    }
    if (!ruda_array_resolve_access(
            function,
            left,
            &receiver,
            &index,
            &element_type,
            &array
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &receiver)
        || ruda_expression_is_never(function, &index)) {
        return true;
    }
    if (compound && !ruda_compound_assignment_kind(
            element_type,
            operator_token->kind,
            &compound_kind
        )) {
        (void)ruda_array_report(
            function,
            RUDA_DIAGNOSTIC_INVALID_OPERATOR,
            "operator is not defined for the array element type",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    if (!ruda_function_add_register(function, RUDA_TYPE_I64, &frozen_index)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_COPY;
    instruction.target = frozen_index;
    instruction.first = index.register_id;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.third = RUDA_MIR_NO_REGISTER;
    instruction.span.start = left->start;
    instruction.span.end = left->end;
    if (!ruda_function_append_instruction(function, &instruction)
        || !ruda_function_add_register(function, receiver.type, &checked)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_REQUIRE_INDEX;
    instruction.target = checked;
    instruction.first = receiver.register_id;
    instruction.second = frozen_index;
    instruction.third = RUDA_MIR_NO_REGISTER;
    instruction.array = array;
    instruction.span.start = left->start;
    instruction.span.end = left->end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    if (compound) {
        if (!ruda_function_add_register(function, element_type, &previous)) {
            return false;
        }
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_ARRAY_GET;
        instruction.target = previous;
        instruction.first = checked;
        instruction.second = frozen_index;
        instruction.third = RUDA_MIR_NO_REGISTER;
        instruction.array = array;
        instruction.span.start = left->start;
        instruction.span.end = left->end;
        if (!ruda_function_append_instruction(function, &instruction)) {
            return false;
        }
    }
    memset(&value, 0, sizeof(value));
    memset(&combined, 0, sizeof(combined));
    value.register_id = RUDA_MIR_NO_REGISTER;
    combined.register_id = RUDA_MIR_NO_REGISTER;
    if (!ruda_compile_expression(function, right, element_type, &value)) {
        return false;
    }
    if (ruda_expression_is_never(function, &value)) {
        return true;
    }
    if (value.type != element_type) {
        (void)ruda_array_report(
            function,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "assigned value type does not match the array element type",
            right->start,
            right->end
        );
        return false;
    }
    if (compound && !ruda_function_emit_instruction(
            function,
            compound_kind,
            element_type,
            previous,
            value.register_id,
            0,
            false,
            node->start,
            node->end,
            &combined
        )) {
        return false;
    }
    stored_value = compound ? combined.register_id : value.register_id;
    if (!ruda_function_add_register(function, element_type, &stored)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_SET;
    instruction.target = stored;
    instruction.first = checked;
    instruction.second = frozen_index;
    instruction.third = stored_value;
    instruction.array = array;
    instruction.span.start = left->start;
    instruction.span.end = left->end;
    return ruda_function_append_instruction(function, &instruction);
}
