#include "test_support.h"

#include <string.h>

#include "mmay/diagnostic.h"
#include "mmay/generated/diagnostic_codes.h"
#include "mmay/source/source_manager.h"

void mmay_test_diagnostic(void) {
    MarimayTestBuffer buffer;
    MarimayDiagnostic diagnostic;
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = MMAY_DIAGNOSTIC_UNKNOWN_COMMAND;
    diagnostic.severity = MMAY_SEVERITY_ERROR;
    diagnostic.message = "unknown command 'buid'; did you mean 'build'?";
    diagnostic.has_command_argument = true;
    diagnostic.command_argument = 1;

    mmay_test_buffer_init(&buffer);
    MMAY_EXPECT(mmay_diagnostic_render(
        mmay_test_buffer_writer(&buffer),
        MMAY_DIAGNOSTIC_HUMAN,
        &diagnostic
    ));
    MMAY_EXPECT_SNAPSHOT("unknown-command.human.txt", buffer.bytes);

    mmay_test_buffer_init(&buffer);
    MMAY_EXPECT(mmay_diagnostic_render(
        mmay_test_buffer_writer(&buffer),
        MMAY_DIAGNOSTIC_SHORT,
        &diagnostic
    ));
    MMAY_EXPECT_SNAPSHOT("unknown-command.short.txt", buffer.bytes);

    mmay_test_buffer_init(&buffer);
    MMAY_EXPECT(mmay_diagnostic_render(
        mmay_test_buffer_writer(&buffer),
        MMAY_DIAGNOSTIC_JSON,
        &diagnostic
    ));
    MMAY_EXPECT_SNAPSHOT("unknown-command.jsonl", buffer.bytes);

    diagnostic.message = "quote \" slash \\ newline\n";
    diagnostic.has_command_argument = false;
    mmay_test_buffer_init(&buffer);
    MMAY_EXPECT(mmay_diagnostic_render(
        mmay_test_buffer_writer(&buffer),
        MMAY_DIAGNOSTIC_JSON,
        &diagnostic
    ));
    MMAY_EXPECT(strstr(buffer.bytes, "quote \\\" slash \\\\ newline\\n") != NULL);
    MMAY_EXPECT(strstr(buffer.bytes, "\"primary\":null") != NULL);
    MMAY_EXPECT(strstr(buffer.bytes, "\"context\"") == NULL);

    {
        static const char invalid_utf8[] = {'b', 'a', 'd', (char)0xff, '\0'};
        diagnostic.message = invalid_utf8;
        mmay_test_buffer_init(&buffer);
        MMAY_EXPECT(mmay_diagnostic_render(
            mmay_test_buffer_writer(&buffer),
            MMAY_DIAGNOSTIC_JSON,
            &diagnostic
        ));
        MMAY_EXPECT(strstr(buffer.bytes, "bad\\ufffd") != NULL);
    }

    {
        static const char counted[] = {'a', '\0', 'b'};
        diagnostic.message = counted;
        diagnostic.message_length = sizeof(counted);
        diagnostic.message_is_counted = true;
        mmay_test_buffer_init(&buffer);
        MMAY_EXPECT(mmay_diagnostic_render(
            mmay_test_buffer_writer(&buffer),
            MMAY_DIAGNOSTIC_JSON,
            &diagnostic
        ));
        MMAY_EXPECT(strstr(buffer.bytes, "a\\u0000b") != NULL);
        diagnostic.message_is_counted = false;
    }

    {
        static const char source_bytes[] = "const = 1;\r\n";
        MarimaySourceManager sources;
        const MarimaySourceSnapshot *source = NULL;
        MMAY_EXPECT(mmay_source_manager_init(&sources, mmay_system_allocator()));
        MMAY_EXPECT(mmay_source_manager_add(
            &sources,
            "src/broken.my",
            strlen("src/broken.my"),
            source_bytes,
            strlen(source_bytes),
            &source
        ) == MMAY_SOURCE_OK);
        diagnostic.code = MMAY_DIAGNOSTIC_EXPECTED_TOKEN;
        diagnostic.message = "expected identifier after 'const'";
        diagnostic.source = source;
        diagnostic.source_start = 6;
        diagnostic.source_end = 7;

        mmay_test_buffer_init(&buffer);
        MMAY_EXPECT(mmay_diagnostic_render(
            mmay_test_buffer_writer(&buffer),
            MMAY_DIAGNOSTIC_HUMAN,
            &diagnostic
        ));
        MMAY_EXPECT(strstr(buffer.bytes, "src/broken.my:1:7") != NULL);
        MMAY_EXPECT(strstr(buffer.bytes, "1 | const = 1;\n") != NULL);
        MMAY_EXPECT(strstr(buffer.bytes, "^") != NULL);

        mmay_test_buffer_init(&buffer);
        MMAY_EXPECT(mmay_diagnostic_render(
            mmay_test_buffer_writer(&buffer),
            MMAY_DIAGNOSTIC_SHORT,
            &diagnostic
        ));
        MMAY_EXPECT(strstr(
            buffer.bytes,
            "src/broken.my:1:7:P0001:error:"
        ) != NULL);

        mmay_test_buffer_init(&buffer);
        MMAY_EXPECT(mmay_diagnostic_render(
            mmay_test_buffer_writer(&buffer),
            MMAY_DIAGNOSTIC_JSON,
            &diagnostic
        ));
        MMAY_EXPECT(strstr(buffer.bytes, "\"file\":\"src/broken.my\"") != NULL);
        MMAY_EXPECT(strstr(buffer.bytes, "\"startByte\":6") != NULL);
        MMAY_EXPECT(strstr(buffer.bytes, "\"line\":1,\"column\":7") != NULL);
        mmay_source_manager_dispose(&sources);
        diagnostic.source = NULL;
    }

    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = MMAY_DIAGNOSTIC_RUNTIME_FAILURE;
    diagnostic.severity = MMAY_SEVERITY_ERROR;
    diagnostic.message = "managed heap budget exceeded: live=8 bytes, requested=16 bytes, limit=20 bytes";
    diagnostic.runtime_resource = MMAY_RUNTIME_RESOURCE_MANAGED_HEAP_BUDGET;
    diagnostic.resource_live_bytes = 8;
    diagnostic.resource_requested_bytes = 16;
    diagnostic.resource_limit_bytes = 20;
    mmay_test_buffer_init(&buffer);
    MMAY_EXPECT(mmay_diagnostic_render(
        mmay_test_buffer_writer(&buffer), MMAY_DIAGNOSTIC_JSON, &diagnostic
    ));
    MMAY_EXPECT(strstr(
        buffer.bytes,
        "\"context\":{\"kind\":\"runtime-resource\",\"resource\":\"managed-heap\",\"classification\":\"budget-exceeded\",\"liveBytes\":8,\"requestedBytes\":16,\"limitBytes\":20}"
    ) != NULL);

    diagnostic.message = "runtime host allocation failed";
    diagnostic.runtime_resource = MMAY_RUNTIME_RESOURCE_HOST_ALLOCATION;
    mmay_test_buffer_init(&buffer);
    MMAY_EXPECT(mmay_diagnostic_render(
        mmay_test_buffer_writer(&buffer), MMAY_DIAGNOSTIC_JSON, &diagnostic
    ));
    MMAY_EXPECT(strstr(
        buffer.bytes,
        "\"context\":{\"kind\":\"runtime-resource\",\"resource\":\"host-memory\",\"classification\":\"allocation-failed\"}"
    ) != NULL);

    mmay_test_buffer_init(&buffer);
    buffer.failure_index = 0;
    MMAY_EXPECT(!mmay_diagnostic_render(
        mmay_test_buffer_writer(&buffer),
        MMAY_DIAGNOSTIC_JSON,
        &diagnostic
    ));
}
