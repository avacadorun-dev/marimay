#ifndef MMAY_CHECKER_JSON_INTERNAL_H
#define MMAY_CHECKER_JSON_INTERNAL_H

#include "checker_json_api.h"
#include "package_imports_api.h"

/* Borrowed stack chain; each component is a string field or an int index. */
typedef struct MarimayJsonPath {
    const struct MarimayJsonPath *parent;
    MarimayExpressionValue component;
} MarimayJsonPath;

/* One expansion borrows checker state; it owns no allocation or source tree. */
typedef struct MarimayJsonExpansion {
    MarimayFunctionChecker *function;
    const MarimayCheckPackageSurface *surface;
    MarimayMirSpan span;
    MarimayTypeId error_type;
    MarimayTypeId result_type;
    MarimayExpressionValue cursor;
    MarimayExpressionValue result;
    MarimayMirBlockId done;
    const MarimayJsonPath *path;
} MarimayJsonExpansion;

typedef struct MarimayJsonRecord {
    const MarimayCheckRecordDeclaration *local;
    const MarimayCheckedRecord *imported;
    MarimayTypeId type;
    size_t count;
} MarimayJsonRecord;

typedef struct MarimayJsonField {
    const unsigned char *name;
    size_t name_length;
    MarimayTypeId type;
    bool is_public;
} MarimayJsonField;

bool mmay_json_error(MarimayJsonExpansion *json, const char *message);
bool mmay_json_failed(MarimayJsonExpansion *json);
const MarimayCheckedFunction *mmay_json_function(
    MarimayJsonExpansion *json, const char *name, MarimayTypeId receiver);
bool mmay_json_record(MarimayJsonExpansion *json, MarimayTypeId type,
    MarimayJsonRecord *record);
bool mmay_json_field(MarimayJsonExpansion *json, const MarimayJsonRecord *record,
    size_t index, MarimayJsonField *field);
bool mmay_json_eligible(MarimayJsonExpansion *json, MarimayTypeId type);
bool mmay_json_layout(MarimayJsonExpansion *json, MarimayTypeId type);
bool mmay_json_record_layout(MarimayJsonExpansion *json,
    const MarimayJsonRecord *record, MarimayMirRecordId *layout);
bool mmay_json_prelude_type(MarimayJsonExpansion *json, bool result,
    MarimayTypeId payload, MarimayTypeId *type);

bool mmay_json_call(MarimayJsonExpansion *json, const char *name,
    const MarimayExpressionValue *arguments, size_t count,
    MarimayExpressionValue *result);
bool mmay_json_try_call(MarimayJsonExpansion *json, const char *name,
    const MarimayExpressionValue *arguments, size_t count,
    MarimayExpressionValue *result);
bool mmay_json_enum(MarimayJsonExpansion *json, MarimayTypeId type,
    size_t variant, const MarimayExpressionValue *payload,
    MarimayExpressionValue *result);
bool mmay_json_enum_access(MarimayJsonExpansion *json,
    MarimayExpressionValue value, size_t variant, bool test,
    MarimayExpressionValue *result);
bool mmay_json_finish(MarimayJsonExpansion *json, bool error,
    MarimayExpressionValue value);
bool mmay_json_block(MarimayJsonExpansion *json, MarimayMirBlockId *block);
void mmay_json_enter(MarimayJsonExpansion *json, MarimayMirBlockId block);
bool mmay_json_goto(MarimayJsonExpansion *json, MarimayMirBlockId block);
bool mmay_json_branch(MarimayJsonExpansion *json, MarimayExpressionValue value,
    MarimayMirBlockId yes, MarimayMirBlockId no);
bool mmay_json_copy(MarimayJsonExpansion *json, MarimayExpressionValue target,
    MarimayExpressionValue value);
bool mmay_json_string(MarimayJsonExpansion *json, const unsigned char *bytes,
    size_t length, MarimayExpressionValue *result);
bool mmay_json_instruction(MarimayJsonExpansion *json,
    MarimayMirInstructionKind kind, MarimayTypeId type,
    MarimayExpressionValue first, MarimayExpressionValue second,
    int64_t integer, MarimayExpressionValue *result);
bool mmay_json_array(MarimayJsonExpansion *json, MarimayTypeId array_type,
    MarimayMirInstructionKind kind, MarimayTypeId result_type,
    MarimayExpressionValue first, MarimayExpressionValue second,
    MarimayExpressionValue *result);
bool mmay_json_read(MarimayJsonExpansion *json, MarimayTypeId type,
    MarimayExpressionValue *result);
bool mmay_json_read_at(MarimayJsonExpansion *json, MarimayTypeId type,
    MarimayExpressionValue component, MarimayExpressionValue *result);
bool mmay_json_write(MarimayJsonExpansion *json, MarimayExpressionValue value);
bool mmay_json_write_at(MarimayJsonExpansion *json, MarimayExpressionValue value,
    MarimayExpressionValue component);

#endif
