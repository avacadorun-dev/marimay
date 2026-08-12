#include "test_support.h"

#include <string.h>

#include "ruda/diagnostic.h"
#include "ruda/generated/diagnostic_codes.h"
#include "ruda/source/source_manager.h"

void ruda_test_diagnostic(void) {
    RudaTestBuffer buffer;
    RudaDiagnostic diagnostic;
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = RUDA_DIAGNOSTIC_UNKNOWN_COMMAND;
    diagnostic.severity = RUDA_SEVERITY_ERROR;
    diagnostic.message = "unknown command 'buid'; did you mean 'build'?";
    diagnostic.has_command_argument = true;
    diagnostic.command_argument = 1;

    ruda_test_buffer_init(&buffer);
    RUDA_EXPECT(ruda_diagnostic_render(
        ruda_test_buffer_writer(&buffer),
        RUDA_DIAGNOSTIC_HUMAN,
        &diagnostic
    ));
    RUDA_EXPECT_SNAPSHOT("unknown-command.human.txt", buffer.bytes);

    ruda_test_buffer_init(&buffer);
    RUDA_EXPECT(ruda_diagnostic_render(
        ruda_test_buffer_writer(&buffer),
        RUDA_DIAGNOSTIC_SHORT,
        &diagnostic
    ));
    RUDA_EXPECT_SNAPSHOT("unknown-command.short.txt", buffer.bytes);

    ruda_test_buffer_init(&buffer);
    RUDA_EXPECT(ruda_diagnostic_render(
        ruda_test_buffer_writer(&buffer),
        RUDA_DIAGNOSTIC_JSON,
        &diagnostic
    ));
    RUDA_EXPECT_SNAPSHOT("unknown-command.jsonl", buffer.bytes);

    diagnostic.message = "quote \" slash \\ newline\n";
    diagnostic.has_command_argument = false;
    ruda_test_buffer_init(&buffer);
    RUDA_EXPECT(ruda_diagnostic_render(
        ruda_test_buffer_writer(&buffer),
        RUDA_DIAGNOSTIC_JSON,
        &diagnostic
    ));
    RUDA_EXPECT(strstr(buffer.bytes, "quote \\\" slash \\\\ newline\\n") != NULL);
    RUDA_EXPECT(strstr(buffer.bytes, "\"primary\":null") != NULL);
    RUDA_EXPECT(strstr(buffer.bytes, "\"context\"") == NULL);

    {
        static const char invalid_utf8[] = {'b', 'a', 'd', (char)0xff, '\0'};
        diagnostic.message = invalid_utf8;
        ruda_test_buffer_init(&buffer);
        RUDA_EXPECT(ruda_diagnostic_render(
            ruda_test_buffer_writer(&buffer),
            RUDA_DIAGNOSTIC_JSON,
            &diagnostic
        ));
        RUDA_EXPECT(strstr(buffer.bytes, "bad\\ufffd") != NULL);
    }

    {
        static const char source_bytes[] = "const = 1;\r\n";
        RudaSourceManager sources;
        const RudaSourceSnapshot *source = NULL;
        RUDA_EXPECT(ruda_source_manager_init(&sources, ruda_system_allocator()));
        RUDA_EXPECT(ruda_source_manager_add(
            &sources,
            "src/broken.ruda",
            strlen("src/broken.ruda"),
            source_bytes,
            strlen(source_bytes),
            &source
        ) == RUDA_SOURCE_OK);
        diagnostic.code = RUDA_DIAGNOSTIC_EXPECTED_TOKEN;
        diagnostic.message = "expected identifier after 'const'";
        diagnostic.source = source;
        diagnostic.source_start = 6;
        diagnostic.source_end = 7;

        ruda_test_buffer_init(&buffer);
        RUDA_EXPECT(ruda_diagnostic_render(
            ruda_test_buffer_writer(&buffer),
            RUDA_DIAGNOSTIC_HUMAN,
            &diagnostic
        ));
        RUDA_EXPECT(strstr(buffer.bytes, "src/broken.ruda:1:7") != NULL);
        RUDA_EXPECT(strstr(buffer.bytes, "1 | const = 1;\n") != NULL);
        RUDA_EXPECT(strstr(buffer.bytes, "^") != NULL);

        ruda_test_buffer_init(&buffer);
        RUDA_EXPECT(ruda_diagnostic_render(
            ruda_test_buffer_writer(&buffer),
            RUDA_DIAGNOSTIC_SHORT,
            &diagnostic
        ));
        RUDA_EXPECT(strstr(
            buffer.bytes,
            "src/broken.ruda:1:7:P0001:error:"
        ) != NULL);

        ruda_test_buffer_init(&buffer);
        RUDA_EXPECT(ruda_diagnostic_render(
            ruda_test_buffer_writer(&buffer),
            RUDA_DIAGNOSTIC_JSON,
            &diagnostic
        ));
        RUDA_EXPECT(strstr(buffer.bytes, "\"file\":\"src/broken.ruda\"") != NULL);
        RUDA_EXPECT(strstr(buffer.bytes, "\"startByte\":6") != NULL);
        RUDA_EXPECT(strstr(buffer.bytes, "\"line\":1,\"column\":7") != NULL);
        ruda_source_manager_dispose(&sources);
        diagnostic.source = NULL;
    }

    ruda_test_buffer_init(&buffer);
    buffer.failure_index = 0;
    RUDA_EXPECT(!ruda_diagnostic_render(
        ruda_test_buffer_writer(&buffer),
        RUDA_DIAGNOSTIC_JSON,
        &diagnostic
    ));
}
