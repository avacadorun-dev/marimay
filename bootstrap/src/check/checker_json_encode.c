#include "checker_json_internal.h"

#include "checker_core_api.h"
#include "checker_lookup_api.h"

static bool mmay_json_write_array(MarimayJsonExpansion *json,
    MarimayExpressionValue value, MarimayTypeId element
) {
    MarimayExpressionValue unused, index, count, has_next, checked, item, one, next;
    MarimayMirBlockId head, body, done;
    if (!mmay_json_try_call(json, "beginArray", &json->cursor, 1, &unused)
        || !mmay_json_instruction(json, MMAY_MIR_CONSTANT_I64, MMAY_TYPE_I64,
            (MarimayExpressionValue){0}, (MarimayExpressionValue){0}, 0, &index)
        || !mmay_json_array(json, value.type, MMAY_MIR_ARRAY_LENGTH, MMAY_TYPE_I64,
            value, (MarimayExpressionValue){0}, &count)
        || !mmay_json_block(json, &head) || !mmay_json_block(json, &body)
        || !mmay_json_block(json, &done) || !mmay_json_goto(json, head)) return false;
    mmay_json_enter(json, head);
    if (!mmay_json_instruction(json, MMAY_MIR_LESS_I64, MMAY_TYPE_BOOL, index, count, 0, &has_next)
        || !mmay_json_branch(json, has_next, body, done)) return false;
    mmay_json_enter(json, body);
    if (!mmay_json_array(json, value.type, MMAY_MIR_ARRAY_REQUIRE_INDEX, value.type,
            value, index, &checked)
        || !mmay_json_array(json, value.type, MMAY_MIR_ARRAY_GET, element, checked, index, &item)
        || !mmay_json_write_at(json, item, index)
        || !mmay_json_instruction(json, MMAY_MIR_CONSTANT_I64, MMAY_TYPE_I64,
            (MarimayExpressionValue){0}, (MarimayExpressionValue){0}, 1, &one)
        || !mmay_json_instruction(json, MMAY_MIR_ADD_CHECKED_I64, MMAY_TYPE_I64, index, one, 0, &next)
        || !mmay_json_copy(json, index, next) || !mmay_json_goto(json, head)) return false;
    mmay_json_enter(json, done);
    return mmay_json_try_call(json, "endArray", &json->cursor, 1, &unused);
}

static bool mmay_json_write_record(MarimayJsonExpansion *json,
    const MarimayJsonRecord *record, MarimayExpressionValue value
) {
    MarimayExpressionValue unused;
    MarimayMirRecordId layout;
    size_t index = 0;
    if (!mmay_json_record_layout(json, record, &layout)
        || !mmay_json_try_call(json, "beginObject", &json->cursor, 1, &unused)) return false;
    for (index = 0; index < record->count; index += 1) {
        MarimayJsonField field;
        MarimayExpressionValue arguments[2] = {json->cursor, {0}}, member;
        MarimayJsonPath path;
        bool accepted;
        if (!mmay_json_field(json, record, index, &field)
            || !mmay_json_string(json, field.name, field.name_length, &arguments[1])) return false;
        path = (MarimayJsonPath){json->path, arguments[1]};
        json->path = &path;
        accepted = mmay_json_try_call(json, "writeKey", arguments, 2, &unused)
            && mmay_checker_private_function_emit_record_access(json->function,
                MMAY_MIR_RECORD_GET, field.type, value.register_id,
                MMAY_MIR_NO_REGISTER, layout, index, json->span.start, json->span.end, &member)
            && mmay_json_write(json, member);
        json->path = path.parent;
        if (!accepted) return false;
    }
    return mmay_json_try_call(json, "endObject", &json->cursor, 1, &unused);
}

bool mmay_json_write(MarimayJsonExpansion *json, MarimayExpressionValue value) {
    const char *primitive = value.type == MMAY_TYPE_I64 ? "writeInt"
        : value.type == MMAY_TYPE_BOOL ? "writeBool" : value.type == MMAY_TYPE_STRING ? "writeString" : NULL;
    MarimayTypeId element = MMAY_TYPE_INVALID;
    MarimayExpressionValue arguments[2] = {json->cursor, value}, unused;
    MarimayJsonRecord record;
    if (primitive != NULL) return mmay_json_try_call(json, primitive, arguments, 2, &unused);
    if (mmay_type_array_element(&json->function->checker->result->types, value.type, &element)) {
        return mmay_json_write_array(json, value, element);
    }
    if (mmay_json_record(json, value.type, &record)) return mmay_json_write_record(json, &record, value);
    return mmay_json_error(json, "unsupported automatic JSON type; use an explicit codec");
}

bool mmay_json_write_at(MarimayJsonExpansion *json, MarimayExpressionValue value,
    MarimayExpressionValue component
) {
    const MarimayJsonPath path = {json->path, component};
    bool accepted;
    json->path = &path;
    accepted = mmay_json_write(json, value);
    json->path = path.parent;
    return accepted;
}
