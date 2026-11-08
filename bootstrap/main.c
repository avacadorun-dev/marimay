#include <signal.h>
#include <stdio.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

#include "mmay/base/allocator.h"
#include "mmay/base/stream.h"
#include "mmay/base/writer.h"
#include "mmay/cli.h"
#include "mmay/platform/process.h"
#include "mmay/runtime/program_arguments.h"
#include "mmay/session.h"

enum {
    MMAY_HOST_COMMAND_ARGUMENT_MAX_COUNT = 8192,
    MMAY_HOST_COMMAND_ARGUMENT_MAX_BYTES = 8 * 1024 * 1024
};

#if defined(_WIN32)
int wmain(int argument_count, wchar_t **host_arguments) {
#else
int main(int argument_count, char **host_arguments) {
#endif
    MarimayOwnedBytes executable_path = {0};
    MarimayHostArguments imported = {0};
    MarimayProgramArgumentsStatus import_status;
    MarimaySession session;
    int exit_code = 0;
#if defined(_WIN32)
    int input_descriptor = _fileno(stdin);
    int output_descriptor = _fileno(stdout);
    int error_descriptor = _fileno(stderr);
    if ((input_descriptor >= 0 && _setmode(input_descriptor, _O_BINARY) == -1)
        || (output_descriptor >= 0 && _setmode(output_descriptor, _O_BINARY) == -1)
        || (error_descriptor >= 0 && _setmode(error_descriptor, _O_BINARY) == -1)) {
        fputs("error[I0002]: runtime host stream setup failed\n", stderr);
        return 70;
    }
#endif
#if defined(SIGPIPE)
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        fputs("error[I0002]: runtime host signal setup failed\n", stderr);
        return 70;
    }
#endif
    mmay_session_init(&session, mmay_system_allocator());
    import_status = argument_count < 1
        ? MMAY_PROGRAM_ARGUMENTS_INVALID_ARGUMENT
        : mmay_host_arguments_import(
            mmay_system_allocator(),
            (const MarimayHostArgumentChar *const *)host_arguments,
            argument_count > 0 ? (size_t)argument_count : 0,
            MMAY_HOST_COMMAND_ARGUMENT_MAX_COUNT,
            MMAY_HOST_COMMAND_ARGUMENT_MAX_BYTES,
            &imported
        );
    if (import_status == MMAY_PROGRAM_ARGUMENTS_ALLOCATION_FAILED) {
        fputs("error[I0002]: runtime host allocation failed\n", stderr);
        return 70;
    }
    if (import_status != MMAY_PROGRAM_ARGUMENTS_OK) {
        fputs("error[U0007]: host command arguments are not bounded UTF-8\n", stderr);
        return 2;
    }
    if (mmay_process_executable_path(
            mmay_system_allocator(), 32768, &executable_path
        ) == MMAY_PROCESS_OK) {
        imported.values[0] = (char *)executable_path.data;
    }
    exit_code = mmay_cli_run(
        &session,
        argument_count,
        (const char *const *)imported.values,
        mmay_file_read_stream(stdin),
        mmay_file_writer(stdout),
        mmay_file_writer(stderr)
    );
    mmay_owned_bytes_dispose(&executable_path);
    mmay_host_arguments_dispose(&imported);
    return exit_code;
}
