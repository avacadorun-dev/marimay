#include "formatter_internal.h"

bool mmay_formatter_type_prefix_is_adjacent(
    const MarimayFormatDelimiter *delimiter,
    const MarimayFormatToken *close,
    const MarimayFormatToken *next
) {
    return delimiter != NULL && close != NULL && next != NULL
        && delimiter->open == MMAY_TOKEN_LEFT_BRACKET
        && delimiter->parent_kind == MMAY_SYNTAX_TYPE
        && delimiter->empty
        && close->token.kind == MMAY_TOKEN_RIGHT_BRACKET
        && next->parent_kind == MMAY_SYNTAX_TYPE
        && !mmay_format_token_is_comment(next->token.kind);
}
