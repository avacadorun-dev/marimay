#ifndef MMAY_MANIFEST_INTERNAL_H
#define MMAY_MANIFEST_INTERNAL_H

#include "mmay/manifest/manifest.h"

typedef struct MarimayManifestParser {
    const MarimaySourceSnapshot *snapshot;
    const unsigned char *bytes;
    size_t length;
    MarimayManifestParseResult *result;
    MarimayManifestParseOptions options;
    MarimayManifestDirectiveKind group_kind;
    bool in_group;
    bool allocation_failed;
    bool limit_reached;
} MarimayManifestParser;

typedef struct MarimayManifestFormatEntry {
    MarimayManifestDirectiveKind kind;
    MarimayOwnedBytes name;
    MarimayOwnedBytes values[2];
    size_t value_count;
    size_t source_order;
} MarimayManifestFormatEntry;

typedef struct MarimayManifestOutput {
    MarimayVector bytes;
    size_t max_bytes;
    bool limit_reached;
    bool allocation_failed;
} MarimayManifestOutput;

MarimayManifestParseStatus mmay_manifest_parse_content(MarimayManifestParser *parser);
bool mmay_manifest_first_parent(const MarimayOwnedBytes *path);

#endif
