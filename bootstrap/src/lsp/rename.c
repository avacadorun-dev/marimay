#include "rename.h"
#include "analysis_overrides.h"
#include "output.h"
#include "positions.h"

#include <stdlib.h>
#include <string.h>

typedef struct LspRenameEdit {
    size_t start, end, new_start;
    MarimayOwnedBytes replacement;
    size_t target_start, target_length;
    size_t partner_start, partner_length;
    MarimaySymbolId partner;
    bool has_partner;
} LspRenameEdit;

typedef struct LspRenameFile {
    const MarimaySourceSnapshot *snapshot;
    MarimayVector edits;
    MarimayOwnedBytes text;
} LspRenameFile;

typedef struct LspRename {
    LspWorkspace *workspace;
    LspAnalysis original, staged;
    MarimayOwnedBytes uri, new_name;
    MarimayVector files, overrides;
    const MarimayCheckedSymbol *target;
    const MarimaySourceSnapshot *selected_snapshot;
    size_t selected_start, selected_end;
    size_t remaining_work;
} LspRename;

static bool lsp_rename_charge(LspRename *rename) {
    if (rename->remaining_work == 0) return false;
    rename->remaining_work -= 1;
    return true;
}

static bool lsp_rename_path_equal(const MarimaySourceSnapshot *left,
    const MarimaySourceSnapshot *right) {
    size_t length = mmay_source_path_length(left);
    return length == mmay_source_path_length(right)
        && memcmp(mmay_source_path(left), mmay_source_path(right), length) == 0;
}

static bool lsp_rename_openable(const MarimaySourceSnapshot *snapshot) {
    const char *path = mmay_source_path(snapshot);
    size_t length = mmay_source_path_length(snapshot);
    return length != 0 && (path[0] == '/'
        || (length >= 3 && path[1] == ':' && (path[2] == '/' || path[2] == '\\')));
}

static void lsp_rename_dispose(LspRename *rename) {
    size_t index, edit_index;
    lsp_analysis_dispose(&rename->staged);
    for (index = 0; index < rename->files.length; index += 1) {
        LspRenameFile *file = mmay_vector_at(&rename->files, index);
        for (edit_index = 0; edit_index < file->edits.length; edit_index += 1) {
            LspRenameEdit *edit = mmay_vector_at(&file->edits, edit_index);
            mmay_owned_bytes_dispose(&edit->replacement);
        }
        mmay_vector_dispose(&file->edits);
        mmay_owned_bytes_dispose(&file->text);
    }
    mmay_vector_dispose(&rename->files);
    mmay_vector_dispose(&rename->overrides);
    mmay_owned_bytes_dispose(&rename->uri);
    mmay_owned_bytes_dispose(&rename->new_name);
    lsp_analysis_dispose(&rename->original);
}

static LspQueryStatus lsp_rename_identifier(MarimayAllocator allocator,
    const MarimayOwnedBytes *name) {
    MarimaySourceManager sources = {0};
    const MarimaySourceSnapshot *snapshot = NULL;
    MarimayLexResult lexed = {0};
    const MarimayToken *token;
    LspQueryStatus status = LSP_QUERY_FAILED;
    if (name->length == 0 || name->length > 4096
        || memchr(name->data, 0, name->length) != NULL) return LSP_QUERY_REJECTED;
    if (!mmay_source_manager_init(&sources, allocator)) goto cleanup;
    if (mmay_source_manager_add(&sources, "rename.my", sizeof("rename.my") - 1,
            name->data, name->length, &snapshot) != MMAY_SOURCE_OK) goto cleanup;
    if (mmay_lex_snapshot(snapshot, allocator, mmay_lex_default_options(),
            &lexed) != MMAY_LEX_OK) goto cleanup;
    token = mmay_lex_token(&lexed, 0);
    status = lexed.diagnostics.length == 0 && lexed.tokens.length == 2
        && token != NULL && token->kind == MMAY_TOKEN_IDENTIFIER
        && token->start == 0 && token->end == name->length
        ? LSP_QUERY_OK : LSP_QUERY_REJECTED;
cleanup:
    mmay_lex_result_dispose(&lexed);
    mmay_source_manager_dispose(&sources);
    return status;
}

