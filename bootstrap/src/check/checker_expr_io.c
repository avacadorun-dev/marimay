#include "checker_expr_io_api.h"

#include "checker_core_api.h"
#include "checker_imported_nominal_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

#include <string.h>

static bool mmay_checker_io_bytes_equal(
    const void *left,
    size_t left_length,
    const void *right,
    size_t right_length
) {
    return left_length == right_length
        && (left_length == 0 || memcmp(left, right, left_length) == 0);
}

static size_t mmay_checker_io_arguments(
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

static bool mmay_checker_io_local_package(const MarimayChecker *checker) {
    return checker != NULL
        && checker->package_name_length == sizeof("std/io") - 1
        && memcmp(
            checker->package_name, "std/io", sizeof("std/io") - 1
        ) == 0;
}

static bool mmay_checker_io_compile_binary_primitive(
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
    const MarimayAstNode *argument_nodes[1] = {NULL};
    MarimayCheckCallable callable;
    MarimayExpressionValue value;
    const MarimayTypeDescriptor *descriptor = NULL;
    MarimayCheckEnumDeclaration *result_enum = NULL;
    MarimayCheckEnumDeclaration *error_enum = NULL;
    MarimayCheckEnumDeclaration concrete;
    MarimayMirEnumId result_layout = MMAY_MIR_NO_ENUM;
    MarimayMirEnumId error_layout = MMAY_MIR_NO_ENUM;
    MarimayMirArrayId array_layout = MMAY_MIR_NO_ARRAY;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    bool reads = false;
    bool stderr_stream = false;
    MarimayCheckName check_name;
    if (!mmay_checker_io_local_package(function->checker)
        || name == NULL || name->synthetic) return true;
    reads = mmay_checker_private_check_text_equal(
        function->checker, name->start, name->end, "mmayReadBytes"
    );
    stderr_stream = mmay_checker_private_check_text_equal(
        function->checker, name->start, name->end, "mmayEwriteBytes"
    );
    if (!reads && !stderr_stream && !mmay_checker_private_check_text_equal(
            function->checker, name->start, name->end, "mmayWriteBytes"
        )) return true;
    *resolved = true;
    if (mmay_checker_private_check_child_node(
            node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0
        ) != NULL || mmay_checker_io_arguments(arguments, argument_nodes, 1)
            != 1U) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_CALL_ARITY,
            "binary std/io primitive requires one argument",
            node->start, node->end
        );
        return false;
    }
    memset(&callable, 0, sizeof(callable));
    check_name = (MarimayCheckName){
        function->checker->ast->snapshot, name->start, name->end
    };
    if (mmay_check_resolve_call(
            function->checker, check_name, false, MMAY_TYPE_INVALID,
            &callable
        ) != MMAY_CHECK_CALL_FOUND || !callable.is_external
        || callable.parameter_count != 1U
        || callable.parameter_types == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    memset(&value, 0, sizeof(value));
    if (!mmay_compile_expression(
            function, argument_nodes[0], callable.parameter_types[0], &value
        )) return false;
    if (mmay_expression_is_never(function, &value)) {
        *result = value;
        return true;
    }
    descriptor = mmay_type_descriptor(
        &function->checker->result->types, callable.result_type
    );
    if (value.type != callable.parameter_types[0]
        || (expected != MMAY_TYPE_INVALID && expected != callable.result_type)
        || descriptor == NULL || descriptor->kind != MMAY_TYPE_KIND_NOMINAL
        || descriptor->argument_count != 2
        || (reads
            ? (callable.parameter_types[0] != MMAY_TYPE_I64
                || descriptor->arguments[0] < MMAY_TYPE_FIRST_DYNAMIC)
            : descriptor->arguments[0] != MMAY_TYPE_VOID)) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "binary std/io primitive type does not match its contract",
            node->start, node->end
        );
        return false;
    }
    result_enum = mmay_checker_private_checker_find_enum_by_type(
        function->checker, callable.result_type
    );
    error_enum = mmay_checker_private_checker_find_enum_by_type(
        function->checker, descriptor->arguments[1]
    );
    if (result_enum == NULL || error_enum == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    concrete = *result_enum;
    concrete.type = callable.result_type;
    if (!mmay_checker_private_function_enum_layout(
            function, &concrete, &result_layout
        )) return false;
    concrete = *error_enum;
    concrete.type = descriptor->arguments[1];
    if (!mmay_checker_private_function_enum_layout(
            function, &concrete, &error_layout
        ) || (reads && !mmay_array_layout(
            function, descriptor->arguments[0], &array_layout
        )) || !mmay_function_add_register(
            function, callable.result_type, &target
        )) return false;
    (void)error_layout;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = reads
        ? MMAY_MIR_IO_READ_BYTES : MMAY_MIR_IO_WRITE_BYTES;
    instruction.target = target;
    instruction.first = value.register_id;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.enum_id = result_layout;
    instruction.array = array_layout;
    instruction.io_operation = stderr_stream
        ? MMAY_MIR_IO_STDERR_WRITE : MMAY_MIR_IO_STDOUT_WRITE;
    instruction.span = (MarimayMirSpan){node->start, node->end};
    if (!mmay_function_append_instruction(function, &instruction)) return false;
    result->type = callable.result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}

