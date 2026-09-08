#include "checker_function_values_internal.h"
#include "checker_core_api.h"
#include "checker_imported_nominal_api.h"
#include "checker_expr_interfaces_api.h"
#include "checker_lookup_api.h"

#include <string.h>

void mmay_function_value_intern_failed(MarimayFunctionChecker *function) {
    const size_t maximum = function->checker->options.type_options.max_types;
    const size_t count = function->checker->result->types.descriptors.length;
    mmay_checker_fail(function->checker, maximum <= MMAY_TYPE_FIRST_DYNAMIC
        || count >= maximum - MMAY_TYPE_FIRST_DYNAMIC
        ? MMAY_CHECK_LIMIT_REACHED : MMAY_CHECK_ALLOCATION_FAILED);
}

MarimayTypeId mmay_function_value_abi_result(MarimayTypeId type) {
    return type == MMAY_TYPE_NEVER ? MMAY_TYPE_BOOL : type;
}

bool mmay_function_value_layout(MarimayFunctionChecker *function, MarimayTypeId type) {
    MarimayMirInterfaceId ignored;
    if (mmay_type_kind(&function->checker->result->types, type) != MMAY_TYPE_KIND_FUNCTION) return true;
    for (size_t i = 0; i < function->mir.interfaces.length; i += 1) {
        const MarimayMirInterface *layout = mmay_vector_at_const(&function->mir.interfaces, i);
        if (layout->type == type) return layout->method_count == 1;
    }
    if (function->mir.interfaces.length >= function->mir.options.max_interface_layouts) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED); return false;
    }
    if (!mmay_mir_add_interface(&function->mir, type, 1, &ignored)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED); return false;
    }
    return true;
}

bool mmay_function_value_symbol(MarimayFunctionChecker *function, size_t location,
    unsigned tag, MarimaySymbolId *symbol, MarimayTypeId *type) {
    unsigned char key[24];
    memcpy(key, function->mir.symbol.bytes, 16);
    for (size_t i = 0; i < 8; i += 1) key[16 + i] = (unsigned char)((uint64_t)location >> (i * 8));
    if (!mmay_symbol_id_from_bytes("mmay/closure", 12, NULL, 0, key, sizeof(key), tag, symbol)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR); return false;
    }
    if (type != NULL && !mmay_type_intern_nominal(&function->checker->result->types,
            MMAY_TYPE_KIND_NOMINAL, *symbol, NULL, 0, type)) {
        mmay_function_value_intern_failed(function); return false;
    }
    return true;
}

bool mmay_function_value_publish(MarimayFunctionChecker *function, MarimayTypeId signature) {
    MarimayCheckResult *result = function->checker->result;
    MarimayCheckedFunction checked = {0};
    MarimayMirVerifyStatus status;
    size_t index = result->functions.length;
    char name[48];
    static const char hex[] = "0123456789abcdef";
    memcpy(name, "$closure_", 9);
    for (size_t i = 0; i < 16; i += 1) {
        name[9 + i * 2] = hex[function->mir.symbol.bytes[i] >> 4];
        name[10 + i * 2] = hex[function->mir.symbol.bytes[i] & 15];
    }
    if (!mmay_checker_private_function_set_goto(function, 0, 1, 0, 0)) return false;
    if (!mmay_checker_private_function_ensure_nominal_layouts(function)
        || !mmay_checker_private_function_ensure_interface_layouts(function)) return false;
    status = mmay_mir_verify(&function->mir);
    if (status != MMAY_MIR_VERIFY_OK) {
        mmay_checker_fail(function->checker, status == MMAY_MIR_VERIFY_ALLOCATION_FAILED
            ? MMAY_CHECK_ALLOCATION_FAILED : status == MMAY_MIR_VERIFY_LIMIT_REACHED
            ? MMAY_CHECK_LIMIT_REACHED : MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (index >= function->checker->options.max_functions) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED); return false;
    }
    if (mmay_map_get_const(&result->function_index, function->mir.symbol.bytes, 16) != NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR); return false;
    }
    if (!mmay_owned_bytes_copy(&checked.name, result->allocator, name, 41)
        || !mmay_vector_reserve(&result->functions, index + 1)
        || !mmay_map_put(&result->function_index, function->mir.symbol.bytes, 16, &index)) {
        mmay_owned_bytes_dispose(&checked.name);
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED); return false;
    }
    checked.signature_type = signature;
    checked.source_symbol = function->source_symbol;
    checked.mir = function->mir;
    if (!mmay_vector_push(&result->functions, &checked)) {
        (void)mmay_map_remove(&result->function_index, function->mir.symbol.bytes, 16);
        mmay_owned_bytes_dispose(&checked.name);
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED); return false;
    }
    memset(&function->mir, 0, sizeof(function->mir));
    function->mir_initialized = false;
    return true;
}