static const MarimayCheckedSymbol *lsp_rename_symbol_at(const LspAnalysis *analysis,
    const MarimaySourceSnapshot *snapshot, size_t offset, size_t *start, size_t *end) {
    const MarimayCheckedSymbol *symbol = mmay_checked_symbol_defined_at(
        &analysis->result.checked, snapshot, offset);
    const MarimayCheckedOccurrence *occurrence = symbol == NULL
        ? mmay_checked_occurrence_at(&analysis->result.checked, snapshot, offset) : NULL;
    size_t index;
    MarimaySymbolId selected;
    /* Editors keep the caret at the end of a selected identifier. Accept only
     * that exact token boundary, never a name separated by whitespace. */
    if (symbol == NULL && occurrence == NULL && offset != 0) {
        symbol = mmay_checked_symbol_defined_at(&analysis->result.checked, snapshot, offset - 1);
        occurrence = mmay_checked_occurrence_at(&analysis->result.checked, snapshot, offset - 1);
        if (symbol != NULL && symbol->end != offset) symbol = NULL;
        if (occurrence != NULL && occurrence->end != offset) occurrence = NULL;
    }
    if (symbol != NULL) {
        *start = symbol->start;
        *end = symbol->end;
        selected = symbol->symbol;
    } else {
        if (occurrence == NULL) return NULL;
        *start = occurrence->start;
        *end = occurrence->end;
        selected = occurrence->symbol;
    }
    for (index = 0; index < analysis->result.checked.symbols.length; index += 1) {
        const MarimayCheckedSymbol *other = mmay_checked_symbol(&analysis->result.checked, index);
        if (other->snapshot == snapshot && other->start == *start && other->end == *end
            && !mmay_symbol_id_equal(other->symbol, selected)) return NULL;
    }
    for (index = 0; index < analysis->result.checked.occurrences.length; index += 1) {
        const MarimayCheckedOccurrence *other = mmay_checked_occurrence(
            &analysis->result.checked, index);
        if (other->snapshot == snapshot && other->start == *start
            && other->end == *end
            && !mmay_symbol_id_equal(other->symbol, selected)) return NULL;
    }
    return mmay_checked_symbol_by_id(&analysis->result.checked, selected);
}

static bool lsp_rename_field_site(LspRename *rename, const MarimayAstNode *node,
    size_t start, size_t end, size_t depth) {
    size_t index;
    if (!lsp_rename_charge(rename)) return true;
    if (node == NULL || depth > 256 || start < node->start || end > node->end) return false;
    if (node->kind == MMAY_SYNTAX_MEMBER_EXPRESSION
        || node->kind == MMAY_SYNTAX_FIELD_VALUE
        || node->kind == MMAY_SYNTAX_PATTERN_FIELD
        || node->kind == MMAY_SYNTAX_FIELD_DECL) {
        for (index = 0; index < node->child_count; index += 1) {
            const MarimayAstElement *element = mmay_ast_child(node, index);
            if (element->kind == MMAY_AST_ELEMENT_TOKEN
                && element->as.token.kind == MMAY_TOKEN_IDENTIFIER
                && element->as.token.start == start && element->as.token.end == end) return true;
        }
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element->kind == MMAY_AST_ELEMENT_NODE
            && lsp_rename_field_site(rename, element->as.node, start, end, depth + 1)) return true;
    }
    return false;
}

/* Unindexed same-spelling identifiers (for example import aliases or generic
 * bodies) make a complete rename unprovable. Comments/strings are not identifiers. */
