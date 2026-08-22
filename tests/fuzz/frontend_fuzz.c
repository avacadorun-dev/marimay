#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "mmay/lex/lexer.h"
#include "mmay/parse/parser.h"
#include "mmay/source/source_manager.h"
#include "mmay/syntax/syntax.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    MarimaySourceManager sources;
    const MarimaySourceSnapshot *snapshot = NULL;
    MarimayLexResult lexed;
    MarimayParseResult parsed;
    MarimayLexOptions lex_options = mmay_lex_default_options();
    MarimayParseOptions parse_options = mmay_parse_default_options();
    MarimayLexStatus lex_status;
    MarimayParseStatus parse_status;
    if (size > 1024U * 1024U) {
        return 0;
    }
    lex_options.max_tokens = 100000;
    lex_options.max_diagnostics = 128;
    lex_options.max_nesting = 128;
    parse_options.max_events = 400000;
    parse_options.max_diagnostics = 128;
    parse_options.max_nesting = 128;
    if (!mmay_source_manager_init(&sources, mmay_system_allocator())) {
        abort();
    }
    if (mmay_source_manager_add(
            &sources,
            "fuzz.my",
            sizeof("fuzz.my") - 1,
            data,
            size,
            &snapshot
        ) != MMAY_SOURCE_OK) {
        mmay_source_manager_dispose(&sources);
        abort();
    }
    lex_status = mmay_lex_snapshot(
        snapshot,
        mmay_system_allocator(),
        lex_options,
        &lexed
    );
    if (lex_status == MMAY_LEX_OK) {
        parse_status = mmay_parse_file(
            &lexed,
            mmay_system_allocator(),
            parse_options,
            &parsed
        );
        if (parse_status == MMAY_PARSE_OK) {
            if (!mmay_syntax_tree_is_lossless(&parsed.tree)) {
                abort();
            }
            mmay_parse_result_dispose(&parsed);
        } else if (parse_status != MMAY_PARSE_LIMIT_REACHED) {
            (void)fprintf(
                stderr,
                "unexpected parser status: %d\n",
                (int)parse_status
            );
            abort();
        }
        mmay_lex_result_dispose(&lexed);
    } else if (lex_status == MMAY_LEX_LIMIT_REACHED) {
        mmay_lex_result_dispose(&lexed);
    } else {
        (void)fprintf(
            stderr,
            "unexpected lexer status: %d\n",
            (int)lex_status
        );
        abort();
    }
    mmay_source_manager_dispose(&sources);
    return 0;
}
