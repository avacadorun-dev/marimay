#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "test_support.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define mmay_cli_test_change_directory _chdir
#else
#include <unistd.h>
#define mmay_cli_test_change_directory chdir
#endif

#include "mmay/base/allocator.h"
#include "mmay/base/stream.h"
#include "mmay/cli.h"
#include "mmay/platform/directory.h"
#include "mmay/session.h"

typedef struct MarimayCliMemoryInput {
    const unsigned char *bytes;
    size_t length;
    size_t offset;
    bool fail;
} MarimayCliMemoryInput;

static MarimayReadStatus mmay_cli_memory_read(
    void *context,
    unsigned char *destination,
    size_t capacity,
    size_t *read_count
) {
    MarimayCliMemoryInput *input = context;
    size_t count = 0;
    if (input->fail) {
        return MMAY_READ_ERROR;
    }
    if (input->offset == input->length) {
        *read_count = 0;
        return MMAY_READ_END;
    }
    count = input->length - input->offset;
    if (count > capacity) {
        count = capacity;
    }
    memcpy(destination, input->bytes + input->offset, count);
    input->offset += count;
    *read_count = count;
    return MMAY_READ_OK;
}

static MarimayReadStream mmay_cli_memory_stream(MarimayCliMemoryInput *input) {
    MarimayReadStream stream;
    stream.context = input;
    stream.read = mmay_cli_memory_read;
    return stream;
}

static bool mmay_cli_test_write_file(
    const char *path,
    const unsigned char *bytes,
    size_t length
) {
    FILE *file = fopen(path, "wb");
    bool ok = file != NULL;
    if (ok) {
        ok = fwrite(bytes, 1, length, file) == length;
        ok = fclose(file) == 0 && ok;
    }
    return ok;
}

static bool mmay_cli_test_read_file(
    const char *path,
    unsigned char *bytes,
    size_t capacity,
    size_t *length
) {
    FILE *file = fopen(path, "rb");
    bool ok = file != NULL;
    size_t count = 0;
    if (ok) {
        count = fread(bytes, 1, capacity, file);
        ok = ferror(file) == 0 && feof(file) != 0;
        ok = fclose(file) == 0 && ok;
    }
    if (ok) {
        *length = count;
    }
    return ok;
}

static int mmay_run_cli(
    MarimaySession *session,
    int argument_count,
    const char *const arguments[],
    MarimayTestBuffer *output,
    MarimayTestBuffer *error
) {
    MarimayCliMemoryInput input = {0};
    mmay_test_buffer_init(output);
    mmay_test_buffer_init(error);
    return mmay_cli_run(
        session,
        argument_count,
        arguments,
        mmay_cli_memory_stream(&input),
        mmay_test_buffer_writer(output),
        mmay_test_buffer_writer(error)
    );
}

