#ifndef MMAY_FORMAT_FORMATTER_INTERNAL_H
#define MMAY_FORMAT_FORMATTER_INTERNAL_H

#include "mmay/format/formatter.h"

#include "mmay/base/owned_bytes.h"
#include "mmay/base/vector.h"
#include "mmay/format/document.h"

typedef struct MarimayFormatToken {
    MarimaySyntaxToken token;
    MarimaySyntaxKind parent_kind;
    MarimaySyntaxKind top_level_kind;
    size_t top_level_id;
    bool in_for_header;
    bool in_attribute;
    bool in_record_expression;
    unsigned int import_group;
} MarimayFormatToken;

typedef struct MarimayFormatCollector {
    MarimayVector tokens;
    size_t max_tokens;
    size_t max_nesting;
    size_t next_top_level_id;
    MarimayFormatStatus status;
} MarimayFormatCollector;

typedef struct MarimayFormatDelimiter {
    MarimayTokenKind open;
    MarimaySyntaxKind parent_kind;
    bool empty;
    bool grouped;
    bool trailing_comma;
} MarimayFormatDelimiter;

typedef struct MarimayFormatter {
    MarimayAllocator allocator;
    const MarimaySourceSnapshot *snapshot;
    const unsigned char *source;
    size_t source_length;
    const MarimayVector *tokens;
    MarimayDocument document;
    MarimayVector delimiters;
    MarimayVector owned_text;
    MarimayFormatOptions options;
    MarimayFormatStatus status;
    MarimayTokenKind previous_code_kind;
    size_t last_top_level_id;
    bool has_previous_code;
    bool pending_space;
    bool at_line_start;
    bool after_flexible_line;
    bool last_was_comment;
} MarimayFormatter;

typedef struct MarimayFormatOutput {
    MarimayVector bytes;
    size_t max_bytes;
    bool allocation_failed;
    bool limit_reached;
} MarimayFormatOutput;

bool mmay_format_token_is_trivia(MarimayTokenKind kind);
bool mmay_format_token_is_comment(MarimayTokenKind kind);
bool mmay_format_token_is_visible(MarimayTokenKind kind);
MarimayFormatStatus mmay_format_decode_ordinary_string(
    const unsigned char *source, size_t source_length,
    const MarimaySyntaxToken *token, MarimayAllocator allocator,
    MarimayOwnedBytes *decoded);
bool mmay_format_collect_node(MarimayFormatCollector *collector,
    const MarimayGreenNode *node, size_t depth, MarimaySyntaxKind top_level_kind,
    size_t top_level_id, bool in_for_header, bool in_attribute,
    bool in_record_expression);
MarimayFormatStatus mmay_format_order_imports(MarimayVector *tokens,
    const unsigned char *source, size_t source_length, MarimayAllocator allocator);
bool mmay_formatter_type_prefix_is_adjacent(
    const MarimayFormatDelimiter *delimiter,
    const MarimayFormatToken *close,
    const MarimayFormatToken *next);

#endif
