#include "cli_internal.h"

#include <stdio.h>
#include <string.h>

#include "ruda/format/formatter.h"
#include "ruda/generated/diagnostic_codes.h"
#include "ruda/lex/lexer.h"
#include "ruda/parse/parser.h"
#include "ruda/source/source_manager.h"
static bool ruda_cli_source_error(
    RudaWriter error_writer,
    RudaDiagnosticStyle style,
    const RudaSourceSnapshot *source,
    const char *code,
    const char *message,
    size_t start,
    size_t end
) {
    RudaDiagnostic diagnostic;
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = code;
    diagnostic.severity = RUDA_SEVERITY_ERROR;
    diagnostic.message = message;
    diagnostic.source = source;
    diagnostic.source_start = start;
    diagnostic.source_end = end;
    return ruda_diagnostic_render(error_writer, style, &diagnostic);
}

static bool ruda_cli_render_frontend_diagnostics(
    RudaWriter error_writer,
    RudaDiagnosticStyle style,
    const RudaLexResult *lexed,
    const RudaParseResult *parsed
) {
    size_t lex_index = 0;
    size_t parse_index = 0;
    while (lex_index < lexed->diagnostics.length
        || parse_index < parsed->diagnostics.length) {
        const RudaLexDiagnostic *lex_diagnostic = lex_index
                < lexed->diagnostics.length
            ? ruda_lex_diagnostic(lexed, lex_index)
            : NULL;
        const RudaParseDiagnostic *parse_diagnostic = parse_index
                < parsed->diagnostics.length
            ? ruda_parse_diagnostic(parsed, parse_index)
            : NULL;
        bool use_lex = parse_diagnostic == NULL
            || (lex_diagnostic != NULL
                && (lex_diagnostic->start < parse_diagnostic->start
                    || (lex_diagnostic->start == parse_diagnostic->start
                        && strcmp(
                            lex_diagnostic->code,
                            parse_diagnostic->code
                        ) <= 0)));
        if ((use_lex && lex_diagnostic == NULL)
            || (!use_lex && parse_diagnostic == NULL)) {
            return false;
        }
        if (use_lex) {
            if (!ruda_cli_source_error(
                    error_writer,
                    style,
                    lexed->snapshot,
                    lex_diagnostic->code,
                    lex_diagnostic->message,
                    lex_diagnostic->start,
                    lex_diagnostic->end
                )) {
                return false;
            }
            lex_index += 1;
        } else {
            if (!ruda_cli_source_error(
                    error_writer,
                    style,
                    lexed->snapshot,
                    parse_diagnostic->code,
                    parse_diagnostic->message,
                    parse_diagnostic->start,
                    parse_diagnostic->end
                )) {
                return false;
            }
            parse_index += 1;
        }
    }
    if (lexed->suppressed_diagnostics != 0
        || parsed->suppressed_diagnostics != 0) {
        char message[160];
        size_t suppressed = 0;
        int length = 0;
        if (!ruda_size_add(
                lexed->suppressed_diagnostics,
                parsed->suppressed_diagnostics,
                &suppressed
            )) {
            return false;
        }
        length = snprintf(
            message,
            sizeof(message),
            "%zu additional frontend diagnostics were suppressed by the configured limit",
            suppressed
        );
        if (length < 0 || (size_t)length >= sizeof(message)
            || !ruda_cli_source_error(
                error_writer,
                style,
                lexed->snapshot,
                RUDA_DIAGNOSTIC_FORMAT_LIMIT,
                message,
                ruda_source_length(lexed->snapshot),
                ruda_source_length(lexed->snapshot)
            )) {
            return false;
        }
    }
    return true;
}
bool ruda_cli_manifest_kind(
    const char *path,
    RudaManifestKind *kind
) {
    const char *base = path;
    const char *cursor = path;
    if (path == NULL || kind == NULL) {
        return false;
    }
    while (*cursor != '\0') {
        if (*cursor == '/' || *cursor == '\\') {
            base = cursor + 1;
        }
        cursor += 1;
    }
    if (strcmp(base, "ruda.mod") == 0) {
        *kind = RUDA_MANIFEST_MODULE;
        return true;
    }
    if (strcmp(base, "ruda.work") == 0) {
        *kind = RUDA_MANIFEST_WORKSPACE;
        return true;
    }
    return false;
}

