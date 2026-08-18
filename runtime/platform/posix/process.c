#define _POSIX_C_SOURCE 200809L

#include "ruda/platform/process.h"

#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ruda/base/vector.h"

extern char **environ;

static void ruda_process_result_init(RudaProcessResult *result, RudaAllocator allocator) {
    result->output.allocator = allocator;
    result->output.data = NULL;
    result->output.length = 0;
    result->exit_code = 0;
    result->signaled = false;
    result->signal_code = 0;
}

static void ruda_process_stop_and_wait(pid_t process) {
    int wait_status = 0;
    (void)kill(process, SIGKILL);
    while (waitpid(process, &wait_status, 0) < 0 && errno == EINTR) {
    }
}

RudaProcessStatus ruda_process_run(
    RudaAllocator allocator,
    const char *const arguments[],
    size_t argument_count,
    size_t max_output,
    RudaProcessResult *result
) {
    int descriptors[2] = {-1, -1};
    posix_spawn_file_actions_t actions;
    bool actions_initialized = false;
    pid_t process = 0;
    int spawn_status = 0;
    int wait_status = 0;
    RudaVector output;
    RudaProcessStatus status = RUDA_PROCESS_OK;
    unsigned char chunk[4096];
    size_t index = 0;

    if (result == NULL) {
        return RUDA_PROCESS_INVALID_ARGUMENT;
    }
    ruda_process_result_init(result, allocator);
    if (arguments == NULL || argument_count == 0 || arguments[0] == NULL
        || arguments[argument_count] != NULL) {
        return RUDA_PROCESS_INVALID_ARGUMENT;
    }
    for (index = 0; index < argument_count; index += 1) {
        if (arguments[index] == NULL) {
            return RUDA_PROCESS_INVALID_ARGUMENT;
        }
    }
    if (!ruda_vector_init(&output, allocator, sizeof(unsigned char))) {
        return RUDA_PROCESS_INVALID_ARGUMENT;
    }
    if (pipe(descriptors) != 0) {
        status = RUDA_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    if (posix_spawn_file_actions_init(&actions) != 0) {
        status = RUDA_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    actions_initialized = true;
    if (posix_spawn_file_actions_adddup2(&actions, descriptors[1], STDOUT_FILENO) != 0
        || posix_spawn_file_actions_adddup2(&actions, descriptors[1], STDERR_FILENO) != 0
        || posix_spawn_file_actions_addclose(&actions, descriptors[0]) != 0
        || posix_spawn_file_actions_addclose(&actions, descriptors[1]) != 0) {
        status = RUDA_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    spawn_status = posix_spawnp(
        &process,
        arguments[0],
        &actions,
        NULL,
        (char *const *)arguments,
        environ
    );
    if (spawn_status != 0) {
        status = RUDA_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    (void)close(descriptors[1]);
    descriptors[1] = -1;
    for (;;) {
        ssize_t count = read(descriptors[0], chunk, sizeof(chunk));
        if (count == 0) {
            break;
        }
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            status = RUDA_PROCESS_IO_FAILED;
            ruda_process_stop_and_wait(process);
            process = 0;
            goto cleanup;
        }
        if (output.length > max_output || (size_t)count > max_output - output.length) {
            status = RUDA_PROCESS_OUTPUT_LIMIT;
            ruda_process_stop_and_wait(process);
            process = 0;
            goto cleanup;
        }
        if (!ruda_vector_append(&output, chunk, (size_t)count)) {
            status = RUDA_PROCESS_ALLOCATION_FAILED;
            ruda_process_stop_and_wait(process);
            process = 0;
            goto cleanup;
        }
    }
    (void)close(descriptors[0]);
    descriptors[0] = -1;
    while (waitpid(process, &wait_status, 0) < 0) {
        if (errno != EINTR) {
            status = RUDA_PROCESS_WAIT_FAILED;
            process = 0;
            goto cleanup;
        }
    }
    process = 0;
    if (WIFEXITED(wait_status)) {
        result->exit_code = (uint32_t)WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        result->signaled = true;
        result->signal_code = (uint32_t)WTERMSIG(wait_status);
        result->exit_code = 128U + result->signal_code;
    } else {
        status = RUDA_PROCESS_WAIT_FAILED;
        goto cleanup;
    }
    if (!ruda_owned_bytes_copy(&result->output, allocator, output.data, output.length)) {
        status = RUDA_PROCESS_ALLOCATION_FAILED;
        goto cleanup;
    }

cleanup:
    if (process != 0) {
        ruda_process_stop_and_wait(process);
    }
    if (descriptors[0] >= 0) {
        (void)close(descriptors[0]);
    }
    if (descriptors[1] >= 0) {
        (void)close(descriptors[1]);
    }
    if (actions_initialized) {
        (void)posix_spawn_file_actions_destroy(&actions);
    }
    ruda_vector_dispose(&output);
    if (status != RUDA_PROCESS_OK) {
        ruda_owned_bytes_dispose(&result->output);
    }
    return status;
}

void ruda_process_result_dispose(RudaProcessResult *result) {
    if (result != NULL) {
        ruda_owned_bytes_dispose(&result->output);
        result->exit_code = 0;
        result->signaled = false;
        result->signal_code = 0;
    }
}

static bool ruda_process_capture_arguments_valid(
    const char *const arguments[],
    size_t argument_count
) {
    size_t index = 0;
    if (arguments == NULL || argument_count == 0 || arguments[0] == NULL
        || arguments[argument_count] != NULL) return false;
    for (index = 0; index < argument_count; index += 1) {
        if (arguments[index] == NULL) return false;
    }
    return true;
}

static RudaProcessStatus ruda_process_capture_read(
    int descriptor,
    RudaVector *output,
    size_t limit,
    bool *open
) {
    unsigned char chunk[4096];
    ssize_t count;
    do {
        count = read(descriptor, chunk, sizeof(chunk));
    } while (count < 0 && errno == EINTR);
    if (count == 0) {
        (void)close(descriptor);
        *open = false;
        return RUDA_PROCESS_OK;
    }
    if (count < 0) return RUDA_PROCESS_IO_FAILED;
    if (output->length > limit || (size_t)count > limit - output->length) {
        return RUDA_PROCESS_OUTPUT_LIMIT;
    }
    return ruda_vector_append(output, chunk, (size_t)count)
        ? RUDA_PROCESS_OK : RUDA_PROCESS_ALLOCATION_FAILED;
}

RudaProcessStatus ruda_process_run_capture(
    RudaAllocator allocator,
    const char *const arguments[],
    size_t argument_count,
    size_t max_output,
    size_t max_error,
    RudaProcessCaptureResult *result
) {
    int output_pipe[2] = {-1, -1};
    int error_pipe[2] = {-1, -1};
    posix_spawn_file_actions_t actions;
    bool actions_ready = false;
    bool output_open = false;
    bool error_open = false;
    pid_t process = 0;
    int spawn_status = 0;
    int wait_status = 0;
    RudaVector output;
    RudaVector error;
    RudaProcessStatus status = RUDA_PROCESS_OK;
    memset(&output, 0, sizeof(output));
    memset(&error, 0, sizeof(error));
    if (result == NULL) return RUDA_PROCESS_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));
    result->standard_output.allocator = allocator;
    result->standard_error.allocator = allocator;
    if (!ruda_process_capture_arguments_valid(arguments, argument_count)) {
        return RUDA_PROCESS_INVALID_ARGUMENT;
    }
    if (!ruda_vector_init(&output, allocator, sizeof(unsigned char))
        || !ruda_vector_init(&error, allocator, sizeof(unsigned char))) {
        status = RUDA_PROCESS_ALLOCATION_FAILED;
        goto cleanup;
    }
    if (pipe(output_pipe) != 0 || pipe(error_pipe) != 0
        || posix_spawn_file_actions_init(&actions) != 0) {
        status = RUDA_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    actions_ready = true;
    if (posix_spawn_file_actions_adddup2(
            &actions, output_pipe[1], STDOUT_FILENO
        ) != 0 || posix_spawn_file_actions_adddup2(
            &actions, error_pipe[1], STDERR_FILENO
        ) != 0 || posix_spawn_file_actions_addclose(
            &actions, output_pipe[0]
        ) != 0 || posix_spawn_file_actions_addclose(
            &actions, error_pipe[0]
        ) != 0 || posix_spawn_file_actions_addclose(
            &actions, output_pipe[1]
        ) != 0 || posix_spawn_file_actions_addclose(
            &actions, error_pipe[1]
        ) != 0) {
        status = RUDA_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    spawn_status = posix_spawnp(
        &process, arguments[0], &actions, NULL,
        (char *const *)arguments, environ
    );
    if (spawn_status != 0) {
        status = RUDA_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    (void)close(output_pipe[1]);
    output_pipe[1] = -1;
    (void)close(error_pipe[1]);
    error_pipe[1] = -1;
    output_open = true;
    error_open = true;
    while (output_open || error_open) {
        struct pollfd descriptors[2];
        descriptors[0].fd = output_open ? output_pipe[0] : -1;
        descriptors[0].events = POLLIN | POLLHUP;
        descriptors[0].revents = 0;
        descriptors[1].fd = error_open ? error_pipe[0] : -1;
        descriptors[1].events = POLLIN | POLLHUP;
        descriptors[1].revents = 0;
        if (poll(descriptors, 2, -1) < 0) {
            if (errno == EINTR) continue;
            status = RUDA_PROCESS_IO_FAILED;
            goto terminate;
        }
        if (output_open && descriptors[0].revents != 0) {
            status = ruda_process_capture_read(
                output_pipe[0], &output, max_output, &output_open
            );
            if (!output_open) output_pipe[0] = -1;
            if (status != RUDA_PROCESS_OK) goto terminate;
        }
        if (error_open && descriptors[1].revents != 0) {
            status = ruda_process_capture_read(
                error_pipe[0], &error, max_error, &error_open
            );
            if (!error_open) error_pipe[0] = -1;
            if (status != RUDA_PROCESS_OK) goto terminate;
        }
    }
    while (waitpid(process, &wait_status, 0) < 0) {
        if (errno != EINTR) {
            status = RUDA_PROCESS_WAIT_FAILED;
            goto cleanup;
        }
    }
    process = 0;
    if (WIFEXITED(wait_status)) {
        result->exit_code = (uint32_t)WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        result->signaled = true;
        result->signal_code = (uint32_t)WTERMSIG(wait_status);
        result->exit_code = 128U + result->signal_code;
    } else {
        status = RUDA_PROCESS_WAIT_FAILED;
        goto cleanup;
    }
    if (!ruda_owned_bytes_copy(
            &result->standard_output, allocator,
            output.data, output.length
        ) || !ruda_owned_bytes_copy(
            &result->standard_error, allocator,
            error.data, error.length
        )) {
        status = RUDA_PROCESS_ALLOCATION_FAILED;
    }
    goto cleanup;
terminate:
    ruda_process_stop_and_wait(process);
    process = 0;
cleanup:
    if (process != 0) ruda_process_stop_and_wait(process);
    if (output_pipe[0] >= 0) (void)close(output_pipe[0]);
    if (output_pipe[1] >= 0) (void)close(output_pipe[1]);
    if (error_pipe[0] >= 0) (void)close(error_pipe[0]);
    if (error_pipe[1] >= 0) (void)close(error_pipe[1]);
    if (actions_ready) (void)posix_spawn_file_actions_destroy(&actions);
    ruda_vector_dispose(&output);
    ruda_vector_dispose(&error);
    if (status != RUDA_PROCESS_OK) {
        ruda_process_capture_result_dispose(result);
    }
    return status;
}

void ruda_process_capture_result_dispose(RudaProcessCaptureResult *result) {
    if (result == NULL) return;
    ruda_owned_bytes_dispose(&result->standard_output);
    ruda_owned_bytes_dispose(&result->standard_error);
    result->exit_code = 0;
    result->signaled = false;
    result->signal_code = 0;
}
