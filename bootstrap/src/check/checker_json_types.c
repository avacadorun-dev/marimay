#include "checker_json_internal.h"

#include "checker_core_api.h"
#include "checker_generic_types_api.h"
#include "checker_imported_nominal_api.h"
#include "checker_lookup_api.h"

bool mmay_json_error(MarimayJsonExpansion *json, const char *message) {
    (void)mmay_checker_add_diagnostic(json->function->checker,
        MMAY_DIAGNOSTIC_TYPE_MISMATCH, message, json->span.start, json->span.end);
    return false;
}

bool mmay_json_failed(MarimayJsonExpansion *json) {
    if (!mmay_checker_has_failed(json->function->checker)) {
        mmay_checker_fail(json->function->checker, MMAY_CHECK_ALLOCATION_FAILED);
    }
    return false;
}

const MarimayCheckedFunction *mmay_json_function(
    MarimayJsonExpansion *json, const char *name, MarimayTypeId receiver
) {
    size_t index = 0;
    size_t length = strlen(name);
    for (index = 0; index < json->surface->function_count; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            json->function->checker->result, json->surface->function_start + index);
        if (function != NULL && function->is_public
            && function->name.length == length
            && memcmp(function->name.data, name, length) == 0
            && (receiver == MMAY_TYPE_INVALID || !function->dot_eligible
                || receiver == function->receiver_type)) return function;
    }
    return NULL;
}

bool mmay_json_record(MarimayJsonExpansion *json, MarimayTypeId type,
    MarimayJsonRecord *record
) {
    MarimayChecker *checker = json->function->checker;
    size_t index = 0;
    memset(record, 0, sizeof(*record));
    record->type = type;
    record->local = mmay_checker_private_checker_find_record_by_type(checker, type);
    if (record->local != NULL) {
        record->count = record->local->fields.length;
        return true;
    }
    /* Reachable public field types need not have a separate direct import. */
    for (index = 0; index < checker->result->records.length; index += 1) {
        const MarimayCheckedRecord *candidate = mmay_checked_record(checker->result, index);
        if (candidate != NULL && mmay_checker_private_type_matches_nominal(
                checker, type, candidate->symbol, candidate->type_parameters.length)) {
            record->imported = candidate;
            record->count = candidate->fields.length;
            return true;
        }
    }
    return false;
}

bool mmay_json_field(MarimayJsonExpansion *json, const MarimayJsonRecord *record,
    size_t index, MarimayJsonField *field
) {
    const MarimayVector *parameters = NULL;
    MarimayTypeId type = MMAY_TYPE_INVALID;
    if (record->local != NULL) {
        const MarimayCheckRecordFieldDeclaration *item =
            mmay_vector_at_const(&record->local->fields, index);
        if (item == NULL) return false;
        field->name = item->owned_name.data;
        field->name_length = item->owned_name.length;
        field->is_public = item->is_public;
        type = item->type;
        parameters = &record->local->type_parameters;
    } else {
        const MarimayCheckedRecordField *item =
            mmay_vector_at_const(&record->imported->fields, index);
        if (item == NULL) return false;
        field->name = item->name.data;
        field->name_length = item->name.length;
        field->is_public = item->is_public;
        type = item->type;
        parameters = &record->imported->type_parameters;
    }
    return mmay_checker_private_substitute_nominal_member_type(
        json->function, record->type, parameters, type, &field->type);
}

