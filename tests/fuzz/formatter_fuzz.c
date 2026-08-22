#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mmay/ast/ast.h"
#include "mmay/format/formatter.h"
#include "mmay/lex/lexer.h"
#include "mmay/parse/parser.h"
#include "mmay/source/source_manager.h"

#include "semantic_oracle.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    MarimaySourceManager sources;
    const MarimaySourceSnapshot *original_snapshot = NULL;
    const MarimaySourceSnapshot *formatted_snapshot = NULL;
    MarimayLexResult original_lexed;
    MarimayLexResult formatted_lexed;
    MarimayParseResult original_parsed;
    MarimayParseResult formatted_parsed;
    MarimayFormatResult first;
    MarimayFormatResult second;
    MarimayAstTree original_ast;
    MarimayAstTree formatted_ast;
    MarimayLexOptions lex_options = mmay_lex_default_options();
    MarimayParseOptions parse_options = mmay_parse_default_options();
    MarimayFormatOptions format_options = mmay_format_default_options();
    MarimayLexStatus lex_status;
    MarimayParseStatus parse_status;
    MarimayFormatStatus format_status;
    if (size > 512U * 1024U) {
        return 0;
    }
    lex_options.max_tokens = 100000;
    lex_options.max_diagnostics = 128;
    lex_options.max_nesting = 128;
    parse_options.max_events = 400000;
    parse_options.max_diagnostics = 128;
    parse_options.max_nesting = 128;
    format_options.max_tokens = 100000;
    format_options.max_document_parts = 400000;
    format_options.max_output_bytes = 2U * 1024U * 1024U;
    format_options.max_nesting = 128;
    format_options.max_render_steps = 400000;
    memset(&original_ast, 0, sizeof(original_ast));
    memset(&formatted_ast, 0, sizeof(formatted_ast));
    if (!mmay_source_manager_init(&sources, mmay_system_allocator())) {
        abort();
    }
    if (mmay_source_manager_add(
            &sources,
            "fuzz.my",
            sizeof("fuzz.my") - 1,
            data,
            size,
            &original_snapshot
        ) != MMAY_SOURCE_OK) {
        mmay_source_manager_dispose(&sources);
        abort();
    }
    lex_status = mmay_lex_snapshot(
        original_snapshot,
        mmay_system_allocator(),
        lex_options,
        &original_lexed
    );
    if (lex_status != MMAY_LEX_OK) {
        if (lex_status == MMAY_LEX_LIMIT_REACHED) {
            mmay_lex_result_dispose(&original_lexed);
        } else {
            abort();
        }
        mmay_source_manager_dispose(&sources);
        return 0;
    }
    parse_status = mmay_parse_file(
        &original_lexed,
        mmay_system_allocator(),
        parse_options,
        &original_parsed
    );
    if (parse_status != MMAY_PARSE_OK) {
        if (parse_status != MMAY_PARSE_LIMIT_REACHED) {
            abort();
        }
        mmay_lex_result_dispose(&original_lexed);
        mmay_source_manager_dispose(&sources);
        return 0;
    }
    if (original_lexed.diagnostics.length != 0
        || original_lexed.suppressed_diagnostics != 0
        || original_parsed.diagnostics.length != 0
        || original_parsed.suppressed_diagnostics != 0) {
        mmay_parse_result_dispose(&original_parsed);
        mmay_lex_result_dispose(&original_lexed);
        mmay_source_manager_dispose(&sources);
        return 0;
    }
    format_status = mmay_format_source(
        &original_lexed,
        &original_parsed,
        mmay_system_allocator(),
        format_options,
        &first
    );
    if (format_status != MMAY_FORMAT_OK) {
        if (format_status != MMAY_FORMAT_LIMIT_REACHED) {
            abort();
        }
        mmay_parse_result_dispose(&original_parsed);
        mmay_lex_result_dispose(&original_lexed);
        mmay_source_manager_dispose(&sources);
        return 0;
    }
    if (mmay_source_manager_overlay(
            &sources,
            mmay_source_file_id(original_snapshot),
            first.output.data,
            first.output.length,
            &formatted_snapshot
        ) != MMAY_SOURCE_OK) {
        abort();
    }
    if (mmay_lex_snapshot(
            formatted_snapshot,
            mmay_system_allocator(),
            lex_options,
            &formatted_lexed
        ) != MMAY_LEX_OK
        || formatted_lexed.diagnostics.length != 0
        || mmay_parse_file(
            &formatted_lexed,
            mmay_system_allocator(),
            parse_options,
            &formatted_parsed
        ) != MMAY_PARSE_OK
        || formatted_parsed.diagnostics.length != 0) {
        abort();
    }
    if (mmay_ast_project(
            &original_parsed.tree,
            mmay_system_allocator(),
            mmay_ast_default_options(),
            &original_ast
        ) != MMAY_AST_OK
        || mmay_ast_project(
            &formatted_parsed.tree,
            mmay_system_allocator(),
            mmay_ast_default_options(),
            &formatted_ast
        ) != MMAY_AST_OK
        || !mmay_fuzz_ast_semantically_equal(
            &original_ast,
            original_snapshot,
            &formatted_ast,
            formatted_snapshot
        )) {
        abort();
    }
    if (mmay_format_source(
            &formatted_lexed,
            &formatted_parsed,
            mmay_system_allocator(),
            format_options,
            &second
        ) != MMAY_FORMAT_OK
        || second.changed || second.output.length != first.output.length
        || memcmp(second.output.data, first.output.data, first.output.length) != 0) {
        abort();
    }
    mmay_format_result_dispose(&second);
    mmay_ast_tree_dispose(&formatted_ast);
    mmay_ast_tree_dispose(&original_ast);
    mmay_parse_result_dispose(&formatted_parsed);
    mmay_lex_result_dispose(&formatted_lexed);
    mmay_format_result_dispose(&first);
    mmay_parse_result_dispose(&original_parsed);
    mmay_lex_result_dispose(&original_lexed);
    mmay_source_manager_dispose(&sources);
    return 0;
}
