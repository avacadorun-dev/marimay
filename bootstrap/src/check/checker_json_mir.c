#include "checker_json_internal.h"

#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_expr_string_api.h"
#include "checker_lookup_api.h"

bool mmay_json_block(MarimayJsonExpansion *json, MarimayMirBlockId *block) {
    return mmay_checker_private_function_add_block(json->function, block);
}

void mmay_json_enter(MarimayJsonExpansion *json, MarimayMirBlockId block) {
    json->function->block = block;
    json->function->terminated = false;
}

bool mmay_json_goto(MarimayJsonExpansion *json, MarimayMirBlockId block) {
    return mmay_checker_private_function_set_goto(json->function,
        json->function->block, block, json->span.start, json->span.end);
}

bool mmay_json_branch(MarimayJsonExpansion *json, MarimayExpressionValue value,
    MarimayMirBlockId yes, MarimayMirBlockId no
) {
    return mmay_checker_private_function_set_branch(json->function,
        json->function->block, value.register_id, yes, no,
        json->span.start, json->span.end);
}

bool mmay_json_copy(MarimayJsonExpansion *json, MarimayExpressionValue target,
    MarimayExpressionValue value
) {
    return mmay_checker_private_function_emit_copy(json->function,
        target.register_id, value.register_id, json->span.start, json->span.end);
}

bool mmay_json_string(MarimayJsonExpansion *json, const unsigned char *bytes,
    size_t length, MarimayExpressionValue *result
) {
    return mmay_checker_private_emit_string_constant(json->function,
        bytes, length, json->span.start, json->span.end, result);
}

bool mmay_json_instruction(MarimayJsonExpansion *json,
    MarimayMirInstructionKind kind, MarimayTypeId type,
    MarimayExpressionValue first, MarimayExpressionValue second,
    int64_t integer, MarimayExpressionValue *result
) {
    return mmay_function_emit_instruction(json->function, kind, type,
        first.valid ? first.register_id : MMAY_MIR_NO_REGISTER,
        second.valid ? second.register_id : MMAY_MIR_NO_REGISTER,
        integer, integer != 0, json->span.start, json->span.end, result);
}

bool mmay_json_call(MarimayJsonExpansion *json, const char *name,
    const MarimayExpressionValue *arguments, size_t count,
    MarimayExpressionValue *result
) {
    MarimayFunctionChecker *function = json->function;
    const MarimayCheckedFunction *callee = mmay_json_function(json, name,
        count == 0 ? MMAY_TYPE_INVALID : arguments[0].type);
    MarimayMirCallArgument call_arguments[3];
    MarimayMirInstruction instruction;
    MarimayMirCallId call = MMAY_MIR_NO_CALL;
    size_t index = 0;
    size_t total = 0;
    if (callee == NULL || count > 3 || callee->mir.parameter_count != count) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < count; index += 1) {
        const MarimayTypeId *formal = mmay_vector_at_const(
            &callee->mir.register_types, index);
        if (formal == NULL || arguments[index].type != *formal) {
            return mmay_json_error(json, "JSON argument does not match the required type");
        }
        call_arguments[index].register_id = arguments[index].register_id;
        call_arguments[index].type = arguments[index].type;
    }
    if (function->mir.calls.length >= function->mir.options.max_calls
        || !mmay_size_add(function->mir.call_arguments.length, count, &total)
        || total > function->mir.options.max_call_arguments) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!mmay_json_layout(json, callee->mir.result_type)
        || !mmay_mir_add_call(&function->mir, callee->mir.symbol,
            callee->mir.result_type, call_arguments, count, &call)) return mmay_json_failed(json);
    memset(&instruction, 0, sizeof(instruction));
    if (!mmay_function_add_register(function, callee->mir.result_type, &instruction.target)) {
        return false;
    }
    instruction.kind = MMAY_MIR_CALL;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.call = call;
    instruction.span = json->span;
    if (!mmay_function_append_instruction(function, &instruction)) return false;
    *result = (MarimayExpressionValue){callee->mir.result_type, instruction.target, true};
    return true;
}

static bool mmay_json_enum_layout(MarimayJsonExpansion *json,
    MarimayTypeId type, MarimayMirEnumId *layout
) {
    size_t index = 0;
    if (!mmay_json_layout(json, type)) return false;
    for (index = 0; index < json->function->mir.enums.length; index += 1) {
        const MarimayMirEnum *item = mmay_vector_at_const(&json->function->mir.enums, index);
        if (item != NULL && item->type == type) {
            *layout = (MarimayMirEnumId)index;
            return true;
        }
    }
    mmay_checker_fail(json->function->checker, MMAY_CHECK_INTERNAL_ERROR);
    return false;
}

