#include "completion.h"
#include "analysis_overrides.h"
#include "completion_probe.h"
#include "output.h"
#include "positions.h"
#include "analysis_cache.h"

#include <string.h>
#include "mmay/check/visible_symbols.h"

typedef struct LspCompletionCandidate {
    const unsigned char *name;
    size_t length;
    const MarimayCheckedSymbol *symbol;
    const MarimayIntrinsicSignature *intrinsic;
    size_t kind;
} LspCompletionCandidate;

typedef struct LspCompletionCandidates {
    MarimayVector values;
    bool incomplete;
} LspCompletionCandidates;

static bool lsp_completion_bytes(const void *left, size_t left_length,
    const void *right, size_t right_length) {
    return left_length == right_length
        && (left_length == 0 || memcmp(left, right, left_length) == 0);
}

static bool lsp_completion_prefix(const void *name, size_t length,
    const unsigned char *prefix, size_t prefix_length) {
    return prefix_length <= length
        && (prefix_length == 0 || memcmp(name, prefix, prefix_length) == 0);
}

static bool lsp_completion_local(const MarimayCheckedSymbol *symbol) {
    return symbol != NULL && (symbol->kind == MMAY_CHECKED_SYMBOL_LOCAL
        || symbol->kind == MMAY_CHECKED_SYMBOL_PARAMETER);
}

static bool lsp_completion_visible_local(const MarimayCheckedSymbol *symbol,
    const MarimaySourceSnapshot *snapshot, size_t offset) {
    return lsp_completion_local(symbol) && symbol->snapshot == snapshot
        && symbol->scope_start <= offset && offset < symbol->scope_end
        && symbol->visible_from <= offset;
}

static bool lsp_completion_nearer(const MarimayCheckedSymbol *left,
    const MarimayCheckedSymbol *right) {
    size_t left_width = left->scope_end - left->scope_start;
    size_t right_width = right->scope_end - right->scope_start;
    return left_width < right_width
        || (left_width == right_width && (left->scope_depth > right->scope_depth
            || (left->scope_depth == right->scope_depth && left->visible_from > right->visible_from)));
}

static size_t lsp_completion_kind(const MarimayCheckedSymbol *symbol) {
    if (symbol == NULL) return 6;
    switch (symbol->kind) {
        case MMAY_CHECKED_SYMBOL_FUNCTION: return 3;
        case MMAY_CHECKED_SYMBOL_CONSTANT: return 21;
        case MMAY_CHECKED_SYMBOL_RECORD: return 22;
        case MMAY_CHECKED_SYMBOL_ENUM: return 13;
        case MMAY_CHECKED_SYMBOL_INTERFACE: return 8;
        case MMAY_CHECKED_SYMBOL_TYPE_ALIAS: return 25;
        case MMAY_CHECKED_SYMBOL_FIELD: return 5;
        case MMAY_CHECKED_SYMBOL_LOCAL:
        case MMAY_CHECKED_SYMBOL_PARAMETER: return 6;
    }
    return 6;
}

static bool lsp_completion_add(LspCompletionCandidates *candidates,
    const void *name, size_t length, const MarimayCheckedSymbol *symbol,
    const MarimayIntrinsicSignature *intrinsic, size_t kind,
    const unsigned char *prefix, size_t prefix_length) {
    LspCompletionCandidate candidate;
    size_t index;
    if (length == 0 || !lsp_completion_prefix(name, length, prefix, prefix_length)) return true;
    for (index = 0; index < candidates->values.length; index += 1) {
        LspCompletionCandidate *existing = mmay_vector_at(&candidates->values, index);
        if (!lsp_completion_bytes(existing->name, existing->length, name, length)) continue;
        if (lsp_completion_local(symbol) && (!lsp_completion_local(existing->symbol)
            || lsp_completion_nearer(symbol, existing->symbol))) {
            existing->symbol = symbol;
            existing->intrinsic = intrinsic;
            existing->kind = kind;
        }
        return true;
    }
    if (candidates->values.length >= 4096) {
        candidates->incomplete = true;
        return true;
    }
    candidate = (LspCompletionCandidate){name, length, symbol, intrinsic, kind};
    return mmay_vector_push(&candidates->values, &candidate);
}