static const char *ruda_cli_path_base(const char *path) {
    const char *base = path;
    const char *cursor = path;
    while (*cursor != '\0') {
        if (*cursor == '/' || *cursor == '\\') {
            base = cursor + 1;
        }
        cursor += 1;
    }
    return base;
}

bool ruda_cli_source_path(const char *path) {
    const char *base = ruda_cli_path_base(path);
    size_t length = strlen(base);
    return length >= 5 && memcmp(base + length - 5, ".ruda", 5) == 0;
}

bool ruda_cli_format_file_path(const char *path) {
    RudaManifestKind kind;
    return ruda_cli_source_path(path) || ruda_cli_manifest_kind(path, &kind);
}

static bool ruda_cli_write_size(RudaWriter writer, size_t value) {
    char buffer[32];
    int length = snprintf(buffer, sizeof(buffer), "%zu", value);
    return length >= 0 && (size_t)length < sizeof(buffer)
        && ruda_writer_write(writer, buffer, (size_t)length);
}

static size_t ruda_cli_diff_line_count(
    const unsigned char *bytes,
    size_t length
) {
    size_t cursor = 0;
    size_t count = 0;
    while (cursor < length) {
        count += 1;
        while (cursor < length && bytes[cursor] != '\r'
            && bytes[cursor] != '\n') {
            cursor += 1;
        }
        if (cursor < length && bytes[cursor] == '\r'
            && cursor + 1 < length && bytes[cursor + 1] == '\n') {
            cursor += 2;
        } else if (cursor < length) {
            cursor += 1;
        }
    }
    return count;
}

static bool ruda_cli_write_diff_lines(
    RudaWriter writer,
    const char *prefix,
    const unsigned char *bytes,
    size_t length
) {
    size_t cursor = 0;
    while (cursor < length) {
        size_t start = cursor;
        bool newline = false;
        while (cursor < length && bytes[cursor] != '\r'
            && bytes[cursor] != '\n') {
            cursor += 1;
        }
        if (!ruda_writer_write_cstr(writer, prefix)
            || !ruda_writer_write(
                writer,
                (const char *)bytes + start,
                cursor - start
            )
            || !ruda_writer_write_cstr(writer, "\n")) {
            return false;
        }
        if (cursor < length && bytes[cursor] == '\r'
            && cursor + 1 < length && bytes[cursor + 1] == '\n') {
            cursor += 2;
            newline = true;
        } else if (cursor < length) {
            cursor += 1;
            newline = true;
        }
        if (!newline
            && !ruda_writer_write_cstr(
                writer,
                "\\ No newline at end of file\n"
            )) {
            return false;
        }
    }
    return true;
}

static bool ruda_cli_write_format_diff(
    RudaWriter writer,
    const char *path,
    const unsigned char *original,
    size_t original_length,
    const unsigned char *formatted,
    size_t formatted_length
) {
    char display[128];
    size_t original_lines = ruda_cli_diff_line_count(original, original_length);
    size_t formatted_lines = ruda_cli_diff_line_count(formatted, formatted_length);
    ruda_display_argument(path, display, sizeof(display));
    return ruda_writer_write_cstr(writer, "--- ")
        && ruda_writer_write_cstr(writer, display)
        && ruda_writer_write_cstr(writer, "\n+++ ")
        && ruda_writer_write_cstr(writer, display)
        && ruda_writer_write_cstr(writer, "\n@@ -")
        && ruda_cli_write_size(writer, original_lines == 0 ? 0 : 1)
        && ruda_writer_write_cstr(writer, ",")
        && ruda_cli_write_size(writer, original_lines)
        && ruda_writer_write_cstr(writer, " +")
        && ruda_cli_write_size(writer, formatted_lines == 0 ? 0 : 1)
        && ruda_writer_write_cstr(writer, ",")
        && ruda_cli_write_size(writer, formatted_lines)
        && ruda_writer_write_cstr(writer, " @@\n")
        && ruda_cli_write_diff_lines(
            writer,
            "-",
            original,
            original_length
        )
        && ruda_cli_write_diff_lines(
            writer,
            "+",
            formatted,
            formatted_length
        );
}

