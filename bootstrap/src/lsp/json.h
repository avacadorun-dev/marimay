#ifndef MMAY_LSP_JSON_H
#define MMAY_LSP_JSON_H

#include <stdint.h>
#include "mmay/base/owned_bytes.h"
#include "mmay/base/vector.h"
#include "mmay/base/writer.h"

typedef enum MarimayLspJsonKind {
    MMAY_LSP_JSON_OBJECT, MMAY_LSP_JSON_ARRAY, MMAY_LSP_JSON_STRING,
    MMAY_LSP_JSON_NUMBER, MMAY_LSP_JSON_TRUE, MMAY_LSP_JSON_FALSE,
    MMAY_LSP_JSON_NULL
} MarimayLspJsonKind;

typedef struct MarimayLspJsonToken {
    MarimayLspJsonKind kind;
    size_t start;
    size_t end;
    size_t next; /* First token after this value and its descendants. */
} MarimayLspJsonToken;

typedef struct MarimayLspJson {
    MarimayAllocator allocator;
    const unsigned char *source; /* Borrowed until dispose. */
    size_t length;
    MarimayVector tokens;
} MarimayLspJson;

/* Zero-initialized destination. Failure leaves it empty and disposable. */
bool mmay_lsp_json_parse(MarimayLspJson *json, MarimayAllocator allocator,
    const void *source, size_t length);
void mmay_lsp_json_dispose(MarimayLspJson *json);
const MarimayLspJsonToken *mmay_lsp_json_token(const MarimayLspJson *json,
    size_t index);
/* Missing/wrong-container lookups return SIZE_MAX. Keys compare decoded text. */
size_t mmay_lsp_json_get(const MarimayLspJson *json, size_t object,
    const char *key);
size_t mmay_lsp_json_at(const MarimayLspJson *json, size_t array, size_t index);
bool mmay_lsp_json_string(const MarimayLspJson *json, size_t index,
    MarimayOwnedBytes *output);
bool mmay_lsp_json_equal(const MarimayLspJson *json, size_t index,
    const char *text);
bool mmay_lsp_json_integer(const MarimayLspJson *json, size_t index,
    int64_t *value);
/* Writes a quoted JSON string, rejecting invalid UTF-8. */
bool mmay_lsp_json_write_string(MarimayWriter writer, const void *bytes,
    size_t length);

#endif
