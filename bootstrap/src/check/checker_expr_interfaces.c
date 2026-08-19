#include "checker_expr_interfaces_api.h"
#include "checker_core_api.h"
#include "checker_imported_nominal_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

static bool ruda_checker_private_function_ensure_interface_type(
    RudaFunctionChecker *function,
    RudaTypeId type
) {
    RudaCheckInterfaceDeclaration *declaration = NULL;
    RudaMirInterfaceId layout = RUDA_MIR_NO_INTERFACE;
    size_t index = 0;
    if (function == NULL || function->checker == NULL
        || !function->mir.initialized) {
        return false;
    }
    declaration = ruda_checker_private_checker_find_interface_by_type(
        function->checker,
        type
    );
    if (declaration == NULL) {
        return true;
    }
    if (!declaration->methods_resolved) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < function->mir.interfaces.length; index += 1) {
        const RudaMirInterface *existing = ruda_vector_at_const(
            &function->mir.interfaces,
            index
        );
        if (existing != NULL && existing->type == type) {
            if (existing->method_count != declaration->methods.length) {
                ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
                return false;
            }
            return true;
        }
    }
    if (function->mir.interfaces.length
        >= function->mir.options.max_interface_layouts) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!ruda_mir_add_interface(
            &function->mir,
            type,
            declaration->methods.length,
            &layout
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

bool ruda_checker_private_function_ensure_interface_layouts(
    RudaFunctionChecker *function
) {
    size_t index = 0;
    if (function == NULL || function->checker == NULL
        || !function->mir.initialized
        || !ruda_checker_private_function_ensure_interface_type(
            function,
            function->mir.result_type
        )) {
        return false;
    }
    for (index = 0; index < function->mir.register_types.length; index += 1) {
        const RudaTypeId *type = ruda_vector_at_const(
            &function->mir.register_types,
            index
        );
        if (type == NULL
            || !ruda_checker_private_function_ensure_interface_type(
                function,
                *type
            )) {
            if (type == NULL) {
                ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    for (index = 0; index < function->mir.calls.length; index += 1) {
        const RudaMirCall *call = ruda_vector_at_const(
            &function->mir.calls,
            index
        );
        if (call == NULL
            || !ruda_checker_private_function_ensure_interface_type(
                function,
                call->result_type
            )) {
            if (call == NULL) {
                ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    for (index = 0; index < function->mir.call_arguments.length; index += 1) {
        const RudaMirCallArgument *argument = ruda_vector_at_const(
            &function->mir.call_arguments,
            index
        );
        if (argument == NULL
            || !ruda_checker_private_function_ensure_interface_type(
                function,
                argument->type
            )) {
            if (argument == NULL) {
                ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    for (index = 0; index < function->mir.record_fields.length; index += 1) {
        const RudaMirRecordField *field = ruda_vector_at_const(
            &function->mir.record_fields,
            index
        );
        if (field == NULL
            || !ruda_checker_private_function_ensure_interface_type(
                function,
                field->type
            )) {
            if (field == NULL) {
                ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    for (index = 0; index < function->mir.enum_payloads.length; index += 1) {
        const RudaMirEnumPayload *payload = ruda_vector_at_const(
            &function->mir.enum_payloads,
            index
        );
        if (payload == NULL
            || !ruda_checker_private_function_ensure_interface_type(
                function,
                payload->type
            )) {
            if (payload == NULL) {
                ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    return true;
}

bool ruda_checker_private_maybe_box_interface(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
) {
    RudaCheckInterfaceDeclaration *interface_declaration = NULL;
    RudaSymbolId *methods = NULL;
    size_t bytes = 0;
    size_t index = 0;
    RudaMirInterfaceId layout = RUDA_MIR_NO_INTERFACE;
    RudaMirInterfaceConstructionId construction =
        RUDA_MIR_NO_INTERFACE_CONSTRUCTION;
    RudaMirInstruction instruction;
    if (function == NULL || result == NULL || !result->valid
        || expected == RUDA_TYPE_INVALID || expected == result->type
        || ruda_expression_is_never(function, result)) {
        return true;
    }
    interface_declaration = ruda_checker_private_checker_find_interface_by_type(
        function->checker, expected
    );
    if (interface_declaration == NULL || !interface_declaration->methods_resolved) {
        return true;
    }
    if (!ruda_size_multiply(interface_declaration->methods.length,
            sizeof(*methods), &bytes)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    methods = bytes == 0 ? NULL : ruda_allocate(
        &function->checker->result->allocator, bytes
    );
    if (bytes != 0 && methods == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < interface_declaration->methods.length; index += 1) {
        const RudaCheckInterfaceMethodDeclaration *method = ruda_vector_at_const(
            &interface_declaration->methods, index
        );
        RudaCheckCallable implementation;
        RudaCheckCallResolution resolution = method == NULL
            ? RUDA_CHECK_CALL_INTERNAL_ERROR : ruda_check_resolve_call(
                function->checker, method->name, true, result->type,
                &implementation
            );
        size_t parameter = 0;
        if (resolution != RUDA_CHECK_CALL_FOUND
            || implementation.result_type != method->result_type
            || implementation.parameter_count
                != method->parameter_types.length + 1) {
            goto mismatch;
        }
        for (parameter = 0; parameter < method->parameter_types.length;
            parameter += 1) {
            const RudaTypeId *expected_parameter = ruda_vector_at_const(
                &method->parameter_types, parameter
            );
            const RudaTypeId *actual_parameter =
                &implementation.parameter_types[parameter + 1];
            if (expected_parameter == NULL || actual_parameter == NULL
                || *expected_parameter != *actual_parameter) {
                goto mismatch;
            }
        }
        methods[index] = implementation.symbol;
    }
    {
        RudaCheckRecordDeclaration *record =
            ruda_checker_private_checker_find_record_by_type(
                function->checker,
                result->type
            );
        RudaCheckEnumDeclaration *enum_declaration = record == NULL
            ? ruda_checker_private_checker_find_enum_by_type(
                function->checker,
                result->type
            )
            : NULL;
        const RudaCheckedRecord *imported_record = record == NULL
            ? ruda_check_imported_record_by_type(
                function->checker, result->type
            ) : NULL;
        const RudaCheckedEnum *imported_enum = record == NULL
                && enum_declaration == NULL && imported_record == NULL
            ? ruda_check_imported_enum_by_type(
                function->checker, result->type
            ) : NULL;
        if (record != NULL) {
            RudaMirRecordId record_layout = RUDA_MIR_NO_RECORD;
            if (!ruda_checker_private_function_record_layout(
                    function,
                    record,
                    &record_layout
                )) {
                ruda_deallocate(
                    &function->checker->result->allocator,
                    methods,
                    bytes
                );
                return false;
            }
        } else if (enum_declaration != NULL) {
            RudaMirEnumId enum_layout = RUDA_MIR_NO_ENUM;
            if (!ruda_checker_private_function_enum_layout(
                    function,
                    enum_declaration,
                    &enum_layout
                )) {
                ruda_deallocate(
                    &function->checker->result->allocator,
                    methods,
                    bytes
                );
                return false;
            }
        } else if (imported_record != NULL) {
            RudaMirRecordId imported_layout = RUDA_MIR_NO_RECORD;
            if (!ruda_checker_private_imported_record_layout(
                    function, imported_record, result->type, &imported_layout
                )) {
                ruda_deallocate(&function->checker->result->allocator,
                    methods, bytes);
                return false;
            }
        } else if (imported_enum != NULL) {
            RudaMirEnumId imported_layout = RUDA_MIR_NO_ENUM;
            if (!ruda_checker_private_imported_enum_layout(
                    function, imported_enum, result->type, &imported_layout
                )) {
                ruda_deallocate(&function->checker->result->allocator,
                    methods, bytes);
                return false;
            }
        } else {
            goto mismatch;
        }
    }
    if (!ruda_mir_add_interface(
            &function->mir, expected, interface_declaration->methods.length,
            &layout
        ) || !ruda_mir_add_interface_construction(
            &function->mir, layout, result->register_id, result->type,
            methods, interface_declaration->methods.length, &construction
        )) {
        ruda_deallocate(&function->checker->result->allocator, methods, bytes);
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    ruda_deallocate(&function->checker->result->allocator, methods, bytes);
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_INTERFACE_NEW;
    instruction.interface_construction = construction;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!ruda_function_add_register(function, expected, &instruction.target)
        || !ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = expected;
    result->register_id = instruction.target;
    return true;
mismatch:
    ruda_deallocate(&function->checker->result->allocator, methods, bytes);
    (void)ruda_checker_add_diagnostic(
        function->checker, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "value does not satisfy the required interface through the D003 free/dot resolver",
        node->start, node->end
    );
    return false;
}

bool ruda_checker_private_compile_interface_call(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    bool *resolved,
    RudaExpressionValue *result
) {
    const RudaAstNode *member = ruda_checker_private_check_child_node(
        node, RUDA_SYNTAX_MEMBER_EXPRESSION, 0
    );
    const RudaAstNode *arguments_node = ruda_checker_private_check_child_node(
        node, RUDA_SYNTAX_ARGUMENTS, 0
    );
    const RudaAstNode *receiver_node = NULL;
    const RudaSyntaxToken *name_token = NULL;
    const RudaSyntaxToken *receiver_token = NULL;
    const RudaCheckLocal *receiver_local = NULL;
    RudaExpressionValue receiver;
    RudaCheckInterfaceDeclaration *interface_declaration = NULL;
    const RudaCheckInterfaceMethodDeclaration *method = NULL;
    RudaMirCallArgument *arguments = NULL;
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    RudaCheckName name;
    size_t explicit_count = 0;
    size_t index = 0;
    size_t slot = SIZE_MAX;
    size_t bytes = 0;
    if (function == NULL || resolved == NULL || result == NULL) {
        return false;
    }
    *resolved = false;
    if (member == NULL || arguments_node == NULL
        || !ruda_checker_private_member_parts(member, &receiver_node, &name_token)
        || receiver_node == NULL || name_token == NULL || name_token->synthetic) {
        return true;
    }
    if (receiver_node->kind != RUDA_SYNTAX_NAME_EXPRESSION
        || (receiver_token = ruda_checker_private_check_child_token(
                receiver_node, RUDA_TOKEN_IDENTIFIER, 0
            )) == NULL || receiver_token->synthetic) {
        return true;
    }
    name = (RudaCheckName){
        function->checker->ast->snapshot, receiver_token->start, receiver_token->end
    };
    receiver_local = ruda_checker_private_function_find_local(function, name);
    if (receiver_local == NULL
        || (interface_declaration =
                ruda_checker_private_checker_find_interface_by_type(
                    function->checker, receiver_local->type
                )) == NULL) {
        return true;
    }
    memset(&receiver, 0, sizeof(receiver));
    if (!ruda_compile_expression(
            function, receiver_node, RUDA_TYPE_INVALID, &receiver
        ) || ruda_expression_is_never(function, &receiver)) {
        if (ruda_expression_is_never(function, &receiver)) {
            *result = receiver;
            *resolved = true;
        }
        return !ruda_checker_has_failed(function->checker);
    }
    if (receiver.type != receiver_local->type) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    *resolved = true;
    name = (RudaCheckName){
        function->checker->ast->snapshot, name_token->start, name_token->end
    };
    for (index = 0; index < interface_declaration->methods.length; index += 1) {
        const RudaCheckInterfaceMethodDeclaration *candidate = ruda_vector_at_const(
            &interface_declaration->methods, index
        );
        if (candidate != NULL && ruda_checker_private_check_names_equal(
                function->checker, candidate->name, name
            )) {
            method = candidate;
            slot = index;
            break;
        }
    }
    if (method == NULL || slot > UINT32_MAX) {
        (void)ruda_checker_add_diagnostic(
            function->checker, RUDA_DIAGNOSTIC_UNKNOWN_NAME,
            "interface does not declare this method", name.start, name.end
        );
        return false;
    }
    for (index = 0; index < arguments_node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(arguments_node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE) {
            explicit_count += 1;
        }
    }
    if (explicit_count != method->parameter_types.length
        || !ruda_size_add(explicit_count, 1, &index)
        || !ruda_size_multiply(index, sizeof(*arguments), &bytes)) {
        (void)ruda_checker_add_diagnostic(
            function->checker, RUDA_DIAGNOSTIC_CALL_ARITY,
            "call argument count does not match the interface method",
            arguments_node->start, arguments_node->end
        );
        return false;
    }
    arguments = ruda_allocate(&function->checker->result->allocator, bytes);
    if (arguments == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    arguments[0] = (RudaMirCallArgument){receiver.register_id, receiver.type};
    explicit_count = 0;
    for (index = 0; index < arguments_node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(arguments_node, index);
        const RudaTypeId *expected = NULL;
        RudaExpressionValue value;
        if (element == NULL || element->kind != RUDA_AST_ELEMENT_NODE) {
            continue;
        }
        expected = ruda_vector_at_const(&method->parameter_types, explicit_count);
        memset(&value, 0, sizeof(value));
        if (expected == NULL || !ruda_compile_expression(
                function, element->as.node, *expected, &value
            ) || value.type != *expected) {
            ruda_deallocate(&function->checker->result->allocator, arguments, bytes);
            return false;
        }
        arguments[explicit_count + 1] = (RudaMirCallArgument){
            value.register_id, value.type
        };
        explicit_count += 1;
    }
    if (!ruda_mir_add_call(
            &function->mir, interface_declaration->symbol, method->result_type,
            arguments, explicit_count + 1, &call
        ) || !ruda_mir_set_call_interface_slot(
            &function->mir, call, (uint32_t)slot
        ) || !ruda_function_add_register(
            function, method->result_type, &target
        )) {
        ruda_deallocate(&function->checker->result->allocator, arguments, bytes);
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    ruda_deallocate(&function->checker->result->allocator, arguments, bytes);
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CALL_INTERFACE;
    instruction.target = target;
    instruction.call = call;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = method->result_type;
    result->register_id = instruction.target;
    result->valid = true;
    return true;
}
