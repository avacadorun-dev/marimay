#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/cli.h"
#include "mmay/session.h"

static int mmay_release_run(
    MarimaySession *session,
    int argument_count,
    const char *const arguments[],
    MarimayTestBuffer *output,
    MarimayTestBuffer *error
) {
    mmay_test_buffer_init(output);
    mmay_test_buffer_init(error);
    return mmay_cli_run(
        session,
        argument_count,
        arguments,
        (MarimayReadStream){0},
        mmay_test_buffer_writer(output),
        mmay_test_buffer_writer(error)
    );
}

static void mmay_release_expect_help_inventory(MarimaySession *session) {
    static const char *const available[] = {
        "build", "check", "doctor", "fmt", "help", "init", "lsp", "run", "test",
        "version"
    };
    static const char *const staged[] = {
        "add", "audit", "bench", "doc", "exec", "explain", "fix", "fuzz",
        "generate", "list", "outdated", "remove", "update", "vendor",
        "vet", "why"
    };
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    size_t index = 0;

    for (index = 0; index < sizeof(available) / sizeof(available[0]); index += 1) {
        const char *arguments[] = {"mmay", "help", available[index]};
        int result = mmay_release_run(session, 3, arguments, &output, &error);
        MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
        MMAY_EXPECT(strstr(output.bytes, "Availability: available") != NULL);
        MMAY_EXPECT(strstr(output.bytes, "Availability: staged") == NULL);
        MMAY_EXPECT(error.length == 0);
    }
    for (index = 0; index < sizeof(staged) / sizeof(staged[0]); index += 1) {
        const char *help_arguments[] = {"mmay", "help", staged[index]};
        const char *invoke_arguments[] = {"mmay", staged[index], "placeholder"};
        int result = mmay_release_run(
            session,
            3,
            help_arguments,
            &output,
            &error
        );
        MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
        MMAY_EXPECT(strstr(output.bytes, "Availability: staged; owner ") != NULL);
        MMAY_EXPECT(strstr(output.bytes, "returns U0003") != NULL);
        MMAY_EXPECT(error.length == 0);

        result = mmay_release_run(
            session,
            3,
            invoke_arguments,
            &output,
            &error
        );
        MMAY_EXPECT(result == MMAY_EXIT_USAGE);
        MMAY_EXPECT(output.length == 0);
        MMAY_EXPECT(strstr(error.bytes, "error[U0003]") != NULL);
    }
}

static void mmay_release_expect_write_failures(
    MarimaySession *session,
    int argument_count,
    const char *const arguments[]
) {
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    size_t failure_index = 0;
    bool reached_success = false;

    for (failure_index = 0; failure_index < 128; failure_index += 1) {
        int result = 0;
        mmay_test_buffer_init(&output);
        mmay_test_buffer_init(&error);
        output.failure_index = failure_index;
        result = mmay_cli_run(
            session,
            argument_count,
            arguments,
            (MarimayReadStream){0},
            mmay_test_buffer_writer(&output),
            mmay_test_buffer_writer(&error)
        );
        if (result == MMAY_EXIT_SUCCESS) {
            reached_success = true;
            MMAY_EXPECT(output.length != 0);
            MMAY_EXPECT(error.length == 0);
            break;
        }
        MMAY_EXPECT(result == MMAY_EXIT_INTERNAL);
        MMAY_EXPECT(error.length == 0);
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_cli_release_metadata(void) {
    MarimaySession session;
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    const char *top_help[] = {"mmay", "help"};
    const char *human_version[] = {"mmay", "version"};
    const char *version_flag[] = {"mmay", "--version"};
    const char *json_version[] = {"mmay", "version", "--json"};
    const char *staged_help[] = {"mmay", "help", "explain"};
    int result = 0;

    mmay_session_init(&session, mmay_system_allocator());

    result = mmay_release_run(&session, 2, top_help, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT_SNAPSHOT("help.txt", output.bytes);
    MMAY_EXPECT(error.length == 0);

    result = mmay_release_run(&session, 2, human_version, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT_SNAPSHOT("version.txt", output.bytes);
    MMAY_EXPECT(error.length == 0);

    result = mmay_release_run(&session, 2, version_flag, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT_SNAPSHOT("version.txt", output.bytes);
    MMAY_EXPECT(error.length == 0);

    result = mmay_release_run(&session, 3, json_version, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT_SNAPSHOT("version.json", output.bytes);
    MMAY_EXPECT(error.length == 0);

    mmay_release_expect_help_inventory(&session);
    mmay_release_expect_write_failures(&session, 2, top_help);
    mmay_release_expect_write_failures(&session, 2, human_version);
    mmay_release_expect_write_failures(&session, 3, json_version);
    mmay_release_expect_write_failures(&session, 3, staged_help);
}
