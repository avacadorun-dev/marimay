#include "diagnostics.h"
#include "analysis_cache.h"
#include "output.h"
#include "source_locations.h"
#include "transport.h"

#include <string.h>

static bool lsp_diagnostic_begin(MarimayWriter writer,
    const MarimaySourceSnapshot *snapshot, size_t start, size_t end,
    const char *code, const char *message, MarimaySeverity severity, bool *first) {
    if (!*first && !mmay_writer_write_cstr(writer, ",")) return false;
    *first = false;
    return mmay_writer_write_cstr(writer, "{\"range\":")
        && (snapshot != NULL ? lsp_write_range(writer, snapshot, start, end)
            : mmay_writer_write_cstr(writer,
                "{\"start\":{\"line\":0,\"character\":0},"
                "\"end\":{\"line\":0,\"character\":0}}"))
        && mmay_writer_write_cstr(writer, ",\"severity\":")
        && lsp_write_size(writer, (size_t)severity + 1)
        && mmay_writer_write_cstr(writer, ",\"source\":\"mmay\",\"code\":")
        && mmay_lsp_json_write_string(writer, code, strlen(code))
        && mmay_writer_write_cstr(writer, ",\"message\":")
        && mmay_lsp_json_write_string(writer, message, strlen(message));
}

static bool lsp_diagnostic(MarimayWriter writer,
    const MarimaySourceSnapshot *snapshot, size_t start, size_t end,
    const char *code, const char *message, MarimaySeverity severity, bool *first) {
    return lsp_diagnostic_begin(writer, snapshot, start, end, code, message,
        severity, first) && mmay_writer_write_cstr(writer, "}");
}

static bool lsp_diagnostic_openable(const MarimaySourceSnapshot *snapshot) {
    const char *path = mmay_source_path(snapshot);
    size_t length = mmay_source_path_length(snapshot);
    return length != 0 && (path[0] == '/'
        || (length >= 3 && path[1] == ':' && (path[2] == '/' || path[2] == '\\')));
}

static bool lsp_related_diagnostic(MarimayWriter writer, MarimayAllocator allocator,
    const MarimaySourceSnapshot *snapshot, const char *code, const char *message,
    const MarimaySourceSnapshot *related, size_t start, size_t end,
    const char *related_message, bool *first) {
    if (!lsp_diagnostic_begin(writer, snapshot, 0, 0, code, message,
            MMAY_SEVERITY_ERROR, first)) return false;
    if (related != NULL && lsp_diagnostic_openable(related)) {
        if (!mmay_writer_write_cstr(writer, ",\"relatedInformation\":[{\"location\":")
            || !lsp_write_location(writer, allocator, related, start, end)
            || !mmay_writer_write_cstr(writer, ",\"message\":")
            || !mmay_lsp_json_write_string(writer, related_message,
                strlen(related_message))
            || !mmay_writer_write_cstr(writer, "}]")) return false;
    }
    return mmay_writer_write_cstr(writer, "}");
}

static const char *lsp_analysis_failure_message(LspStatus status) {
    switch (status) {
        case LSP_LIMIT:
            return "Analysis exceeded compiler limits; reduce the package or source size.";
        case LSP_ALLOCATION_FAILED:
            return "The compiler could not allocate memory to check this document.";
        case LSP_INPUT_ERROR:
            return "Cannot read this document or its module; check file access and mmay.mod.";
        default:
            return "Compiler analysis failed; restart the language server and check the editor log.";
    }
}