static const MarimayCheckImportBinding *mmay_checker_io_binding(
    MarimayFunctionChecker *function,
    const MarimaySyntaxToken *namespace_token,
    const MarimaySyntaxToken *name_token
) {
    static const unsigned char package_name[] = "std/io";
    const MarimayCheckImportEnvironment *imports = function != NULL
        ? function->checker->imports : NULL;
    size_t index = 0;
    for (index = 0; imports != NULL && index < imports->binding_count;
        index += 1) {
        const MarimayCheckImportBinding *binding = &imports->bindings[index];
        const MarimayCheckPackageSurface *surface = binding->surface_index
                < imports->surface_count
            ? imports->surfaces[binding->surface_index] : NULL;
        const unsigned char *local = namespace_token != NULL
            ? function->checker->source + namespace_token->start
            : function->checker->source + name_token->start;
        size_t local_length = namespace_token != NULL
            ? namespace_token->end - namespace_token->start
            : name_token->end - name_token->start;
        if (binding->ast == function->checker->ast
            && binding->is_namespace == (namespace_token != NULL)
            && surface != NULL
            && mmay_checker_io_bytes_equal(
                surface->path, surface->path_length,
                package_name, sizeof(package_name) - 1
            ) && mmay_checker_io_bytes_equal(
                binding->local, binding->local_length, local, local_length
            )) {
            return binding;
        }
    }
    return NULL;
}

