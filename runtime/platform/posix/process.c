#define _POSIX_C_SOURCE 200809L

#include "mmay/platform/process.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include "mmay/base/vector.h"

extern char **environ;

MarimayProcessStatus mmay_process_executable_path(
    MarimayAllocator allocator,
    size_t max_path,
    MarimayOwnedBytes *path
) {
    char buffer[32769];
    size_t length = 0;
    if (path == NULL || max_path == 0) {
        return MMAY_PROCESS_INVALID_ARGUMENT;
    }
    memset(path, 0, sizeof(*path));
#if defined(__APPLE__)
    {
        uint32_t capacity = (uint32_t)sizeof(buffer);
        if (_NSGetExecutablePath(buffer, &capacity) != 0) {
            return MMAY_PROCESS_OUTPUT_LIMIT;
        }
        length = strlen(buffer);
    }
#elif defined(__linux__)
    {
        ssize_t count = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
        if (count < 0) return MMAY_PROCESS_IO_FAILED;
        length = (size_t)count;
        buffer[length] = 0;
    }
#else
    (void)allocator;
    return MMAY_PROCESS_IO_FAILED;
#endif
    if (length == 0 || length > max_path) {
        return MMAY_PROCESS_OUTPUT_LIMIT;
    }
    return mmay_owned_bytes_copy(path, allocator, buffer, length)
        ? MMAY_PROCESS_OK : MMAY_PROCESS_ALLOCATION_FAILED;
}

static void mmay_process_result_init(MarimayProcessResult *result, MarimayAllocator allocator) {
    result->output.allocator = allocator;
    result->output.data = NULL;
    result->output.length = 0;
    result->exit_code = 0;
    result->signaled = false;
    result->signal_code = 0;
}

static void mmay_process_stop_and_wait(pid_t process) {
    int wait_status = 0;
    (void)kill(process, SIGKILL);
    while (waitpid(process, &wait_status, 0) < 0 && errno == EINTR) {
    }
}

