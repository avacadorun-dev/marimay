#include "mmay/lex/lexer.h"

MarimayLexOptions mmay_lex_default_options(void) {
    MarimayLexOptions options;
    options.max_tokens = 1000000;
    options.max_diagnostics = 100;
    options.max_nesting = 256;
    return options;
}

bool mmay_lex_options_valid(MarimayLexOptions options) {
    return options.max_tokens >= 2 && options.max_diagnostics != 0
        && options.max_nesting >= 2;
}

void mmay_lex_result_dispose(MarimayLexResult *result) {
    if (result == NULL || !result->initialized) {
        return;
    }
    mmay_vector_dispose(&result->tokens);
    mmay_vector_dispose(&result->diagnostics);
    result->snapshot = NULL;
    result->suppressed_diagnostics = 0;
    result->initialized = false;
}

const MarimayToken *mmay_lex_token(const MarimayLexResult *result, size_t index) {
    if (result == NULL || !result->initialized) {
        return NULL;
    }
    return mmay_vector_at_const(&result->tokens, index);
}

const MarimayLexDiagnostic *mmay_lex_diagnostic(
    const MarimayLexResult *result,
    size_t index
) {
    if (result == NULL || !result->initialized) {
        return NULL;
    }
    return mmay_vector_at_const(&result->diagnostics, index);
}
