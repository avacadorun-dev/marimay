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
        static const char counted[] = {'a', '\0', 'b'};
        diagnostic.message = counted;
        diagnostic.message_length = sizeof(counted);
        diagnostic.message_is_counted = true;
        ruda_test_buffer_init(&buffer);
        RUDA_EXPECT(ruda_diagnostic_render(
            ruda_test_buffer_writer(&buffer),
            RUDA_DIAGNOSTIC_JSON,
            &diagnostic
        ));
        RUDA_EXPECT(strstr(buffer.bytes, "a\\u0000b") != NULL);
        diagnostic.message_is_counted = false;
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

    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = RUDA_DIAGNOSTIC_RUNTIME_FAILURE;
    diagnostic.severity = RUDA_SEVERITY_ERROR;
    diagnostic.message = "managed heap budget exceeded: live=8 bytes, requested=16 bytes, limit=20 bytes";
    diagnostic.runtime_resource = RUDA_RUNTIME_RESOURCE_MANAGED_HEAP_BUDGET;
    diagnostic.resource_live_bytes = 8;
    diagnostic.resource_requested_bytes = 16;
    diagnostic.resource_limit_bytes = 20;
    ruda_test_buffer_init(&buffer);
    RUDA_EXPECT(ruda_diagnostic_render(
        ruda_test_buffer_writer(&buffer), RUDA_DIAGNOSTIC_JSON, &diagnostic
    ));
    RUDA_EXPECT(strstr(
        buffer.bytes,
        "\"context\":{\"kind\":\"runtime-resource\",\"resource\":\"managed-heap\",\"classification\":\"budget-exceeded\",\"liveBytes\":8,\"requestedBytes\":16,\"limitBytes\":20}"
    ) != NULL);

    diagnostic.message = "runtime host allocation failed";
    diagnostic.runtime_resource = RUDA_RUNTIME_RESOURCE_HOST_ALLOCATION;
    ruda_test_buffer_init(&buffer);
    RUDA_EXPECT(ruda_diagnostic_render(
        ruda_test_buffer_writer(&buffer), RUDA_DIAGNOSTIC_JSON, &diagnostic
    ));
    RUDA_EXPECT(strstr(
        buffer.bytes,
        "\"context\":{\"kind\":\"runtime-resource\",\"resource\":\"host-memory\",\"classification\":\"allocation-failed\"}"
    ) != NULL);

    ruda_test_buffer_init(&buffer);
    buffer.failure_index = 0;
    RUDA_EXPECT(!ruda_diagnostic_render(
        ruda_test_buffer_writer(&buffer),
        RUDA_DIAGNOSTIC_JSON,
        &diagnostic
    ));
}