bool mmay_json_enum(MarimayJsonExpansion *json, MarimayTypeId type,
    size_t variant, const MarimayExpressionValue *payload,
    MarimayExpressionValue *result
) {
    MarimayFunctionChecker *function = json->function;
    MarimayMirEnumId layout = MMAY_MIR_NO_ENUM;
    MarimayMirEnumArgument argument;
    MarimayMirInstruction instruction;
    size_t count = payload != NULL && payload->type != MMAY_TYPE_VOID ? 1 : 0;
    if (!mmay_json_enum_layout(json, type, &layout)) return false;
    if (function->mir.enum_constructions.length >= function->mir.options.max_enum_constructions
        || function->mir.enum_arguments.length > function->mir.options.max_enum_arguments
        || count > function->mir.options.max_enum_arguments - function->mir.enum_arguments.length) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    memset(&argument, 0, sizeof(argument));
    if (count != 0) argument = (MarimayMirEnumArgument){payload->register_id, payload->type};
    if (!mmay_mir_add_enum_construction(&function->mir, layout, variant,
            count == 0 ? NULL : &argument, count, &instruction.enum_construction)) {
        return mmay_json_failed(json);
    }
    if (!mmay_function_add_register(function, type, &instruction.target)) return false;
    instruction.kind = MMAY_MIR_ENUM_NEW;
    instruction.enum_id = layout;
    instruction.variant = variant;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.span = json->span;
    if (!mmay_function_append_instruction(function, &instruction)) return false;
    *result = (MarimayExpressionValue){type, instruction.target, true};
    return true;
}

bool mmay_json_enum_access(MarimayJsonExpansion *json,
    MarimayExpressionValue value, size_t variant, bool test,
    MarimayExpressionValue *result
) {
    MarimayMirEnumId layout = MMAY_MIR_NO_ENUM;
    MarimayExpressionValue required;
    MarimayTypeId payload = MMAY_TYPE_BOOL;
    if (!test) {
        const MarimayTypeDescriptor *descriptor = mmay_type_descriptor(
            &json->function->checker->result->types, value.type);
        if (descriptor == NULL || variant >= descriptor->argument_count) {
            mmay_checker_fail(json->function->checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        payload = descriptor->arguments[variant];
    }
    if (!mmay_json_enum_layout(json, value.type, &layout)) return false;
    if (test) return mmay_checker_private_function_emit_enum_access(json->function,
        MMAY_MIR_ENUM_IS_VARIANT, MMAY_TYPE_BOOL, value.register_id,
        layout, variant, 0, json->span.start, json->span.end, result);
    if (!mmay_checker_private_function_emit_enum_access(json->function,
            MMAY_MIR_ENUM_REQUIRE_VARIANT, value.type, value.register_id,
            layout, variant, 0, json->span.start, json->span.end, &required)) return false;
    if (payload == MMAY_TYPE_VOID) {
        *result = (MarimayExpressionValue){MMAY_TYPE_VOID, MMAY_MIR_NO_REGISTER, true};
        return true;
    }
    return mmay_checker_private_function_emit_enum_access(json->function,
        MMAY_MIR_ENUM_GET_PAYLOAD, payload, required.register_id,
        layout, variant, 0, json->span.start, json->span.end, result);
}

bool mmay_json_finish(MarimayJsonExpansion *json, bool error,
    MarimayExpressionValue value
) {
    MarimayExpressionValue wrapped;
    const MarimayJsonPath *path = json->path;
    while (error && path != NULL) {
        MarimayExpressionValue arguments[2] = {value, path->component};
        if (!mmay_json_call(json, path->component.type == MMAY_TYPE_STRING
                ? "atField" : "atIndex", arguments, 2, &value)) return false;
        path = path->parent;
    }
    return mmay_json_enum(json, json->result_type, error ? 1 : 0, &value, &wrapped)
        && mmay_json_copy(json, json->result, wrapped) && mmay_json_goto(json, json->done);
}

bool mmay_json_try_call(MarimayJsonExpansion *json, const char *name,
    const MarimayExpressionValue *arguments, size_t count,
    MarimayExpressionValue *result
) {
    MarimayExpressionValue value, is_error, error;
    MarimayMirBlockId fail, success;
    if (!mmay_json_call(json, name, arguments, count, &value)
        || !mmay_json_enum_access(json, value, 1, true, &is_error)
        || !mmay_json_block(json, &fail) || !mmay_json_block(json, &success)
        || !mmay_json_branch(json, is_error, fail, success)) return false;
    mmay_json_enter(json, fail);
    if (!mmay_json_enum_access(json, value, 1, false, &error)
        || !mmay_json_finish(json, true, error)) return false;
    mmay_json_enter(json, success);
    return mmay_json_enum_access(json, value, 0, false, result);
}

bool mmay_json_array(MarimayJsonExpansion *json, MarimayTypeId array_type,
    MarimayMirInstructionKind kind, MarimayTypeId result_type,
    MarimayExpressionValue first, MarimayExpressionValue second,
    MarimayExpressionValue *result
) {
    MarimayMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    if (!mmay_array_layout(json->function, array_type, &instruction.array)
        || !mmay_function_add_register(json->function, result_type, &instruction.target)) return false;
    if (kind == MMAY_MIR_ARRAY_NEW) {
        if (json->function->mir.array_constructions.length
            >= json->function->mir.options.max_array_constructions) {
            mmay_checker_fail(json->function->checker, MMAY_CHECK_LIMIT_REACHED);
            return false;
        }
        if (!mmay_mir_add_array_construction(&json->function->mir,
                instruction.array, NULL, 0, &instruction.array_construction)) return mmay_json_failed(json);
    }
    instruction.kind = kind;
    instruction.first = first.valid ? first.register_id : MMAY_MIR_NO_REGISTER;
    instruction.second = second.valid ? second.register_id : MMAY_MIR_NO_REGISTER;
    instruction.third = MMAY_MIR_NO_REGISTER;
    instruction.span = json->span;
    if (!mmay_function_append_instruction(json->function, &instruction)) return false;
    *result = (MarimayExpressionValue){result_type, instruction.target, true};
    return true;
}
