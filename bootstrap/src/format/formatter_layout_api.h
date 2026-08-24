#ifndef MMAY_FORMAT_FORMATTER_LAYOUT_API_H
#define MMAY_FORMAT_FORMATTER_LAYOUT_API_H

#include "formatter_internal.h"

bool mmay_formatter_source_gap_has_blank_line(
    const unsigned char *source,
    size_t source_length,
    size_t start,
    size_t end
);

bool mmay_formatter_index_is_compact(
    const MarimayVector *tokens,
    size_t open_index,
    size_t source_length,
    bool postfix_context
);

bool mmay_formatter_brace_is_grouped(
    const MarimayFormatToken *token,
    const MarimayVector *tokens,
    size_t open_index,
    size_t source_length
);

bool mmay_formatter_postfix_predecessor(MarimayTokenKind kind);
bool mmay_formatter_delimiter_matches(
    MarimayTokenKind open,
    MarimayTokenKind close
);
bool mmay_formatter_required_tuple_comma(
    const MarimayFormatDelimiter *delimiter
);
bool mmay_formatter_operator_is_spaced(const MarimayFormatToken *token);
bool mmay_formatter_token_is_operator(MarimayTokenKind kind);
bool mmay_formatter_open_group_needs_space(
    const MarimayFormatter *formatter,
    const MarimayFormatToken *token
);
bool mmay_formatter_trailing_comma_kind(MarimaySyntaxKind parent_kind);

#endif
