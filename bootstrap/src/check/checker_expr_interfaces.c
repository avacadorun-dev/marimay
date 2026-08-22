#include "checker_expr_interfaces_api.h"
#include "checker_core_api.h"
#include "checker_imported_nominal_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

static bool mmay_checker_private_function_ensure_interface_type(
    MarimayFunctionChecker *function,
    MarimayTypeId type
) {
    MarimayCheckInterfaceDeclaration *declaration = NULL;
    MarimayMirInterfaceId layout = MMAY_MIR_NO_INTERFACE;
    size_t index = 0;
    if (function == NULL || function->checker == NULL
        || !function->mir.initialized) {
        return false;
    }
    declaration = mmay_checker_private_checker_find_interface_by_type(
        function->checker,
        type
    );
    if (declaration == NULL) {
        return true;
    }
    if (!declaration->methods_resolved) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < function->mir.interfaces.length; index += 1) {
        const MarimayMirInterface *existing = mmay_vector_at_const(
            &function->mir.interfaces,
            index
        );
        if (existing != NULL && existing->type == type) {
            if (existing->method_count != declaration->methods.length) {
                mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
                return false;
            }
            return true;
        }
    }
    if (function->mir.interfaces.length
        >= function->mir.options.max_interface_layouts) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!mmay_mir_add_interface(
            &function->mir,
            type,
            declaration->methods.length,
            &layout
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

bool mmay_checker_private_function_ensure_interface_layouts(
    MarimayFunctionChecker *function
) {
    size_t index = 0;
    if (function == NULL || function->checker == NULL
        || !function->mir.initialized
        || !mmay_checker_private_function_ensure_interface_type(
            function,
            function->mir.result_type
        )) {
        return false;
    }
    for (index = 0; index < function->mir.register_types.length; index += 1) {
        const MarimayTypeId *type = mmay_vector_at_const(
            &function->mir.register_types,
            index
        );
        if (type == NULL
            || !mmay_checker_private_function_ensure_interface_type(
                function,
                *type
            )) {
            if (type == NULL) {
                mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    for (index = 0; index < function->mir.calls.length; index += 1) {
        const MarimayMirCall *call = mmay_vector_at_const(
            &function->mir.calls,
            index
        );
        if (call == NULL
            || !mmay_checker_private_function_ensure_interface_type(
                function,
                call->result_type
            )) {
            if (call == NULL) {
                mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    for (index = 0; index < function->mir.call_arguments.length; index += 1) {
        const MarimayMirCallArgument *argument = mmay_vector_at_const(
            &function->mir.call_arguments,
            index
        );
        if (argument == NULL
            || !mmay_checker_private_function_ensure_interface_type(
                function,
                argument->type
            )) {
            if (argument == NULL) {
                mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    for (index = 0; index < function->mir.record_fields.length; index += 1) {
        const MarimayMirRecordField *field = mmay_vector_at_const(
            &function->mir.record_fields,
            index
        );
        if (field == NULL
            || !mmay_checker_private_function_ensure_interface_type(
                function,
                field->type
            )) {
            if (field == NULL) {
                mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    for (index = 0; index < function->mir.enum_payloads.length; index += 1) {
        const MarimayMirEnumPayload *payload = mmay_vector_at_const(
            &function->mir.enum_payloads,
            index
        );
        if (payload == NULL
            || !mmay_checker_private_function_ensure_interface_type(
                function,
                payload->type
            )) {
            if (payload == NULL) {
                mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    return true;
}

bool mmay_checker_private_maybe_box_interface(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
) {
    MarimayCheckInterfaceDeclaration *interface_declaration = NULL;
    MarimaySymbolId *methods = NULL;
    size_t bytes = 0;
    size_t index = 0;
    MarimayMirInterfaceId layout = MMAY_MIR_NO_INTERFACE;
    MarimayMirInterfaceConstructionId construction =
        MMAY_MIR_NO_INTERFACE_CONSTRUCTION;
    MarimayMirInstruction instruction;
    if (function == NULL || result == NULL || !result->valid
        || expected == MMAY_TYPE_INVALID || expected == result->type
        || mmay_expression_is_never(function, result)) {
        return true;
    }
    interface_declaration = mmay_checker_private_checker_find_interface_by_type(
        function->checker, expected
    );
    if (interface_declaration == NULL || !interface_declaration->methods_resolved) {
        return true;
    }
    if (!mmay_size_multiply(interface_declaration->methods.length,
            sizeof(*methods), &bytes)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    methods = bytes == 0 ? NULL : mmay_allocate(
        &function->checker->result->allocator, bytes
    );
    if (bytes != 0 && methods == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < interface_declaration->methods.length; index += 1) {
        const MarimayCheckInterfaceMethodDeclaration *method = mmay_vector_at_const(
            &interface_declaration->methods, index
        );
        MarimayCheckCallable implementation;
        MarimayCheckCallResolution resolution = method == NULL
            ? MMAY_CHECK_CALL_INTERNAL_ERROR : mmay_check_resolve_call(
                function->checker, method->name, true, result->type,
                &implementation
            );
        size_t parameter = 0;
        if (resolution != MMAY_CHECK_CALL_FOUND
            || implementation.result_type != method->result_type
            || implementation.parameter_count
                != method->parameter_types.length + 1) {
            goto mismatch;
        }
        for (parameter = 0; parameter < method->parameter_types.length;
            parameter += 1) {
            const MarimayTypeId *expected_parameter = mmay_vector_at_const(
                &method->parameter_types, parameter
            );
            const MarimayTypeId *actual_parameter =
                &implementation.parameter_types[parameter + 1];
            if (expected_parameter == NULL || actual_parameter == NULL
                || *expected_parameter != *actual_parameter) {
                goto mismatch;
            }
        }
        methods[index] = implementation.symbol;
    }
    {
        MarimayCheckRecordDeclaration *record =
            mmay_checker_private_checker_find_record_by_type(
                function->checker,
                result->type
            );
        MarimayCheckEnumDeclaration *enum_declaration = record == NULL
            ? mmay_checker_private_checker_find_enum_by_type(
                function->checker,
                result->type
            )
            : NULL;
        const MarimayCheckedRecord *imported_record = record == NULL
            ? mmay_check_imported_record_by_type(
                function->checker, result->type
            ) : NULL;
        const MarimayCheckedEnum *imported_enum = record == NULL
                && enum_declaration == NULL && imported_record == NULL
            ? mmay_check_imported_enum_by_type(
                function->checker, result->type
            ) : NULL;
        if (record != NULL) {
            MarimayMirRecordId record_layout = MMAY_MIR_NO_RECORD;
            if (!mmay_checker_private_function_record_layout(
                    function,
                    record,
                    &record_layout
                )) {
                mmay_deallocate(
                    &function->checker->result->allocator,
                    methods,
                    bytes
                );
                return false;
            }
        } else if (enum_declaration != NULL) {
            MarimayMirEnumId enum_layout = MMAY_MIR_NO_ENUM;
            if (!mmay_checker_private_function_enum_layout(
                    function,
                    enum_declaration,
                    &enum_layout
                )) {
                mmay_deallocate(
                    &function->checker->result->allocator,
                    methods,
                    bytes
                );
                return false;
            }
        } else if (imported_record != NULL) {
            MarimayMirRecordId imported_layout = MMAY_MIR_NO_RECORD;
            if (!mmay_checker_private_imported_record_layout(
                    function, imported_record, result->type, &imported_layout
                )) {
                mmay_deallocate(&function->checker->result->allocator,
                    methods, bytes);
                return false;
            }
        } else if (imported_enum != NULL) {
            MarimayMirEnumId imported_layout = MMAY_MIR_NO_ENUM;
            if (!mmay_checker_private_imported_enum_layout(
                    function, imported_enum, result->type, &imported_layout
                )) {
                mmay_deallocate(&function->checker->result->allocator,
                    methods, bytes);
                return false;
            }
        } else {
            goto mismatch;
        }
    }
    if (!mmay_mir_add_interface(
            &function->mir, expected, interface_declaration->methods.length,
            &layout
        ) || !mmay_mir_add_interface_construction(
            &function->mir, layout, result->register_id, result->type,
            methods, interface_declaration->methods.length, &construction
        )) {
        mmay_deallocate(&function->checker->result->allocator, methods, bytes);
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    mmay_deallocate(&function->checker->result->allocator, methods, bytes);
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_INTERFACE_NEW;
    instruction.interface_construction = construction;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!mmay_function_add_register(function, expected, &instruction.target)
        || !mmay_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = expected;
    result->register_id = instruction.target;
    return true;
mismatch:
    mmay_deallocate(&function->checker->result->allocator, methods, bytes);
    (void)mmay_checker_add_diagnostic(
        function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "value does not satisfy the required interface through the D003 free/dot resolver",
        node->start, node->end
    );
    return false;
}

bool mmay_checker_private_compile_interface_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    bool *resolved,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *member = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_MEMBER_EXPRESSION, 0
    );
    const MarimayAstNode *arguments_node = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_ARGUMENTS, 0
    );
    const MarimayAstNode *receiver_node = NULL;
    const MarimaySyntaxToken *name_token = NULL;
    const MarimaySyntaxToken *receiver_token = NULL;
    const MarimayCheckLocal *receiver_local = NULL;
    MarimayExpressionValue receiver;
    MarimayCheckInterfaceDeclaration *interface_declaration = NULL;
    const MarimayCheckInterfaceMethodDeclaration *method = NULL;
    MarimayMirCallArgument *arguments = NULL;
    MarimayMirCallId call = MMAY_MIR_NO_CALL;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    MarimayCheckName name;
    size_t explicit_count = 0;
    size_t index = 0;
    size_t slot = SIZE_MAX;
    size_t bytes = 0;
    if (function == NULL || resolved == NULL || result == NULL) {
        return false;
    }
    *resolved = false;
    if (member == NULL || arguments_node == NULL
        || !mmay_checker_private_member_parts(member, &receiver_node, &name_token)
        || receiver_node == NULL || name_token == NULL || name_token->synthetic) {
        return true;
    }
    if (receiver_node->kind != MMAY_SYNTAX_NAME_EXPRESSION
        || (receiver_token = mmay_checker_private_check_child_token(
                receiver_node, MMAY_TOKEN_IDENTIFIER, 0
            )) == NULL || receiver_token->synthetic) {
        return true;
    }
    name = (MarimayCheckName){
        function->checker->ast->snapshot, receiver_token->start, receiver_token->end
    };
    receiver_local = mmay_checker_private_function_find_local(function, name);
    if (receiver_local == NULL
        || (interface_declaration =
                mmay_checker_private_checker_find_interface_by_type(
                    function->checker, receiver_local->type
                )) == NULL) {
        return true;
    }
    memset(&receiver, 0, sizeof(receiver));
    if (!mmay_compile_expression(
            function, receiver_node, MMAY_TYPE_INVALID, &receiver
        ) || mmay_expression_is_never(function, &receiver)) {
        if (mmay_expression_is_never(function, &receiver)) {
            *result = receiver;
            *resolved = true;
        }
        return !mmay_checker_has_failed(function->checker);
    }
    if (receiver.type != receiver_local->type) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    *resolved = true;
    name = (MarimayCheckName){
        function->checker->ast->snapshot, name_token->start, name_token->end
    };
    for (index = 0; index < interface_declaration->methods.length; index += 1) {
        const MarimayCheckInterfaceMethodDeclaration *candidate = mmay_vector_at_const(
            &interface_declaration->methods, index
        );
        if (candidate != NULL && mmay_checker_private_check_names_equal(
                function->checker, candidate->name, name
            )) {
            method = candidate;
            slot = index;
            break;
        }
    }
    if (method == NULL || slot > UINT32_MAX) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_UNKNOWN_NAME,
            "interface does not declare this method", name.start, name.end
        );
        return false;
    }
    for (index = 0; index < arguments_node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(arguments_node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE) {
            explicit_count += 1;
        }
    }
    if (explicit_count != method->parameter_types.length
        || !mmay_size_add(explicit_count, 1, &index)
        || !mmay_size_multiply(index, sizeof(*arguments), &bytes)) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_CALL_ARITY,
            "call argument count does not match the interface method",
            arguments_node->start, arguments_node->end
        );
        return false;
    }
    arguments = mmay_allocate(&function->checker->result->allocator, bytes);
    if (arguments == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    arguments[0] = (MarimayMirCallArgument){receiver.register_id, receiver.type};
    explicit_count = 0;
    for (index = 0; index < arguments_node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(arguments_node, index);
        const MarimayTypeId *expected = NULL;
        MarimayExpressionValue value;
        if (element == NULL || element->kind != MMAY_AST_ELEMENT_NODE) {
            continue;
        }
        expected = mmay_vector_at_const(&method->parameter_types, explicit_count);
        memset(&value, 0, sizeof(value));
        if (expected == NULL || !mmay_compile_expression(
                function, element->as.node, *expected, &value
            ) || value.type != *expected) {
            mmay_deallocate(&function->checker->result->allocator, arguments, bytes);
            return false;
        }
        arguments[explicit_count + 1] = (MarimayMirCallArgument){
            value.register_id, value.type
        };
        explicit_count += 1;
    }
    if (!mmay_mir_add_call(
            &function->mir, interface_declaration->symbol, method->result_type,
            arguments, explicit_count + 1, &call
        ) || !mmay_mir_set_call_interface_slot(
            &function->mir, call, (uint32_t)slot
        ) || !mmay_function_add_register(
            function, method->result_type, &target
        )) {
        mmay_deallocate(&function->checker->result->allocator, arguments, bytes);
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    mmay_deallocate(&function->checker->result->allocator, arguments, bytes);
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CALL_INTERFACE;
    instruction.target = target;
    instruction.call = call;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!mmay_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = method->result_type;
    result->register_id = instruction.target;
    result->valid = true;
    return true;
}