static bool lsp_rename_names_indexed(LspRename *rename) {
    size_t unit_index, token_index, index;
    for (unit_index = 0; unit_index < rename->original.result.units.length; unit_index += 1) {
        const MarimayCompilerUnit *unit = mmay_compiler_result_unit(
            &rename->original.result, unit_index);
        if (!lsp_rename_openable(unit->snapshot)) continue;
        for (token_index = 0; token_index < unit->lexed.tokens.length; token_index += 1) {
            const MarimayToken *token = mmay_lex_token(&unit->lexed, token_index);
            const unsigned char *bytes;
            size_t length;
            bool known = false;
            if (!lsp_rename_charge(rename)) return false;
            if (token->kind != MMAY_TOKEN_IDENTIFIER) continue;
            if ((rename->target->kind == MMAY_CHECKED_SYMBOL_LOCAL
                    || rename->target->kind == MMAY_CHECKED_SYMBOL_PARAMETER)
                && (rename->target->snapshot != unit->snapshot
                    || token->start < rename->target->scope_start
                    || token->end > rename->target->scope_end)) continue;
            length = token->end - token->start;
            bytes = mmay_source_bytes(unit->snapshot) + token->start;
            if (!(length == rename->target->name.length
                    && memcmp(bytes, rename->target->name.data, length) == 0)
                && !(length == rename->new_name.length
                    && memcmp(bytes, rename->new_name.data, length) == 0)) continue;
            for (index = 0; index < rename->original.result.checked.symbols.length; index += 1) {
                if (!lsp_rename_charge(rename)) return false;
                const MarimayCheckedSymbol *symbol = mmay_checked_symbol(
                    &rename->original.result.checked, index);
                if (symbol->snapshot == unit->snapshot && symbol->start == token->start
                    && symbol->end == token->end) { known = true; break; }
            }
            for (index = 0; !known && index < rename->original.result.checked.occurrences.length;
                index += 1) {
                if (!lsp_rename_charge(rename)) return false;
                const MarimayCheckedOccurrence *occurrence = mmay_checked_occurrence(
                    &rename->original.result.checked, index);
                known = occurrence->snapshot == unit->snapshot && occurrence->start == token->start
                    && occurrence->end == token->end;
            }
            if (!known && (rename->target->kind != MMAY_CHECKED_SYMBOL_FIELD
                    || lsp_rename_field_site(rename, unit->ast.root, token->start, token->end, 0)))
                return false;
        }
    }
    return true;
}

static bool lsp_rename_is_shorthand(LspRename *rename, const MarimayAstNode *node,
    size_t start, size_t end, size_t depth) {
    size_t index;
    if (!lsp_rename_charge(rename)) return false;
    if (node == NULL || depth > 256 || start < node->start || end > node->end) return false;
    if (node->kind == MMAY_SYNTAX_FIELD_VALUE || node->kind == MMAY_SYNTAX_PATTERN_FIELD) {
        const MarimaySyntaxToken *identifier = NULL;
        for (index = 0; index < node->child_count; index += 1) {
            const MarimayAstElement *element = mmay_ast_child(node, index);
            if (element->kind == MMAY_AST_ELEMENT_NODE) return false;
            if (element->as.token.kind == MMAY_TOKEN_COLON) return false;
            if (element->as.token.kind == MMAY_TOKEN_IDENTIFIER) identifier = &element->as.token;
        }
        return identifier != NULL && identifier->start == start && identifier->end == end;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element->kind == MMAY_AST_ELEMENT_NODE
            && lsp_rename_is_shorthand(rename, element->as.node, start, end, depth + 1)) return true;
    }
    return false;
}

static bool lsp_rename_shorthand(LspRename *rename,
    const MarimaySourceSnapshot *snapshot, size_t start, size_t end) {
    size_t index;
    for (index = 0; index < rename->original.result.units.length; index += 1) {
        const MarimayCompilerUnit *unit = mmay_compiler_result_unit(&rename->original.result, index);
        if (unit->snapshot == snapshot)
            return lsp_rename_is_shorthand(rename, unit->ast.root, start, end, 0);
    }
    return false;
}

