#include "queries.h"
#include "output.h"
#include "positions.h"
#include "completion.h"
#include "rename.h"
#include "intrinsic_queries.h"
#include "analysis_cache.h"
#include "source_locations.h"

#include <string.h>
#include "mmay/format/formatter.h"

static const MarimayCheckedSymbol *lsp_symbol_at(const LspAnalysis *analysis,
    const MarimaySourceSnapshot *snapshot, size_t offset) {
    const MarimayCheckedSymbol *symbol = NULL;
    if (snapshot == NULL) return NULL;
    if (mmay_compiler_service_definition_from_reference(&analysis->target.service,
            &analysis->result, snapshot, offset, &symbol) != MMAY_COMPILER_OK)
        return NULL;
    if (symbol == NULL) symbol = mmay_checked_symbol_defined_at(
        &analysis->result.checked, snapshot, offset);
    if (symbol == NULL && offset != 0) {
        const MarimayCheckedOccurrence *previous = mmay_checked_occurrence_at(
            &analysis->result.checked, snapshot, offset - 1);
        if (previous != NULL && previous->end == offset)
            symbol = mmay_checked_symbol_by_id(&analysis->result.checked, previous->symbol);
        if (symbol == NULL) {
            const MarimayCheckedSymbol *previous_definition = mmay_checked_symbol_defined_at(
                &analysis->result.checked, snapshot, offset - 1);
            if (previous_definition != NULL && previous_definition->end == offset)
                symbol = previous_definition;
        }
    }
    return symbol;
}

static bool lsp_hover(const LspAnalysis *analysis,
    const MarimayCheckedSymbol *symbol, MarimayWriter writer) {
    MarimayOwnedBytes spelling = {0};
    bool ok = true;
    if (symbol == NULL) return mmay_writer_write_cstr(writer, "null");
    if (mmay_compiler_service_symbol_type_spelling(&analysis->target.service,
            &analysis->result, symbol->symbol, analysis->target.allocator,
            mmay_type_spelling_default_options(), &spelling) != MMAY_COMPILER_OK)
        return false;
    ok = mmay_writer_write_cstr(writer, "{\"contents\":{\"language\":\"marimay\",\"value\":")
        && mmay_lsp_json_write_string(writer, spelling.data, spelling.length)
        && mmay_writer_write_cstr(writer, "}}");
    mmay_owned_bytes_dispose(&spelling);
    return ok;
}

static size_t lsp_symbol_kind(const MarimayCheckedSymbol *symbol) {
    switch (symbol->kind) {
        case MMAY_CHECKED_SYMBOL_FUNCTION: return 12;
        case MMAY_CHECKED_SYMBOL_CONSTANT: return 14;
        case MMAY_CHECKED_SYMBOL_RECORD: return 23;
        case MMAY_CHECKED_SYMBOL_ENUM: return 10;
        case MMAY_CHECKED_SYMBOL_INTERFACE: return 11;
        case MMAY_CHECKED_SYMBOL_TYPE_ALIAS: return 26;
        case MMAY_CHECKED_SYMBOL_LOCAL:
        case MMAY_CHECKED_SYMBOL_PARAMETER: return 13;
        case MMAY_CHECKED_SYMBOL_FIELD: return 8;
    }
    return 13;
}

static bool lsp_symbols(const LspAnalysis *analysis,
    const MarimaySourceSnapshot *snapshot, const void *prefix, size_t prefix_length,
    bool workspace_symbols, MarimayWriter writer) {
    size_t count = 0, index = 0;
    bool first = true;
    if (mmay_compiler_service_symbol_search_count(&analysis->target.service,
            &analysis->result, prefix, prefix_length, &count) != MMAY_COMPILER_OK)
        return mmay_writer_write_cstr(writer, "[]");
    if (!mmay_writer_write_cstr(writer, "[")) return false;
    for (index = 0; index < count && index < 4096; index += 1) {
        const MarimayCheckedSymbol *symbol = NULL;
        if (mmay_compiler_service_symbol_search(&analysis->target.service,
                &analysis->result, prefix, prefix_length, index, &symbol)
                != MMAY_COMPILER_OK || symbol == NULL) return false;
        if (!workspace_symbols && symbol->snapshot != snapshot) continue;
        if (!lsp_source_openable(symbol->snapshot)) continue;
        if (!first && !mmay_writer_write_cstr(writer, ",")) return false;
        first = false;
        if (!mmay_writer_write_cstr(writer, "{\"name\":")
            || !mmay_lsp_json_write_string(writer, symbol->name.data, symbol->name.length)
            || !mmay_writer_write_cstr(writer, ",\"kind\":")
            || !lsp_write_size(writer, lsp_symbol_kind(symbol))) return false;
        if (workspace_symbols) {
            if (!mmay_writer_write_cstr(writer, ",\"location\":")
                || !lsp_source_write_location(writer, analysis,
                    symbol->snapshot, symbol->start, symbol->end)) return false;
        } else if (!mmay_writer_write_cstr(writer, ",\"range\":")
            || !lsp_write_range(writer, symbol->snapshot, symbol->start, symbol->end)
            || !mmay_writer_write_cstr(writer, ",\"selectionRange\":")
            || !lsp_write_range(writer, symbol->snapshot, symbol->start, symbol->end)) return false;
        if (!mmay_writer_write_cstr(writer, "}")) return false;
    }
    return mmay_writer_write_cstr(writer, "]");
}