bool mmay_function_value_wrap(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayTypeId type, MarimayExpressionValue environment, MarimaySymbolId body,
    MarimayExpressionValue *result) {
    MarimayMirInterfaceId layout;
    MarimayMirInstruction instruction = {0};
    if (function->mir.interface_constructions.length >= function->mir.options.max_interface_constructions
        || function->mir.interface_methods.length >= function->mir.options.max_interface_methods) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED); return false;
    }
    if (!mmay_function_value_layout(function, type)
        || !mmay_mir_add_interface(&function->mir, type, 1, &layout)
        || !mmay_mir_add_interface_construction(&function->mir, layout,
            environment.register_id, environment.type, &body, 1, &instruction.interface_construction)) {
        if (!mmay_checker_has_failed(function->checker))
            mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    instruction.kind = MMAY_MIR_INTERFACE_NEW;
    instruction.interface_id = layout;
    instruction.span = (MarimayMirSpan){node->start, node->end};
    if (!mmay_function_add_register(function, type, &instruction.target)
        || !mmay_function_append_instruction(function, &instruction)) return false;
    *result = (MarimayExpressionValue){type, instruction.target, true};
    return true;
}

bool mmay_function_value_init(MarimayFunctionChecker *body, MarimayFunctionChecker *parent,
    MarimaySymbolId symbol, const MarimayTypeId *parameters, size_t count, MarimayTypeId result) {
    memset(body, 0, sizeof(*body));
    body->checker = parent->checker;
    body->source_symbol = parent->source_symbol;
    body->result_type = result;
    body->test_context = parent->test_context;
    body->type_parameters = parent->type_parameters;
    body->type_arguments = parent->type_arguments;
    body->diagnostic_start = parent->checker->result->diagnostics.length;
    if (count > parent->checker->options.max_locals_per_function
        || count > parent->checker->options.mir_options.max_registers) {
        mmay_checker_fail(parent->checker, MMAY_CHECK_LIMIT_REACHED); return false;
    }
    if (!mmay_vector_init(&body->locals, parent->checker->result->allocator, sizeof(MarimayCheckLocal))
        || !mmay_vector_init(&body->loops, parent->checker->result->allocator, sizeof(MarimayLoopContext))
        || !mmay_vector_init(&body->defers, parent->checker->result->allocator, sizeof(MarimayDeferredCall))
        || !mmay_mir_function_init(&body->mir, parent->checker->result->allocator,
            symbol, parameters, count, mmay_function_value_abi_result(result), parent->checker->options.mir_options)) {
        mmay_checker_fail(parent->checker, MMAY_CHECK_ALLOCATION_FAILED); return false;
    }
    body->mir_initialized = true;
    return mmay_checker_private_function_add_block(body, &body->block)
        && mmay_checker_private_function_add_block(body, &body->block);
}

void mmay_function_value_dispose(MarimayFunctionChecker *body) {
    if (body->mir_initialized) mmay_mir_function_dispose(&body->mir);
    mmay_vector_dispose(&body->locals);
    mmay_vector_dispose(&body->loops);
    mmay_vector_dispose(&body->defers);
}

bool mmay_function_value_record(MarimayFunctionChecker *function, MarimayTypeId type,
    const MarimayMirRecordField *fields, size_t count, MarimayMirRecordId *layout) {
    bool exists = false;
    for (size_t i = 0; i < function->mir.records.length; i += 1)
        if (((const MarimayMirRecord *)mmay_vector_at_const(&function->mir.records, i))->type == type) exists = true;
    size_t field_count;
    if (!exists && (function->mir.records.length >= function->mir.options.max_record_layouts
            || !mmay_size_add(function->mir.record_fields.length, count, &field_count)
            || field_count > function->mir.options.max_record_fields)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED); return false;
    }
    if (!mmay_mir_add_record(&function->mir, type, fields, count, layout)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED); return false;
    }
    return true;
}

bool mmay_function_value_environment(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayMirRecordId layout, const MarimayMirRecordArgument *arguments, size_t count,
    MarimayExpressionValue *result) {
    MarimayMirInstruction instruction = {0};
    const MarimayMirRecord *record = mmay_vector_at_const(&function->mir.records, layout);
    MarimayTypeId type = record->type;
    size_t argument_count;
    if (function->mir.record_constructions.length >= function->mir.options.max_record_constructions
        || !mmay_size_add(function->mir.record_arguments.length, count, &argument_count)
        || argument_count > function->mir.options.max_record_arguments) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED); return false;
    }
    if (!mmay_mir_add_record_construction(&function->mir, layout, arguments, count,
            &instruction.record_construction)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED); return false;
    }
    instruction.kind = MMAY_MIR_RECORD_NEW;
    instruction.record = layout;
    instruction.span = (MarimayMirSpan){node->start, node->end};
    if (!mmay_function_add_register(function, type, &instruction.target)
        || !mmay_function_append_instruction(function, &instruction)) return false;
    *result = (MarimayExpressionValue){type, instruction.target, true};
    return true;
}
