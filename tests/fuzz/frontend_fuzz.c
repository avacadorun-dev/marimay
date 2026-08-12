#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "ruda/lex/lexer.h"
#include "ruda/parse/parser.h"
#include "ruda/source/source_manager.h"
#include "ruda/syntax/syntax.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    RudaSourceManager sources;
    const RudaSourceSnapshot *snapshot = NULL;
    RudaLexResult lexed;
    RudaParseResult parsed;
    RudaLexOptions lex_options = ruda_lex_default_options();
    RudaParseOptions parse_options = ruda_parse_default_options();
    RudaLexStatus lex_status;
    RudaParseStatus parse_status;
    if (size > 1024U * 1024U) {
        return 0;
    }
    lex_options.max_tokens = 100000;
    lex_options.max_diagnostics = 128;
    lex_options.max_nesting = 128;
    parse_options.max_events = 400000;
    parse_options.max_diagnostics = 128;
    parse_options.max_nesting = 128;
    if (!ruda_source_manager_init(&sources, ruda_system_allocator())) {
        abort();
    }
    if (ruda_source_manager_add(
            &sources,
            "fuzz.ruda",
            sizeof("fuzz.ruda") - 1,
            data,
            size,
            &snapshot
        ) != RUDA_SOURCE_OK) {
        ruda_source_manager_dispose(&sources);
        abort();
    }
    lex_status = ruda_lex_snapshot(
        snapshot,
        ruda_system_allocator(),
        lex_options,
        &lexed
    );
    if (lex_status == RUDA_LEX_OK) {
        parse_status = ruda_parse_file(
            &lexed,
            ruda_system_allocator(),
            parse_options,
            &parsed
        );
        if (parse_status == RUDA_PARSE_OK) {
            if (!ruda_syntax_tree_is_lossless(&parsed.tree)) {
                abort();
            }
            ruda_parse_result_dispose(&parsed);
        } else if (parse_status != RUDA_PARSE_LIMIT_REACHED) {
            abort();
        }
        ruda_lex_result_dispose(&lexed);
    } else if (lex_status == RUDA_LEX_LIMIT_REACHED) {
        ruda_lex_result_dispose(&lexed);
    } else {
        abort();
    }
    ruda_source_manager_dispose(&sources);
    return 0;
}