static RudaFileStatus ruda_cli_replace_formatted_file(
    RudaAllocator allocator,
    const char *path,
    const unsigned char *bytes,
    size_t length
) {
    RudaVector temporary;
    size_t base_length = strlen(path);
    size_t attempt = 0;
    RudaFileStatus status = RUDA_FILE_ALREADY_EXISTS;
    if (!ruda_vector_init(&temporary, allocator, sizeof(char))
        || !ruda_vector_append(&temporary, path, base_length)) {
        return RUDA_FILE_ALLOCATION_FAILED;
    }
    for (attempt = 0; attempt < 128; attempt += 1) {
        char suffix[32];
        int suffix_length = snprintf(
            suffix,
            sizeof(suffix),
            ".ruda-fmt-%03zu.tmp",
            attempt
        );
        temporary.length = base_length;
        if (suffix_length < 0 || (size_t)suffix_length >= sizeof(suffix)
            || !ruda_vector_append(
                &temporary,
                suffix,
                (size_t)suffix_length
            )) {
            status = RUDA_FILE_ALLOCATION_FAILED;
            break;
        }
        status = ruda_file_replace_atomic(
            allocator,
            temporary.data,
            temporary.length,
            path,
            base_length,
            bytes,
            length
        );
        if (status != RUDA_FILE_ALREADY_EXISTS) {
            break;
        }
    }
    ruda_vector_dispose(&temporary);
    return status;
}

static int ruda_cli_finish_format(
    RudaSession *session,
    const RudaCliOptions *options,
    const char *path,
    const RudaOwnedBytes *input,
    const RudaSourceSnapshot *source,
    const unsigned char *formatted,
    size_t formatted_length,
    bool changed,
    RudaCliFormatAction action,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    if (action == RUDA_CLI_FORMAT_STDOUT) {
        if (!ruda_writer_write(
                standard_output,
                (const char *)formatted,
                formatted_length
            )) {
            return ruda_cli_plain_error(
                standard_error,
                options->diagnostic_style,
                RUDA_DIAGNOSTIC_FORMAT_OUTPUT_IO,
                "cannot write canonical formatter output",
                RUDA_EXIT_SOURCE_FAILURE
            );
        }
    } else if (action == RUDA_CLI_FORMAT_CHECK && changed) {
        return ruda_cli_source_error(
            standard_error,
            options->diagnostic_style,
            source,
            RUDA_DIAGNOSTIC_FORMAT_CHECK_FAILED,
            "file does not match canonical formatting",
            0,
            ruda_source_length(source)
        ) ? RUDA_EXIT_SOURCE_FAILURE : RUDA_EXIT_INTERNAL;
    } else if (action == RUDA_CLI_FORMAT_DIFF && changed) {
        if (!ruda_cli_write_format_diff(
                standard_output,
                path,
                input->data,
                input->length,
                formatted,
                formatted_length
            )) {
            return ruda_cli_plain_error(
                standard_error,
                options->diagnostic_style,
                RUDA_DIAGNOSTIC_FORMAT_OUTPUT_IO,
                "cannot write canonical formatter diff",
                RUDA_EXIT_SOURCE_FAILURE
            );
        }
    } else if (action == RUDA_CLI_FORMAT_WRITE && changed) {
        RudaFileStatus file_status = ruda_cli_replace_formatted_file(
            session->allocator,
            path,
            formatted,
            formatted_length
        );
        if (file_status != RUDA_FILE_OK) {
            return ruda_cli_plain_error(
                standard_error,
                options->diagnostic_style,
                RUDA_DIAGNOSTIC_FORMAT_OUTPUT_IO,
                file_status == RUDA_FILE_ALLOCATION_FAILED
                    ? "cannot allocate transactional formatter output"
                    : "cannot transactionally replace formatted file",
                file_status == RUDA_FILE_ALLOCATION_FAILED
                    ? RUDA_EXIT_INTERNAL
                    : RUDA_EXIT_SOURCE_FAILURE
            );
        }
    }
    return RUDA_EXIT_SUCCESS;
}