MarimayProcessStatus mmay_process_run(
    MarimayAllocator allocator,
    const char *const arguments[],
    size_t argument_count,
    size_t max_output,
    MarimayProcessResult *result
) {
    int descriptors[2] = {-1, -1};
    posix_spawn_file_actions_t actions;
    bool actions_initialized = false;
    pid_t process = 0;
    int spawn_status = 0;
    int wait_status = 0;
    MarimayVector output;
    MarimayProcessStatus status = MMAY_PROCESS_OK;
    unsigned char chunk[4096];
    size_t index = 0;

    if (result == NULL) {
        return MMAY_PROCESS_INVALID_ARGUMENT;
    }
    mmay_process_result_init(result, allocator);
    if (arguments == NULL || argument_count == 0 || arguments[0] == NULL
        || arguments[argument_count] != NULL) {
        return MMAY_PROCESS_INVALID_ARGUMENT;
    }
    for (index = 0; index < argument_count; index += 1) {
        if (arguments[index] == NULL) {
            return MMAY_PROCESS_INVALID_ARGUMENT;
        }
    }
    if (!mmay_vector_init(&output, allocator, sizeof(unsigned char))) {
        return MMAY_PROCESS_INVALID_ARGUMENT;
    }
    if (pipe(descriptors) != 0) {
        status = MMAY_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    if (posix_spawn_file_actions_init(&actions) != 0) {
        status = MMAY_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    actions_initialized = true;
    if (posix_spawn_file_actions_adddup2(&actions, descriptors[1], STDOUT_FILENO) != 0
        || posix_spawn_file_actions_adddup2(&actions, descriptors[1], STDERR_FILENO) != 0
        || posix_spawn_file_actions_addclose(&actions, descriptors[0]) != 0
        || posix_spawn_file_actions_addclose(&actions, descriptors[1]) != 0) {
        status = MMAY_PROCESS_SPAWN_FAILED;
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
        status = MMAY_PROCESS_SPAWN_FAILED;
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
            status = MMAY_PROCESS_IO_FAILED;
            mmay_process_stop_and_wait(process);
            process = 0;
            goto cleanup;
        }
        if (output.length > max_output || (size_t)count > max_output - output.length) {
            status = MMAY_PROCESS_OUTPUT_LIMIT;
            mmay_process_stop_and_wait(process);
            process = 0;
            goto cleanup;
        }
        if (!mmay_vector_append(&output, chunk, (size_t)count)) {
            status = MMAY_PROCESS_ALLOCATION_FAILED;
            mmay_process_stop_and_wait(process);
            process = 0;
            goto cleanup;
        }
    }
    (void)close(descriptors[0]);
    descriptors[0] = -1;
    while (waitpid(process, &wait_status, 0) < 0) {
        if (errno != EINTR) {
            status = MMAY_PROCESS_WAIT_FAILED;
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
        status = MMAY_PROCESS_WAIT_FAILED;
        goto cleanup;
    }
    if (!mmay_owned_bytes_copy(&result->output, allocator, output.data, output.length)) {
        status = MMAY_PROCESS_ALLOCATION_FAILED;
        goto cleanup;
    }

cleanup:
    if (process != 0) {
        mmay_process_stop_and_wait(process);
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
    mmay_vector_dispose(&output);
    if (status != MMAY_PROCESS_OK) {
        mmay_owned_bytes_dispose(&result->output);
    }
    return status;
}

MarimayProcessStatus mmay_process_run_stdio(
    MarimayAllocator allocator,
    const char *const arguments[],
    size_t argument_count,
    FILE *standard_input,
    FILE *standard_output,
    FILE *standard_error,
    MarimayProcessResult *result
) {
    posix_spawn_file_actions_t actions;
    bool actions_ready = false;
    pid_t process = 0;
    int wait_status = 0;
    int input_descriptor;
    int output_descriptor;
    int error_descriptor;
    size_t index = 0;
    MarimayProcessStatus status = MMAY_PROCESS_OK;
    if (result == NULL) return MMAY_PROCESS_INVALID_ARGUMENT;
    mmay_process_result_init(result, allocator);
    if (arguments == NULL || argument_count == 0 || arguments[0] == NULL
        || arguments[argument_count] != NULL || standard_input == NULL
        || standard_output == NULL || standard_error == NULL) {
        return MMAY_PROCESS_INVALID_ARGUMENT;
    }
    for (index = 0; index < argument_count; index += 1) {
        if (arguments[index] == NULL) return MMAY_PROCESS_INVALID_ARGUMENT;
    }
    input_descriptor = fileno(standard_input);
    output_descriptor = fileno(standard_output);
    error_descriptor = fileno(standard_error);
    if (input_descriptor < 0 || output_descriptor < 0 || error_descriptor < 0
        || fflush(standard_output) != 0 || fflush(standard_error) != 0) {
        return MMAY_PROCESS_IO_FAILED;
    }
    if (posix_spawn_file_actions_init(&actions) != 0) {
        return MMAY_PROCESS_SPAWN_FAILED;
    }
    actions_ready = true;
    if (posix_spawn_file_actions_adddup2(
            &actions, input_descriptor, STDIN_FILENO
        ) != 0 || posix_spawn_file_actions_adddup2(
            &actions, output_descriptor, STDOUT_FILENO
        ) != 0 || posix_spawn_file_actions_adddup2(
            &actions, error_descriptor, STDERR_FILENO
        ) != 0 || posix_spawnp(
            &process, arguments[0], &actions, NULL,
            (char *const *)arguments, environ
        ) != 0) {
        status = MMAY_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    while (waitpid(process, &wait_status, 0) < 0) {
        if (errno != EINTR) {
            status = MMAY_PROCESS_WAIT_FAILED;
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
        status = MMAY_PROCESS_WAIT_FAILED;
    }
cleanup:
    if (process != 0) mmay_process_stop_and_wait(process);
    if (actions_ready) (void)posix_spawn_file_actions_destroy(&actions);
    return status;
}

void mmay_process_result_dispose(MarimayProcessResult *result) {
    if (result != NULL) {
        mmay_owned_bytes_dispose(&result->output);
        result->exit_code = 0;
        result->signaled = false;
        result->signal_code = 0;
    }
}

static bool mmay_process_capture_arguments_valid(
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

static MarimayProcessStatus mmay_process_capture_read(
    int descriptor,
    MarimayVector *output,
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
        return MMAY_PROCESS_OK;
    }
    if (count < 0) return MMAY_PROCESS_IO_FAILED;
    if (output->length > limit || (size_t)count > limit - output->length) {
        return MMAY_PROCESS_OUTPUT_LIMIT;
    }
    return mmay_vector_append(output, chunk, (size_t)count)
        ? MMAY_PROCESS_OK : MMAY_PROCESS_ALLOCATION_FAILED;
}

static MarimayProcessStatus mmay_process_stdin_file(
    const unsigned char *bytes,
    size_t length,
    int *descriptor
) {
    char path[] = "/tmp/mmay-process-stdin-XXXXXX";
    size_t offset = 0;
    int file = -1;
    if (descriptor == NULL || (bytes == NULL && length != 0)) {
        return MMAY_PROCESS_INVALID_ARGUMENT;
    }
    file = mkstemp(path);
    if (file < 0) return MMAY_PROCESS_IO_FAILED;
    (void)unlink(path);
    while (offset < length) {
        ssize_t count = write(file, bytes + offset, length - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            (void)close(file);
            return MMAY_PROCESS_IO_FAILED;
        }
        offset += (size_t)count;
    }
    if (lseek(file, 0, SEEK_SET) < 0) {
        (void)close(file);
        return MMAY_PROCESS_IO_FAILED;
    }
    *descriptor = file;
    return MMAY_PROCESS_OK;
}

MarimayProcessStatus mmay_process_run_capture_input(
    MarimayAllocator allocator,
    const char *const arguments[],
    size_t argument_count,
    const unsigned char *standard_input,
    size_t standard_input_length,
    size_t max_output,
    size_t max_error,
    MarimayProcessCaptureResult *result
) {
    int input_file = -1;
    int output_pipe[2] = {-1, -1};
    int error_pipe[2] = {-1, -1};
    posix_spawn_file_actions_t actions;
    bool actions_ready = false;
    bool output_open = false;
    bool error_open = false;
    pid_t process = 0;
    int spawn_status = 0;
    int wait_status = 0;
    MarimayVector output;
    MarimayVector error;
    MarimayProcessStatus status = MMAY_PROCESS_OK;
    memset(&output, 0, sizeof(output));
    memset(&error, 0, sizeof(error));
    if (result == NULL) return MMAY_PROCESS_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));
    result->standard_output.allocator = allocator;
    result->standard_error.allocator = allocator;
    if (!mmay_process_capture_arguments_valid(arguments, argument_count)
        || (standard_input == NULL && standard_input_length != 0)) {
        return MMAY_PROCESS_INVALID_ARGUMENT;
    }
    if (!mmay_vector_init(&output, allocator, sizeof(unsigned char))
        || !mmay_vector_init(&error, allocator, sizeof(unsigned char))) {
        status = MMAY_PROCESS_ALLOCATION_FAILED;
        goto cleanup;
    }
    status = mmay_process_stdin_file(
        standard_input, standard_input_length, &input_file
    );
    if (status != MMAY_PROCESS_OK) goto cleanup;
    if (pipe(output_pipe) != 0 || pipe(error_pipe) != 0
        || posix_spawn_file_actions_init(&actions) != 0) {
        status = MMAY_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    actions_ready = true;
    if (posix_spawn_file_actions_adddup2(
            &actions, input_file, STDIN_FILENO
        ) != 0 || posix_spawn_file_actions_adddup2(
            &actions, output_pipe[1], STDOUT_FILENO
        ) != 0 || posix_spawn_file_actions_adddup2(
            &actions, error_pipe[1], STDERR_FILENO
        ) != 0 || posix_spawn_file_actions_addclose(
            &actions, input_file
        ) != 0 || posix_spawn_file_actions_addclose(
            &actions, output_pipe[0]
        ) != 0 || posix_spawn_file_actions_addclose(
            &actions, error_pipe[0]
        ) != 0 || posix_spawn_file_actions_addclose(
            &actions, output_pipe[1]
        ) != 0 || posix_spawn_file_actions_addclose(
            &actions, error_pipe[1]
        ) != 0) {
        status = MMAY_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    spawn_status = posix_spawnp(
        &process, arguments[0], &actions, NULL,
        (char *const *)arguments, environ
    );
    if (spawn_status != 0) {
        status = MMAY_PROCESS_SPAWN_FAILED;
        goto cleanup;
    }
    (void)close(input_file);
    input_file = -1;
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
            status = MMAY_PROCESS_IO_FAILED;
            goto terminate;
        }
        if (output_open && descriptors[0].revents != 0) {
            status = mmay_process_capture_read(
                output_pipe[0], &output, max_output, &output_open
            );
            if (!output_open) output_pipe[0] = -1;
            if (status != MMAY_PROCESS_OK) goto terminate;
        }
        if (error_open && descriptors[1].revents != 0) {
            status = mmay_process_capture_read(
                error_pipe[0], &error, max_error, &error_open
            );
            if (!error_open) error_pipe[0] = -1;
            if (status != MMAY_PROCESS_OK) goto terminate;
        }
    }
    while (waitpid(process, &wait_status, 0) < 0) {
        if (errno != EINTR) {
            status = MMAY_PROCESS_WAIT_FAILED;
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
        status = MMAY_PROCESS_WAIT_FAILED;
        goto cleanup;
    }
    if (!mmay_owned_bytes_copy(
            &result->standard_output, allocator,
            output.data, output.length
        ) || !mmay_owned_bytes_copy(
            &result->standard_error, allocator,
            error.data, error.length
        )) {
        status = MMAY_PROCESS_ALLOCATION_FAILED;
    }
    goto cleanup;
terminate:
    mmay_process_stop_and_wait(process);
    process = 0;
cleanup:
    if (process != 0) mmay_process_stop_and_wait(process);
    if (input_file >= 0) (void)close(input_file);
    if (output_pipe[0] >= 0) (void)close(output_pipe[0]);
    if (output_pipe[1] >= 0) (void)close(output_pipe[1]);
    if (error_pipe[0] >= 0) (void)close(error_pipe[0]);
    if (error_pipe[1] >= 0) (void)close(error_pipe[1]);
    if (actions_ready) (void)posix_spawn_file_actions_destroy(&actions);
    mmay_vector_dispose(&output);
    mmay_vector_dispose(&error);
    if (status != MMAY_PROCESS_OK) {
        mmay_process_capture_result_dispose(result);
    }
    return status;
}

MarimayProcessStatus mmay_process_run_capture(
    MarimayAllocator allocator,
    const char *const arguments[],
    size_t argument_count,
    size_t max_output,
    size_t max_error,
    MarimayProcessCaptureResult *result
) {
    return mmay_process_run_capture_input(
        allocator, arguments, argument_count, NULL, 0,
        max_output, max_error, result
    );
}

void mmay_process_capture_result_dispose(MarimayProcessCaptureResult *result) {
    if (result == NULL) return;
    mmay_owned_bytes_dispose(&result->standard_output);
    mmay_owned_bytes_dispose(&result->standard_error);
    result->exit_code = 0;
    result->signaled = false;
    result->signal_code = 0;
}