static bool lsp_references(const LspAnalysis *analysis,
    const MarimayCheckedSymbol *symbol, bool include_declaration, MarimayWriter writer) {
    size_t index = 0;
    bool first = true;
    if (!mmay_writer_write_cstr(writer, "[")) return false;
    if (symbol != NULL && include_declaration) {
        if (!lsp_source_write_location(writer, analysis,
                symbol->snapshot, symbol->start, symbol->end)) return false;
        first = false;
    }
    for (index = 0; symbol != NULL && index < analysis->result.checked.occurrences.length;
        index += 1) {
        const MarimayCheckedOccurrence *occurrence = mmay_checked_occurrence(
            &analysis->result.checked, index);
        if (!mmay_symbol_id_equal(occurrence->symbol, symbol->symbol)) continue;
        if (!first && !mmay_writer_write_cstr(writer, ",")) return false;
        first = false;
        if (!lsp_source_write_location(writer, analysis, occurrence->snapshot,
                occurrence->start, occurrence->end)) return false;
    }
    return mmay_writer_write_cstr(writer, "]");
}

static bool lsp_format(const LspAnalysis *analysis,
    const MarimaySourceSnapshot *snapshot, MarimayWriter writer) {
    size_t index = 0;
    for (index = 0; index < analysis->result.units.length; index += 1) {
        const MarimayCompilerUnit *unit = mmay_compiler_result_unit(&analysis->result, index);
        MarimayFormatResult formatted = {0};
        MarimayFormatStatus status;
        bool ok;
        if (unit->snapshot != snapshot) continue;
        status = mmay_format_source(&unit->lexed, &unit->parsed,
            analysis->target.allocator, mmay_format_default_options(), &formatted);
        if (status == MMAY_FORMAT_SOURCE_INVALID) return mmay_writer_write_cstr(writer, "[]");
        if (status != MMAY_FORMAT_OK) return false;
        ok = mmay_writer_write_cstr(writer, "[");
        if (ok && formatted.changed) ok = mmay_writer_write_cstr(writer, "{\"range\":")
            && lsp_write_range(writer, snapshot, 0, mmay_source_length(snapshot))
            && mmay_writer_write_cstr(writer, ",\"newText\":")
            && mmay_lsp_json_write_string(writer, formatted.output.data, formatted.output.length)
            && mmay_writer_write_cstr(writer, "}");
        if (ok) ok = mmay_writer_write_cstr(writer, "]");
        mmay_format_result_dispose(&formatted);
        return ok;
    }
    return mmay_writer_write_cstr(writer, "[]");
}