static const MarimayCheckedSymbol *lsp_completion_resolve_name(
    const LspAnalysis *analysis, const MarimaySourceSnapshot *snapshot,
    size_t offset, const void *name, size_t length) {
    const MarimayCheckedSymbol *best = NULL;
    size_t index, count;
    for (index = 0; index < analysis->result.checked.symbols.length; index += 1) {
        const MarimayCheckedSymbol *symbol = mmay_checked_symbol(&analysis->result.checked, index);
        if (lsp_completion_visible_local(symbol, snapshot, offset)
            && lsp_completion_bytes(symbol->name.data, symbol->name.length, name, length)
            && (best == NULL || lsp_completion_nearer(symbol, best))) best = symbol;
    }
    if (best != NULL) return best;
    count = mmay_checked_visible_symbol_count(&analysis->result.checked, snapshot, name, length);
    for (index = 0; index < count; index += 1) {
        const MarimayCheckedVisibleSymbol *visible = mmay_checked_visible_symbol(
            &analysis->result.checked, snapshot, name, length, index);
        if (lsp_completion_bytes(visible->spelling.data, visible->spelling.length, name, length))
            return mmay_checked_symbol_by_id(&analysis->result.checked, visible->symbol);
    }
    return NULL;
}

static const MarimayCompilerPackage *lsp_completion_package(
    const LspAnalysis *analysis, const MarimaySourceSnapshot *snapshot) {
    size_t index, file;
    MarimayFileId id = mmay_source_file_id(snapshot);
    for (index = 0; index < analysis->target.compiler_packages.length; index += 1) {
        const MarimayCompilerPackage *package = mmay_vector_at_const(
            &analysis->target.compiler_packages, index);
        for (file = 0; file < package->file_count; file += 1)
            if (package->file_ids[file] == id) return package;
    }
    return NULL;
}

static bool lsp_completion_namespace_shadowed(const LspAnalysis *analysis,
    const MarimaySourceSnapshot *snapshot, size_t offset,
    const MarimayOwnedBytes *spelling) {
    const unsigned char *dot = memchr(spelling->data, '.', spelling->length);
    if (dot == NULL) return false;
    return lsp_completion_local(lsp_completion_resolve_name(analysis, snapshot,
        offset, spelling->data, (size_t)(dot - spelling->data)));
}

static bool lsp_completion_field_visible(const MarimayCheckedSymbol *field,
    const MarimayCompilerPackage *package) {
    return field->is_public || (package != NULL
        && lsp_completion_bytes(field->package_name.data, field->package_name.length,
            package->import_path, package->import_path_length));
}

static const MarimayCheckedSymbol *lsp_completion_field(
    const LspAnalysis *analysis, const MarimayCompilerPackage *package,
    MarimayTypeId receiver, const void *name, size_t length) {
    const MarimayTypeDescriptor *type = mmay_type_descriptor(&analysis->result.checked.types, receiver);
    size_t index;
    if (type == NULL || type->kind != MMAY_TYPE_KIND_NOMINAL) return NULL;
    for (index = 0; index < analysis->result.checked.symbols.length; index += 1) {
        const MarimayCheckedSymbol *field = mmay_checked_symbol(&analysis->result.checked, index);
        if (field->kind == MMAY_CHECKED_SYMBOL_FIELD
            && mmay_symbol_id_equal(field->owner_symbol, type->symbol)
            && lsp_completion_bytes(field->name.data, field->name.length, name, length)
            && lsp_completion_field_visible(field, package)) return field;
    }
    return NULL;
}

