#include <stdio.h>

#include "ruda/base/allocator.h"
#include "ruda/base/stream.h"
#include "ruda/base/writer.h"
#include "ruda/cli.h"
#include "ruda/platform/process.h"
#include "ruda/session.h"

int main(int argument_count, char **arguments) {
    RudaOwnedBytes executable_path = {0};
    RudaSession session;
    int exit_code = 0;
    ruda_session_init(&session, ruda_system_allocator());
    if (ruda_process_executable_path(
            ruda_system_allocator(), 32768, &executable_path
        ) == RUDA_PROCESS_OK) {
        arguments[0] = (char *)executable_path.data;
    }
    exit_code = ruda_cli_run(
        &session,
        argument_count,
        (const char *const *)arguments,
        ruda_file_read_stream(stdin),
        ruda_file_writer(stdout),
        ruda_file_writer(stderr)
    );
    ruda_owned_bytes_dispose(&executable_path);
    return exit_code;
}
