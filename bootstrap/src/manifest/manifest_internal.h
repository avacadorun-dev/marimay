#ifndef RUDA_MANIFEST_INTERNAL_H
#define RUDA_MANIFEST_INTERNAL_H

#include "ruda/manifest/manifest.h"

typedef struct RudaManifestParser {
    const RudaSourceSnapshot *snapshot;
    const unsigned char *bytes;
    size_t length;
    RudaManifestParseResult *result;
    RudaManifestParseOptions options;
    RudaManifestDirectiveKind group_kind;
    bool in_group;
    bool allocation_failed;
    bool limit_reached;
} RudaManifestParser;

typedef struct RudaManifestFormatEntry {
    RudaManifestDirectiveKind kind;
    RudaOwnedBytes name;
    RudaOwnedBytes values[2];
    size_t value_count;
    size_t source_order;
} RudaManifestFormatEntry;

typedef struct RudaManifestOutput {
    RudaVector bytes;
    size_t max_bytes;
    bool limit_reached;
    bool allocation_failed;
} RudaManifestOutput;

RudaManifestParseStatus ruda_manifest_parse_content(RudaManifestParser *parser);
bool ruda_manifest_first_parent(const RudaOwnedBytes *path);

#endif