static bool lsp_completion_unqualified(LspCompletionCandidates *candidates,
    const LspAnalysis *analysis, const MarimaySourceSnapshot *snapshot,
    size_t offset, const unsigned char *prefix, size_t prefix_length) {
    static const char *const keywords[] = {
        "const", "let", "return", "if", "else", "match", "for", "while",
        "break", "continue", "defer", "scope", "spawn", "unsafe", "true", "false",
        "bool", "int", "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64",
        "f32", "f64", "byte", "uint", "float", "rune", "string", "void", "never",
        "Option", "Result", "Map", "Set"
    };
    size_t index, count = mmay_checked_visible_symbol_count(&analysis->result.checked,
        snapshot, prefix, prefix_length);
    for (index = 0; index < count; index += 1) {
        const MarimayCheckedVisibleSymbol *visible = mmay_checked_visible_symbol(
            &analysis->result.checked, snapshot, prefix, prefix_length, index);
        const MarimayCheckedSymbol *symbol = mmay_checked_symbol_by_id(
            &analysis->result.checked, visible->symbol);
        if (lsp_completion_namespace_shadowed(analysis, snapshot, offset, &visible->spelling)) continue;
        if (symbol != NULL && !lsp_completion_add(candidates, visible->spelling.data,
                visible->spelling.length, symbol, NULL, lsp_completion_kind(symbol),
                prefix, prefix_length)) return false;
    }
    for (index = 0; index < analysis->result.checked.symbols.length; index += 1) {
        const MarimayCheckedSymbol *symbol = mmay_checked_symbol(&analysis->result.checked, index);
        if (lsp_completion_visible_local(symbol, snapshot, offset)
            && !lsp_completion_add(candidates, symbol->name.data, symbol->name.length,
                symbol, NULL, 6, prefix, prefix_length)) return false;
    }
    count = mmay_checked_intrinsic_completion_count(&analysis->result.checked,
        snapshot, prefix, prefix_length);
    for (index = 0; index < count; index += 1) {
        const MarimayCheckedIntrinsicCompletion *item = mmay_checked_intrinsic_completion(
            &analysis->result.checked, snapshot, prefix, prefix_length, index);
        if (lsp_completion_namespace_shadowed(analysis, snapshot, offset, &item->spelling)) continue;
        if (!lsp_completion_add(candidates, item->spelling.data, item->spelling.length,
                NULL, item->intrinsic, 3, prefix, prefix_length)) return false;
    }
    for (index = 0; index < sizeof(keywords) / sizeof(keywords[0]); index += 1)
        if (!lsp_completion_add(candidates, keywords[index], strlen(keywords[index]),
                NULL, NULL, 14, prefix, prefix_length)) return false;
    return true;
}

static bool lsp_completion_namespace(LspCompletionCandidates *candidates,
    const LspAnalysis *analysis, const MarimaySourceSnapshot *snapshot,
    const unsigned char *receiver, size_t receiver_length,
    const unsigned char *prefix, size_t prefix_length) {
    size_t count = mmay_checked_visible_symbol_count(&analysis->result.checked,
        snapshot, receiver, receiver_length);
    size_t index;
    for (index = 0; index < count; index += 1) {
        const MarimayCheckedVisibleSymbol *visible = mmay_checked_visible_symbol(
            &analysis->result.checked, snapshot, receiver, receiver_length, index);
        const MarimayCheckedSymbol *symbol;
        if (visible->spelling.length <= receiver_length
            || visible->spelling.data[receiver_length] != '.') continue;
        symbol = mmay_checked_symbol_by_id(&analysis->result.checked, visible->symbol);
        if (symbol != NULL && !lsp_completion_add(candidates,
                visible->spelling.data + receiver_length + 1,
                visible->spelling.length - receiver_length - 1,
                symbol, NULL, lsp_completion_kind(symbol), prefix, prefix_length)) return false;
    }
    count = mmay_checked_intrinsic_completion_count(&analysis->result.checked,
        snapshot, receiver, receiver_length);
    for (index = 0; index < count; index += 1) {
        const MarimayCheckedIntrinsicCompletion *item = mmay_checked_intrinsic_completion(
            &analysis->result.checked, snapshot, receiver, receiver_length, index);
        if (item->spelling.length <= receiver_length || item->spelling.data[receiver_length] != '.') continue;
        if (!lsp_completion_add(candidates, item->spelling.data + receiver_length + 1,
                item->spelling.length - receiver_length - 1, NULL, item->intrinsic,
                3, prefix, prefix_length)) return false;
    }
    return true;
}