LspQueryStatus lsp_query(LspWorkspace *workspace, const MarimayLspJson *json,
    size_t method, size_t params, MarimayWriter writer) {
    if (mmay_lsp_json_equal(json, method, "textDocument/completion"))
        return lsp_completion(workspace, json, params, writer);
    if (mmay_lsp_json_equal(json, method, "textDocument/prepareRename"))
        return lsp_rename(workspace, json, params, true, writer);
    if (mmay_lsp_json_equal(json, method, "textDocument/rename"))
        return lsp_rename(workspace, json, params, false, writer);
    bool definition = mmay_lsp_json_equal(json, method, "textDocument/definition");
    bool hover = mmay_lsp_json_equal(json, method, "textDocument/hover");
    bool references = mmay_lsp_json_equal(json, method, "textDocument/references");
    bool document_symbols = mmay_lsp_json_equal(json, method, "textDocument/documentSymbol");
    bool workspace_symbols = mmay_lsp_json_equal(json, method, "workspace/symbol");
    bool formatting = mmay_lsp_json_equal(json, method, "textDocument/formatting");
    MarimayOwnedBytes uri = {0}, prefix = {0};
    const LspDocument *document = NULL;
    const MarimaySourceSnapshot *snapshot = NULL;
    const MarimayCheckedSymbol *symbol = NULL;
    const LspAnalysis *analysis = NULL;
    LspQueryStatus status = LSP_QUERY_INVALID;
    size_t offset = 0;
    bool ok = false;
    if (!definition && !hover && !references && !document_symbols
        && !workspace_symbols && !formatting) return LSP_QUERY_UNKNOWN;
    if (workspace_symbols) {
        if (!mmay_lsp_json_string(json, mmay_lsp_json_get(json, params, "query"), &prefix)) goto cleanup;
        for (size_t index = 0; index < workspace->documents.length; index += 1) {
            const LspDocument *candidate = mmay_vector_at_const(&workspace->documents, index);
            if (!lsp_source_is_view((const char *)candidate->path.data, candidate->path.length)) {
                document = candidate;
                break;
            }
        }
        if (document == NULL) {
            status = mmay_writer_write_cstr(writer, "[]") ? LSP_QUERY_OK : LSP_QUERY_FAILED;
            goto cleanup;
        }
    } else {
        size_t text_document = mmay_lsp_json_get(json, params, "textDocument");
        if (!mmay_lsp_json_string(json, mmay_lsp_json_get(json, text_document, "uri"), &uri)) goto cleanup;
        document = lsp_workspace_document(workspace, (const char *)uri.data, uri.length);
        if (document == NULL) goto cleanup;
    }
    if (definition || hover || references) {
        int64_t line = 0, character = 0;
        size_t position = mmay_lsp_json_get(json, params, "position");
        if (!mmay_lsp_json_integer(json, mmay_lsp_json_get(json, position, "line"), &line)
            || !mmay_lsp_json_integer(json, mmay_lsp_json_get(json, position, "character"), &character)
            || line < 0 || character < 0 || (uint64_t)line > SIZE_MAX
            || (uint64_t)character > SIZE_MAX
            || !lsp_position_to_offset(document->text.data, document->text.length,
                (size_t)line, (size_t)character, &offset)) goto cleanup;
    }
    status = LSP_QUERY_FAILED;
    if (lsp_workspace_analysis(workspace, (const char *)document->uri.data,
            document->uri.length, &analysis) != LSP_OK) goto cleanup;
    snapshot = lsp_source_snapshot(analysis, (const char *)document->path.data, document->path.length);
    if (definition || hover || references) symbol = lsp_symbol_at(analysis, snapshot, offset);
    if (symbol == NULL && (definition || hover || references)) {
        bool intrinsic_found = false;
        size_t context = mmay_lsp_json_get(json, params, "context");
        const MarimayLspJsonToken *include = mmay_lsp_json_token(json,
            mmay_lsp_json_get(json, context, "includeDeclaration"));
        LspIntrinsicQueryKind kind = definition ? LSP_INTRINSIC_DEFINITION
            : hover ? LSP_INTRINSIC_HOVER : LSP_INTRINSIC_REFERENCES;
        ok = lsp_intrinsic_query(analysis, snapshot, offset, kind,
            include != NULL && include->kind == MMAY_LSP_JSON_TRUE, writer, &intrinsic_found);
        if (!ok || intrinsic_found) {
            status = ok ? LSP_QUERY_OK : LSP_QUERY_FAILED;
            goto cleanup;
        }
    }
    if (definition && symbol != NULL && !lsp_source_openable(symbol->snapshot)) symbol = NULL;
    if (definition) ok = symbol == NULL ? mmay_writer_write_cstr(writer, "null")
        : lsp_source_write_location(writer, analysis, symbol->snapshot, symbol->start, symbol->end);
    else if (hover) ok = lsp_hover(analysis, symbol, writer);
    else if (references) {
        size_t context = mmay_lsp_json_get(json, params, "context");
        const MarimayLspJsonToken *include = mmay_lsp_json_token(json,
            mmay_lsp_json_get(json, context, "includeDeclaration"));
        ok = lsp_references(analysis, symbol,
            include != NULL && include->kind == MMAY_LSP_JSON_TRUE, writer);
    } else if (formatting) ok = lsp_format(analysis, snapshot, writer);
    else ok = lsp_symbols(analysis, snapshot, prefix.data, prefix.length, workspace_symbols, writer);
    status = ok ? LSP_QUERY_OK : LSP_QUERY_FAILED;
cleanup:
    mmay_owned_bytes_dispose(&uri);
    mmay_owned_bytes_dispose(&prefix);
    return status;
}