static int ruda_cli_format_source_loaded(
    RudaSession *session,
    const RudaCliOptions *options,
    const char *path,
    const RudaOwnedBytes *input,
    RudaCliFormatAction action,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    RudaSourceManager sources;
    const RudaSourceSnapshot *source = NULL;
    RudaLexResult lexed;
    RudaParseResult parsed;
    RudaFormatResult formatted;
    RudaLexStatus lex_status;
    RudaParseStatus parse_status;
    RudaFormatStatus format_status;
    int result = RUDA_EXIT_INTERNAL;
    bool sources_initialized = false;
    memset(&sources, 0, sizeof(sources));
    memset(&lexed, 0, sizeof(lexed));
    memset(&parsed, 0, sizeof(parsed));
    memset(&formatted, 0, sizeof(formatted));
    sources_initialized = ruda_source_manager_init(&sources, session->allocator);
    if (!sources_initialized
        || ruda_source_manager_add(
            &sources,
            path,
            strlen(path),
            input->data,
            input->length,
            &source
        ) != RUDA_SOURCE_OK) {
        goto cleanup;
    }
    lex_status = ruda_lex_snapshot(
        source,
        session->allocator,
        ruda_lex_default_options(),
        &lexed
    );
    if (lex_status == RUDA_LEX_LIMIT_REACHED) {
        result = ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            RUDA_DIAGNOSTIC_FORMAT_LIMIT,
            "formatter lexer exhausted a defensive limit",
            RUDA_EXIT_SOURCE_FAILURE
        );
        goto cleanup;
    }
    if (lex_status != RUDA_LEX_OK) {
        goto cleanup;
    }
    parse_status = ruda_parse_file(
        &lexed,
        session->allocator,
        ruda_parse_default_options(),
        &parsed
    );
    if (parse_status == RUDA_PARSE_LIMIT_REACHED) {
        result = ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            RUDA_DIAGNOSTIC_FORMAT_LIMIT,
            "formatter parser exhausted a defensive limit",
            RUDA_EXIT_SOURCE_FAILURE
        );
        goto cleanup;
    }
    if (parse_status != RUDA_PARSE_OK) {
        goto cleanup;
    }
    if (lexed.diagnostics.length != 0 || parsed.diagnostics.length != 0
        || lexed.suppressed_diagnostics != 0
        || parsed.suppressed_diagnostics != 0) {
        result = ruda_cli_render_frontend_diagnostics(
            standard_error,
            options->diagnostic_style,
            &lexed,
            &parsed
        ) ? RUDA_EXIT_SOURCE_FAILURE : RUDA_EXIT_INTERNAL;
        goto cleanup;
    }
    format_status = ruda_format_source(
        &lexed,
        &parsed,
        session->allocator,
        ruda_format_default_options(),
        &formatted
    );
    if (format_status == RUDA_FORMAT_LIMIT_REACHED) {
        result = ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            RUDA_DIAGNOSTIC_FORMAT_LIMIT,
            "formatter exhausted a defensive document or output limit",
            RUDA_EXIT_SOURCE_FAILURE
        );
        goto cleanup;
    }
    if (format_status != RUDA_FORMAT_OK) {
        goto cleanup;
    }
    result = ruda_cli_finish_format(
        session,
        options,
        path,
        input,
        source,
        formatted.output.data,
        formatted.output.length,
        formatted.changed,
        action,
        standard_output,
        standard_error
    );

cleanup:
    ruda_format_result_dispose(&formatted);
    ruda_parse_result_dispose(&parsed);
    ruda_lex_result_dispose(&lexed);
    if (sources_initialized) {
        ruda_source_manager_dispose(&sources);
    }
    return result;
}

