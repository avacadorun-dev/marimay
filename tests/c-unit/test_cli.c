#include "test_support.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define ruda_cli_test_change_directory _chdir
#else
#include <unistd.h>
#define ruda_cli_test_change_directory chdir
#endif

#include "ruda/base/allocator.h"
#include "ruda/base/stream.h"
#include "ruda/cli.h"
#include "ruda/platform/directory.h"
#include "ruda/session.h"

typedef struct RudaCliMemoryInput {
    const unsigned char *bytes;
    size_t length;
    size_t offset;
    bool fail;
} RudaCliMemoryInput;

static RudaReadStatus ruda_cli_memory_read(
    void *context,
    unsigned char *destination,
    size_t capacity,
    size_t *read_count
) {
    RudaCliMemoryInput *input = context;
    size_t count = 0;
    if (input->fail) {
        return RUDA_READ_ERROR;
    }
    if (input->offset == input->length) {
        *read_count = 0;
        return RUDA_READ_END;
    }
    count = input->length - input->offset;
    if (count > capacity) {
        count = capacity;
    }
    memcpy(destination, input->bytes + input->offset, count);
    input->offset += count;
    *read_count = count;
    return RUDA_READ_OK;
}

static RudaReadStream ruda_cli_memory_stream(RudaCliMemoryInput *input) {
    RudaReadStream stream;
    stream.context = input;
    stream.read = ruda_cli_memory_read;
    return stream;
}

