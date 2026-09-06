#include "checker_intrinsics_api.h"
#include "checker_json_api.h"

#include <string.h>

static const char mmay_json_constraint[] =
    "T: int, bool, string, arrays, or acyclic public-field records; "
    "other types require an explicit codec";

static const MarimayIntrinsicSignature mmay_json_intrinsics[] = {
    {MMAY_INTRINSIC_JSON_DECODE, "std/json", "decode",
        "fn decode<T>(contents: bytes.Bytes): Result<T, json.Error>",
        mmay_json_constraint, 1, 1, 1, true},
    {MMAY_INTRINSIC_JSON_DECODE_WITH_LIMITS, "std/json", "decodeWithLimits",
        "fn decodeWithLimits<T>(contents: bytes.Bytes, limits: json.Limits): Result<T, json.Error>",
        mmay_json_constraint, 1, 1, 2, true},
    {MMAY_INTRINSIC_JSON_ENCODE, "std/json", "encode",
        "fn encode<T>(value: T): Result<bytes.Bytes, json.Error>",
        mmay_json_constraint, 0, 1, 1, true},
    {MMAY_INTRINSIC_JSON_ENCODE_WITH_LIMITS, "std/json", "encodeWithLimits",
        "fn encodeWithLimits<T>(value: T, limits: json.Limits): Result<bytes.Bytes, json.Error>",
        mmay_json_constraint, 0, 1, 2, true}
};

const MarimayIntrinsicSignature *mmay_json_intrinsic_at(size_t index) {
    return index < sizeof(mmay_json_intrinsics) / sizeof(mmay_json_intrinsics[0])
        ? &mmay_json_intrinsics[index] : NULL;
}

const MarimayIntrinsicSignature *mmay_json_intrinsic_find(
    const unsigned char *package, size_t package_length,
    const unsigned char *name, size_t name_length
) {
    size_t index;
    if (package == NULL || name == NULL) return NULL;
    for (index = 0; index < sizeof(mmay_json_intrinsics) / sizeof(mmay_json_intrinsics[0]); index += 1) {
        const MarimayIntrinsicSignature *item = &mmay_json_intrinsics[index];
        if (package_length == strlen(item->package_name)
            && memcmp(package, item->package_name, package_length) == 0
            && name_length == strlen(item->name)
            && memcmp(name, item->name, name_length) == 0) return item;
    }
    return NULL;
}

bool mmay_checker_json_export(const unsigned char *package, size_t package_length,
    const unsigned char *name, size_t name_length
) {
    return mmay_json_intrinsic_find(package, package_length, name, name_length) != NULL;
}
