#ifndef RUDA_FORMAT_FORMATTER_INTERNAL_H
#define RUDA_FORMAT_FORMATTER_INTERNAL_H

#include "ruda/format/formatter.h"

#include "ruda/base/owned_bytes.h"
#include "ruda/base/vector.h"
#include "ruda/format/document.h"

typedef struct RudaFormatToken {
    RudaSyntaxToken token;
    RudaSyntaxKind parent_kind;
    RudaSyntaxKind top_level_kind;
    size_t top_level_id;
    bool in_for_header;
    bool in_attribute;
    bool in_record_expression;
    unsigned int import_group;
} RudaFormatToken;

typedef struct RudaFormatCollector {
    RudaVector tokens;
    size_t max_tokens;
    size_t max_nesting;
    size_t next_top_level_id;
    RudaFormatStatus status;
} RudaFormatCollector;

typedef struct RudaFormatDelimiter {
    RudaTokenKind open;
    RudaSyntaxKind parent_kind;
    bool empty;
    bool grouped;
    bool trailing_comma;
} RudaFormatDelimiter;

typedef struct RudaFormatter {
    RudaAllocator allocator;
    const RudaSourceSnapshot *snapshot;
    const unsigned char *source;
    size_t source_length;
    const RudaVector *tokens;
    RudaDocument document;
    RudaVector delimiters;
    RudaVector owned_text;
    RudaFormatOptions options;
    RudaFormatStatus status;
    RudaTokenKind previous_code_kind;
    size_t last_top_level_id;
    bool has_previous_code;
    bool pending_space;
    bool at_line_start;
    bool after_flexible_line;
    bool last_was_comment;
} RudaFormatter;

typedef struct RudaFormatOutput {
    RudaVector bytes;
    size_t max_bytes;
    bool allocation_failed;
    bool limit_reached;
} RudaFormatOutput;

bool ruda_format_token_is_trivia(RudaTokenKind kind);
bool ruda_format_token_is_comment(RudaTokenKind kind);
bool ruda_format_token_is_visible(RudaTokenKind kind);
RudaFormatStatus ruda_format_decode_ordinary_string(
    const unsigned char *source, size_t source_length,
    const RudaSyntaxToken *token, RudaAllocator allocator,
    RudaOwnedBytes *decoded);
bool ruda_format_collect_node(RudaFormatCollector *collector,
    const RudaGreenNode *node, size_t depth, RudaSyntaxKind top_level_kind,
    size_t top_level_id, bool in_for_header, bool in_attribute,
    bool in_record_expression);
RudaFormatStatus ruda_format_order_imports(RudaVector *tokens,
    const unsigned char *source, size_t source_length, RudaAllocator allocator);
bool ruda_formatter_type_prefix_is_adjacent(
    const RudaFormatDelimiter *delimiter,
    const RudaFormatToken *close,
    const RudaFormatToken *next);

#endif
