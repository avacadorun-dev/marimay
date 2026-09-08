#include "checker_closure_internal.h"
#include "checker_core_api.h"
#include "checker_lookup_api.h"

const MarimayCheckLocal *mmay_closure_find_captured_local(const MarimayFunctionChecker *function,
    MarimayCheckName name) {
    if (function->closure == NULL) return NULL;
    for (size_t i = 0; i < function->closure->captures.length; i += 1) {
        const MarimayClosureCapture *capture = mmay_vector_at_const(&function->closure->captures, i);
        if (mmay_checker_private_check_names_equal(function->checker, name, capture->local.name)) return &capture->local;
    }
    return NULL;
}

MarimayCheckLocal *mmay_closure_find_local(MarimayFunctionChecker *function, MarimayCheckName name) {
    MarimayCheckLocal *found = mmay_checker_private_function_find_local_mutable(function, name);
    MarimayClosureContext *context = function->closure;
    if (found != NULL || context == NULL) return found;
    for (size_t i = 0; i < context->captures.length; i += 1) {
        MarimayClosureCapture *capture = mmay_vector_at(&context->captures, i);
        if (mmay_checker_private_check_names_equal(function->checker, name, capture->local.name)) return &capture->local;
    }
    found = mmay_closure_find_local(context->parent, name);
    if (found == NULL) return NULL;
    MarimayCheckLocal outer = *found;
    if (!outer.initialized) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_UNINITIALIZED_LOCAL,
            "captured local must be initialized at closure creation", context->node->start, context->node->end);
        return NULL;
    }
    if (context->captures.length >= function->checker->options.max_locals_per_function) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED); return NULL;
    }
    MarimayClosureCapture capture = {0};
    MarimayMirInstruction load = {0};
    MarimayTypeId field_type = outer.cell_type != MMAY_TYPE_INVALID ? outer.cell_type : outer.type;
    capture.local = outer;
    if (outer.scope_depth == SIZE_MAX) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED); return NULL;
    }
    capture.declaring_depth = outer.scope_depth + 1;
    capture.local.scope_depth = 0;
    capture.argument = (MarimayMirRecordArgument){outer.cell_type != MMAY_TYPE_INVALID
        ? outer.cell_register : outer.register_id, field_type};
    if (!mmay_function_add_register(function, field_type, &load.target)) return NULL;
    if (outer.cell_type != MMAY_TYPE_INVALID) {
        capture.local.cell_register = load.target;
        capture.local.register_id = MMAY_MIR_NO_REGISTER;
        MarimayMirRecordField field = {outer.cell_payload_type, true};
        MarimayMirRecordId layout;
        if (!mmay_function_value_record(function, outer.cell_type, &field, 1, &layout)) return NULL;
    } else capture.local.register_id = load.target;
    load.kind = MMAY_MIR_RECORD_GET;
    load.first = 0; /* Synthetic environment is always parameter zero. */
    load.record = MMAY_MIR_NO_RECORD; /* Completed once all captures are known. */
    load.field = context->captures.length;
    load.span = (MarimayMirSpan){name.start, name.end};
    MarimayMirBlockId previous = function->block;
    function->block = 0;
    bool ok = mmay_function_append_instruction(function, &load);
    function->block = previous;
    if (!ok) return NULL;
    if (!mmay_vector_push(&context->captures, &capture)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED); return NULL;
    }
    return &((MarimayClosureCapture *)mmay_vector_at(&context->captures, context->captures.length - 1))->local;
}

bool mmay_closure_finish_captures(MarimayFunctionChecker *body, MarimayExpressionValue *environment) {
    MarimayClosureContext *context = body->closure;
    MarimayVector fields = {0}, arguments = {0};
    MarimayMirRecordId layout;
    bool ok = false;
    if (!mmay_vector_init(&fields, body->checker->result->allocator, sizeof(MarimayMirRecordField))
        || !mmay_vector_init(&arguments, body->checker->result->allocator, sizeof(MarimayMirRecordArgument))) goto allocation;
    for (size_t i = 0; i < context->captures.length; i += 1) {
        const MarimayClosureCapture *capture = mmay_vector_at_const(&context->captures, i);
        MarimayMirRecordField field = {capture->argument.type, false};
        if (!mmay_vector_push(&fields, &field) || !mmay_vector_push(&arguments, &capture->argument)) goto allocation;
    }
    if (!mmay_function_value_record(body, context->environment_type, fields.data, fields.length, &layout)) goto cleanup;
    MarimayMirBlock *entry = mmay_vector_at(&body->mir.blocks, 0);
    for (size_t i = 0; i < entry->instructions.length; i += 1) {
        MarimayMirInstruction *instruction = mmay_vector_at(&entry->instructions, i);
        if (instruction->kind == MMAY_MIR_RECORD_GET && instruction->record == MMAY_MIR_NO_RECORD)
            instruction->record = layout;
    }
    if (!mmay_function_value_record(context->parent, context->environment_type, fields.data, fields.length, &layout)
        || !mmay_function_value_environment(context->parent, context->node, layout,
            arguments.data, arguments.length, environment)) goto cleanup;
    ok = true;
    goto cleanup;
allocation:
    mmay_checker_fail(body->checker, MMAY_CHECK_ALLOCATION_FAILED);
cleanup:
    mmay_vector_dispose(&fields);
    mmay_vector_dispose(&arguments);
    return ok;
}