static LspRenameFile *lsp_rename_file(LspRename *rename,
    const MarimaySourceSnapshot *snapshot) {
    size_t index;
    LspRenameFile file = {0};
    for (index = 0; index < rename->files.length; index += 1) {
        if (!lsp_rename_charge(rename)) return NULL;
        LspRenameFile *existing = mmay_vector_at(&rename->files, index);
        if (existing->snapshot == snapshot) return existing;
    }
    file.snapshot = snapshot;
    if (!mmay_vector_init(&file.edits, rename->workspace->allocator, sizeof(LspRenameEdit))) return NULL;
    if (!mmay_vector_push(&rename->files, &file)) {
        mmay_vector_dispose(&file.edits);
        return NULL;
    }
    return mmay_vector_at(&rename->files, rename->files.length - 1);
}

static LspQueryStatus lsp_rename_add_edit(LspRename *rename,
    const MarimaySourceSnapshot *snapshot, size_t start, size_t end) {
    LspRenameFile *file;
    LspRenameEdit edit = {0};
    const MarimayCheckedSymbol *partner = NULL;
    size_t index;
    LspOutput replacement;
    MarimayWriter writer;
    bool ok;
    if (!lsp_rename_openable(snapshot)) return LSP_QUERY_REJECTED;
    file = lsp_rename_file(rename, snapshot);
    if (file == NULL) return rename->remaining_work == 0 ? LSP_QUERY_REJECTED : LSP_QUERY_FAILED;
    for (index = 0; index < file->edits.length; index += 1) {
        if (!lsp_rename_charge(rename)) return LSP_QUERY_REJECTED;
        const LspRenameEdit *existing = mmay_vector_at_const(&file->edits, index);
        if (existing->start == start && existing->end == end) return LSP_QUERY_OK;
    }
    for (index = 0; index < rename->original.result.checked.occurrences.length; index += 1) {
        if (!lsp_rename_charge(rename)) return LSP_QUERY_REJECTED;
        const MarimayCheckedOccurrence *occurrence = mmay_checked_occurrence(
            &rename->original.result.checked, index);
        if (occurrence->snapshot != snapshot || occurrence->start != start
            || occurrence->end != end || mmay_symbol_id_equal(
                occurrence->symbol, rename->target->symbol)) continue;
        const MarimayCheckedSymbol *other = mmay_checked_symbol_by_id(
            &rename->original.result.checked, occurrence->symbol);
        if (other == NULL || (partner != NULL
                && !mmay_symbol_id_equal(partner->symbol, other->symbol))) return LSP_QUERY_REJECTED;
        partner = other;
    }
    for (index = 0; index < rename->original.result.checked.symbols.length; index += 1) {
        if (!lsp_rename_charge(rename)) return LSP_QUERY_REJECTED;
        const MarimayCheckedSymbol *other = mmay_checked_symbol(
            &rename->original.result.checked, index);
        if (other->snapshot != snapshot || other->start != start || other->end != end
            || mmay_symbol_id_equal(other->symbol, rename->target->symbol)) continue;
        if (partner != NULL && !mmay_symbol_id_equal(partner->symbol, other->symbol))
            return LSP_QUERY_REJECTED;
        partner = other;
    }
    edit.start = start;
    edit.end = end;
    edit.target_length = rename->new_name.length;
    if (partner != NULL) {
        bool field = rename->target->kind == MMAY_CHECKED_SYMBOL_FIELD;
        bool local = rename->target->kind == MMAY_CHECKED_SYMBOL_LOCAL
            || rename->target->kind == MMAY_CHECKED_SYMBOL_PARAMETER;
        if ((!field && !local) || (field && partner->kind != MMAY_CHECKED_SYMBOL_LOCAL
                && partner->kind != MMAY_CHECKED_SYMBOL_PARAMETER)
            || (local && partner->kind != MMAY_CHECKED_SYMBOL_FIELD)
            || !lsp_rename_shorthand(rename, snapshot, start, end)) return LSP_QUERY_REJECTED;
        edit.has_partner = true;
        edit.partner = partner->symbol;
        edit.partner_length = end - start;
        edit.target_start = field ? 0 : end - start + 2;
        edit.partner_start = field ? rename->new_name.length + 2 : 0;
    }
    if (!lsp_output_init(&replacement, rename->workspace->allocator)) return LSP_QUERY_FAILED;
    writer = lsp_output_writer(&replacement);
    if (partner == NULL) ok = mmay_writer_write(writer,
        (const char *)rename->new_name.data, rename->new_name.length);
    else if (rename->target->kind == MMAY_CHECKED_SYMBOL_FIELD) ok =
        mmay_writer_write(writer, (const char *)rename->new_name.data, rename->new_name.length)
        && mmay_writer_write_cstr(writer, ": ")
        && mmay_writer_write(writer, (const char *)mmay_source_bytes(snapshot) + start, end - start);
    else ok = mmay_writer_write(writer, (const char *)mmay_source_bytes(snapshot) + start, end - start)
        && mmay_writer_write_cstr(writer, ": ")
        && mmay_writer_write(writer, (const char *)rename->new_name.data, rename->new_name.length);
    if (ok) ok = mmay_owned_bytes_copy(&edit.replacement, rename->workspace->allocator,
        replacement.bytes.data, replacement.bytes.length);
    lsp_output_dispose(&replacement);
    if (ok) ok = mmay_vector_push(&file->edits, &edit);
    if (!ok) mmay_owned_bytes_dispose(&edit.replacement);
    return ok ? LSP_QUERY_OK : LSP_QUERY_FAILED;
}

