#include "checker_json_internal.h"

#include "checker_core_api.h"

static bool mmay_json_read_array(MarimayJsonExpansion *json, MarimayTypeId type,
    MarimayTypeId element, MarimayExpressionValue *result
) {
    MarimayExpressionValue unused, has_next, item, index;
    MarimayMirBlockId head, body, done;
    if (!mmay_json_try_call(json, "beginArray", &json->cursor, 1, &unused)
        || !mmay_json_array(json, type, MMAY_MIR_ARRAY_NEW, type,
            (MarimayExpressionValue){0}, (MarimayExpressionValue){0}, result)
        || !mmay_json_block(json, &head) || !mmay_json_block(json, &body)
        || !mmay_json_block(json, &done) || !mmay_json_goto(json, head)) return false;
    mmay_json_enter(json, head);
    if (!mmay_json_try_call(json, "nextElement", &json->cursor, 1, &has_next)
        || !mmay_json_branch(json, has_next, body, done)) return false;
    mmay_json_enter(json, body);
    if (!mmay_json_array(json, type, MMAY_MIR_ARRAY_LENGTH, MMAY_TYPE_I64,
            *result, (MarimayExpressionValue){0}, &index)
        || !mmay_json_read_at(json, element, index, &item)
        || !mmay_json_array(json, type, MMAY_MIR_ARRAY_PUSH, type, *result, item, &unused)
        || !mmay_json_goto(json, head)) return false;
    mmay_json_enter(json, done);
    return true;
}