static bool lsp_completion_members(LspCompletionCandidates *candidates,
    const LspAnalysis *analysis, const MarimaySourceSnapshot *snapshot,
    const LspCompletionProbe *probe, const unsigned char *original,
    const unsigned char *prefix) {
    const MarimayToken *first = &probe->receiver[0];
    const MarimayCheckedSymbol *receiver = lsp_completion_resolve_name(analysis,
        snapshot, probe->query_offset, original + first->start, first->end - first->start);
    const MarimayCompilerPackage *package = lsp_completion_package(analysis, snapshot);
    const MarimayTypeDescriptor *type;
    size_t index, count = 0;
    if (receiver == NULL) return probe->receiver_count == 1
        ? lsp_completion_namespace(candidates, analysis, snapshot, original + first->start,
            first->end - first->start, prefix, probe->prefix_length) : true;
    for (index = 1; index < probe->receiver_count && receiver != NULL; index += 1) {
        const MarimayToken *part = &probe->receiver[index];
        receiver = lsp_completion_field(analysis, package, receiver->type,
            original + part->start, part->end - part->start);
    }
    if (receiver == NULL) return true;
    type = mmay_type_descriptor(&analysis->result.checked.types, receiver->type);
    if (type == NULL || type->kind != MMAY_TYPE_KIND_NOMINAL) return true;
    for (index = 0; index < analysis->result.checked.symbols.length; index += 1) {
        const MarimayCheckedSymbol *field = mmay_checked_symbol(&analysis->result.checked, index);
        if (field->kind == MMAY_CHECKED_SYMBOL_FIELD
            && mmay_symbol_id_equal(field->owner_symbol, type->symbol)
            && lsp_completion_field_visible(field, package)
            && !lsp_completion_add(candidates, field->name.data, field->name.length,
                field, NULL, 5, prefix, probe->prefix_length)) return false;
    }
    if (mmay_compiler_service_completion_count(&analysis->target.service, &analysis->result,
            snapshot, type->symbol, &count) != MMAY_COMPILER_OK) return false;
    for (index = 0; index < count; index += 1) {
        const MarimayCheckedCompletion *item = NULL;
        const MarimayCheckedSymbol *symbol;
        if (mmay_compiler_service_completion(&analysis->target.service, &analysis->result,
                snapshot, type->symbol, index, &item) != MMAY_COMPILER_OK || item == NULL) return false;
        symbol = mmay_checked_symbol_by_id(&analysis->result.checked, item->symbol);
        if (!lsp_completion_add(candidates, item->name.data, item->name.length,
                symbol, NULL, 2, prefix, probe->prefix_length)) return false;
    }
    return true;
}

static bool lsp_completion_write(const LspCompletionCandidates *candidates,
    const LspAnalysis *analysis, const MarimaySourceSnapshot *original,
    const LspCompletionProbe *probe, MarimayWriter writer) {
    size_t index;
    if (!mmay_writer_write_cstr(writer, candidates->incomplete
            ? "{\"isIncomplete\":true,\"items\":[" : "{\"isIncomplete\":false,\"items\":[")) return false;
    for (index = 0; index < candidates->values.length; index += 1) {
        const LspCompletionCandidate *candidate = mmay_vector_at_const(&candidates->values, index);
        MarimayOwnedBytes spelling = {0};
        bool ok;
        if (candidate->symbol != NULL
            && mmay_checked_type_spelling(&analysis->result.checked, candidate->symbol->type,
                analysis->target.allocator, mmay_type_spelling_default_options(), &spelling)
                != MMAY_TYPE_SPELLING_OK) return false;
        ok = (index == 0 || mmay_writer_write_cstr(writer, ","))
            && mmay_writer_write_cstr(writer, "{\"label\":")
            && mmay_lsp_json_write_string(writer, candidate->name, candidate->length)
            && mmay_writer_write_cstr(writer, ",\"kind\":")
            && lsp_write_size(writer, candidate->kind)
            && mmay_writer_write_cstr(writer, ",\"detail\":");
        if (ok) {
            const char *detail = candidate->intrinsic != NULL
                ? candidate->intrinsic->signature : "keyword";
            ok = spelling.data != NULL
                ? mmay_lsp_json_write_string(writer, spelling.data, spelling.length)
                : mmay_lsp_json_write_string(writer, detail, strlen(detail));
        }
        if (ok) ok = mmay_writer_write_cstr(writer, ",\"textEdit\":{\"range\":")
            && lsp_write_range(writer, original, probe->prefix_start, probe->prefix_end)
            && mmay_writer_write_cstr(writer, ",\"newText\":")
            && mmay_lsp_json_write_string(writer, candidate->name, candidate->length)
            && mmay_writer_write_cstr(writer, "}}");
        mmay_owned_bytes_dispose(&spelling);
        if (!ok) return false;
    }
    return mmay_writer_write_cstr(writer, "]}");
}

