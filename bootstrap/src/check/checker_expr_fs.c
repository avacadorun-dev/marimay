#include "checker_expr_fs_api.h"

#include <string.h>

#include "checker_core_api.h"
#include "checker_imported_nominal_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

typedef struct MarimayFsPrimitive {
    const char *package;
    const char *name;
    MarimayMirFsOperation operation;
    size_t argument_count;
    bool path;
} MarimayFsPrimitive;

static const MarimayFsPrimitive *mmay_checker_fs_primitive(
    const MarimayChecker *checker,
    const MarimaySyntaxToken *token
) {
    static const MarimayFsPrimitive primitives[] = {
        {"std/path", "mmayPathNormalize", MMAY_MIR_PATH_NORMALIZE, 1, true},
        {"std/path", "mmayPathIsAbsolute", MMAY_MIR_PATH_IS_ABSOLUTE, 1, true},
        {"std/fs", "mmayFsOpen", MMAY_MIR_FS_OPEN_READ, 1, false},
        {"std/fs", "mmayFsRead", MMAY_MIR_FS_READ, 2, false},
        {"std/fs", "mmayFsClose", MMAY_MIR_FS_CLOSE, 1, false},
        {"std/fs", "mmayFsCloseBestEffort", MMAY_MIR_FS_CLOSE_BEST_EFFORT, 1, false},
        {"std/fs", "mmayFsReadAll", MMAY_MIR_FS_READ_ALL, 2, false},
        {"std/fs", "mmayFsWriteAtomic", MMAY_MIR_FS_WRITE_ATOMIC, 2, false},
        {"std/fs", "mmayFsMetadata", MMAY_MIR_FS_METADATA, 1, false},
        {"std/fs", "mmayFsReadDirectory", MMAY_MIR_FS_READ_DIRECTORY, 2, false}
    };
    size_t index = 0;
    if (checker == NULL || token == NULL || token->synthetic) return NULL;
    for (index = 0; index < sizeof(primitives) / sizeof(primitives[0]);
        index += 1) {
        const MarimayFsPrimitive *primitive = &primitives[index];
        size_t package_length = strlen(primitive->package);
        if (checker->package_name_length == package_length
            && memcmp(checker->package_name, primitive->package,
                package_length) == 0
            && mmay_checker_private_check_text_equal(
                checker, token->start, token->end, primitive->name
            )) return primitive;
    }
    return NULL;
}

static size_t mmay_checker_fs_arguments(
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
            if (count < capacity) nodes[count] = element->as.node;
            count += 1;
        }
    }
    return count;
}

bool mmay_checker_private_compile_fs_primitive_call(
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
    const MarimayAstNode *nodes[2] = {NULL, NULL};
    const MarimayFsPrimitive *primitive = NULL;
    MarimayCheckName check_name;
    MarimayCheckCallable callable;
    MarimayExpressionValue values[2];
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    size_t count = 0;
    size_t index = 0;
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) {
        return false;
    }
    *resolved = false;
    primitive = mmay_checker_fs_primitive(function->checker, name);
    if (primitive == NULL) return true;
    *resolved = true;
    count = mmay_checker_fs_arguments(arguments, nodes, 2);
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
            "standard path/filesystem primitive has an invalid argument list",
            node->start, node->end
        );
        return false;
    }
    if (expected != MMAY_TYPE_INVALID && expected != callable.result_type) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "standard path/filesystem primitive result type does not match",
            node->start, node->end
        );
        return false;
    }
    memset(values, 0, sizeof(values));
    for (index = 0; index < count; index += 1) {
        values[index].register_id = MMAY_MIR_NO_REGISTER;
        if (!mmay_compile_expression(
                function, nodes[index], callable.parameter_types[index],
                &values[index]
            )) {
            return false;
        }
        if (mmay_expression_is_never(function, &values[index])) {
            *result = values[index];
            return true;
        }
        if (values[index].type != callable.parameter_types[index]) {
            (void)mmay_checker_add_diagnostic(
                function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "standard path/filesystem primitive argument type does not match",
                nodes[index]->start, nodes[index]->end
            );
            return false;
        }
    }
    if (!mmay_function_add_register(
            function, callable.result_type, &target
        )) {
        return false;
    }
    if (!mmay_checker_private_function_ensure_nominal_layouts(function)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = primitive->path ? MMAY_MIR_PATH
        : (count == 2U ? MMAY_MIR_FS_BINARY : MMAY_MIR_FS_UNARY);
    instruction.fs_operation = primitive->operation;
    instruction.target = target;
    instruction.first = values[0].register_id;
    instruction.second = count == 2U
        ? values[1].register_id : MMAY_MIR_NO_REGISTER;
    instruction.span = (MarimayMirSpan){node->start, node->end};
    if (!mmay_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = callable.result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}