static bool mmay_json_read_unknown(MarimayJsonExpansion *json,
    MarimayExpressionValue name
) {
    const MarimayCheckedFunction *callee = mmay_json_function(json,
        "handleUnknown", json->cursor.type);
    const MarimayTypeId *policy = callee != NULL
        ? mmay_vector_at_const(&callee->mir.register_types, 2) : NULL;
    MarimayExpressionValue arguments[3] = {json->cursor, name, {0}};
    MarimayExpressionValue unused;
    if (policy == NULL) {
        mmay_checker_fail(json->function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    return mmay_json_enum(json, *policy, 0, NULL, &arguments[2])
        && mmay_json_try_call(json, "handleUnknown", arguments, 3, &unused);
}

static bool mmay_json_required(MarimayJsonExpansion *json,
    MarimayExpressionValue slot, const MarimayJsonField *field,
    MarimayExpressionValue *result
) {
    MarimayExpressionValue present, error, arguments[2];
    MarimayMirBlockId yes, no;
    if (!mmay_json_enum_access(json, slot, 0, true, &present)
        || !mmay_json_block(json, &yes) || !mmay_json_block(json, &no)
        || !mmay_json_branch(json, present, yes, no)) return false;
    mmay_json_enter(json, no);
    arguments[0] = json->cursor;
    if (!mmay_json_string(json, field->name, field->name_length, &arguments[1])
        || !mmay_json_call(json, "missingField", arguments, 2, &error)
        || !mmay_json_finish(json, true, error)) return false;
    mmay_json_enter(json, yes);
    return mmay_json_enum_access(json, slot, 0, false, result);
}

static bool mmay_json_new_record(MarimayJsonExpansion *json,
    const MarimayJsonRecord *record, const MarimayMirRecordArgument *arguments,
    MarimayExpressionValue *result
) {
    MarimayFunctionChecker *function = json->function;
    MarimayMirInstruction instruction;
    size_t total = 0;
    memset(&instruction, 0, sizeof(instruction));
    if (function->mir.record_constructions.length >= function->mir.options.max_record_constructions
        || !mmay_size_add(function->mir.record_arguments.length, record->count, &total)
        || total > function->mir.options.max_record_arguments) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!mmay_json_record_layout(json, record, &instruction.record)
        || !mmay_mir_add_record_construction(&function->mir, instruction.record,
            arguments, record->count, &instruction.record_construction)) return mmay_json_failed(json);
    if (!mmay_function_add_register(function, record->type, &instruction.target)) return false;
    instruction.kind = MMAY_MIR_RECORD_NEW;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.span = json->span;
    if (!mmay_function_append_instruction(function, &instruction)) return false;
    *result = (MarimayExpressionValue){record->type, instruction.target, true};
    return true;
}

static bool mmay_json_read_record(MarimayJsonExpansion *json,
    const MarimayJsonRecord *record, MarimayExpressionValue *result
) {
    MarimayAllocator allocator = json->function->checker->result->allocator;
    MarimayExpressionValue *slots = NULL;
    MarimayMirRecordArgument *arguments = NULL;
    size_t slots_bytes = 0, arguments_bytes = 0, index = 0;
    MarimayExpressionValue unused, field_option, has_field, name;
    MarimayMirBlockId head, body, done;
    bool accepted = false;
    if (!mmay_size_multiply(record->count, sizeof(*slots), &slots_bytes)
        || !mmay_size_multiply(record->count, sizeof(*arguments), &arguments_bytes)) {
        mmay_checker_fail(json->function->checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    slots = slots_bytes == 0 ? NULL : mmay_allocate(&allocator, slots_bytes);
    arguments = arguments_bytes == 0 ? NULL : mmay_allocate(&allocator, arguments_bytes);
    if ((slots_bytes != 0 && slots == NULL) || (arguments_bytes != 0 && arguments == NULL)) {
        (void)mmay_json_failed(json);
        goto cleanup;
    }
    if (!mmay_json_try_call(json, "beginObject", &json->cursor, 1, &unused)) goto cleanup;
    for (index = 0; index < record->count; index += 1) {
        MarimayJsonField field;
        MarimayTypeId option;
        if (!mmay_json_field(json, record, index, &field)
            || !mmay_json_prelude_type(json, false, field.type, &option)
            || !mmay_json_enum(json, option, 1, NULL, &slots[index])) goto cleanup;
    }
    if (!mmay_json_block(json, &head) || !mmay_json_block(json, &body)
        || !mmay_json_block(json, &done) || !mmay_json_goto(json, head)) goto cleanup;
    mmay_json_enter(json, head);
    if (!mmay_json_try_call(json, "nextField", &json->cursor, 1, &field_option)
        || !mmay_json_enum_access(json, field_option, 0, true, &has_field)
        || !mmay_json_branch(json, has_field, body, done)) goto cleanup;
    mmay_json_enter(json, body);
    if (!mmay_json_enum_access(json, field_option, 0, false, &name)) goto cleanup;
    for (index = 0; index < record->count; index += 1) {
        MarimayJsonField field;
        MarimayExpressionValue key, same, value, some;
        MarimayMirBlockId yes, no;
        if (!mmay_json_field(json, record, index, &field)
            || !mmay_json_string(json, field.name, field.name_length, &key)
            || !mmay_json_instruction(json, MMAY_MIR_EQUAL_STRING, MMAY_TYPE_BOOL,
                name, key, 0, &same)
            || !mmay_json_block(json, &yes) || !mmay_json_block(json, &no)
            || !mmay_json_branch(json, same, yes, no)) goto cleanup;
        mmay_json_enter(json, yes);
        if (!mmay_json_read_at(json, field.type, key, &value)
            || !mmay_json_enum(json, slots[index].type, 0, &value, &some)
            || !mmay_json_copy(json, slots[index], some)
            || !mmay_json_goto(json, head)) goto cleanup;
        mmay_json_enter(json, no);
    }
    if (!mmay_json_read_unknown(json, name) || !mmay_json_goto(json, head)) goto cleanup;
    mmay_json_enter(json, done);
    for (index = 0; index < record->count; index += 1) {
        MarimayJsonField field;
        MarimayExpressionValue value;
        if (!mmay_json_field(json, record, index, &field)
            || !mmay_json_required(json, slots[index], &field, &value)) goto cleanup;
        arguments[index] = (MarimayMirRecordArgument){value.register_id, value.type};
    }
    accepted = mmay_json_new_record(json, record, arguments, result);
cleanup:
    mmay_deallocate(&allocator, arguments, arguments_bytes);
    mmay_deallocate(&allocator, slots, slots_bytes);
    return accepted;
}

bool mmay_json_read(MarimayJsonExpansion *json, MarimayTypeId type,
    MarimayExpressionValue *result
) {
    const char *primitive = type == MMAY_TYPE_I64 ? "readInt"
        : type == MMAY_TYPE_BOOL ? "readBool" : type == MMAY_TYPE_STRING ? "readString" : NULL;
    MarimayTypeId element = MMAY_TYPE_INVALID;
    MarimayJsonRecord record;
    if (primitive != NULL) return mmay_json_try_call(json, primitive, &json->cursor, 1, result);
    if (mmay_type_array_element(&json->function->checker->result->types, type, &element)) {
        return mmay_json_read_array(json, type, element, result);
    }
    if (mmay_json_record(json, type, &record)) return mmay_json_read_record(json, &record, result);
    return mmay_json_error(json, "unsupported automatic JSON type; use an explicit codec");
}

bool mmay_json_read_at(MarimayJsonExpansion *json, MarimayTypeId type,
    MarimayExpressionValue component, MarimayExpressionValue *result
) {
    const MarimayJsonPath path = {json->path, component};
    bool accepted;
    json->path = &path;
    accepted = mmay_json_read(json, type, result);
    json->path = path.parent;
    return accepted;
}