static bool lsp_analysis_diagnostic(MarimayWriter writer, MarimayAllocator allocator,
    const LspAnalysis *analysis, LspStatus status) {
    bool first = true;
    if (analysis != NULL) {
        const MarimayCliCheckTarget *target = &analysis->target;
        if (target->problem_code != NULL && target->problem_message != NULL)
            return lsp_related_diagnostic(writer, allocator, NULL,
                target->problem_code, target->problem_message, target->problem_source,
                target->problem_start, target->problem_end,
                "Fix this module configuration error before checking source files.", &first);
        if (target->manifest.diagnostics.length != 0) {
            size_t index = 0;
            for (index = 0; index < target->manifest.diagnostics.length; index += 1) {
                const MarimayManifestDiagnostic *diagnostic = mmay_manifest_diagnostic(
                    &target->manifest, index);
                if (!lsp_related_diagnostic(writer, allocator, NULL,
                        diagnostic->code, diagnostic->message, target->problem_source,
                        diagnostic->start, diagnostic->end,
                        "Fix this module configuration error before checking source files.",
                        &first)) return false;
            }
            return true;
        }
    }
    return lsp_diagnostic(writer, NULL, 0, 0, "lsp-analysis",
        lsp_analysis_failure_message(status), MMAY_SEVERITY_ERROR, &first);
}

static bool lsp_dependency_diagnostics(MarimayWriter writer,
    MarimayAllocator allocator, const LspAnalysis *analysis,
    const MarimaySourceSnapshot *snapshot, bool *first) {
    size_t index = 0;
    for (index = 0; index < analysis->result.units.length; index += 1) {
        const MarimayCompilerUnit *unit = mmay_compiler_result_unit(&analysis->result, index);
        const char *message = NULL;
        size_t start = 0, end = 0, diagnostic_index = 0;
        if (unit->snapshot == snapshot || !lsp_diagnostic_openable(unit->snapshot)) continue;
        if (unit->lexed.diagnostics.length != 0) {
            const MarimayLexDiagnostic *d = mmay_lex_diagnostic(&unit->lexed, 0);
            message = d->message;
            start = d->start;
            end = d->end;
        } else if (unit->parsed.diagnostics.length != 0) {
            const MarimayParseDiagnostic *d = mmay_parse_diagnostic(&unit->parsed, 0);
            message = d->message;
            start = d->start;
            end = d->end;
        } else {
            for (diagnostic_index = 0;
                diagnostic_index < analysis->result.checked.diagnostics.length;
                diagnostic_index += 1) {
                const MarimayCheckDiagnostic *d = mmay_check_diagnostic(
                    &analysis->result.checked, diagnostic_index);
                if (d->snapshot == unit->snapshot && d->severity == MMAY_SEVERITY_ERROR) {
                    message = d->message;
                    start = d->start;
                    end = d->end;
                    break;
                }
            }
        }
        if (message != NULL && !lsp_related_diagnostic(writer, allocator, snapshot,
                "lsp-dependency", "Checking is blocked by an error in another file; see the related location.",
                unit->snapshot, start, end, message, first)) return false;
    }
    return true;
}