bool mmay_checker_private_compile_io_call(
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
    const MarimayAstNode *argument_nodes[1] = {NULL};
    const MarimaySyntaxToken *namespace_token = NULL;
    const MarimaySyntaxToken *name_token = mmay_checker_private_check_child_token(
        callee, MMAY_TOKEN_IDENTIFIER, 0
    );
    const MarimayCheckImportBinding *binding = NULL;
    const unsigned char *io_name = NULL;
    size_t io_name_length = 0;
    MarimayMirIoOperation operation = MMAY_MIR_IO_STDOUT_LINE;
    bool takes_text = false;
    bool reads_line = false;
    MarimayCheckName namespace_name = {0};
    MarimayCheckName function_name = {0};
    MarimayCheckCallable callable;
    MarimayExpressionValue text;
    const MarimayTypeDescriptor *result_descriptor = NULL;
    const MarimayCheckedEnum *error_enum = NULL;
    MarimayCheckEnumDeclaration *result_enum = NULL;
    MarimayCheckEnumDeclaration concrete_result;
    MarimayCheckEnumDeclaration concrete_option;
    MarimayMirEnumId error_layout = MMAY_MIR_NO_ENUM;
    MarimayMirEnumId result_layout = MMAY_MIR_NO_ENUM;
    MarimayMirEnumId option_layout = MMAY_MIR_NO_ENUM;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) {
        return false;
    }
    *resolved = false;
    if (!mmay_checker_io_compile_binary_primitive(
            function, node, expected, resolved, result
        )) return false;
    if (*resolved) return true;
    if (name_token == NULL && member != NULL
        && mmay_checker_private_member_parts(member, &receiver, &name_token)
        && receiver != NULL
        && receiver->kind == MMAY_SYNTAX_NAME_EXPRESSION) {
        namespace_token = mmay_checker_private_check_child_token(
            receiver, MMAY_TOKEN_IDENTIFIER, 0
        );
    }
    if (name_token == NULL || name_token->synthetic
        || (namespace_token != NULL && namespace_token->synthetic)) {
        return true;
    }
    binding = mmay_checker_io_binding(function, namespace_token, name_token);
    if (binding == NULL) {
        return true;
    }
    io_name = namespace_token != NULL
        ? function->checker->source + name_token->start
        : binding->original;
    io_name_length = namespace_token != NULL
        ? name_token->end - name_token->start
        : binding->original_length;
    if (mmay_checker_io_bytes_equal(io_name, io_name_length, "print", 5)) {
        operation = MMAY_MIR_IO_STDOUT_WRITE; takes_text = true;
    } else if (mmay_checker_io_bytes_equal(io_name, io_name_length, "println", 7)) {
        operation = MMAY_MIR_IO_STDOUT_LINE; takes_text = true;
    } else if (mmay_checker_io_bytes_equal(io_name, io_name_length, "eprint", 6)) {
        operation = MMAY_MIR_IO_STDERR_WRITE; takes_text = true;
    } else if (mmay_checker_io_bytes_equal(io_name, io_name_length, "eprintln", 8)) {
        operation = MMAY_MIR_IO_STDERR_LINE; takes_text = true;
    } else if (mmay_checker_io_bytes_equal(io_name, io_name_length, "readLine", 8)) {
        reads_line = true;
    } else if (mmay_checker_io_bytes_equal(io_name, io_name_length, "flushStdout", 11)) {
        operation = MMAY_MIR_IO_STDOUT_FLUSH;
    } else if (mmay_checker_io_bytes_equal(io_name, io_name_length, "flushStderr", 11)) {
        operation = MMAY_MIR_IO_STDERR_FLUSH;
    } else {
        return true;
    }
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
        ) != NULL || mmay_checker_io_arguments(arguments, argument_nodes, 1)
            != ((takes_text || reads_line) ? 1U : 0U)) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            mmay_checker_private_check_child_node(
                    node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0
                ) != NULL
                ? MMAY_DIAGNOSTIC_TYPE_MISMATCH
                : MMAY_DIAGNOSTIC_CALL_ARITY,
            "std/io stream operation has an invalid argument list",
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
            namespace_token->start,
            namespace_token->end
        };
    }
    if (mmay_check_resolve_imported_call(
            function->checker,
            namespace_name,
            namespace_token != NULL,
            function_name,
            false,
            MMAY_TYPE_INVALID,
            &callable
        ) != MMAY_CHECK_CALL_FOUND
        || callable.parameter_count != ((takes_text || reads_line) ? 1U : 0U)
        || (takes_text && (callable.parameter_types == NULL
            || callable.parameter_types[0] != MMAY_TYPE_STRING))
        || (reads_line && (callable.parameter_types == NULL
            || callable.parameter_types[0] != MMAY_TYPE_I64))
        || !callable.is_external) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    memset(&text, 0, sizeof(text));
    if (takes_text) {
        if (!mmay_compile_expression(
                function, argument_nodes[0], MMAY_TYPE_STRING, &text
            )) return false;
        if (mmay_expression_is_never(function, &text)) {
            *result = text;
            return true;
        }
    }
    if (reads_line && !mmay_compile_expression(
            function, argument_nodes[0], MMAY_TYPE_I64, &text
        )) return false;
    result_descriptor = mmay_type_descriptor(
        &function->checker->result->types, callable.result_type
    );
    if ((takes_text && text.type != MMAY_TYPE_STRING)
        || (expected != MMAY_TYPE_INVALID && expected != callable.result_type)
        || result_descriptor == NULL
        || result_descriptor->kind != MMAY_TYPE_KIND_NOMINAL
        || result_descriptor->argument_count != 2
        || (reads_line ? result_descriptor->arguments[0] == MMAY_TYPE_VOID
            : result_descriptor->arguments[0] != MMAY_TYPE_VOID)) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "std/io.println argument or Result<void, io.Error> type does not match",
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
            function,
            error_enum,
            result_descriptor->arguments[1],
            &error_layout
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
        )) {
        return false;
    }
    if (reads_line) {
        MarimayCheckEnumDeclaration *option =
            mmay_checker_private_checker_find_enum_by_type(
                function->checker, result_descriptor->arguments[0]
            );
        if (option == NULL) { mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR); return false; }
        concrete_option = *option;
        concrete_option.type = result_descriptor->arguments[0];
        if (!mmay_checker_private_function_enum_layout(
                function, &concrete_option, &option_layout
            )) return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = reads_line ? MMAY_MIR_IO_READ_LINE
        : takes_text
        ? (operation == MMAY_MIR_IO_STDOUT_LINE
                && error_enum->variants.length == 2
            ? MMAY_MIR_IO_PRINTLN : MMAY_MIR_IO_WRITE)
        : MMAY_MIR_IO_FLUSH;
    instruction.target = target;
    instruction.first = takes_text || reads_line
        ? text.register_id : MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.enum_id = result_layout;
    instruction.option_enum_id = option_layout;
    instruction.io_operation = operation;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!mmay_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = callable.result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}
