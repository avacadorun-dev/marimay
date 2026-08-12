#include "test_support.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/format/formatter.h"
#include "ruda/lex/lexer.h"
#include "ruda/parse/parser.h"
#include "ruda/platform/file.h"
#include "ruda/source/source_manager.h"

static bool ruda_doc_line_equals(
    const unsigned char *bytes,
    size_t start,
    size_t end,
    const char *text
) {
    size_t length = strlen(text);
    if (end > start && bytes[end - 1] == '\r') {
        end -= 1;
    }
    return end - start == length
        && memcmp(bytes + start, text, length) == 0;
}

static void ruda_test_readme_ruda_fences(void) {
    RudaOwnedBytes document = {0};
    RudaSourceManager sources;
    size_t cursor = 0;
    size_t content_start = 0;
    size_t fence_count = 0;
    bool inside_ruda = false;
    bool sources_initialized = false;
    RUDA_EXPECT(ruda_file_read_all(
        ruda_system_allocator(),
        "README.fixture",
        strlen("README.fixture"),
        1024 * 1024,
        &document
    ) == RUDA_FILE_OK);
    if (document.data == NULL) {
        return;
    }
    sources_initialized = ruda_source_manager_init(
        &sources,
        ruda_system_allocator()
    );
    RUDA_EXPECT(sources_initialized);
    if (!sources_initialized) {
        ruda_owned_bytes_dispose(&document);
        return;
    }
    while (cursor < document.length) {
        size_t line_start = cursor;
        size_t line_end = cursor;
        while (line_end < document.length
            && document.data[line_end] != '\n') {
            line_end += 1;
        }
        cursor = line_end < document.length ? line_end + 1 : line_end;
        if (!inside_ruda && ruda_doc_line_equals(
                document.data,
                line_start,
                line_end,
                "```ruda"
            )) {
            inside_ruda = true;
            content_start = cursor;
        } else if (inside_ruda && ruda_doc_line_equals(
                document.data,
                line_start,
                line_end,
                "```"
            )) {
            char virtual_path[64];
            int path_length = snprintf(
                virtual_path,
                sizeof(virtual_path),
                "README.md#ruda-%zu",
                fence_count + 1
            );
            const RudaSourceSnapshot *snapshot = NULL;
            RudaLexResult lexed;
            RudaParseResult parsed;
            RudaFormatResult formatted;
            RudaLexStatus lex_status;
            RudaParseStatus parse_status;
            RudaFormatStatus format_status;
            memset(&lexed, 0, sizeof(lexed));
            memset(&parsed, 0, sizeof(parsed));
            memset(&formatted, 0, sizeof(formatted));
            RUDA_EXPECT(path_length > 0
                && (size_t)path_length < sizeof(virtual_path));
            RUDA_EXPECT(path_length > 0
                && (size_t)path_length < sizeof(virtual_path)
                && ruda_source_manager_add(
                    &sources,
                    virtual_path,
                    (size_t)path_length,
                    document.data + content_start,
                    line_start - content_start,
                    &snapshot
                ) == RUDA_SOURCE_OK);
            if (snapshot != NULL) {
                lex_status = ruda_lex_snapshot(
                    snapshot,
                    ruda_system_allocator(),
                    ruda_lex_default_options(),
                    &lexed
                );
                RUDA_EXPECT(lex_status == RUDA_LEX_OK);
                if (lex_status == RUDA_LEX_OK) {
                    parse_status = ruda_parse_file(
                        &lexed,
                        ruda_system_allocator(),
                        ruda_parse_default_options(),
                        &parsed
                    );
                    RUDA_EXPECT(parse_status == RUDA_PARSE_OK);
                    RUDA_EXPECT(lexed.diagnostics.length == 0);
                    RUDA_EXPECT(parsed.diagnostics.length == 0);
                    if (parse_status == RUDA_PARSE_OK
                        && lexed.diagnostics.length == 0
                        && parsed.diagnostics.length == 0) {
                        format_status = ruda_format_source(
                            &lexed,
                            &parsed,
                            ruda_system_allocator(),
                            ruda_format_default_options(),
                            &formatted
                        );
                        RUDA_EXPECT(format_status == RUDA_FORMAT_OK);
                    }
                }
            }
            ruda_format_result_dispose(&formatted);
            ruda_parse_result_dispose(&parsed);
            ruda_lex_result_dispose(&lexed);
            fence_count += 1;
            inside_ruda = false;
        }
    }
    RUDA_EXPECT(!inside_ruda);
    RUDA_EXPECT(fence_count != 0);
    ruda_source_manager_dispose(&sources);
    ruda_owned_bytes_dispose(&document);
}

void ruda_test_doc_examples(void) {
    ruda_test_readme_ruda_fences();
}