void mmay_test_cli(void) {
    MarimaySession session;
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    const char *help_arguments[] = {"mmay"};
    const char *version_flag_arguments[] = {"mmay", "--version"};
    const char *version_arguments[] = {"mmay", "version", "--json"};
    const char *unknown_arguments[] = {"mmay", "buid"};
    const char *unknown_json_arguments[] = {"mmay", "buid", "--diagnostic", "json"};
    const char *planned_arguments[] = {"mmay", "audit"};
    const char *task_help_arguments[] = {"mmay", "build", "--help"};
    const char *unknown_option_arguments[] = {"mmay", "--diagostic"};
    const char *quoted_unknown_arguments[] = {"mmay", "ba'd"};
    const char *format_stdin_arguments[] = {
        "mmay", "fmt", "--stdin", "stdin.my"
    };
    const char *format_module_stdin_arguments[] = {
        "mmay", "fmt", "--stdin", "nested/mmay.mod"
    };
    const char *format_workspace_stdin_arguments[] = {
        "mmay", "fmt", "--stdin", "nested\\mmay.work"
    };
    const char *format_json_arguments[] = {
        "mmay", "fmt", "--stdin", "broken.my", "--diagnostic", "json"
    };
    const char *format_invalid_options[] = {
        "mmay", "fmt", "--stdin", "--check"
    };
    const char *format_file_arguments[] = {
        "mmay", "fmt", "mmay-cli-format-test.my"
    };
    const char *format_check_arguments[] = {
        "mmay", "fmt", "mmay-cli-format-test.my", "--check"
    };
    const char *format_diff_arguments[] = {
        "mmay", "fmt", "mmay-cli-format-test.my", "--diff"
    };
    const char *format_conflicting_arguments[] = {
        "mmay", "fmt", "mmay-cli-format-test.my", "--check", "--diff"
    };
    const char *format_tree_arguments[] = {
        "mmay", "fmt", "format-tree"
    };
    const char *format_tree_check_arguments[] = {
        "mmay", "fmt", "format-tree", "--check"
    };
    const char *format_no_target_arguments[] = {
        "mmay", "fmt"
    };
    const char *format_no_target_check_arguments[] = {
        "mmay", "fmt", "--check"
    };
    int result = 0;

    mmay_session_init(&session, mmay_system_allocator());

    result = mmay_run_cli(&session, 1, help_arguments, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT_SNAPSHOT("help.txt", output.bytes);
    MMAY_EXPECT(error.length == 0);

    result = mmay_run_cli(&session, 2, version_flag_arguments, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(strstr(output.bytes, "Marimay compiler ") != NULL);
    MMAY_EXPECT(error.length == 0);

    result = mmay_run_cli(&session, 3, version_arguments, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT_SNAPSHOT("version.json", output.bytes);
    MMAY_EXPECT(error.length == 0);

    result = mmay_run_cli(&session, 2, unknown_arguments, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_USAGE);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT_SNAPSHOT("unknown-command.human.txt", error.bytes);

    result = mmay_run_cli(&session, 4, unknown_json_arguments, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_USAGE);
    MMAY_EXPECT_SNAPSHOT("unknown-command.jsonl", error.bytes);

    result = mmay_run_cli(&session, 2, planned_arguments, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_USAGE);
    MMAY_EXPECT(strstr(error.bytes, "error[U0003]") != NULL);

    result = mmay_run_cli(&session, 3, task_help_arguments, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(strstr(output.bytes, "Usage: mmay build") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "not implemented") == NULL);

    {
        const char *format_help_arguments[] = {"mmay", "fmt", "--help"};
        result = mmay_run_cli(
            &session,
            3,
            format_help_arguments,
            &output,
            &error
        );
        MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
        MMAY_EXPECT(strstr(output.bytes, "Usage: mmay fmt [target] [options]") != NULL);
        MMAY_EXPECT(strstr(output.bytes, "source and manifests") != NULL);
        MMAY_EXPECT(strstr(output.bytes, "not implemented") == NULL);
    }

    result = mmay_run_cli(&session, 2, unknown_option_arguments, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_USAGE);
    MMAY_EXPECT(strstr(error.bytes, "error[U0002]") != NULL);
    MMAY_EXPECT(strstr(error.bytes, "--diagnostic") != NULL);

    result = mmay_run_cli(&session, 2, quoted_unknown_arguments, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_USAGE);
    MMAY_EXPECT(strstr(error.bytes, "ba\\'d") != NULL);

    {
        static const char source[] = "fn main():void{return;}\r\n";
        static const char expected[] =
            "fn main(): void {\n"
            "  return;\n"
            "}\n";
        MarimayCliMemoryInput input = {
            (const unsigned char *)source,
            sizeof(source) - 1,
            0,
            false
        };
        mmay_test_buffer_init(&output);
        mmay_test_buffer_init(&error);
        result = mmay_cli_run(
            &session,
            4,
            format_stdin_arguments,
            mmay_cli_memory_stream(&input),
            mmay_test_buffer_writer(&output),
            mmay_test_buffer_writer(&error)
        );
        MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
        MMAY_EXPECT(strcmp(output.bytes, expected) == 0);
        MMAY_EXPECT(error.length == 0);
    }

    {
        static const char source[] =
            "language \"\\u0030.1\"\r\n"
            "module \"example.com/acme/orders\"\r\n";
        static const char expected[] =
            "module \"example.com/acme/orders\"\n"
            "\n"
            "language \"0.1\"\n";
        MarimayCliMemoryInput input = {
            (const unsigned char *)source,
            sizeof(source) - 1,
            0,
            false
        };
        mmay_test_buffer_init(&output);
        mmay_test_buffer_init(&error);
        result = mmay_cli_run(
            &session,
            4,
            format_module_stdin_arguments,
            mmay_cli_memory_stream(&input),
            mmay_test_buffer_writer(&output),
            mmay_test_buffer_writer(&error)
        );
        MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
        MMAY_EXPECT(strcmp(output.bytes, expected) == 0);
        MMAY_EXPECT(error.length == 0);
    }

    {
        static const char source[] =
            "use \"b/./c\"\n"
            "use \"a/x/../y\"\n";
        static const char expected[] =
            "use \"a/y\"\n"
            "use \"b/c\"\n";
        MarimayCliMemoryInput input = {
            (const unsigned char *)source,
            sizeof(source) - 1,
            0,
            false
        };
        mmay_test_buffer_init(&output);
        mmay_test_buffer_init(&error);
        result = mmay_cli_run(
            &session,
            4,
            format_workspace_stdin_arguments,
            mmay_cli_memory_stream(&input),
            mmay_test_buffer_writer(&output),
            mmay_test_buffer_writer(&error)
        );
        MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
        MMAY_EXPECT(strcmp(output.bytes, expected) == 0);
        MMAY_EXPECT(error.length == 0);
    }

    {
        static const char source[] = "module \"\\q\"\n";
        MarimayCliMemoryInput input = {
            (const unsigned char *)source,
            sizeof(source) - 1,
            0,
            false
        };
        mmay_test_buffer_init(&output);
        mmay_test_buffer_init(&error);
        result = mmay_cli_run(
            &session,
            4,
            format_module_stdin_arguments,
            mmay_cli_memory_stream(&input),
            mmay_test_buffer_writer(&output),
            mmay_test_buffer_writer(&error)
        );
        MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
        MMAY_EXPECT(output.length == 0);
        MMAY_EXPECT(strstr(error.bytes, "error[K0002]") != NULL);
        MMAY_EXPECT(strstr(error.bytes, "nested/mmay.mod:1:") != NULL);
    }

    {
        static const char source[] = "fn main():void{return;}\n";
        MarimayCliMemoryInput input = {
            (const unsigned char *)source,
            sizeof(source) - 1,
            0,
            false
        };
        mmay_test_buffer_init(&output);
        mmay_test_buffer_init(&error);
        output.failure_index = 0;
        result = mmay_cli_run(
            &session,
            4,
            format_stdin_arguments,
            mmay_cli_memory_stream(&input),
            mmay_test_buffer_writer(&output),
            mmay_test_buffer_writer(&error)
        );
        MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
        MMAY_EXPECT(strstr(error.bytes, "error[F0002]") != NULL);
    }

    {
        static const char source[] = "fn broken(: void {\n";
        MarimayCliMemoryInput input = {
            (const unsigned char *)source,
            sizeof(source) - 1,
            0,
            false
        };
        mmay_test_buffer_init(&output);
        mmay_test_buffer_init(&error);
        result = mmay_cli_run(
            &session,
            6,
            format_json_arguments,
            mmay_cli_memory_stream(&input),
            mmay_test_buffer_writer(&output),
            mmay_test_buffer_writer(&error)
        );
        MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
        MMAY_EXPECT(output.length == 0);
        MMAY_EXPECT(strstr(error.bytes, "\"primary\":{") != NULL);
        MMAY_EXPECT(strstr(error.bytes, "\"file\":\"broken.my\"") != NULL);
        MMAY_EXPECT(strstr(error.bytes, "\"code\":\"P0001\"") != NULL);
    }

    {
        MarimayCliMemoryInput input = {0};
        mmay_test_buffer_init(&output);
        mmay_test_buffer_init(&error);
        result = mmay_cli_run(
            &session,
            4,
            format_invalid_options,
            mmay_cli_memory_stream(&input),
            mmay_test_buffer_writer(&output),
            mmay_test_buffer_writer(&error)
        );
        MMAY_EXPECT(result == MMAY_EXIT_USAGE);
        MMAY_EXPECT(strstr(error.bytes, "error[U0005]") != NULL);
    }

    result = mmay_run_cli(
        &session,
        5,
        format_conflicting_arguments,
        &output,
        &error
    );
    MMAY_EXPECT(result == MMAY_EXIT_USAGE);
    MMAY_EXPECT(strstr(error.bytes, "error[U0005]") != NULL);

    {
        MarimayCliMemoryInput input = {0};
        input.fail = true;
        mmay_test_buffer_init(&output);
        mmay_test_buffer_init(&error);
        result = mmay_cli_run(
            &session,
            4,
            format_stdin_arguments,
            mmay_cli_memory_stream(&input),
            mmay_test_buffer_writer(&output),
            mmay_test_buffer_writer(&error)
        );
        MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
        MMAY_EXPECT(strstr(error.bytes, "error[F0001]") != NULL);
    }

    {
        static const char path[] = "mmay-cli-format-test.my";
        static const char collision[] =
            "mmay-cli-format-test.my.mmay-fmt-000.tmp";
        static const unsigned char source[] =
            "fn main():void{return;}\r\n";
        static const unsigned char invalid[] =
            "fn broken(:void{\n";
        static const unsigned char canonical[] =
            "fn main(): void {\n"
            "  return;\n"
            "}\n";
        static const unsigned char sentinel[] = "occupied";
        unsigned char observed[4096];
        size_t observed_length = 0;
        (void)remove(path);
        (void)remove(collision);
        MMAY_EXPECT(mmay_cli_test_write_file(
            path,
            source,
            sizeof(source) - 1
        ));
        MMAY_EXPECT(mmay_cli_test_write_file(
            collision,
            sentinel,
            sizeof(sentinel) - 1
        ));

        result = mmay_run_cli(
            &session,
            3,
            format_file_arguments,
            &output,
            &error
        );
        MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
        MMAY_EXPECT(output.length == 0);
        MMAY_EXPECT(error.length == 0);
        MMAY_EXPECT(mmay_cli_test_read_file(
            path,
            observed,
            sizeof(observed),
            &observed_length
        ));
        MMAY_EXPECT(observed_length == sizeof(canonical) - 1);
        MMAY_EXPECT(observed_length != sizeof(canonical) - 1
            || memcmp(observed, canonical, sizeof(canonical) - 1) == 0);
        MMAY_EXPECT(mmay_cli_test_read_file(
            collision,
            observed,
            sizeof(observed),
            &observed_length
        ));
        MMAY_EXPECT(observed_length == sizeof(sentinel) - 1);
        MMAY_EXPECT(observed_length != sizeof(sentinel) - 1
            || memcmp(observed, sentinel, sizeof(sentinel) - 1) == 0);

        result = mmay_run_cli(
            &session,
            4,
            format_check_arguments,
            &output,
            &error
        );
        MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
        MMAY_EXPECT(output.length == 0);
        MMAY_EXPECT(error.length == 0);

        MMAY_EXPECT(mmay_cli_test_write_file(
            path,
            source,
            sizeof(source) - 1
        ));
        result = mmay_run_cli(
            &session,
            4,
            format_check_arguments,
            &output,
            &error
        );
        MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
        MMAY_EXPECT(output.length == 0);
        MMAY_EXPECT(strstr(error.bytes, "error[F0004]") != NULL);
        MMAY_EXPECT(mmay_cli_test_read_file(
            path,
            observed,
            sizeof(observed),
            &observed_length
        ));
        MMAY_EXPECT(observed_length == sizeof(source) - 1);
        MMAY_EXPECT(observed_length != sizeof(source) - 1
            || memcmp(observed, source, sizeof(source) - 1) == 0);

        result = mmay_run_cli(
            &session,
            4,
            format_diff_arguments,
            &output,
            &error
        );
        MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
        MMAY_EXPECT(error.length == 0);
        MMAY_EXPECT(strstr(output.bytes, "--- mmay-cli-format-test.my") != NULL);
        MMAY_EXPECT(strstr(output.bytes, "-fn main():void{return;}") != NULL);
        MMAY_EXPECT(strstr(output.bytes, "+fn main(): void {") != NULL);

        MMAY_EXPECT(mmay_cli_test_write_file(
            path,
            invalid,
            sizeof(invalid) - 1
        ));
        result = mmay_run_cli(
            &session,
            3,
            format_file_arguments,
            &output,
            &error
        );
        MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
        MMAY_EXPECT(output.length == 0);
        MMAY_EXPECT(strstr(error.bytes, "error[P0001]") != NULL);
        MMAY_EXPECT(mmay_cli_test_read_file(
            path,
            observed,
            sizeof(observed),
            &observed_length
        ));
        MMAY_EXPECT(observed_length == sizeof(invalid) - 1);
        MMAY_EXPECT(observed_length != sizeof(invalid) - 1
            || memcmp(observed, invalid, sizeof(invalid) - 1) == 0);

        MMAY_EXPECT(remove(collision) == 0);
        MMAY_EXPECT(remove(path) == 0);
    }

    {
        static const unsigned char module_source[] =
            "language \"0.1\"\n"
            "module \"example/tree\"\n";
        static const unsigned char module_canonical[] =
            "module \"example/tree\"\n"
            "\n"
            "language \"0.1\"\n";
        static const unsigned char workspace_source[] =
            "use \"module/./x/../y\"\n";
        static const unsigned char workspace_canonical[] =
            "use \"module/y\"\n";
        static const unsigned char source[] =
            "fn main():void{return;}\n";
        static const unsigned char source_canonical[] =
            "fn main(): void {\n"
            "  return;\n"
            "}\n";
        static const unsigned char nested_module[] =
            "language \"0.1\"\n"
            "module \"example/nested\"\n";
        static const unsigned char nested_source[] =
            "fn nested():void{return;}\n";
        static const unsigned char ignored_source[] =
            "fn ignored():void{return;}\n";
        unsigned char observed[4096];
        size_t observed_length = 0;
        const char *unrecognized_arguments[] = {
            "mmay", "fmt", "README.fixture"
        };
        const char *legacy_extension_arguments[] = {
            "mmay", "fmt", "legacy.ruda"
        };
#if !defined(_WIN32)
        const char *symlink_arguments[] = {
            "mmay", "fmt", "format-tree/link.my"
        };
#endif

        MMAY_EXPECT(mmay_cli_test_write_file(
            "format-tree/mmay.mod",
            module_source,
            sizeof(module_source) - 1
        ));
        MMAY_EXPECT(mmay_cli_test_write_file(
            "format-tree/mmay.work",
            workspace_source,
            sizeof(workspace_source) - 1
        ));
        MMAY_EXPECT(mmay_cli_test_write_file(
            "format-tree/src/a.my",
            source,
            sizeof(source) - 1
        ));
        MMAY_EXPECT(mmay_cli_test_write_file(
            "format-tree/nested/mmay.mod",
            nested_module,
            sizeof(nested_module) - 1
        ));
        MMAY_EXPECT(mmay_cli_test_write_file(
            "format-tree/nested/skip.my",
            nested_source,
            sizeof(nested_source) - 1
        ));
        MMAY_EXPECT(mmay_cli_test_write_file(
            "format-tree/.mmay/ignored.my",
            ignored_source,
            sizeof(ignored_source) - 1
        ));
#if !defined(_WIN32)
        (void)unlink("format-tree/link.my");
        (void)unlink("format-tree/linkdir");
        MMAY_EXPECT(symlink(
            "src/a.my",
            "format-tree/link.my"
        ) == 0);
        MMAY_EXPECT(symlink(
            ".mmay",
            "format-tree/linkdir"
        ) == 0);
#endif

        result = mmay_run_cli(
            &session,
            4,
            format_tree_check_arguments,
            &output,
            &error
        );
        MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
        MMAY_EXPECT(output.length == 0);
        MMAY_EXPECT(strstr(error.bytes, "error[F0004]") != NULL);

        result = mmay_run_cli(
            &session,
            3,
            format_tree_arguments,
            &output,
            &error
        );
        MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
        MMAY_EXPECT(output.length == 0);
        MMAY_EXPECT(error.length == 0);

        MMAY_EXPECT(mmay_cli_test_read_file(
            "format-tree/mmay.mod",
            observed,
            sizeof(observed),
            &observed_length
        ));
        MMAY_EXPECT(observed_length == sizeof(module_canonical) - 1);
        MMAY_EXPECT(observed_length != sizeof(module_canonical) - 1
            || memcmp(
                observed,
                module_canonical,
                sizeof(module_canonical) - 1
            ) == 0);
        MMAY_EXPECT(mmay_cli_test_read_file(
            "format-tree/mmay.work",
            observed,
            sizeof(observed),
            &observed_length
        ));
        MMAY_EXPECT(observed_length == sizeof(workspace_canonical) - 1);
        MMAY_EXPECT(observed_length != sizeof(workspace_canonical) - 1
            || memcmp(
                observed,
                workspace_canonical,
                sizeof(workspace_canonical) - 1
            ) == 0);
        MMAY_EXPECT(mmay_cli_test_read_file(
            "format-tree/src/a.my",
            observed,
            sizeof(observed),
            &observed_length
        ));
        MMAY_EXPECT(observed_length == sizeof(source_canonical) - 1);
        MMAY_EXPECT(observed_length != sizeof(source_canonical) - 1
            || memcmp(
                observed,
                source_canonical,
                sizeof(source_canonical) - 1
            ) == 0);
        MMAY_EXPECT(mmay_cli_test_read_file(
            "format-tree/nested/mmay.mod",
            observed,
            sizeof(observed),
            &observed_length
        ));
        MMAY_EXPECT(observed_length == sizeof(nested_module) - 1);
        MMAY_EXPECT(observed_length != sizeof(nested_module) - 1
            || memcmp(
                observed,
                nested_module,
                sizeof(nested_module) - 1
            ) == 0);
        MMAY_EXPECT(mmay_cli_test_read_file(
            "format-tree/nested/skip.my",
            observed,
            sizeof(observed),
            &observed_length
        ));
        MMAY_EXPECT(observed_length == sizeof(nested_source) - 1);
        MMAY_EXPECT(observed_length != sizeof(nested_source) - 1
            || memcmp(
                observed,
                nested_source,
                sizeof(nested_source) - 1
            ) == 0);
        MMAY_EXPECT(mmay_cli_test_read_file(
            "format-tree/.mmay/ignored.my",
            observed,
            sizeof(observed),
            &observed_length
        ));
        MMAY_EXPECT(observed_length == sizeof(ignored_source) - 1);
        MMAY_EXPECT(observed_length != sizeof(ignored_source) - 1
            || memcmp(
                observed,
                ignored_source,
                sizeof(ignored_source) - 1
            ) == 0);

        result = mmay_run_cli(
            &session,
            4,
            format_tree_check_arguments,
            &output,
            &error
        );
        MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
        MMAY_EXPECT(output.length == 0);
        MMAY_EXPECT(error.length == 0);

        result = mmay_run_cli(
            &session,
            3,
            unrecognized_arguments,
            &output,
            &error
        );
        MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
        MMAY_EXPECT(strstr(error.bytes, "error[F0001]") != NULL);

        MMAY_EXPECT(mmay_cli_test_write_file(
            "legacy.ruda",
            source,
            sizeof(source) - 1
        ));
        result = mmay_run_cli(
            &session,
            3,
            legacy_extension_arguments,
            &output,
            &error
        );
        MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
        MMAY_EXPECT(strstr(error.bytes, "error[F0001]") != NULL);
        MMAY_EXPECT(remove("legacy.ruda") == 0);

#if !defined(_WIN32)
        result = mmay_run_cli(
            &session,
            3,
            symlink_arguments,
            &output,
            &error
        );
        MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
        MMAY_EXPECT(strstr(error.bytes, "error[F0001]") != NULL);
        MMAY_EXPECT(unlink("format-tree/link.my") == 0);
        MMAY_EXPECT(unlink("format-tree/linkdir") == 0);
#endif

        {
            size_t failure_index = 0;
            bool reached_success = false;
            for (failure_index = 0;
                failure_index < 1024;
                failure_index += 1) {
                MarimayFailAllocator state;
                MarimaySession failing_session;
                mmay_fail_allocator_init(
                    &state,
                    mmay_system_allocator(),
                    failure_index
                );
                mmay_session_init(
                    &failing_session,
                    mmay_fail_allocator(&state)
                );
                result = mmay_run_cli(
                    &failing_session,
                    4,
                    format_tree_check_arguments,
                    &output,
                    &error
                );
                if (result == MMAY_EXIT_SUCCESS) {
                    reached_success = true;
                } else {
                    MMAY_EXPECT(result == MMAY_EXIT_INTERNAL);
                }
                MMAY_EXPECT(state.live_allocations == 0);
                MMAY_EXPECT(state.live_bytes == 0);
                if (reached_success) {
                    break;
                }
            }
            MMAY_EXPECT(reached_success);
        }
    }

    {
        static const unsigned char source[] =
            "fn main():void{return;}\n";
        static const unsigned char source_canonical[] =
            "fn main(): void {\n"
            "  return;\n"
            "}\n";
        static const unsigned char ancestor_workspace[] =
            "use \"module/./src/..\"\n";
        static const unsigned char ancestor_workspace_canonical[] =
            "use \"module\"\n";
        static const unsigned char module_source[] =
            "language \"0.1\"\n"
            "module \"example/workspace/module\"\n";
        MarimayOwnedBytes original_directory = {0};
        unsigned char observed[4096];
        size_t observed_length = 0;

        MMAY_EXPECT(mmay_directory_current(
            mmay_system_allocator(),
            32768,
            &original_directory
        ) == MMAY_FILE_OK);
        MMAY_EXPECT(mmay_cli_test_write_file(
            "format-tree/src/a.my",
            source,
            sizeof(source) - 1
        ));
        if (original_directory.data != NULL
            && mmay_cli_test_change_directory("format-tree/src") == 0) {
            result = mmay_run_cli(
                &session,
                3,
                format_no_target_check_arguments,
                &output,
                &error
            );
            MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
            MMAY_EXPECT(strstr(error.bytes, "error[F0004]") != NULL);
            result = mmay_run_cli(
                &session,
                2,
                format_no_target_arguments,
                &output,
                &error
            );
            MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
            MMAY_EXPECT(mmay_cli_test_change_directory(
                (const char *)original_directory.data
            ) == 0);
        } else {
            MMAY_EXPECT(false);
        }
        MMAY_EXPECT(mmay_cli_test_read_file(
            "format-tree/src/a.my",
            observed,
            sizeof(observed),
            &observed_length
        ));
        MMAY_EXPECT(observed_length == sizeof(source_canonical) - 1);
        MMAY_EXPECT(observed_length != sizeof(source_canonical) - 1
            || memcmp(
                observed,
                source_canonical,
                sizeof(source_canonical) - 1
            ) == 0);

        MMAY_EXPECT(mmay_cli_test_write_file(
            "format-tree/workspace/mmay.work",
            ancestor_workspace,
            sizeof(ancestor_workspace) - 1
        ));
        MMAY_EXPECT(mmay_cli_test_write_file(
            "format-tree/workspace/module/mmay.mod",
            module_source,
            sizeof(module_source) - 1
        ));
        MMAY_EXPECT(mmay_cli_test_write_file(
            "format-tree/workspace/module/main.my",
            source,
            sizeof(source) - 1
        ));
        if (original_directory.data != NULL
            && mmay_cli_test_change_directory(
                "format-tree/workspace/module"
            ) == 0) {
            result = mmay_run_cli(
                &session,
                2,
                format_no_target_arguments,
                &output,
                &error
            );
            MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
            MMAY_EXPECT(mmay_cli_test_change_directory(
                (const char *)original_directory.data
            ) == 0);
        } else {
            MMAY_EXPECT(false);
        }
        MMAY_EXPECT(mmay_cli_test_read_file(
            "format-tree/workspace/mmay.work",
            observed,
            sizeof(observed),
            &observed_length
        ));
        MMAY_EXPECT(observed_length
            == sizeof(ancestor_workspace_canonical) - 1);
        MMAY_EXPECT(observed_length
                != sizeof(ancestor_workspace_canonical) - 1
            || memcmp(
                observed,
                ancestor_workspace_canonical,
                sizeof(ancestor_workspace_canonical) - 1
            ) == 0);
        MMAY_EXPECT(mmay_cli_test_read_file(
            "format-tree/workspace/module/main.my",
            observed,
            sizeof(observed),
            &observed_length
        ));
        MMAY_EXPECT(observed_length == sizeof(source_canonical) - 1);
        MMAY_EXPECT(observed_length != sizeof(source_canonical) - 1
            || memcmp(
                observed,
                source_canonical,
                sizeof(source_canonical) - 1
            ) == 0);
        mmay_owned_bytes_dispose(&original_directory);
    }

    {
        static const char source[] = "fn main():void{return;}\n";
        static const char expected[] =
            "fn main(): void {\n"
            "  return;\n"
            "}\n";
        size_t failure_index = 0;
        bool reached_success = false;
        for (failure_index = 0; failure_index < 128; failure_index += 1) {
            MarimayFailAllocator state;
            MarimaySession failing_session;
            MarimayCliMemoryInput input = {
                (const unsigned char *)source,
                sizeof(source) - 1,
                0,
                false
            };
            mmay_fail_allocator_init(
                &state,
                mmay_system_allocator(),
                failure_index
            );
            mmay_session_init(&failing_session, mmay_fail_allocator(&state));
            mmay_test_buffer_init(&output);
            mmay_test_buffer_init(&error);
            result = mmay_cli_run(
                &failing_session,
                4,
                format_stdin_arguments,
                mmay_cli_memory_stream(&input),
                mmay_test_buffer_writer(&output),
                mmay_test_buffer_writer(&error)
            );
            if (result == MMAY_EXIT_SUCCESS) {
                reached_success = true;
                MMAY_EXPECT(strcmp(output.bytes, expected) == 0);
            } else {
                MMAY_EXPECT(result == MMAY_EXIT_INTERNAL);
            }
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
            if (reached_success) {
                break;
            }
        }
        MMAY_EXPECT(reached_success);
    }

    {
        static const char path[] = "mmay-cli-format-fail-test.my";
        static const unsigned char source[] =
            "fn main():void{return;}\n";
        const char *arguments[] = {"mmay", "fmt", path};
        unsigned char observed[4096];
        size_t observed_length = 0;
        size_t failure_index = 0;
        bool reached_success = false;
        for (failure_index = 0; failure_index < 128; failure_index += 1) {
            MarimayFailAllocator state;
            MarimaySession failing_session;
            MMAY_EXPECT(mmay_cli_test_write_file(
                path,
                source,
                sizeof(source) - 1
            ));
            mmay_fail_allocator_init(
                &state,
                mmay_system_allocator(),
                failure_index
            );
            mmay_session_init(&failing_session, mmay_fail_allocator(&state));
            result = mmay_run_cli(
                &failing_session,
                3,
                arguments,
                &output,
                &error
            );
            if (result == MMAY_EXIT_SUCCESS) {
                reached_success = true;
            } else {
                MMAY_EXPECT(result == MMAY_EXIT_INTERNAL);
                MMAY_EXPECT(mmay_cli_test_read_file(
                    path,
                    observed,
                    sizeof(observed),
                    &observed_length
                ));
                MMAY_EXPECT(observed_length == sizeof(source) - 1);
                MMAY_EXPECT(observed_length != sizeof(source) - 1
                    || memcmp(observed, source, sizeof(source) - 1) == 0);
            }
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
            if (reached_success) {
                break;
            }
        }
        MMAY_EXPECT(reached_success);
        MMAY_EXPECT(remove(path) == 0);
    }

    mmay_test_buffer_init(&output);
    mmay_test_buffer_init(&error);
    output.failure_index = 0;
    result = mmay_cli_run(
        &session,
        1,
        help_arguments,
        (MarimayReadStream){0},
        mmay_test_buffer_writer(&output),
        mmay_test_buffer_writer(&error)
    );
    MMAY_EXPECT(result == MMAY_EXIT_INTERNAL);

    mmay_test_buffer_init(&output);
    mmay_test_buffer_init(&error);
    error.failure_index = 0;
    result = mmay_cli_run(
        &session,
        2,
        unknown_arguments,
        (MarimayReadStream){0},
        mmay_test_buffer_writer(&output),
        mmay_test_buffer_writer(&error)
    );
    MMAY_EXPECT(result == MMAY_EXIT_INTERNAL);
}