bool lsp_write_document_diagnostics(MarimayWriter writer,
    MarimayAllocator allocator, const LspDocument *document,
    const LspAnalysis *analysis, LspStatus status) {
    const MarimaySourceSnapshot *snapshot = lsp_source_snapshot(analysis,
        (const char *)document->path.data, document->path.length);
    bool first = true;
    bool has_errors = false;
    bool source_view = lsp_source_is_view((const char *)document->path.data,
        document->path.length);
    size_t index = 0;
    if (source_view && analysis == NULL && status == LSP_INPUT_ERROR)
        return lsp_diagnostic(writer, NULL, 0, 0, "lsp-source-view",
            "Open a project source file and fix its compile errors to enable navigation in this generated standard-library view.",
            MMAY_SEVERITY_INFORMATION, &first);
    if (source_view && snapshot != NULL
        && (document->text.length != mmay_source_length(snapshot)
            || memcmp(document->text.data, mmay_source_bytes(snapshot),
                document->text.length) != 0))
        return lsp_diagnostic(writer, NULL, 0, 0, "lsp-source-view",
            "This is a generated standard-library source view. Changes here do not affect compilation; reopen the definition to restore the compiler source.",
            MMAY_SEVERITY_INFORMATION, &first);
    if (status != LSP_OK) {
        return lsp_analysis_diagnostic(writer, allocator, analysis, status);
    }
    if (analysis == NULL) return false;
    for (index = 0; index < analysis->result.units.length; index += 1) {
        const MarimayCompilerUnit *unit = mmay_compiler_result_unit(&analysis->result, index);
        size_t diagnostic_index = 0;
        if (unit->snapshot != snapshot) continue;
        for (diagnostic_index = 0; diagnostic_index < unit->lexed.diagnostics.length;
            diagnostic_index += 1) {
            const MarimayLexDiagnostic *d = mmay_lex_diagnostic(&unit->lexed, diagnostic_index);
            has_errors = true;
            if (!lsp_diagnostic(writer, snapshot, d->start, d->end, d->code,
                    d->message, MMAY_SEVERITY_ERROR, &first)) return false;
        }
        for (diagnostic_index = 0; diagnostic_index < unit->parsed.diagnostics.length;
            diagnostic_index += 1) {
            const MarimayParseDiagnostic *d = mmay_parse_diagnostic(&unit->parsed, diagnostic_index);
            has_errors = true;
            if (!lsp_diagnostic(writer, snapshot, d->start, d->end, d->code,
                    d->message, MMAY_SEVERITY_ERROR, &first)) return false;
        }
    }
    for (index = 0; index < analysis->result.checked.diagnostics.length; index += 1) {
        const MarimayCheckDiagnostic *d = mmay_check_diagnostic(&analysis->result.checked, index);
        if (d->snapshot == snapshot && d->severity == MMAY_SEVERITY_ERROR)
            has_errors = true;
        if (d->snapshot == snapshot && !lsp_diagnostic(writer, snapshot, d->start,
                d->end, d->code, d->message, d->severity, &first)) return false;
    }
    return source_view || has_errors
        || lsp_dependency_diagnostics(writer, allocator, analysis, snapshot, &first);
}

static bool lsp_diagnostics_prefix(MarimayWriter writer, const void *uri, size_t length) {
    return mmay_writer_write_cstr(writer,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":")
        && mmay_lsp_json_write_string(writer, uri, length);
}

bool lsp_clear_diagnostics(MarimayAllocator allocator, const void *uri,
    size_t length, MarimayWriter transport) {
    LspOutput output;
    bool ok = lsp_output_init(&output, allocator);
    MarimayWriter writer = lsp_output_writer(&output);
    if (ok) ok = lsp_diagnostics_prefix(writer, uri, length)
        && mmay_writer_write_cstr(writer, ",\"diagnostics\":[]}}")
        && mmay_lsp_write_message(transport, output.bytes.data, output.bytes.length);
    lsp_output_dispose(&output);
    return ok;
}

bool lsp_publish_diagnostics(LspWorkspace *workspace, MarimayWriter transport) {
    size_t index = 0;
    for (index = 0; index < workspace->documents.length; index += 1) {
        const LspDocument *document = mmay_vector_at_const(&workspace->documents, index);
        const LspAnalysis *analysis = NULL;
        LspOutput output;
        LspStatus status = lsp_workspace_analysis(workspace,
            (const char *)document->uri.data, document->uri.length, &analysis);
        bool ok = lsp_output_init(&output, workspace->allocator);
        MarimayWriter writer = lsp_output_writer(&output);
        if (ok) ok = lsp_diagnostics_prefix(writer, document->uri.data, document->uri.length)
            && mmay_writer_write_cstr(writer, ",\"version\":")
            && lsp_write_integer(writer, document->version)
            && mmay_writer_write_cstr(writer, ",\"diagnostics\":[")
            && lsp_write_document_diagnostics(writer, workspace->allocator,
                document, analysis, status)
            && mmay_writer_write_cstr(writer, "]}}")
            && mmay_lsp_write_message(transport, output.bytes.data, output.bytes.length);
        lsp_output_dispose(&output);
        if (!ok) return false;
    }
    return true;
}