static int lsp_rename_compare_edits(const void *left, const void *right) {
    const LspRenameEdit *a = left, *b = right;
    return a->start < b->start ? -1 : a->start > b->start ? 1 : 0;
}

static LspQueryStatus lsp_rename_build_edits(LspRename *rename) {
    LspQueryStatus status = lsp_rename_add_edit(rename, rename->target->snapshot,
        rename->target->start, rename->target->end);
    size_t index, edit_index;
    if (status != LSP_QUERY_OK) return status;
    for (index = 0; index < rename->original.result.checked.occurrences.length; index += 1) {
        const MarimayCheckedOccurrence *occurrence = mmay_checked_occurrence(
            &rename->original.result.checked, index);
        if (!mmay_symbol_id_equal(occurrence->symbol, rename->target->symbol)) continue;
        status = lsp_rename_add_edit(rename, occurrence->snapshot, occurrence->start, occurrence->end);
        if (status != LSP_QUERY_OK) return status;
    }
    for (index = 0; index < rename->files.length; index += 1) {
        LspRenameFile *file = mmay_vector_at(&rename->files, index);
        LspOutput text;
        MarimayWriter writer;
        size_t cursor = 0;
        bool ok = lsp_output_init(&text, rename->workspace->allocator);
        writer = lsp_output_writer(&text);
        qsort(file->edits.data, file->edits.length, file->edits.element_size,
            lsp_rename_compare_edits);
        for (edit_index = 0; ok && edit_index < file->edits.length; edit_index += 1) {
            LspRenameEdit *edit = mmay_vector_at(&file->edits, edit_index);
            if (edit->start < cursor || edit->end > mmay_source_length(file->snapshot)) {
                lsp_output_dispose(&text);
                return LSP_QUERY_REJECTED;
            }
            ok = mmay_writer_write(writer, (const char *)mmay_source_bytes(file->snapshot)
                + cursor, edit->start - cursor);
            edit->new_start = text.bytes.length;
            if (ok) ok = mmay_writer_write(writer, (const char *)edit->replacement.data,
                edit->replacement.length);
            cursor = edit->end;
        }
        if (ok) ok = mmay_writer_write(writer,
            (const char *)mmay_source_bytes(file->snapshot) + cursor,
            mmay_source_length(file->snapshot) - cursor)
            && mmay_owned_bytes_copy(&file->text, rename->workspace->allocator,
                text.bytes.data, text.bytes.length);
        lsp_output_dispose(&text);
        if (!ok) return LSP_QUERY_FAILED;
        LspSourceOverride override = {mmay_source_path(file->snapshot),
            mmay_source_path_length(file->snapshot), file->text.data, file->text.length};
        if (!mmay_vector_push(&rename->overrides, &override)) return LSP_QUERY_FAILED;
    }
    return LSP_QUERY_OK;
}

