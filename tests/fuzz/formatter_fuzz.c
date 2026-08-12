#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ruda/ast/ast.h"
#include "ruda/format/formatter.h"
#include "ruda/lex/lexer.h"
#include "ruda/parse/parser.h"
#include "ruda/source/source_manager.h"

#include "semantic_oracle.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    RudaSourceManager sources;
    const RudaSourceSnapshot *original_snapshot = NULL;
    const RudaSourceSnapshot *formatted_snapshot = NULL;
    RudaLexResult original_lexed;
    RudaLexResult formatted_lexed;
    RudaParseResult original_parsed;
    RudaParseResult formatted_parsed;
    RudaFormatResult first;
    RudaFormatResult second;
    RudaAstTree original_ast;
    RudaAstTree formatted_ast;
    RudaLexOptions lex_options = ruda_lex_default_options();
    RudaParseOptions parse_options = ruda_parse_default_options();
    RudaFormatOptions format_options = ruda_format_default_options();
    RudaLexStatus lex_status;
    RudaParseStatus parse_status;
    RudaFormatStatus format_status;
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
    if (!ruda_source_manager_init(&sources, ruda_system_allocator())) {
        abort();
    }
    if (ruda_source_manager_add(
            &sources,
            "fuzz.ruda",
            sizeof("fuzz.ruda") - 1,
            data,
            size,
            &original_snapshot
        ) != RUDA_SOURCE_OK) {
        ruda_source_manager_dispose(&sources);
        abort();
    }
    lex_status = ruda_lex_snapshot(
        original_snapshot,
        ruda_system_allocator(),
        lex_options,
        &original_lexed
    );
    if (lex_status != RUDA_LEX_OK) {
        if (lex_status == RUDA_LEX_LIMIT_REACHED) {
            ruda_lex_result_dispose(&original_lexed);
        } else {
            abort();
        }
        ruda_source_manager_dispose(&sources);
        return 0;
    }
    parse_status = ruda_parse_file(
        &original_lexed,
        ruda_system_allocator(),
        parse_options,
        &original_parsed
    );
    if (parse_status != RUDA_PARSE_OK) {
        if (parse_status != RUDA_PARSE_LIMIT_REACHED) {
            abort();
        }
        ruda_lex_result_dispose(&original_lexed);
        ruda_source_manager_dispose(&sources);
        return 0;
    }
    if (original_lexed.diagnostics.length != 0
        || original_lexed.suppressed_diagnostics != 0
        || original_parsed.diagnostics.length != 0
        || original_parsed.suppressed_diagnostics != 0) {
        ruda_parse_result_dispose(&original_parsed);
        ruda_lex_result_dispose(&original_lexed);
        ruda_source_manager_dispose(&sources);
        return 0;
    }
    format_status = ruda_format_source(
        &original_lexed,
        &original_parsed,
        ruda_system_allocator(),
        format_options,
        &first
    );
    if (format_status != RUDA_FORMAT_OK) {
        if (format_status != RUDA_FORMAT_LIMIT_REACHED) {
            abort();
        }
        ruda_parse_result_dispose(&original_parsed);
        ruda_lex_result_dispose(&original_lexed);
        ruda_source_manager_dispose(&sources);
        return 0;
    }
    if (ruda_source_manager_overlay(
            &sources,
            ruda_source_file_id(original_snapshot),
            first.output.data,
            first.output.length,
            &formatted_snapshot
        ) != RUDA_SOURCE_OK) {
        abort();
    }
    if (ruda_lex_snapshot(
            formatted_snapshot,
            ruda_system_allocator(),
            lex_options,
            &formatted_lexed
        ) != RUDA_LEX_OK
        || formatted_lexed.diagnostics.length != 0
        || ruda_parse_file(
            &formatted_lexed,
            ruda_system_allocator(),
            parse_options,
            &formatted_parsed
        ) != RUDA_PARSE_OK
        || formatted_parsed.diagnostics.length != 0) {
        abort();
    }
    if (ruda_ast_project(
            &original_parsed.tree,
            ruda_system_allocator(),
            ruda_ast_default_options(),
            &original_ast
        ) != RUDA_AST_OK
        || ruda_ast_project(
            &formatted_parsed.tree,
            ruda_system_allocator(),
            ruda_ast_default_options(),
            &formatted_ast
        ) != RUDA_AST_OK
        || !ruda_fuzz_ast_semantically_equal(
            &original_ast,
            original_snapshot,
            &formatted_ast,
            formatted_snapshot
        )) {
        abort();
    }
    if (ruda_format_source(
            &formatted_lexed,
            &formatted_parsed,
            ruda_system_allocator(),
            format_options,
            &second
        ) != RUDA_FORMAT_OK
        || second.changed || second.output.length != first.output.length
        || memcmp(second.output.data, first.output.data, first.output.length) != 0) {
        abort();
    }
    ruda_format_result_dispose(&second);
    ruda_ast_tree_dispose(&formatted_ast);
    ruda_ast_tree_dispose(&original_ast);
    ruda_parse_result_dispose(&formatted_parsed);
    ruda_lex_result_dispose(&formatted_lexed);
    ruda_format_result_dispose(&first);
    ruda_parse_result_dispose(&original_parsed);
    ruda_lex_result_dispose(&original_lexed);
    ruda_source_manager_dispose(&sources);
    return 0;
}
