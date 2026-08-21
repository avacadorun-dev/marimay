#include "formatter_internal.h"

bool ruda_formatter_type_prefix_is_adjacent(
    const RudaFormatDelimiter *delimiter,
    const RudaFormatToken *close,
    const RudaFormatToken *next
) {
    return delimiter != NULL && close != NULL && next != NULL
        && delimiter->open == RUDA_TOKEN_LEFT_BRACKET
        && delimiter->parent_kind == RUDA_SYNTAX_TYPE
        && delimiter->empty
        && close->token.kind == RUDA_TOKEN_RIGHT_BRACKET
        && next->parent_kind == RUDA_SYNTAX_TYPE
        && !ruda_format_token_is_comment(next->token.kind);
}