static bool lsp_rename_map(LspRename *rename,
    const MarimaySourceSnapshot *snapshot, size_t start, size_t end,
    MarimaySymbolId symbol, size_t *mapped_start, size_t *mapped_end) {
    size_t index, edit_index, old_cursor = 0, new_cursor = 0;
    const LspRenameFile *file = NULL;
    for (index = 0; index < rename->files.length; index += 1) {
        if (!lsp_rename_charge(rename)) return false;
        const LspRenameFile *candidate = mmay_vector_at_const(&rename->files, index);
        if (candidate->snapshot == snapshot) { file = candidate; break; }
    }
    if (file == NULL) { *mapped_start = start; *mapped_end = end; return true; }
    for (edit_index = 0; edit_index < file->edits.length; edit_index += 1) {
        if (!lsp_rename_charge(rename)) return false;
        const LspRenameEdit *edit = mmay_vector_at_const(&file->edits, edit_index);
        if (start == edit->start && end == edit->end) {
            if (mmay_symbol_id_equal(symbol, rename->target->symbol)) {
                *mapped_start = edit->new_start + edit->target_start;
                *mapped_end = *mapped_start + edit->target_length;
                return true;
            }
            if (edit->has_partner && mmay_symbol_id_equal(symbol, edit->partner)) {
                *mapped_start = edit->new_start + edit->partner_start;
                *mapped_end = *mapped_start + edit->partner_length;
                return true;
            }
            return false;
        }
        if (end <= edit->start) break;
        if (start < edit->end) return false;
        old_cursor = edit->end;
        new_cursor = edit->new_start + edit->replacement.length;
    }
    if (start < old_cursor || !mmay_size_add(new_cursor, start - old_cursor, mapped_start)
        || !mmay_size_add(new_cursor, end - old_cursor, mapped_end)) return false;
    return true;
}

static const MarimayCheckedSymbol *lsp_rename_corresponding(LspRename *rename,
    const MarimayCheckedSymbol *original) {
    size_t start = 0, end = 0, index;
    const MarimayCheckedSymbol *found = NULL;
    const MarimayOwnedBytes *name = mmay_symbol_id_equal(original->symbol,
        rename->target->symbol) ? &rename->new_name : &original->name;
    if (!lsp_rename_map(rename, original->snapshot, original->start, original->end,
            original->symbol, &start, &end)) return NULL;
    for (index = 0; index < rename->staged.result.checked.symbols.length; index += 1) {
        if (!lsp_rename_charge(rename)) return NULL;
        const MarimayCheckedSymbol *candidate = mmay_checked_symbol(&rename->staged.result.checked, index);
        if (candidate->kind == original->kind && candidate->start == start && candidate->end == end
            && lsp_rename_path_equal(original->snapshot, candidate->snapshot)
            && candidate->name.length == name->length
            && memcmp(candidate->name.data, name->data, name->length) == 0) {
            if (found != NULL) return NULL;
            found = candidate;
        }
    }
    return found;
}

