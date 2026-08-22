#include "test_support.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/format/formatter.h"
#include "mmay/lex/lexer.h"
#include "mmay/parse/parser.h"
#include "mmay/platform/file.h"
#include "mmay/source/source_manager.h"

static bool mmay_doc_line_equals(
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

static void mmay_test_readme_mmay_fences(void) {
    MarimayOwnedBytes document = {0};
    MarimaySourceManager sources;
    size_t cursor = 0;
    size_t content_start = 0;
    size_t fence_count = 0;
    bool inside_mmay = false;
    bool sources_initialized = false;
    MMAY_EXPECT(mmay_file_read_all(
        mmay_system_allocator(),
        "README.fixture",
        strlen("README.fixture"),
        1024 * 1024,
        &document
    ) == MMAY_FILE_OK);
    if (document.data == NULL) {
        return;
    }
    sources_initialized = mmay_source_manager_init(
        &sources,
        mmay_system_allocator()
    );
    MMAY_EXPECT(sources_initialized);
    if (!sources_initialized) {
        mmay_owned_bytes_dispose(&document);
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
        if (!inside_mmay && mmay_doc_line_equals(
                document.data,
                line_start,
                line_end,
                "```marimay"
            )) {
            inside_mmay = true;
            content_start = cursor;
        } else if (inside_mmay && mmay_doc_line_equals(
                document.data,
                line_start,
                line_end,
                "```"
            )) {
            char virtual_path[64];
            int path_length = snprintf(
                virtual_path,
                sizeof(virtual_path),
                "README.md#mmay-%zu",
                fence_count + 1
            );
            const MarimaySourceSnapshot *snapshot = NULL;
            MarimayLexResult lexed;
            MarimayParseResult parsed;
            MarimayFormatResult formatted;
            MarimayLexStatus lex_status;
            MarimayParseStatus parse_status;
            MarimayFormatStatus format_status;
            memset(&lexed, 0, sizeof(lexed));
            memset(&parsed, 0, sizeof(parsed));
            memset(&formatted, 0, sizeof(formatted));
            MMAY_EXPECT(path_length > 0
                && (size_t)path_length < sizeof(virtual_path));
            MMAY_EXPECT(path_length > 0
                && (size_t)path_length < sizeof(virtual_path)
                && mmay_source_manager_add(
                    &sources,
                    virtual_path,
                    (size_t)path_length,
                    document.data + content_start,
                    line_start - content_start,
                    &snapshot
                ) == MMAY_SOURCE_OK);
            if (snapshot != NULL) {
                lex_status = mmay_lex_snapshot(
                    snapshot,
                    mmay_system_allocator(),
                    mmay_lex_default_options(),
                    &lexed
                );
                MMAY_EXPECT(lex_status == MMAY_LEX_OK);
                if (lex_status == MMAY_LEX_OK) {
                    parse_status = mmay_parse_file(
                        &lexed,
                        mmay_system_allocator(),
                        mmay_parse_default_options(),
                        &parsed
                    );
                    MMAY_EXPECT(parse_status == MMAY_PARSE_OK);
                    MMAY_EXPECT(lexed.diagnostics.length == 0);
                    MMAY_EXPECT(parsed.diagnostics.length == 0);
                    if (parse_status == MMAY_PARSE_OK
                        && lexed.diagnostics.length == 0
                        && parsed.diagnostics.length == 0) {
                        format_status = mmay_format_source(
                            &lexed,
                            &parsed,
                            mmay_system_allocator(),
                            mmay_format_default_options(),
                            &formatted
                        );
                        MMAY_EXPECT(format_status == MMAY_FORMAT_OK);
                    }
                }
            }
            mmay_format_result_dispose(&formatted);
            mmay_parse_result_dispose(&parsed);
            mmay_lex_result_dispose(&lexed);
            fence_count += 1;
            inside_mmay = false;
        }
    }
    MMAY_EXPECT(!inside_mmay);
    MMAY_EXPECT(fence_count != 0);
    mmay_source_manager_dispose(&sources);
    mmay_owned_bytes_dispose(&document);
}

void mmay_test_doc_examples(void) {
    mmay_test_readme_mmay_fences();
}