static bool ruda_cli_render_manifest_diagnostics(
    RudaWriter error_writer,
    RudaDiagnosticStyle style,
    const RudaManifestParseResult *parsed
) {
    size_t index = 0;
    for (index = 0; index < parsed->diagnostics.length; index += 1) {
        const RudaManifestDiagnostic *diagnostic = ruda_manifest_diagnostic(
            parsed,
            index
        );
        if (diagnostic == NULL
            || !ruda_cli_source_error(
                error_writer,
                style,
                parsed->snapshot,
                diagnostic->code,
                diagnostic->message,
                diagnostic->start,
                diagnostic->end
            )) {
            return false;
        }
    }
    if (parsed->suppressed_diagnostics != 0) {
        char message[160];
        int length = snprintf(
            message,
            sizeof(message),
            "%zu additional manifest diagnostics were suppressed by the configured limit",
            parsed->suppressed_diagnostics
        );
        if (length < 0 || (size_t)length >= sizeof(message)
            || !ruda_cli_source_error(
                error_writer,
                style,
                parsed->snapshot,
                RUDA_DIAGNOSTIC_FORMAT_LIMIT,
                message,
                ruda_source_length(parsed->snapshot),
                ruda_source_length(parsed->snapshot)
            )) {
            return false;
        }
    }
    return true;
}

static int ruda_cli_format_manifest_loaded(
    RudaSession *session,
    const RudaCliOptions *options,
    const char *path,
    const RudaOwnedBytes *input,
    RudaManifestKind kind,
    RudaCliFormatAction action,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    RudaSourceManager sources;
    const RudaSourceSnapshot *source = NULL;
    RudaManifestParseResult parsed;
    RudaManifestFormatResult formatted;
    RudaManifestParseStatus parse_status;
    RudaManifestFormatStatus format_status;
    int result = RUDA_EXIT_INTERNAL;
    bool sources_initialized = false;
    memset(&sources, 0, sizeof(sources));
    memset(&parsed, 0, sizeof(parsed));
    memset(&formatted, 0, sizeof(formatted));
    sources_initialized = ruda_source_manager_init(&sources, session->allocator);
    if (!sources_initialized
        || ruda_source_manager_add(
            &sources,
            path,
            strlen(path),
            input->data,
            input->length,
            &source
        ) != RUDA_SOURCE_OK) {
        goto cleanup;
    }
    parse_status = ruda_manifest_parse(
        source,
        kind,
        session->allocator,
        ruda_manifest_parse_default_options(),
        &parsed
    );
    if (parse_status == RUDA_MANIFEST_PARSE_LIMIT_REACHED) {
        result = ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            RUDA_DIAGNOSTIC_FORMAT_LIMIT,
            "manifest parser exhausted a defensive entry limit",
            RUDA_EXIT_SOURCE_FAILURE
        );
        goto cleanup;
    }
    if (parse_status != RUDA_MANIFEST_PARSE_OK) {
        goto cleanup;
    }
    if (parsed.diagnostics.length != 0
        || parsed.suppressed_diagnostics != 0) {
        result = ruda_cli_render_manifest_diagnostics(
            standard_error,
            options->diagnostic_style,
            &parsed
        ) ? RUDA_EXIT_SOURCE_FAILURE : RUDA_EXIT_INTERNAL;
        goto cleanup;
    }
    format_status = ruda_manifest_format(
        &parsed,
        session->allocator,
        ruda_manifest_format_default_options(),
        &formatted
    );
    if (format_status == RUDA_MANIFEST_FORMAT_LIMIT_REACHED) {
        result = ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            RUDA_DIAGNOSTIC_FORMAT_LIMIT,
            "manifest formatter exhausted a defensive decoded or output limit",
            RUDA_EXIT_SOURCE_FAILURE
        );
        goto cleanup;
    }
    if (format_status != RUDA_MANIFEST_FORMAT_OK) {
        goto cleanup;
    }
    result = ruda_cli_finish_format(
        session,
        options,
        path,
        input,
        source,
        formatted.output.data,
        formatted.output.length,
        formatted.changed,
        action,
        standard_output,
        standard_error
    );

cleanup:
    ruda_manifest_format_result_dispose(&formatted);
    ruda_manifest_parse_result_dispose(&parsed);
    if (sources_initialized) {
        ruda_source_manager_dispose(&sources);
    }
    return result;
}

