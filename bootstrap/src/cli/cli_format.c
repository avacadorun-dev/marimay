#include "cli_internal.h"

#include <stdio.h>
#include <string.h>

#include "mmay/format/formatter.h"
#include "mmay/generated/diagnostic_codes.h"
#include "mmay/lex/lexer.h"
#include "mmay/parse/parser.h"
#include "mmay/source/source_manager.h"
static bool mmay_cli_source_error(
    MarimayWriter error_writer,
    MarimayDiagnosticStyle style,
    const MarimaySourceSnapshot *source,
    const char *code,
    const char *message,
    size_t start,
    size_t end
) {
    MarimayDiagnostic diagnostic;
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = code;
    diagnostic.severity = MMAY_SEVERITY_ERROR;
    diagnostic.message = message;
    diagnostic.source = source;
    diagnostic.source_start = start;
    diagnostic.source_end = end;
    return mmay_diagnostic_render(error_writer, style, &diagnostic);
}

static bool mmay_cli_render_frontend_diagnostics(
    MarimayWriter error_writer,
    MarimayDiagnosticStyle style,
    const MarimayLexResult *lexed,
    const MarimayParseResult *parsed
) {
    size_t lex_index = 0;
    size_t parse_index = 0;
    while (lex_index < lexed->diagnostics.length
        || parse_index < parsed->diagnostics.length) {
        const MarimayLexDiagnostic *lex_diagnostic = lex_index
                < lexed->diagnostics.length
            ? mmay_lex_diagnostic(lexed, lex_index)
            : NULL;
        const MarimayParseDiagnostic *parse_diagnostic = parse_index
                < parsed->diagnostics.length
            ? mmay_parse_diagnostic(parsed, parse_index)
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
            if (!mmay_cli_source_error(
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
            if (!mmay_cli_source_error(
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
        if (!mmay_size_add(
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
            || !mmay_cli_source_error(
                error_writer,
                style,
                lexed->snapshot,
                MMAY_DIAGNOSTIC_FORMAT_LIMIT,
                message,
                mmay_source_length(lexed->snapshot),
                mmay_source_length(lexed->snapshot)
            )) {
            return false;
        }
    }
    return true;
}
bool mmay_cli_manifest_kind(
    const char *path,
    MarimayManifestKind *kind
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
    if (strcmp(base, "mmay.mod") == 0) {
        *kind = MMAY_MANIFEST_MODULE;
        return true;
    }
    if (strcmp(base, "mmay.work") == 0) {
        *kind = MMAY_MANIFEST_WORKSPACE;
        return true;
    }
    return false;
}

static const char *mmay_cli_path_base(const char *path) {
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

bool mmay_cli_source_path(const char *path) {
    const char *base = mmay_cli_path_base(path);
    size_t length = strlen(base);
    return length >= 3 && memcmp(base + length - 3, ".my", 3) == 0;
}

bool mmay_cli_format_file_path(const char *path) {
    MarimayManifestKind kind;
    return mmay_cli_source_path(path) || mmay_cli_manifest_kind(path, &kind);
}

static bool mmay_cli_write_size(MarimayWriter writer, size_t value) {
    char buffer[32];
    int length = snprintf(buffer, sizeof(buffer), "%zu", value);
    return length >= 0 && (size_t)length < sizeof(buffer)
        && mmay_writer_write(writer, buffer, (size_t)length);
}

static size_t mmay_cli_diff_line_count(
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

static bool mmay_cli_write_diff_lines(
    MarimayWriter writer,
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
        if (!mmay_writer_write_cstr(writer, prefix)
            || !mmay_writer_write(
                writer,
                (const char *)bytes + start,
                cursor - start
            )
            || !mmay_writer_write_cstr(writer, "\n")) {
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
            && !mmay_writer_write_cstr(
                writer,
                "\\ No newline at end of file\n"
            )) {
            return false;
        }
    }
    return true;
}

static bool mmay_cli_write_format_diff(
    MarimayWriter writer,
    const char *path,
    const unsigned char *original,
    size_t original_length,
    const unsigned char *formatted,
    size_t formatted_length
) {
    char display[128];
    size_t original_lines = mmay_cli_diff_line_count(original, original_length);
    size_t formatted_lines = mmay_cli_diff_line_count(formatted, formatted_length);
    mmay_display_argument(path, display, sizeof(display));
    return mmay_writer_write_cstr(writer, "--- ")
        && mmay_writer_write_cstr(writer, display)
        && mmay_writer_write_cstr(writer, "\n+++ ")
        && mmay_writer_write_cstr(writer, display)
        && mmay_writer_write_cstr(writer, "\n@@ -")
        && mmay_cli_write_size(writer, original_lines == 0 ? 0 : 1)
        && mmay_writer_write_cstr(writer, ",")
        && mmay_cli_write_size(writer, original_lines)
        && mmay_writer_write_cstr(writer, " +")
        && mmay_cli_write_size(writer, formatted_lines == 0 ? 0 : 1)
        && mmay_writer_write_cstr(writer, ",")
        && mmay_cli_write_size(writer, formatted_lines)
        && mmay_writer_write_cstr(writer, " @@\n")
        && mmay_cli_write_diff_lines(
            writer,
            "-",
            original,
            original_length
        )
        && mmay_cli_write_diff_lines(
            writer,
            "+",
            formatted,
            formatted_length
        );
}

static MarimayFileStatus mmay_cli_replace_formatted_file(
    MarimayAllocator allocator,
    const char *path,
    const unsigned char *bytes,
    size_t length
) {
    MarimayVector temporary;
    size_t base_length = strlen(path);
    size_t attempt = 0;
    MarimayFileStatus status = MMAY_FILE_ALREADY_EXISTS;
    if (!mmay_vector_init(&temporary, allocator, sizeof(char))
        || !mmay_vector_append(&temporary, path, base_length)) {
        return MMAY_FILE_ALLOCATION_FAILED;
    }
    for (attempt = 0; attempt < 128; attempt += 1) {
        char suffix[32];
        int suffix_length = snprintf(
            suffix,
            sizeof(suffix),
            ".mmay-fmt-%03zu.tmp",
            attempt
        );
        temporary.length = base_length;
        if (suffix_length < 0 || (size_t)suffix_length >= sizeof(suffix)
            || !mmay_vector_append(
                &temporary,
                suffix,
                (size_t)suffix_length
            )) {
            status = MMAY_FILE_ALLOCATION_FAILED;
            break;
        }
        status = mmay_file_replace_atomic(
            allocator,
            temporary.data,
            temporary.length,
            path,
            base_length,
            bytes,
            length
        );
        if (status != MMAY_FILE_ALREADY_EXISTS) {
            break;
        }
    }
    mmay_vector_dispose(&temporary);
    return status;
}

static int mmay_cli_finish_format(
    MarimaySession *session,
    const MarimayCliOptions *options,
    const char *path,
    const MarimayOwnedBytes *input,
    const MarimaySourceSnapshot *source,
    const unsigned char *formatted,
    size_t formatted_length,
    bool changed,
    MarimayCliFormatAction action,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    if (action == MMAY_CLI_FORMAT_STDOUT) {
        if (!mmay_writer_write(
                standard_output,
                (const char *)formatted,
                formatted_length
            )) {
            return mmay_cli_plain_error(
                standard_error,
                options->diagnostic_style,
                MMAY_DIAGNOSTIC_FORMAT_OUTPUT_IO,
                "cannot write canonical formatter output",
                MMAY_EXIT_SOURCE_FAILURE
            );
        }
    } else if (action == MMAY_CLI_FORMAT_CHECK && changed) {
        return mmay_cli_source_error(
            standard_error,
            options->diagnostic_style,
            source,
            MMAY_DIAGNOSTIC_FORMAT_CHECK_FAILED,
            "file does not match canonical formatting",
            0,
            mmay_source_length(source)
        ) ? MMAY_EXIT_SOURCE_FAILURE : MMAY_EXIT_INTERNAL;
    } else if (action == MMAY_CLI_FORMAT_DIFF && changed) {
        if (!mmay_cli_write_format_diff(
                standard_output,
                path,
                input->data,
                input->length,
                formatted,
                formatted_length
            )) {
            return mmay_cli_plain_error(
                standard_error,
                options->diagnostic_style,
                MMAY_DIAGNOSTIC_FORMAT_OUTPUT_IO,
                "cannot write canonical formatter diff",
                MMAY_EXIT_SOURCE_FAILURE
            );
        }
    } else if (action == MMAY_CLI_FORMAT_WRITE && changed) {
        MarimayFileStatus file_status = mmay_cli_replace_formatted_file(
            session->allocator,
            path,
            formatted,
            formatted_length
        );
        if (file_status != MMAY_FILE_OK) {
            return mmay_cli_plain_error(
                standard_error,
                options->diagnostic_style,
                MMAY_DIAGNOSTIC_FORMAT_OUTPUT_IO,
                file_status == MMAY_FILE_ALLOCATION_FAILED
                    ? "cannot allocate transactional formatter output"
                    : "cannot transactionally replace formatted file",
                file_status == MMAY_FILE_ALLOCATION_FAILED
                    ? MMAY_EXIT_INTERNAL
                    : MMAY_EXIT_SOURCE_FAILURE
            );
        }
    }
    return MMAY_EXIT_SUCCESS;
}

static int mmay_cli_format_source_loaded(
    MarimaySession *session,
    const MarimayCliOptions *options,
    const char *path,
    const MarimayOwnedBytes *input,
    MarimayCliFormatAction action,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    MarimaySourceManager sources;
    const MarimaySourceSnapshot *source = NULL;
    MarimayLexResult lexed;
    MarimayParseResult parsed;
    MarimayFormatResult formatted;
    MarimayLexStatus lex_status;
    MarimayParseStatus parse_status;
    MarimayFormatStatus format_status;
    int result = MMAY_EXIT_INTERNAL;
    bool sources_initialized = false;
    memset(&sources, 0, sizeof(sources));
    memset(&lexed, 0, sizeof(lexed));
    memset(&parsed, 0, sizeof(parsed));
    memset(&formatted, 0, sizeof(formatted));
    sources_initialized = mmay_source_manager_init(&sources, session->allocator);
    if (!sources_initialized
        || mmay_source_manager_add(
            &sources,
            path,
            strlen(path),
            input->data,
            input->length,
            &source
        ) != MMAY_SOURCE_OK) {
        goto cleanup;
    }
    lex_status = mmay_lex_snapshot(
        source,
        session->allocator,
        mmay_lex_default_options(),
        &lexed
    );
    if (lex_status == MMAY_LEX_LIMIT_REACHED) {
        result = mmay_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            MMAY_DIAGNOSTIC_FORMAT_LIMIT,
            "formatter lexer exhausted a defensive limit",
            MMAY_EXIT_SOURCE_FAILURE
        );
        goto cleanup;
    }
    if (lex_status != MMAY_LEX_OK) {
        goto cleanup;
    }
    parse_status = mmay_parse_file(
        &lexed,
        session->allocator,
        mmay_parse_default_options(),
        &parsed
    );
    if (parse_status == MMAY_PARSE_LIMIT_REACHED) {
        result = mmay_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            MMAY_DIAGNOSTIC_FORMAT_LIMIT,
            "formatter parser exhausted a defensive limit",
            MMAY_EXIT_SOURCE_FAILURE
        );
        goto cleanup;
    }
    if (parse_status != MMAY_PARSE_OK) {
        goto cleanup;
    }
    if (lexed.diagnostics.length != 0 || parsed.diagnostics.length != 0
        || lexed.suppressed_diagnostics != 0
        || parsed.suppressed_diagnostics != 0) {
        result = mmay_cli_render_frontend_diagnostics(
            standard_error,
            options->diagnostic_style,
            &lexed,
            &parsed
        ) ? MMAY_EXIT_SOURCE_FAILURE : MMAY_EXIT_INTERNAL;
        goto cleanup;
    }
    format_status = mmay_format_source(
        &lexed,
        &parsed,
        session->allocator,
        mmay_format_default_options(),
        &formatted
    );
    if (format_status == MMAY_FORMAT_LIMIT_REACHED) {
        result = mmay_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            MMAY_DIAGNOSTIC_FORMAT_LIMIT,
            "formatter exhausted a defensive document or output limit",
            MMAY_EXIT_SOURCE_FAILURE
        );
        goto cleanup;
    }
    if (format_status != MMAY_FORMAT_OK) {
        goto cleanup;
    }
    result = mmay_cli_finish_format(
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
    mmay_format_result_dispose(&formatted);
    mmay_parse_result_dispose(&parsed);
    mmay_lex_result_dispose(&lexed);
    if (sources_initialized) {
        mmay_source_manager_dispose(&sources);
    }
    return result;
}

static bool mmay_cli_render_manifest_diagnostics(
    MarimayWriter error_writer,
    MarimayDiagnosticStyle style,
    const MarimayManifestParseResult *parsed
) {
    size_t index = 0;
    for (index = 0; index < parsed->diagnostics.length; index += 1) {
        const MarimayManifestDiagnostic *diagnostic = mmay_manifest_diagnostic(
            parsed,
            index
        );
        if (diagnostic == NULL
            || !mmay_cli_source_error(
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
            || !mmay_cli_source_error(
                error_writer,
                style,
                parsed->snapshot,
                MMAY_DIAGNOSTIC_FORMAT_LIMIT,
                message,
                mmay_source_length(parsed->snapshot),
                mmay_source_length(parsed->snapshot)
            )) {
            return false;
        }
    }
    return true;
}

static int mmay_cli_format_manifest_loaded(
    MarimaySession *session,
    const MarimayCliOptions *options,
    const char *path,
    const MarimayOwnedBytes *input,
    MarimayManifestKind kind,
    MarimayCliFormatAction action,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    MarimaySourceManager sources;
    const MarimaySourceSnapshot *source = NULL;
    MarimayManifestParseResult parsed;
    MarimayManifestFormatResult formatted;
    MarimayManifestParseStatus parse_status;
    MarimayManifestFormatStatus format_status;
    int result = MMAY_EXIT_INTERNAL;
    bool sources_initialized = false;
    memset(&sources, 0, sizeof(sources));
    memset(&parsed, 0, sizeof(parsed));
    memset(&formatted, 0, sizeof(formatted));
    sources_initialized = mmay_source_manager_init(&sources, session->allocator);
    if (!sources_initialized
        || mmay_source_manager_add(
            &sources,
            path,
            strlen(path),
            input->data,
            input->length,
            &source
        ) != MMAY_SOURCE_OK) {
        goto cleanup;
    }
    parse_status = mmay_manifest_parse(
        source,
        kind,
        session->allocator,
        mmay_manifest_parse_default_options(),
        &parsed
    );
    if (parse_status == MMAY_MANIFEST_PARSE_LIMIT_REACHED) {
        result = mmay_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            MMAY_DIAGNOSTIC_FORMAT_LIMIT,
            "manifest parser exhausted a defensive entry limit",
            MMAY_EXIT_SOURCE_FAILURE
        );
        goto cleanup;
    }
    if (parse_status != MMAY_MANIFEST_PARSE_OK) {
        goto cleanup;
    }
    if (parsed.diagnostics.length != 0
        || parsed.suppressed_diagnostics != 0) {
        result = mmay_cli_render_manifest_diagnostics(
            standard_error,
            options->diagnostic_style,
            &parsed
        ) ? MMAY_EXIT_SOURCE_FAILURE : MMAY_EXIT_INTERNAL;
        goto cleanup;
    }
    format_status = mmay_manifest_format(
        &parsed,
        session->allocator,
        mmay_manifest_format_default_options(),
        &formatted
    );
    if (format_status == MMAY_MANIFEST_FORMAT_LIMIT_REACHED) {
        result = mmay_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            MMAY_DIAGNOSTIC_FORMAT_LIMIT,
            "manifest formatter exhausted a defensive decoded or output limit",
            MMAY_EXIT_SOURCE_FAILURE
        );
        goto cleanup;
    }
    if (format_status != MMAY_MANIFEST_FORMAT_OK) {
        goto cleanup;
    }
    result = mmay_cli_finish_format(
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
    mmay_manifest_format_result_dispose(&formatted);
    mmay_manifest_parse_result_dispose(&parsed);
    if (sources_initialized) {
        mmay_source_manager_dispose(&sources);
    }
    return result;
}

static int mmay_cli_format_loaded(
    MarimaySession *session,
    const MarimayCliOptions *options,
    const char *path,
    const MarimayOwnedBytes *input,
    MarimayCliFormatAction action,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    MarimayManifestKind kind;
    if (mmay_cli_manifest_kind(path, &kind)) {
        return mmay_cli_format_manifest_loaded(
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
    return mmay_cli_format_source_loaded(
        session,
        options,
        path,
        input,
        action,
        standard_output,
        standard_error
    );
}

int mmay_cli_format_stdin(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayReadStream standard_input,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    enum { MMAY_FORMAT_INPUT_MAX_BYTES = 64 * 1024 * 1024 };
    MarimayOwnedBytes input = {0};
    MarimayReadAllStatus read_status;
    int result = MMAY_EXIT_INTERNAL;
    const char *path = options->target == NULL ? "<stdin>" : options->target;
    if (standard_input.read == NULL) {
        return mmay_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            MMAY_DIAGNOSTIC_FORMAT_INPUT_IO,
            "cannot read formatter standard input",
            MMAY_EXIT_SOURCE_FAILURE
        );
    }
    read_status = mmay_read_all(
        standard_input,
        session->allocator,
        MMAY_FORMAT_INPUT_MAX_BYTES,
        &input
    );
    if (read_status != MMAY_READ_ALL_OK) {
        return mmay_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            read_status == MMAY_READ_ALL_LIMIT_EXCEEDED
                ? MMAY_DIAGNOSTIC_FORMAT_LIMIT
                : MMAY_DIAGNOSTIC_FORMAT_INPUT_IO,
            read_status == MMAY_READ_ALL_LIMIT_EXCEEDED
                ? "formatter input exceeds the 64 MiB stdin limit"
                : (read_status == MMAY_READ_ALL_ALLOCATION_FAILED
                    ? "cannot allocate formatter standard input"
                    : "cannot read formatter standard input"),
            read_status == MMAY_READ_ALL_ALLOCATION_FAILED
                ? MMAY_EXIT_INTERNAL
                : MMAY_EXIT_SOURCE_FAILURE
        );
    }
    result = mmay_cli_format_loaded(
        session,
        options,
        path,
        &input,
        MMAY_CLI_FORMAT_STDOUT,
        standard_output,
        standard_error
    );
    mmay_owned_bytes_dispose(&input);
    return result;
}

int mmay_cli_format_file(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    enum { MMAY_FORMAT_INPUT_MAX_BYTES = 64 * 1024 * 1024 };
    MarimayOwnedBytes input = {0};
    MarimayFileStatus file_status = mmay_file_read_all(
        session->allocator,
        options->target,
        strlen(options->target),
        MMAY_FORMAT_INPUT_MAX_BYTES,
        &input
    );
    MarimayCliFormatAction action = options->format_check
        ? MMAY_CLI_FORMAT_CHECK
        : (options->format_diff ? MMAY_CLI_FORMAT_DIFF : MMAY_CLI_FORMAT_WRITE);
    int result = MMAY_EXIT_INTERNAL;
    if (file_status != MMAY_FILE_OK) {
        return mmay_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            file_status == MMAY_FILE_LIMIT_EXCEEDED
                ? MMAY_DIAGNOSTIC_FORMAT_LIMIT
                : MMAY_DIAGNOSTIC_FORMAT_INPUT_IO,
            file_status == MMAY_FILE_LIMIT_EXCEEDED
                ? "formatter input exceeds the 64 MiB file limit"
                : (file_status == MMAY_FILE_ALLOCATION_FAILED
                    ? "cannot allocate formatter file input"
                    : "cannot read formatter file input"),
            file_status == MMAY_FILE_ALLOCATION_FAILED
                ? MMAY_EXIT_INTERNAL
                : MMAY_EXIT_SOURCE_FAILURE
        );
    }
    result = mmay_cli_format_loaded(
        session,
        options,
        options->target,
        &input,
        action,
        standard_output,
        standard_error
    );
    mmay_owned_bytes_dispose(&input);
    return result;
}