static bool lsp_rename_bindings_preserved(LspRename *rename) {
    size_t index, other_index;
    const MarimayCheckResult *before = &rename->original.result.checked;
    const MarimayCheckResult *after = &rename->staged.result.checked;
    if (before->symbols.length != after->symbols.length
        || before->occurrences.length != after->occurrences.length) return false;
    for (index = 0; index < before->symbols.length; index += 1) {
        if (lsp_rename_corresponding(rename, mmay_checked_symbol(before, index)) == NULL) return false;
    }
    for (index = 0; index < before->occurrences.length; index += 1) {
        const MarimayCheckedOccurrence *occurrence = mmay_checked_occurrence(before, index);
        const MarimayCheckedSymbol *definition = mmay_checked_symbol_by_id(before, occurrence->symbol);
        const MarimayCheckedSymbol *mapped = definition != NULL
            ? lsp_rename_corresponding(rename, definition) : NULL;
        size_t start = 0, end = 0;
        bool found = false;
        if (mapped == NULL || !lsp_rename_map(rename, occurrence->snapshot,
                occurrence->start, occurrence->end, occurrence->symbol, &start, &end)) return false;
        for (other_index = 0; other_index < after->occurrences.length; other_index += 1) {
            if (!lsp_rename_charge(rename)) return false;
            const MarimayCheckedOccurrence *other = mmay_checked_occurrence(after, other_index);
            if (other->start == start && other->end == end
                && lsp_rename_path_equal(occurrence->snapshot, other->snapshot)
                && mmay_symbol_id_equal(other->symbol, mapped->symbol)) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

static bool lsp_rename_write(const LspRename *rename, MarimayWriter writer) {
    size_t index, edit_index, document_index;
    if (!mmay_writer_write_cstr(writer, "{\"documentChanges\":[")) return false;
    for (index = 0; index < rename->files.length; index += 1) {
        const LspRenameFile *file = mmay_vector_at_const(&rename->files, index);
        const LspDocument *open = NULL;
        MarimayOwnedBytes uri = {0};
        bool ok;
        if (lsp_path_to_uri(rename->workspace->allocator, mmay_source_path(file->snapshot),
                mmay_source_path_length(file->snapshot), &uri) != LSP_OK) return false;
        for (document_index = 0; document_index < rename->workspace->documents.length; document_index += 1) {
            const LspDocument *document = mmay_vector_at_const(&rename->workspace->documents, document_index);
            if (document->path.length == mmay_source_path_length(file->snapshot)
                && memcmp(document->path.data, mmay_source_path(file->snapshot), document->path.length) == 0)
                open = document;
        }
        ok = (index == 0 || mmay_writer_write_cstr(writer, ","))
            && mmay_writer_write_cstr(writer, "{\"textDocument\":{\"uri\":")
            && mmay_lsp_json_write_string(writer, uri.data, uri.length)
            && mmay_writer_write_cstr(writer, ",\"version\":")
            && (open != NULL ? lsp_write_integer(writer, open->version)
                : mmay_writer_write_cstr(writer, "null"))
            && mmay_writer_write_cstr(writer, "},\"edits\":[");
        mmay_owned_bytes_dispose(&uri);
        if (!ok) return false;
        for (edit_index = 0; edit_index < file->edits.length; edit_index += 1) {
            const LspRenameEdit *edit = mmay_vector_at_const(&file->edits, edit_index);
            if ((edit_index != 0 && !mmay_writer_write_cstr(writer, ","))
                || !mmay_writer_write_cstr(writer, "{\"range\":")
                || !lsp_write_range(writer, file->snapshot, edit->start, edit->end)
                || !mmay_writer_write_cstr(writer, ",\"newText\":")
                || !mmay_lsp_json_write_string(writer, edit->replacement.data, edit->replacement.length)
                || !mmay_writer_write_cstr(writer, "}")) return false;
        }
        if (!mmay_writer_write_cstr(writer, "]}")) return false;
    }
    return mmay_writer_write_cstr(writer, "]}");
}

LspQueryStatus lsp_rename(LspWorkspace *workspace, const MarimayLspJson *json,
    size_t params, bool prepare, MarimayWriter writer) {
    LspRename rename = {0};
    LspQueryStatus status = LSP_QUERY_INVALID;
    const LspDocument *document;
    int64_t line = 0, character = 0;
    size_t offset = 0;
    size_t text_document = mmay_lsp_json_get(json, params, "textDocument");
    size_t position = mmay_lsp_json_get(json, params, "position");
    rename.workspace = workspace;
    rename.remaining_work = 32U * 1024U * 1024U;
    if (!mmay_vector_init(&rename.files, workspace->allocator, sizeof(LspRenameFile))
        || !mmay_vector_init(&rename.overrides, workspace->allocator, sizeof(LspSourceOverride))) {
        status = LSP_QUERY_FAILED;
        goto cleanup;
    }
    if (!mmay_lsp_json_string(json, mmay_lsp_json_get(json, text_document, "uri"), &rename.uri)) goto cleanup;
    document = lsp_workspace_document(workspace, (const char *)rename.uri.data, rename.uri.length);
    if (document == NULL || !mmay_lsp_json_integer(json,
            mmay_lsp_json_get(json, position, "line"), &line)
        || !mmay_lsp_json_integer(json, mmay_lsp_json_get(json, position, "character"), &character)
        || line < 0 || character < 0 || (uint64_t)line > SIZE_MAX || (uint64_t)character > SIZE_MAX
        || !lsp_position_to_offset(document->text.data, document->text.length,
            (size_t)line, (size_t)character, &offset)) goto cleanup;
    if (!prepare) {
        if (!mmay_lsp_json_string(json, mmay_lsp_json_get(json, params, "newName"), &rename.new_name)) goto cleanup;
        status = lsp_rename_identifier(workspace->allocator, &rename.new_name);
        if (status != LSP_QUERY_OK) goto cleanup;
    }
    status = LSP_QUERY_FAILED;
    if (lsp_workspace_analyze_overrides(workspace, (const char *)rename.uri.data,
            rename.uri.length, NULL, 0, true, &rename.original) != LSP_OK) goto cleanup;
    status = LSP_QUERY_REJECTED;
    if (mmay_compiler_result_has_errors(&rename.original.result)) goto cleanup;
    rename.selected_snapshot = lsp_analysis_snapshot(&rename.original,
        (const char *)document->path.data, document->path.length);
    if (rename.selected_snapshot == NULL) goto cleanup;
    rename.target = lsp_rename_symbol_at(&rename.original, rename.selected_snapshot,
        offset, &rename.selected_start, &rename.selected_end);
    if (rename.target == NULL || !lsp_rename_openable(rename.target->snapshot)
        || !lsp_rename_names_indexed(&rename)) goto cleanup;
    if (!prepare && rename.new_name.length == rename.target->name.length
        && memcmp(rename.new_name.data, rename.target->name.data,
            rename.new_name.length) == 0) {
        status = mmay_writer_write_cstr(writer, "{\"documentChanges\":[]}")
            ? LSP_QUERY_OK : LSP_QUERY_FAILED;
        goto cleanup;
    }
    if (prepare) {
        bool ok = mmay_writer_write_cstr(writer, "{\"range\":")
            && lsp_write_range(writer, rename.selected_snapshot, rename.selected_start, rename.selected_end)
            && mmay_writer_write_cstr(writer, ",\"placeholder\":")
            && mmay_lsp_json_write_string(writer, rename.target->name.data, rename.target->name.length)
            && mmay_writer_write_cstr(writer, "}");
        status = ok ? LSP_QUERY_OK : LSP_QUERY_FAILED;
        goto cleanup;
    }
    status = lsp_rename_build_edits(&rename);
    if (status != LSP_QUERY_OK) goto cleanup;
    status = LSP_QUERY_FAILED;
    if (lsp_workspace_analyze_overrides(workspace, (const char *)rename.uri.data,
            rename.uri.length, rename.overrides.data, rename.overrides.length,
            true, &rename.staged) != LSP_OK) goto cleanup;
    status = LSP_QUERY_REJECTED;
    if (mmay_compiler_result_has_errors(&rename.staged.result)
        || !lsp_rename_bindings_preserved(&rename)) goto cleanup;
    status = lsp_rename_write(&rename, writer) ? LSP_QUERY_OK : LSP_QUERY_FAILED;
cleanup:
    lsp_rename_dispose(&rename);
    return status;
}