static int ruda_cli_format_loaded(
    RudaSession *session,
    const RudaCliOptions *options,
    const char *path,
    const RudaOwnedBytes *input,
    RudaCliFormatAction action,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    RudaManifestKind kind;
    if (ruda_cli_manifest_kind(path, &kind)) {
        return ruda_cli_format_manifest_loaded(
            session,
            options,
            path,
            input,
            kind,
            action,
            standard_output,
            standard_error
        );
    }
    return ruda_cli_format_source_loaded(
        session,
        options,
        path,
        input,
        action,
        standard_output,
        standard_error
    );
}

int ruda_cli_format_stdin(
    RudaSession *session,
    const RudaCliOptions *options,
    RudaReadStream standard_input,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    enum { RUDA_FORMAT_INPUT_MAX_BYTES = 64 * 1024 * 1024 };
    RudaOwnedBytes input = {0};
    RudaReadAllStatus read_status;
    int result = RUDA_EXIT_INTERNAL;
    const char *path = options->target == NULL ? "<stdin>" : options->target;
    if (standard_input.read == NULL) {
        return ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            RUDA_DIAGNOSTIC_FORMAT_INPUT_IO,
            "cannot read formatter standard input",
            RUDA_EXIT_SOURCE_FAILURE
        );
    }
    read_status = ruda_read_all(
        standard_input,
        session->allocator,
        RUDA_FORMAT_INPUT_MAX_BYTES,
        &input
    );
    if (read_status != RUDA_READ_ALL_OK) {
        return ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            read_status == RUDA_READ_ALL_LIMIT_EXCEEDED
                ? RUDA_DIAGNOSTIC_FORMAT_LIMIT
                : RUDA_DIAGNOSTIC_FORMAT_INPUT_IO,
            read_status == RUDA_READ_ALL_LIMIT_EXCEEDED
                ? "formatter input exceeds the 64 MiB stdin limit"
                : (read_status == RUDA_READ_ALL_ALLOCATION_FAILED
                    ? "cannot allocate formatter standard input"
                    : "cannot read formatter standard input"),
            read_status == RUDA_READ_ALL_ALLOCATION_FAILED
                ? RUDA_EXIT_INTERNAL
                : RUDA_EXIT_SOURCE_FAILURE
        );
    }
    result = ruda_cli_format_loaded(
        session,
        options,
        path,
        &input,
        RUDA_CLI_FORMAT_STDOUT,
        standard_output,
        standard_error
    );
    ruda_owned_bytes_dispose(&input);
    return result;
}

int ruda_cli_format_file(
    RudaSession *session,
    const RudaCliOptions *options,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    enum { RUDA_FORMAT_INPUT_MAX_BYTES = 64 * 1024 * 1024 };
    RudaOwnedBytes input = {0};
    RudaFileStatus file_status = ruda_file_read_all(
        session->allocator,
        options->target,
        strlen(options->target),
        RUDA_FORMAT_INPUT_MAX_BYTES,
        &input
    );
    RudaCliFormatAction action = options->format_check
        ? RUDA_CLI_FORMAT_CHECK
        : (options->format_diff ? RUDA_CLI_FORMAT_DIFF : RUDA_CLI_FORMAT_WRITE);
    int result = RUDA_EXIT_INTERNAL;
    if (file_status != RUDA_FILE_OK) {
        return ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            file_status == RUDA_FILE_LIMIT_EXCEEDED
                ? RUDA_DIAGNOSTIC_FORMAT_LIMIT
                : RUDA_DIAGNOSTIC_FORMAT_INPUT_IO,
            file_status == RUDA_FILE_LIMIT_EXCEEDED
                ? "formatter input exceeds the 64 MiB file limit"
                : (file_status == RUDA_FILE_ALLOCATION_FAILED
                    ? "cannot allocate formatter file input"
                    : "cannot read formatter file input"),
            file_status == RUDA_FILE_ALLOCATION_FAILED
                ? RUDA_EXIT_INTERNAL
                : RUDA_EXIT_SOURCE_FAILURE
        );
    }
    result = ruda_cli_format_loaded(
        session,
        options,
        options->target,
        &input,
        action,
        standard_output,
        standard_error
    );
    ruda_owned_bytes_dispose(&input);
    return result;
}