static bool ruda_cli_test_write_file(
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

static bool ruda_cli_test_read_file(
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

static int ruda_run_cli(
    RudaSession *session,
    int argument_count,
    const char *const arguments[],
    RudaTestBuffer *output,
    RudaTestBuffer *error
) {
    RudaCliMemoryInput input = {0};
    ruda_test_buffer_init(output);
    ruda_test_buffer_init(error);
    return ruda_cli_run(
        session,
        argument_count,
        arguments,
        ruda_cli_memory_stream(&input),
        ruda_test_buffer_writer(output),
        ruda_test_buffer_writer(error)
    );
}

void ruda_test_cli(void) {
    RudaSession session;
    RudaTestBuffer output;
    RudaTestBuffer error;
    const char *help_arguments[] = {"ruda"};
    const char *version_flag_arguments[] = {"ruda", "--version"};
    const char *version_arguments[] = {"ruda", "version", "--json"};
    const char *unknown_arguments[] = {"ruda", "buid"};
    const char *unknown_json_arguments[] = {"ruda", "buid", "--diagnostic", "json"};
    const char *planned_arguments[] = {"ruda", "build"};
    const char *task_help_arguments[] = {"ruda", "build", "--help"};
    const char *unknown_option_arguments[] = {"ruda", "--diagostic"};
    const char *quoted_unknown_arguments[] = {"ruda", "ba'd"};
    const char *format_stdin_arguments[] = {
        "ruda", "fmt", "--stdin", "stdin.ruda"
    };
    const char *format_module_stdin_arguments[] = {
        "ruda", "fmt", "--stdin", "nested/ruda.mod"
    };
    const char *format_workspace_stdin_arguments[] = {
        "ruda", "fmt", "--stdin", "nested\\ruda.work"
    };
    const char *format_json_arguments[] = {
        "ruda", "fmt", "--stdin", "broken.ruda", "--diagnostic", "json"
    };
    const char *format_invalid_options[] = {
        "ruda", "fmt", "--stdin", "--check"
    };
    const char *format_file_arguments[] = {
        "ruda", "fmt", "ruda-cli-format-test.ruda"
    };
    const char *format_check_arguments[] = {
        "ruda", "fmt", "ruda-cli-format-test.ruda", "--check"
    };
    const char *format_diff_arguments[] = {
        "ruda", "fmt", "ruda-cli-format-test.ruda", "--diff"
    };
    const char *format_conflicting_arguments[] = {
        "ruda", "fmt", "ruda-cli-format-test.ruda", "--check", "--diff"
    };
    const char *format_tree_arguments[] = {
        "ruda", "fmt", "format-tree"
    };
    const char *format_tree_check_arguments[] = {
        "ruda", "fmt", "format-tree", "--check"
    };
    const char *format_no_target_arguments[] = {
        "ruda", "fmt"
    };
    const char *format_no_target_check_arguments[] = {
        "ruda", "fmt", "--check"
    };
    int result = 0;

    ruda_session_init(&session, ruda_system_allocator());

    result = ruda_run_cli(&session, 1, help_arguments, &output, &error);
    RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT_SNAPSHOT("help.txt", output.bytes);
    RUDA_EXPECT(error.length == 0);

    result = ruda_run_cli(&session, 2, version_flag_arguments, &output, &error);
    RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(strstr(output.bytes, "Ruda compiler ") != NULL);
    RUDA_EXPECT(error.length == 0);

    result = ruda_run_cli(&session, 3, version_arguments, &output, &error);
    RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT_SNAPSHOT("version.json", output.bytes);
    RUDA_EXPECT(error.length == 0);

    result = ruda_run_cli(&session, 2, unknown_arguments, &output, &error);
    RUDA_EXPECT(result == RUDA_EXIT_USAGE);
    RUDA_EXPECT(output.length == 0);
    RUDA_EXPECT_SNAPSHOT("unknown-command.human.txt", error.bytes);

    result = ruda_run_cli(&session, 4, unknown_json_arguments, &output, &error);
    RUDA_EXPECT(result == RUDA_EXIT_USAGE);
    RUDA_EXPECT_SNAPSHOT("unknown-command.jsonl", error.bytes);

    result = ruda_run_cli(&session, 2, planned_arguments, &output, &error);
    RUDA_EXPECT(result == RUDA_EXIT_USAGE);
    RUDA_EXPECT(strstr(error.bytes, "error[U0003]") != NULL);

    result = ruda_run_cli(&session, 3, task_help_arguments, &output, &error);
    RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(strstr(output.bytes, "Usage: ruda build") != NULL);
    RUDA_EXPECT(strstr(output.bytes, "not implemented") != NULL);

    {
        const char *format_help_arguments[] = {"ruda", "fmt", "--help"};
        result = ruda_run_cli(
            &session,
            3,
            format_help_arguments,
            &output,
            &error
        );
        RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
        RUDA_EXPECT(strstr(output.bytes, "Usage: ruda fmt [target] [options]") != NULL);
        RUDA_EXPECT(strstr(output.bytes, "source and manifests") != NULL);
        RUDA_EXPECT(strstr(output.bytes, "not implemented") == NULL);
    }

    result = ruda_run_cli(&session, 2, unknown_option_arguments, &output, &error);
    RUDA_EXPECT(result == RUDA_EXIT_USAGE);
    RUDA_EXPECT(strstr(error.bytes, "error[U0002]") != NULL);
    RUDA_EXPECT(strstr(error.bytes, "--diagnostic") != NULL);

    result = ruda_run_cli(&session, 2, quoted_unknown_arguments, &output, &error);
    RUDA_EXPECT(result == RUDA_EXIT_USAGE);
    RUDA_EXPECT(strstr(error.bytes, "ba\\'d") != NULL);

    {
        static const char source[] = "fn main():void{return;}\r\n";
        static const char expected[] =
            "fn main(): void {\n"
            "  return;\n"
            "}\n";
        RudaCliMemoryInput input = {
            (const unsigned char *)source,
            sizeof(source) - 1,
            0,
            false
        };
        ruda_test_buffer_init(&output);
        ruda_test_buffer_init(&error);
        result = ruda_cli_run(
            &session,
            4,
            format_stdin_arguments,
            ruda_cli_memory_stream(&input),
            ruda_test_buffer_writer(&output),
            ruda_test_buffer_writer(&error)
        );
        RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
        RUDA_EXPECT(strcmp(output.bytes, expected) == 0);
        RUDA_EXPECT(error.length == 0);
    }

    {
        static const char source[] =
            "language \"\\u0030.1\"\r\n"
            "module \"example.com/acme/orders\"\r\n";
        static const char expected[] =
            "module \"example.com/acme/orders\"\n"
            "\n"
            "language \"0.1\"\n";
        RudaCliMemoryInput input = {
            (const unsigned char *)source,
            sizeof(source) - 1,
            0,
            false
        };
        ruda_test_buffer_init(&output);
        ruda_test_buffer_init(&error);
        result = ruda_cli_run(
            &session,
            4,
            format_module_stdin_arguments,
            ruda_cli_memory_stream(&input),
            ruda_test_buffer_writer(&output),
            ruda_test_buffer_writer(&error)
        );
        RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
        RUDA_EXPECT(strcmp(output.bytes, expected) == 0);
        RUDA_EXPECT(error.length == 0);
    }

    {
        static const char source[] =
            "use \"b/./c\"\n"
            "use \"a/x/../y\"\n";
        static const char expected[] =
            "use \"a/y\"\n"
            "use \"b/c\"\n";
        RudaCliMemoryInput input = {
            (const unsigned char *)source,
            sizeof(source) - 1,
            0,
            false
        };
        ruda_test_buffer_init(&output);
        ruda_test_buffer_init(&error);
        result = ruda_cli_run(
            &session,
            4,
            format_workspace_stdin_arguments,
            ruda_cli_memory_stream(&input),
            ruda_test_buffer_writer(&output),
            ruda_test_buffer_writer(&error)
        );
        RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
        RUDA_EXPECT(strcmp(output.bytes, expected) == 0);
        RUDA_EXPECT(error.length == 0);
    }

    {
        static const char source[] = "module \"\\q\"\n";
        RudaCliMemoryInput input = {
            (const unsigned char *)source,
            sizeof(source) - 1,
            0,
            false
        };
        ruda_test_buffer_init(&output);
        ruda_test_buffer_init(&error);
        result = ruda_cli_run(
            &session,
            4,
            format_module_stdin_arguments,
            ruda_cli_memory_stream(&input),
            ruda_test_buffer_writer(&output),
            ruda_test_buffer_writer(&error)
        );
        RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
        RUDA_EXPECT(output.length == 0);
        RUDA_EXPECT(strstr(error.bytes, "error[K0002]") != NULL);
        RUDA_EXPECT(strstr(error.bytes, "nested/ruda.mod:1:") != NULL);
    }

    {
        static const char source[] = "fn main():void{return;}\n";
        RudaCliMemoryInput input = {
            (const unsigned char *)source,
            sizeof(source) - 1,
            0,
            false
        };
        ruda_test_buffer_init(&output);
        ruda_test_buffer_init(&error);
        output.failure_index = 0;
        result = ruda_cli_run(
            &session,
            4,
            format_stdin_arguments,
            ruda_cli_memory_stream(&input),
            ruda_test_buffer_writer(&output),
            ruda_test_buffer_writer(&error)
        );
        RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
        RUDA_EXPECT(strstr(error.bytes, "error[F0002]") != NULL);
    }

    {
        static const char source[] = "fn broken(: void {\n";
        RudaCliMemoryInput input = {
            (const unsigned char *)source,
            sizeof(source) - 1,
            0,
            false
        };
        ruda_test_buffer_init(&output);
        ruda_test_buffer_init(&error);
        result = ruda_cli_run(
            &session,
            6,
            format_json_arguments,
            ruda_cli_memory_stream(&input),
            ruda_test_buffer_writer(&output),
            ruda_test_buffer_writer(&error)
        );
        RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
        RUDA_EXPECT(output.length == 0);
        RUDA_EXPECT(strstr(error.bytes, "\"primary\":{") != NULL);
        RUDA_EXPECT(strstr(error.bytes, "\"file\":\"broken.ruda\"") != NULL);
        RUDA_EXPECT(strstr(error.bytes, "\"code\":\"P0001\"") != NULL);
    }

    {
        RudaCliMemoryInput input = {0};
        ruda_test_buffer_init(&output);
        ruda_test_buffer_init(&error);
        result = ruda_cli_run(
            &session,
            4,
            format_invalid_options,
            ruda_cli_memory_stream(&input),
            ruda_test_buffer_writer(&output),
            ruda_test_buffer_writer(&error)
        );
        RUDA_EXPECT(result == RUDA_EXIT_USAGE);
        RUDA_EXPECT(strstr(error.bytes, "error[U0005]") != NULL);
    }

    result = ruda_run_cli(
        &session,
        5,
        format_conflicting_arguments,
        &output,
        &error
    );
    RUDA_EXPECT(result == RUDA_EXIT_USAGE);
    RUDA_EXPECT(strstr(error.bytes, "error[U0005]") != NULL);

    {
        RudaCliMemoryInput input = {0};
        input.fail = true;
        ruda_test_buffer_init(&output);
        ruda_test_buffer_init(&error);
        result = ruda_cli_run(
            &session,
            4,
            format_stdin_arguments,
            ruda_cli_memory_stream(&input),
            ruda_test_buffer_writer(&output),
            ruda_test_buffer_writer(&error)
        );
        RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
        RUDA_EXPECT(strstr(error.bytes, "error[F0001]") != NULL);
    }

    {
        static const char path[] = "ruda-cli-format-test.ruda";
        static const char collision[] =
            "ruda-cli-format-test.ruda.ruda-fmt-000.tmp";
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
        RUDA_EXPECT(ruda_cli_test_write_file(
            path,
            source,
            sizeof(source) - 1
        ));
        RUDA_EXPECT(ruda_cli_test_write_file(
            collision,
            sentinel,
            sizeof(sentinel) - 1
        ));

        result = ruda_run_cli(
            &session,
            3,
            format_file_arguments,
            &output,
            &error
        );
        RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
        RUDA_EXPECT(output.length == 0);
        RUDA_EXPECT(error.length == 0);
        RUDA_EXPECT(ruda_cli_test_read_file(
            path,
            observed,
            sizeof(observed),
            &observed_length
        ));
        RUDA_EXPECT(observed_length == sizeof(canonical) - 1);
        RUDA_EXPECT(observed_length != sizeof(canonical) - 1
            || memcmp(observed, canonical, sizeof(canonical) - 1) == 0);
        RUDA_EXPECT(ruda_cli_test_read_file(
            collision,
            observed,
            sizeof(observed),
            &observed_length
        ));
        RUDA_EXPECT(observed_length == sizeof(sentinel) - 1);
        RUDA_EXPECT(observed_length != sizeof(sentinel) - 1
            || memcmp(observed, sentinel, sizeof(sentinel) - 1) == 0);

        result = ruda_run_cli(
            &session,
            4,
            format_check_arguments,
            &output,
            &error
        );
        RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
        RUDA_EXPECT(output.length == 0);
        RUDA_EXPECT(error.length == 0);

        RUDA_EXPECT(ruda_cli_test_write_file(
            path,
            source,
            sizeof(source) - 1
        ));
        result = ruda_run_cli(
            &session,
            4,
            format_check_arguments,
            &output,
            &error
        );
        RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
        RUDA_EXPECT(output.length == 0);
        RUDA_EXPECT(strstr(error.bytes, "error[F0004]") != NULL);
        RUDA_EXPECT(ruda_cli_test_read_file(
            path,
            observed,
            sizeof(observed),
            &observed_length
        ));
        RUDA_EXPECT(observed_length == sizeof(source) - 1);
        RUDA_EXPECT(observed_length != sizeof(source) - 1
            || memcmp(observed, source, sizeof(source) - 1) == 0);

        result = ruda_run_cli(
            &session,
            4,
            format_diff_arguments,
            &output,
            &error
        );
        RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
        RUDA_EXPECT(error.length == 0);
        RUDA_EXPECT(strstr(output.bytes, "--- ruda-cli-format-test.ruda") != NULL);
        RUDA_EXPECT(strstr(output.bytes, "-fn main():void{return;}") != NULL);
        RUDA_EXPECT(strstr(output.bytes, "+fn main(): void {") != NULL);

        RUDA_EXPECT(ruda_cli_test_write_file(
            path,
            invalid,
            sizeof(invalid) - 1
        ));
        result = ruda_run_cli(
            &session,
            3,
            format_file_arguments,
            &output,
            &error
        );
        RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
        RUDA_EXPECT(output.length == 0);
        RUDA_EXPECT(strstr(error.bytes, "error[P0001]") != NULL);
        RUDA_EXPECT(ruda_cli_test_read_file(
            path,
            observed,
            sizeof(observed),
            &observed_length
        ));
        RUDA_EXPECT(observed_length == sizeof(invalid) - 1);
        RUDA_EXPECT(observed_length != sizeof(invalid) - 1
            || memcmp(observed, invalid, sizeof(invalid) - 1) == 0);

        RUDA_EXPECT(remove(collision) == 0);
        RUDA_EXPECT(remove(path) == 0);
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
            "ruda", "fmt", "README.fixture"
        };
        const char *legacy_extension_arguments[] = {
            "ruda", "fmt", "legacy.ma"
        };
#if !defined(_WIN32)
        const char *symlink_arguments[] = {
            "ruda", "fmt", "format-tree/link.ruda"
        };
#endif

        RUDA_EXPECT(ruda_cli_test_write_file(
            "format-tree/ruda.mod",
            module_source,
            sizeof(module_source) - 1
        ));
        RUDA_EXPECT(ruda_cli_test_write_file(
            "format-tree/ruda.work",
            workspace_source,
            sizeof(workspace_source) - 1
        ));
        RUDA_EXPECT(ruda_cli_test_write_file(
            "format-tree/src/a.ruda",
            source,
            sizeof(source) - 1
        ));
        RUDA_EXPECT(ruda_cli_test_write_file(
            "format-tree/nested/ruda.mod",
            nested_module,
            sizeof(nested_module) - 1
        ));
        RUDA_EXPECT(ruda_cli_test_write_file(
            "format-tree/nested/skip.ruda",
            nested_source,
            sizeof(nested_source) - 1
        ));
        RUDA_EXPECT(ruda_cli_test_write_file(
            "format-tree/.ruda/ignored.ruda",
            ignored_source,
            sizeof(ignored_source) - 1
        ));
#if !defined(_WIN32)
        (void)unlink("format-tree/link.ruda");
        (void)unlink("format-tree/linkdir");
        RUDA_EXPECT(symlink(
            "src/a.ruda",
            "format-tree/link.ruda"
        ) == 0);
        RUDA_EXPECT(symlink(
            ".ruda",
            "format-tree/linkdir"
        ) == 0);
#endif

        result = ruda_run_cli(
            &session,
            4,
            format_tree_check_arguments,
            &output,
            &error
        );
        RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
        RUDA_EXPECT(output.length == 0);
        RUDA_EXPECT(strstr(error.bytes, "error[F0004]") != NULL);

        result = ruda_run_cli(
            &session,
            3,
            format_tree_arguments,
            &output,
            &error
        );
        RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
        RUDA_EXPECT(output.length == 0);
        RUDA_EXPECT(error.length == 0);

        RUDA_EXPECT(ruda_cli_test_read_file(
            "format-tree/ruda.mod",
            observed,
            sizeof(observed),
            &observed_length
        ));
        RUDA_EXPECT(observed_length == sizeof(module_canonical) - 1);
        RUDA_EXPECT(observed_length != sizeof(module_canonical) - 1
            || memcmp(
                observed,
                module_canonical,
                sizeof(module_canonical) - 1
            ) == 0);
        RUDA_EXPECT(ruda_cli_test_read_file(
            "format-tree/ruda.work",
            observed,
            sizeof(observed),
            &observed_length
        ));
        RUDA_EXPECT(observed_length == sizeof(workspace_canonical) - 1);
        RUDA_EXPECT(observed_length != sizeof(workspace_canonical) - 1
            || memcmp(
                observed,
                workspace_canonical,
                sizeof(workspace_canonical) - 1
            ) == 0);
        RUDA_EXPECT(ruda_cli_test_read_file(
            "format-tree/src/a.ruda",
            observed,
            sizeof(observed),
            &observed_length
        ));
        RUDA_EXPECT(observed_length == sizeof(source_canonical) - 1);
        RUDA_EXPECT(observed_length != sizeof(source_canonical) - 1
            || memcmp(
                observed,
                source_canonical,
                sizeof(source_canonical) - 1
            ) == 0);
        RUDA_EXPECT(ruda_cli_test_read_file(
            "format-tree/nested/ruda.mod",
            observed,
            sizeof(observed),
            &observed_length
        ));
        RUDA_EXPECT(observed_length == sizeof(nested_module) - 1);
        RUDA_EXPECT(observed_length != sizeof(nested_module) - 1
            || memcmp(
                observed,
                nested_module,
                sizeof(nested_module) - 1
            ) == 0);
        RUDA_EXPECT(ruda_cli_test_read_file(
            "format-tree/nested/skip.ruda",
            observed,
            sizeof(observed),
            &observed_length
        ));
        RUDA_EXPECT(observed_length == sizeof(nested_source) - 1);
        RUDA_EXPECT(observed_length != sizeof(nested_source) - 1
            || memcmp(
                observed,
                nested_source,
                sizeof(nested_source) - 1
            ) == 0);
        RUDA_EXPECT(ruda_cli_test_read_file(
            "format-tree/.ruda/ignored.ruda",
            observed,
            sizeof(observed),
            &observed_length
        ));
        RUDA_EXPECT(observed_length == sizeof(ignored_source) - 1);
        RUDA_EXPECT(observed_length != sizeof(ignored_source) - 1
            || memcmp(
                observed,
                ignored_source,
                sizeof(ignored_source) - 1
            ) == 0);

        result = ruda_run_cli(
            &session,
            4,
            format_tree_check_arguments,
            &output,
            &error
        );
        RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
        RUDA_EXPECT(output.length == 0);
        RUDA_EXPECT(error.length == 0);

        result = ruda_run_cli(
            &session,
            3,
            unrecognized_arguments,
            &output,
            &error
        );
        RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
        RUDA_EXPECT(strstr(error.bytes, "error[F0001]") != NULL);

        RUDA_EXPECT(ruda_cli_test_write_file(
            "legacy.ma",
            source,
            sizeof(source) - 1
        ));
        result = ruda_run_cli(
            &session,
            3,
            legacy_extension_arguments,
            &output,
            &error
        );
        RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
        RUDA_EXPECT(strstr(error.bytes, "error[F0001]") != NULL);
        RUDA_EXPECT(remove("legacy.ma") == 0);

#if !defined(_WIN32)
        result = ruda_run_cli(
            &session,
            3,
            symlink_arguments,
            &output,
            &error
        );
        RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
        RUDA_EXPECT(strstr(error.bytes, "error[F0001]") != NULL);
        RUDA_EXPECT(unlink("format-tree/link.ruda") == 0);
        RUDA_EXPECT(unlink("format-tree/linkdir") == 0);
#endif

        {
            size_t failure_index = 0;
            bool reached_success = false;
            for (failure_index = 0;
                failure_index < 1024;
                failure_index += 1) {
                RudaFailAllocator state;
                RudaSession failing_session;
                ruda_fail_allocator_init(
                    &state,
                    ruda_system_allocator(),
                    failure_index
                );
                ruda_session_init(
                    &failing_session,
                    ruda_fail_allocator(&state)
                );
                result = ruda_run_cli(
                    &failing_session,
                    4,
                    format_tree_check_arguments,
                    &output,
                    &error
                );
                if (result == RUDA_EXIT_SUCCESS) {
                    reached_success = true;
                } else {
                    RUDA_EXPECT(result == RUDA_EXIT_INTERNAL);
                }
                RUDA_EXPECT(state.live_allocations == 0);
                RUDA_EXPECT(state.live_bytes == 0);
                if (reached_success) {
                    break;
                }
            }
            RUDA_EXPECT(reached_success);
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
        RudaOwnedBytes original_directory = {0};
        unsigned char observed[4096];
        size_t observed_length = 0;

        RUDA_EXPECT(ruda_directory_current(
            ruda_system_allocator(),
            32768,
            &original_directory
        ) == RUDA_FILE_OK);
        RUDA_EXPECT(ruda_cli_test_write_file(
            "format-tree/src/a.ruda",
            source,
            sizeof(source) - 1
        ));
        if (original_directory.data != NULL
            && ruda_cli_test_change_directory("format-tree/src") == 0) {
            result = ruda_run_cli(
                &session,
                3,
                format_no_target_check_arguments,
                &output,
                &error
            );
            RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
            RUDA_EXPECT(strstr(error.bytes, "error[F0004]") != NULL);
            result = ruda_run_cli(
                &session,
                2,
                format_no_target_arguments,
                &output,
                &error
            );
            RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
            RUDA_EXPECT(ruda_cli_test_change_directory(
                (const char *)original_directory.data
            ) == 0);
        } else {
            RUDA_EXPECT(false);
        }
        RUDA_EXPECT(ruda_cli_test_read_file(
            "format-tree/src/a.ruda",
            observed,
            sizeof(observed),
            &observed_length
        ));
        RUDA_EXPECT(observed_length == sizeof(source_canonical) - 1);
        RUDA_EXPECT(observed_length != sizeof(source_canonical) - 1
            || memcmp(
                observed,
                source_canonical,
                sizeof(source_canonical) - 1
            ) == 0);

        RUDA_EXPECT(ruda_cli_test_write_file(
            "format-tree/workspace/ruda.work",
            ancestor_workspace,
            sizeof(ancestor_workspace) - 1
        ));
        RUDA_EXPECT(ruda_cli_test_write_file(
            "format-tree/workspace/module/ruda.mod",
            module_source,
            sizeof(module_source) - 1
        ));
        RUDA_EXPECT(ruda_cli_test_write_file(
            "format-tree/workspace/module/main.ruda",
            source,
            sizeof(source) - 1
        ));
        if (original_directory.data != NULL
            && ruda_cli_test_change_directory(
                "format-tree/workspace/module"
            ) == 0) {
            result = ruda_run_cli(
                &session,
                2,
                format_no_target_arguments,
                &output,
                &error
            );
            RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
            RUDA_EXPECT(ruda_cli_test_change_directory(
                (const char *)original_directory.data
            ) == 0);
        } else {
            RUDA_EXPECT(false);
        }
        RUDA_EXPECT(ruda_cli_test_read_file(
            "format-tree/workspace/ruda.work",
            observed,
            sizeof(observed),
            &observed_length
        ));
        RUDA_EXPECT(observed_length
            == sizeof(ancestor_workspace_canonical) - 1);
        RUDA_EXPECT(observed_length
                != sizeof(ancestor_workspace_canonical) - 1
            || memcmp(
                observed,
                ancestor_workspace_canonical,
                sizeof(ancestor_workspace_canonical) - 1
            ) == 0);
        RUDA_EXPECT(ruda_cli_test_read_file(
            "format-tree/workspace/module/main.ruda",
            observed,
            sizeof(observed),
            &observed_length
        ));
        RUDA_EXPECT(observed_length == sizeof(source_canonical) - 1);
        RUDA_EXPECT(observed_length != sizeof(source_canonical) - 1
            || memcmp(
                observed,
                source_canonical,
                sizeof(source_canonical) - 1
            ) == 0);
        ruda_owned_bytes_dispose(&original_directory);
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
            RudaFailAllocator state;
            RudaSession failing_session;
            RudaCliMemoryInput input = {
                (const unsigned char *)source,
                sizeof(source) - 1,
                0,
                false
            };
            ruda_fail_allocator_init(
                &state,
                ruda_system_allocator(),
                failure_index
            );
            ruda_session_init(&failing_session, ruda_fail_allocator(&state));
            ruda_test_buffer_init(&output);
            ruda_test_buffer_init(&error);
            result = ruda_cli_run(
                &failing_session,
                4,
                format_stdin_arguments,
                ruda_cli_memory_stream(&input),
                ruda_test_buffer_writer(&output),
                ruda_test_buffer_writer(&error)
            );
            if (result == RUDA_EXIT_SUCCESS) {
                reached_success = true;
                RUDA_EXPECT(strcmp(output.bytes, expected) == 0);
            } else {
                RUDA_EXPECT(result == RUDA_EXIT_INTERNAL);
            }
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
            if (reached_success) {
                break;
            }
        }
        RUDA_EXPECT(reached_success);
    }

    {
        static const char path[] = "ruda-cli-format-fail-test.ruda";
        static const unsigned char source[] =
            "fn main():void{return;}\n";
        const char *arguments[] = {"ruda", "fmt", path};
        unsigned char observed[4096];
        size_t observed_length = 0;
        size_t failure_index = 0;
        bool reached_success = false;
        for (failure_index = 0; failure_index < 128; failure_index += 1) {
            RudaFailAllocator state;
            RudaSession failing_session;
            RUDA_EXPECT(ruda_cli_test_write_file(
                path,
                source,
                sizeof(source) - 1
            ));
            ruda_fail_allocator_init(
                &state,
                ruda_system_allocator(),
                failure_index
            );
            ruda_session_init(&failing_session, ruda_fail_allocator(&state));
            result = ruda_run_cli(
                &failing_session,
                3,
                arguments,
                &output,
                &error
            );
            if (result == RUDA_EXIT_SUCCESS) {
                reached_success = true;
            } else {
                RUDA_EXPECT(result == RUDA_EXIT_INTERNAL);
                RUDA_EXPECT(ruda_cli_test_read_file(
                    path,
                    observed,
                    sizeof(observed),
                    &observed_length
                ));
                RUDA_EXPECT(observed_length == sizeof(source) - 1);
                RUDA_EXPECT(observed_length != sizeof(source) - 1
                    || memcmp(observed, source, sizeof(source) - 1) == 0);
            }
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
            if (reached_success) {
                break;
            }
        }
        RUDA_EXPECT(reached_success);
        RUDA_EXPECT(remove(path) == 0);
    }

    ruda_test_buffer_init(&output);
    ruda_test_buffer_init(&error);
    output.failure_index = 0;
    result = ruda_cli_run(
        &session,
        1,
        help_arguments,
        (RudaReadStream){0},
        ruda_test_buffer_writer(&output),
        ruda_test_buffer_writer(&error)
    );
    RUDA_EXPECT(result == RUDA_EXIT_INTERNAL);

    ruda_test_buffer_init(&output);
    ruda_test_buffer_init(&error);
    error.failure_index = 0;
    result = ruda_cli_run(
        &session,
        2,
        unknown_arguments,
        (RudaReadStream){0},
        ruda_test_buffer_writer(&output),
        ruda_test_buffer_writer(&error)
    );
    RUDA_EXPECT(result == RUDA_EXIT_INTERNAL);
}
