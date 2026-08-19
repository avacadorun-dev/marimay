#include "checker_expr_io_api.h"

#include "checker_core_api.h"
#include "checker_imported_nominal_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

#include <string.h>

static bool ruda_checker_io_bytes_equal(
    const void *left,
    size_t left_length,
    const void *right,
    size_t right_length
) {
    return left_length == right_length
        && (left_length == 0 || memcmp(left, right, left_length) == 0);
}

static size_t ruda_checker_io_arguments(
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

static const RudaCheckImportBinding *ruda_checker_io_binding(
    RudaFunctionChecker *function,
    const RudaSyntaxToken *namespace_token,
    const RudaSyntaxToken *name_token
) {
    static const unsigned char package_name[] = "std/io";
    static const unsigned char function_name[] = "println";
    const RudaCheckImportEnvironment *imports = function != NULL
        ? function->checker->imports : NULL;
    size_t index = 0;
    for (index = 0; imports != NULL && index < imports->binding_count;
        index += 1) {
        const RudaCheckImportBinding *binding = &imports->bindings[index];
        const RudaCheckPackageSurface *surface = binding->surface_index
                < imports->surface_count
            ? imports->surfaces[binding->surface_index] : NULL;
        const unsigned char *local = namespace_token != NULL
            ? function->checker->source + namespace_token->start
            : function->checker->source + name_token->start;
        size_t local_length = namespace_token != NULL
            ? namespace_token->end - namespace_token->start
            : name_token->end - name_token->start;
        const unsigned char *original = namespace_token != NULL
            ? function->checker->source + name_token->start
            : binding->original;
        size_t original_length = namespace_token != NULL
            ? name_token->end - name_token->start
            : binding->original_length;
        if (binding->ast == function->checker->ast
            && binding->is_namespace == (namespace_token != NULL)
            && surface != NULL
            && ruda_checker_io_bytes_equal(
                surface->path, surface->path_length,
                package_name, sizeof(package_name) - 1
            ) && ruda_checker_io_bytes_equal(
                binding->local, binding->local_length, local, local_length
            ) && ruda_checker_io_bytes_equal(
                original, original_length,
                function_name, sizeof(function_name) - 1
            )) {
            return binding;
        }
    }
    return NULL;
}

bool ruda_checker_private_compile_io_call(
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
    const RudaAstNode *argument_nodes[1] = {NULL};
    const RudaSyntaxToken *namespace_token = NULL;
    const RudaSyntaxToken *name_token = ruda_checker_private_check_child_token(
        callee, RUDA_TOKEN_IDENTIFIER, 0
    );
    const RudaCheckImportBinding *binding = NULL;
    RudaCheckName namespace_name = {0};
    RudaCheckName function_name = {0};
    RudaCheckCallable callable;
    RudaExpressionValue text;
    const RudaTypeDescriptor *result_descriptor = NULL;
    const RudaCheckedEnum *error_enum = NULL;
    RudaCheckEnumDeclaration *result_enum = NULL;
    RudaCheckEnumDeclaration concrete_result;
    RudaMirEnumId error_layout = RUDA_MIR_NO_ENUM;
    RudaMirEnumId result_layout = RUDA_MIR_NO_ENUM;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) {
        return false;
    }
    *resolved = false;
    if (name_token == NULL && member != NULL
        && ruda_checker_private_member_parts(member, &receiver, &name_token)
        && receiver != NULL
        && receiver->kind == RUDA_SYNTAX_NAME_EXPRESSION) {
        namespace_token = ruda_checker_private_check_child_token(
            receiver, RUDA_TOKEN_IDENTIFIER, 0
        );
    }
    if (name_token == NULL || name_token->synthetic
        || (namespace_token != NULL && namespace_token->synthetic)) {
        return true;
    }
    binding = ruda_checker_io_binding(function, namespace_token, name_token);
    if (binding == NULL) {
        return true;
    }
    {
        const RudaSyntaxToken *shadow = namespace_token != NULL
            ? namespace_token : name_token;
        RudaCheckName local_name = {
            function->checker->ast->snapshot, shadow->start, shadow->end
        };
        if (ruda_checker_private_function_find_local(
                function, local_name
            ) != NULL) {
            return true;
        }
    }
    *resolved = true;
    if (ruda_checker_private_check_child_node(
            node, RUDA_SYNTAX_TYPE_ARGUMENTS, 0
        ) != NULL || ruda_checker_io_arguments(
            arguments, argument_nodes, 1
        ) != 1) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            ruda_checker_private_check_child_node(
                    node, RUDA_SYNTAX_TYPE_ARGUMENTS, 0
                ) != NULL
                ? RUDA_DIAGNOSTIC_TYPE_MISMATCH
                : RUDA_DIAGNOSTIC_CALL_ARITY,
            "std/io.println requires exactly one string argument and no type arguments",
            node->start, node->end
        );
        return false;
    }
    memset(&callable, 0, sizeof(callable));
    function_name = (RudaCheckName){
        function->checker->ast->snapshot, name_token->start, name_token->end
    };
    if (namespace_token != NULL) {
        namespace_name = (RudaCheckName){
            function->checker->ast->snapshot,
            namespace_token->start,
            namespace_token->end
        };
    }
    if (ruda_check_resolve_imported_call(
            function->checker,
            namespace_name,
            namespace_token != NULL,
            function_name,
            false,
            RUDA_TYPE_INVALID,
            &callable
        ) != RUDA_CHECK_CALL_FOUND
        || callable.parameter_count != 1
        || callable.parameter_types == NULL
        || callable.parameter_types[0] != RUDA_TYPE_STRING
        || !callable.is_external) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    memset(&text, 0, sizeof(text));
    if (!ruda_compile_expression(
            function, argument_nodes[0], RUDA_TYPE_STRING, &text
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &text)) {
        *result = text;
        return true;
    }
    result_descriptor = ruda_type_descriptor(
        &function->checker->result->types, callable.result_type
    );
    if (text.type != RUDA_TYPE_STRING
        || (expected != RUDA_TYPE_INVALID && expected != callable.result_type)
        || result_descriptor == NULL
        || result_descriptor->kind != RUDA_TYPE_KIND_NOMINAL
        || result_descriptor->argument_count != 2
        || result_descriptor->arguments[0] != RUDA_TYPE_VOID) {
        (void)ruda_checker_add_diagnostic(
            function->checker, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "std/io.println argument or Result<void, io.Error> type does not match",
            node->start, node->end
        );
        return false;
    }
    error_enum = ruda_check_imported_enum_by_type(
        function->checker, result_descriptor->arguments[1]
    );
    result_enum = ruda_checker_private_checker_find_enum_by_type(
        function->checker, callable.result_type
    );
    if (error_enum == NULL || result_enum == NULL
        || !ruda_checker_private_imported_enum_layout(
            function,
            error_enum,
            result_descriptor->arguments[1],
            &error_layout
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    (void)error_layout;
    concrete_result = *result_enum;
    concrete_result.type = callable.result_type;
    if (!ruda_checker_private_function_enum_layout(
            function, &concrete_result, &result_layout
        ) || !ruda_function_add_register(
            function, callable.result_type, &target
        )) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_IO_PRINTLN;
    instruction.target = target;
    instruction.first = text.register_id;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.enum_id = result_layout;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = callable.result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}
