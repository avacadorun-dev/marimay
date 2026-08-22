#include <stdio.h>

#include "mmay/base/allocator.h"
#include "mmay/base/stream.h"
#include "mmay/base/writer.h"
#include "mmay/cli.h"
#include "mmay/platform/process.h"
#include "mmay/session.h"

int main(int argument_count, char **arguments) {
    MarimayOwnedBytes executable_path = {0};
    MarimaySession session;
    int exit_code = 0;
    mmay_session_init(&session, mmay_system_allocator());
    if (mmay_process_executable_path(
            mmay_system_allocator(), 32768, &executable_path
        ) == MMAY_PROCESS_OK) {
        arguments[0] = (char *)executable_path.data;
    }
    exit_code = mmay_cli_run(
        &session,
        argument_count,
        (const char *const *)arguments,
        mmay_file_read_stream(stdin),
        mmay_file_writer(stdout),
        mmay_file_writer(stderr)
    );
    mmay_owned_bytes_dispose(&executable_path);
    return exit_code;
}
