#include <stdio.h>

#include "ruda/base/allocator.h"
#include "ruda/base/stream.h"
#include "ruda/base/writer.h"
#include "ruda/cli.h"
#include "ruda/session.h"

int main(int argument_count, char **arguments) {
    RudaSession session;
    ruda_session_init(&session, ruda_system_allocator());
    return ruda_cli_run(
        &session,
        argument_count,
        (const char *const *)arguments,
        ruda_file_read_stream(stdin),
        ruda_file_writer(stdout),
        ruda_file_writer(stderr)
    );
}
