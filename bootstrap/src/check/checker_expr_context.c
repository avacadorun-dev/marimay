#include "checker_expr_context_api.h"

#include <string.h>

#include "checker_core_api.h"
#include "checker_imported_nominal_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

typedef struct MarimayContextPrimitive {
    const char *package;
    const char *name;
    MarimayMirContextOperation operation;
    size_t argument_count;
} MarimayContextPrimitive;

static const MarimayContextPrimitive *mmay_checker_context_primitive(
    const MarimayChecker *checker,
    const MarimaySyntaxToken *token
) {
    static const MarimayContextPrimitive primitives[] = {
        {"std/env", "mmayEnvGet", MMAY_MIR_CONTEXT_ENV_GET, 1},
        {
            "std/process", "mmayProcessCurrentDirectory",
            MMAY_MIR_CONTEXT_CURRENT_DIRECTORY, 0
        },
        {
            "std/process", "mmayProcessExecutablePath",
            MMAY_MIR_CONTEXT_EXECUTABLE_PATH, 0
        }
    };
    size_t index = 0;
    if (checker == NULL || token == NULL || token->synthetic) return NULL;
    for (index = 0; index < sizeof(primitives) / sizeof(primitives[0]);
        index += 1) {
        const MarimayContextPrimitive *primitive = &primitives[index];
        size_t package_length = strlen(primitive->package);
        if (checker->package_name_length == package_length
            && memcmp(
                checker->package_name, primitive->package, package_length
            ) == 0 && mmay_checker_private_check_text_equal(
                checker, token->start, token->end, primitive->name
            )) return primitive;
    }
    return NULL;
}

static size_t mmay_checker_context_arguments(
    const MarimayAstNode *arguments,
    const MarimayAstNode **first
) {
    size_t count = 0;
    size_t index = 0;
    for (index = 0; arguments != NULL && index < arguments->child_count;
        index += 1) {
        const MarimayAstElement *element = mmay_ast_child(arguments, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE) {
            if (count == 0 && first != NULL) *first = element->as.node;
            count += 1;
        }
    }
    return count;
}

bool mmay_checker_private_compile_context_primitive_call(
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
    const MarimaySyntaxToken *name = mmay_checker_private_check_child_token(
        callee, MMAY_TOKEN_IDENTIFIER, 0
    );
    const MarimayAstNode *argument = NULL;
    const MarimayContextPrimitive *primitive;
    MarimayCheckName check_name;
    MarimayCheckCallable callable;
    MarimayExpressionValue value;
    MarimayMirInstruction instruction;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    size_t count = 0;
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) {
        return false;
    }
    *resolved = false;
    primitive = mmay_checker_context_primitive(function->checker, name);
    if (primitive == NULL) return true;
    *resolved = true;
    count = mmay_checker_context_arguments(arguments, &argument);
    check_name = (MarimayCheckName){
        function->checker->ast->snapshot, name->start, name->end
    };
    memset(&callable, 0, sizeof(callable));
    if (mmay_checker_private_check_child_node(
            node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0
        ) != NULL || count != primitive->argument_count
        || mmay_check_resolve_call(
            function->checker, check_name, false, MMAY_TYPE_INVALID, &callable
        ) != MMAY_CHECK_CALL_FOUND || !callable.is_external
        || callable.parameter_count != count) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_CALL_ARITY,
            "standard environment/process primitive has an invalid argument list",
            node->start, node->end
        );
        return false;
    }
    if (expected != MMAY_TYPE_INVALID && expected != callable.result_type) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "standard environment/process primitive result type does not match",
            node->start, node->end
        );
        return false;
    }
    memset(&value, 0, sizeof(value));
    value.register_id = MMAY_MIR_NO_REGISTER;
    if (count == 1) {
        if (callable.parameter_types == NULL || !mmay_compile_expression(
                function, argument, callable.parameter_types[0], &value
            )) return false;
        if (mmay_expression_is_never(function, &value)) {
            *result = value;
            return true;
        }
        if (value.type != callable.parameter_types[0]) {
            (void)mmay_checker_add_diagnostic(
                function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "standard environment primitive argument type does not match",
                argument->start, argument->end
            );
            return false;
        }
    }
    if (!mmay_function_add_register(function, callable.result_type, &target)
        || !mmay_checker_private_function_ensure_nominal_layouts(function)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = count == 1
        ? MMAY_MIR_ENV_READ : MMAY_MIR_PROCESS_CONTEXT;
    instruction.context_operation = primitive->operation;
    instruction.target = target;
    instruction.first = count == 1 ? value.register_id : MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.span = (MarimayMirSpan){node->start, node->end};
    if (!mmay_function_append_instruction(function, &instruction)) return false;
    result->type = callable.result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}
