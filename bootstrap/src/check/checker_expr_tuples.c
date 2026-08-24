#include "checker_expr_tuples_api.h"

#include "checker_core_api.h"

bool mmay_checker_private_function_tuple_layout(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    MarimayMirRecordId *result
) {
    const MarimayTypeDescriptor *descriptor = function != NULL
        ? mmay_type_descriptor(&function->checker->result->types, type)
        : NULL;
    MarimayMirRecordField *fields = NULL;
    size_t field_bytes = 0;
    size_t index = 0;
    bool accepted = false;
    if (descriptor == NULL || descriptor->kind != MMAY_TYPE_KIND_TUPLE
        || descriptor->argument_count == 0 || result == NULL
        || !mmay_size_multiply(
            descriptor->argument_count,
            sizeof(*fields),
            &field_bytes
        )) {
        if (function != NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    fields = mmay_allocate(
        &function->checker->result->allocator,
        field_bytes
    );
    if (fields == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < descriptor->argument_count; index += 1) {
        MarimayTypeId element = descriptor->arguments[index];
        if (!mmay_function_ensure_array_layout(function, element)
            || !mmay_function_ensure_tuple_layout(function, element)) {
            goto cleanup;
        }
        fields[index].type = element;
        fields[index].is_mutable = false;
    }
    accepted = mmay_mir_add_record(
        &function->mir,
        type,
        fields,
        descriptor->argument_count,
        result
    );
    if (!accepted) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
    }

cleanup:
    mmay_deallocate(
        &function->checker->result->allocator,
        fields,
        field_bytes
    );
    return accepted && !mmay_checker_has_failed(function->checker);
}

bool mmay_function_ensure_tuple_layout(
    MarimayFunctionChecker *function,
    MarimayTypeId type
) {
    const MarimayTypeTable *types = function != NULL
        ? &function->checker->result->types : NULL;
    MarimayMirRecordId ignored = MMAY_MIR_NO_RECORD;
    if (types == NULL) {
        return false;
    }
    if (mmay_type_kind(types, type) != MMAY_TYPE_KIND_TUPLE) {
        return true;
    }
    return mmay_checker_private_function_tuple_layout(
        function,
        type,
        &ignored
    );
}

bool mmay_checker_private_compile_tuple(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
) {
    const MarimayTypeDescriptor *expected_descriptor =
        expected != MMAY_TYPE_INVALID
        ? mmay_type_descriptor(&function->checker->result->types, expected)
        : NULL;
    MarimayMirRecordField *fields = NULL;
    MarimayMirRecordArgument *arguments = NULL;
    MarimayTypeId *types = NULL;
    size_t count = 0;
    size_t field_bytes = 0;
    size_t argument_bytes = 0;
    size_t type_bytes = 0;
    size_t cursor = 0;
    size_t index = 0;
    MarimayTypeId tuple_type = MMAY_TYPE_INVALID;
    MarimayMirRecordId record = MMAY_MIR_NO_RECORD;
    MarimayMirRecordConstructionId construction =
        MMAY_MIR_NO_RECORD_CONSTRUCTION;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    bool accepted = false;
    if (function == NULL || node == NULL || result == NULL
        || node->kind != MMAY_SYNTAX_TUPLE_EXPRESSION) {
        return false;
    }
    if (function->constant_evaluation) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE) {
            count += 1;
        }
    }
    if (count == 0 || (expected != MMAY_TYPE_INVALID
            && (expected_descriptor == NULL
                || expected_descriptor->kind != MMAY_TYPE_KIND_TUPLE
                || expected_descriptor->argument_count != count))
        || !mmay_size_multiply(count, sizeof(*fields), &field_bytes)
        || !mmay_size_multiply(count, sizeof(*arguments), &argument_bytes)
        || !mmay_size_multiply(count, sizeof(*types), &type_bytes)) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "tuple value does not match the expected tuple arity",
            node->start,
            node->end
        );
        goto cleanup;
    }
    fields = mmay_allocate(
        &function->checker->result->allocator,
        field_bytes
    );
    arguments = mmay_allocate(
        &function->checker->result->allocator,
        argument_bytes
    );
    types = mmay_allocate(
        &function->checker->result->allocator,
        type_bytes
    );
    if (fields == NULL || arguments == NULL || types == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        MarimayExpressionValue value;
        MarimayTypeId expected_element = MMAY_TYPE_INVALID;
        if (element == NULL || element->kind != MMAY_AST_ELEMENT_NODE) {
            continue;
        }
        if (cursor >= count) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (expected_descriptor != NULL) {
            expected_element = expected_descriptor->arguments[cursor];
        }
        memset(&value, 0, sizeof(value));
        if (!mmay_compile_expression(
                function,
                element->as.node,
                expected_element,
                &value
            )) {
            goto cleanup;
        }
        if (mmay_expression_is_never(function, &value)) {
            *result = value;
            accepted = true;
            goto cleanup;
        }
        if (expected_element != MMAY_TYPE_INVALID
            && value.type != expected_element) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "tuple element does not match its expected type",
                element->as.node->start,
                element->as.node->end
            );
            goto cleanup;
        }
        types[cursor] = value.type;
        fields[cursor].type = value.type;
        fields[cursor].is_mutable = false;
        arguments[cursor].register_id = value.register_id;
        arguments[cursor].type = value.type;
        cursor += 1;
    }
    if (cursor != count || !mmay_type_intern_tuple(
            &function->checker->result->types,
            types,
            count,
            &tuple_type
        ) || (expected != MMAY_TYPE_INVALID && tuple_type != expected)
        || !mmay_mir_add_record(
            &function->mir,
            tuple_type,
            fields,
            count,
            &record
        ) || !mmay_mir_add_record_construction(
            &function->mir,
            record,
            arguments,
            count,
            &construction
        ) || !mmay_function_add_register(function, tuple_type, &target)) {
        if (!mmay_checker_has_failed(function->checker)) {
            mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        }
        goto cleanup;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_RECORD_NEW;
    instruction.target = target;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.record = record;
    instruction.record_construction = construction;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!mmay_function_append_instruction(function, &instruction)) {
        goto cleanup;
    }
    result->type = tuple_type;
    result->register_id = target;
    result->valid = true;
    accepted = true;

cleanup:
    mmay_deallocate(
        &function->checker->result->allocator,
        types,
        type_bytes
    );
    mmay_deallocate(
        &function->checker->result->allocator,
        arguments,
        argument_bytes
    );
    mmay_deallocate(
        &function->checker->result->allocator,
        fields,
        field_bytes
    );
    return accepted && !mmay_checker_has_failed(function->checker);
}