LspQueryStatus lsp_completion(LspWorkspace *workspace,
    const MarimayLspJson *json, size_t params, MarimayWriter writer) {
    MarimayOwnedBytes uri = {0};
    const LspAnalysis *original = NULL;
    const LspAnalysis *checked = NULL;
    LspAnalysis analysis = {0};
    LspCompletionProbe probe = {0};
    LspCompletionCandidates candidates = {0};
    const LspDocument *document;
    const MarimaySourceSnapshot *snapshot = NULL;
    const MarimayCompilerUnit *unit = NULL;
    size_t text_document = mmay_lsp_json_get(json, params, "textDocument");
    size_t position = mmay_lsp_json_get(json, params, "position");
    size_t index, offset;
    int64_t line, character;
    LspStatus probe_status;
    LspQueryStatus status = LSP_QUERY_INVALID;
    LspSourceOverride override;
    if (!mmay_lsp_json_string(json, mmay_lsp_json_get(json, text_document, "uri"), &uri)
        || !mmay_lsp_json_integer(json, mmay_lsp_json_get(json, position, "line"), &line)
        || !mmay_lsp_json_integer(json, mmay_lsp_json_get(json, position, "character"), &character)
        || line < 0 || character < 0 || (uint64_t)line > SIZE_MAX || (uint64_t)character > SIZE_MAX)
        goto cleanup;
    document = lsp_workspace_document(workspace, (const char *)uri.data, uri.length);
    if (document == NULL || !lsp_position_to_offset(document->text.data, document->text.length,
            (size_t)line, (size_t)character, &offset)) goto cleanup;
    status = LSP_QUERY_FAILED;
    if (lsp_workspace_analysis(workspace, (const char *)uri.data, uri.length, &original) != LSP_OK) goto cleanup;
    for (index = 0; index < original->result.units.length; index += 1) {
        const MarimayCompilerUnit *candidate = mmay_compiler_result_unit(&original->result, index);
        if (lsp_completion_bytes(mmay_source_path(candidate->snapshot),
                mmay_source_path_length(candidate->snapshot), document->path.data, document->path.length)) {
            unit = candidate;
            break;
        }
    }
    probe_status = lsp_completion_probe_build(unit, offset, workspace->allocator, &probe);
    if (probe_status == LSP_INVALID) {
        status = mmay_writer_write_cstr(writer, "{\"isIncomplete\":false,\"items\":[]}")
            ? LSP_QUERY_OK : LSP_QUERY_FAILED;
        goto cleanup;
    }
    if (probe_status != LSP_OK) goto cleanup;
    if (!original->result.has_frontend_errors && original->result.checked.error_count == 0) {
        checked = original;
        goto candidates;
    }
    override = (LspSourceOverride){(const char *)document->path.data,
        document->path.length, probe.text.data, probe.text.length};
    for (index = 0; ; index += 1) {
        if (lsp_workspace_analyze_overrides(workspace, (const char *)uri.data, uri.length,
                &override, 1, false, &analysis) != LSP_OK) goto cleanup;
        if (!mmay_compiler_result_has_errors(&analysis.result)) break;
        if (index >= 7) break;
        probe_status = lsp_completion_probe_trim(unit, index, workspace->allocator, &probe);
        if (probe_status == LSP_INVALID) break;
        if (probe_status != LSP_OK) goto cleanup;
        lsp_analysis_dispose(&analysis);
        override.text = probe.text.data;
        override.text_length = probe.text.length;
    }
    checked = &analysis;
candidates:
    snapshot = lsp_analysis_snapshot(checked, (const char *)document->path.data, document->path.length);
    if (snapshot == NULL || !mmay_vector_init(&candidates.values, workspace->allocator,
            sizeof(LspCompletionCandidate))) goto cleanup;
    if (probe.member) {
        if (!lsp_completion_members(&candidates, checked, snapshot, &probe,
                mmay_source_bytes(unit->snapshot), mmay_source_bytes(unit->snapshot) + probe.prefix_start)) goto cleanup;
    } else if (!lsp_completion_unqualified(&candidates, checked, snapshot,
            probe.query_offset, mmay_source_bytes(unit->snapshot) + probe.prefix_start,
            probe.prefix_length)) goto cleanup;
    status = lsp_completion_write(&candidates, checked, unit->snapshot, &probe, writer)
        ? LSP_QUERY_OK : LSP_QUERY_FAILED;
cleanup:
    mmay_vector_dispose(&candidates.values);
    lsp_completion_probe_dispose(&probe);
    lsp_analysis_dispose(&analysis);
    mmay_owned_bytes_dispose(&uri);
    return status;
}