static bool mmay_json_visit(MarimayJsonExpansion *json, MarimayTypeId type,
    MarimayTypeId *ancestors, size_t depth, size_t *remaining
) {
    MarimayTypeId element = MMAY_TYPE_INVALID;
    MarimayJsonRecord record;
    size_t index = 0;
    if (*remaining == 0 || depth == 128) {
        return mmay_json_error(json,
            "automatic JSON type expansion is too large; use an explicit codec");
    }
    *remaining -= 1;
    if (type == MMAY_TYPE_I64 || type == MMAY_TYPE_BOOL || type == MMAY_TYPE_STRING) {
        return true;
    }
    for (index = 0; index < depth; index += 1) {
        if (ancestors[index] == type) return mmay_json_error(json,
            "automatic JSON does not support recursive types; use an explicit codec");
    }
    ancestors[depth] = type;
    if (mmay_type_array_element(&json->function->checker->result->types, type, &element)) {
        return mmay_json_visit(json, element, ancestors, depth + 1, remaining);
    }
    if (!mmay_json_record(json, type, &record)) return mmay_json_error(json,
        "automatic JSON requires int, bool, string, arrays or public-field records; use an explicit codec");
    for (index = 0; index < record.count; index += 1) {
        MarimayJsonField field;
        if (!mmay_json_field(json, &record, index, &field)) return false;
        if (!field.is_public) return mmay_json_error(json,
            "automatic JSON cannot serialize private fields; use an explicit codec");
        if (!mmay_json_visit(json, field.type, ancestors, depth + 1, remaining)) return false;
    }
    return true;
}

bool mmay_json_eligible(MarimayJsonExpansion *json, MarimayTypeId type) {
    MarimayTypeId ancestors[128];
    size_t remaining = json->function->mir.options.max_registers;
    return mmay_json_visit(json, type, ancestors, 0, &remaining);
}

bool mmay_json_record_layout(MarimayJsonExpansion *json,
    const MarimayJsonRecord *record, MarimayMirRecordId *layout
) {
    if (record->local != NULL) {
        MarimayCheckRecordDeclaration concrete = *record->local;
        concrete.type = record->type;
        return mmay_checker_private_function_record_layout(json->function, &concrete, layout);
    }
    return mmay_checker_private_imported_record_layout(
        json->function, record->imported, record->type, layout);
}

bool mmay_json_layout(MarimayJsonExpansion *json, MarimayTypeId type) {
    MarimayFunctionChecker *function = json->function;
    MarimayJsonRecord record;
    MarimayMirRecordId record_id = MMAY_MIR_NO_RECORD;
    MarimayMirEnumId enum_id = MMAY_MIR_NO_ENUM;
    MarimayCheckEnumDeclaration *enum_value = NULL;
    const MarimayCheckedEnum *imported = NULL;
    MarimayTypeId element = MMAY_TYPE_INVALID;
    size_t index = 0;
    if (type < MMAY_TYPE_FIRST_DYNAMIC) return true;
    if (mmay_type_array_element(&function->checker->result->types, type, &element)) {
        return mmay_json_layout(json, element) && mmay_function_ensure_array_layout(function, type);
    }
    if (mmay_json_record(json, type, &record)) {
        if (!mmay_json_record_layout(json, &record, &record_id)) return false;
        for (index = 0; index < record.count; index += 1) {
            MarimayJsonField field;
            if (!mmay_json_field(json, &record, index, &field)
                || !mmay_json_layout(json, field.type)) return false;
        }
        return true;
    }
    enum_value = mmay_checker_private_checker_find_enum_by_type(function->checker, type);
    if (enum_value != NULL) {
        MarimayCheckEnumDeclaration concrete = *enum_value;
        concrete.type = type;
        return mmay_checker_private_function_enum_layout(function, &concrete, &enum_id);
    }
    imported = mmay_check_imported_enum_by_type(function->checker, type);
    if (imported != NULL) return mmay_checker_private_imported_enum_layout(
        function, imported, type, &enum_id);
    /* Collection layouts of cursor internals are ensured by the normal owner. */
    return mmay_checker_private_function_ensure_nominal_layouts(function);
}

bool mmay_json_prelude_type(MarimayJsonExpansion *json, bool result,
    MarimayTypeId payload, MarimayTypeId *type
) {
    MarimayChecker *checker = json->function->checker;
    const MarimayCheckEnumDeclaration *base = mmay_vector_at_const(
        &checker->prelude_enums, result ? 1 : 0);
    MarimayTypeId arguments[2] = {payload, json->error_type};
    if (base == NULL || !mmay_type_intern_nominal(&checker->result->types,
            MMAY_TYPE_KIND_NOMINAL, base->symbol, arguments, result ? 2 : 1, type)) {
        return mmay_json_failed(json);
    }
    return mmay_json_layout(json, payload) && mmay_json_layout(json, *type);
}
