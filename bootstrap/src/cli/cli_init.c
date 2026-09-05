#include "cli_init.h"

#include <stdio.h>
#include <string.h>

#include "mmay/base/path.h"
#include "mmay/generated/diagnostic_codes.h"
#include "mmay/platform/directory.h"
#include "mmay/platform/file.h"

enum { MMAY_CLI_INIT_MAX_PATH = 32768 };

static const unsigned char mmay_cli_init_main[] =
    "import { Error as IoError, println } from \"std/io\";\n"
    "\n"
    "fn main(arguments: []string): Result<void, IoError> {\n"
    "  if arguments.length() == 1 && arguments[0] == \"--help\" {\n"
    "    println(\"Marimay initialized project\")?;\n"
    "    return Ok();\n"
    "  }\n"
    "  println(\"Hello from Marimay\")?;\n"
    "  return Ok();\n"
    "}\n";

static const unsigned char mmay_cli_init_test[] =
    "@test\n"
    "fn initializedProjectWorks(): void {\n"
    "  expect.equal(2 + 2, 4);\n"
    "}\n";

static bool mmay_cli_init_join(
    MarimayAllocator allocator,
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length,
    MarimayOwnedBytes *output
) {
    size_t length = 0;
    size_t allocation_size = 0;
    unsigned char *bytes;
    bool separator = left_length != 0 && left[left_length - 1] != '/';
    memset(output, 0, sizeof(*output));
    if (!mmay_size_add(left_length, separator ? 1 : 0, &length)
        || !mmay_size_add(length, right_length, &length)
        || length > MMAY_CLI_INIT_MAX_PATH
        || !mmay_size_add(length, 1, &allocation_size)) return false;
    bytes = mmay_allocate(&allocator, allocation_size);
    if (bytes == NULL) return false;
    memcpy(bytes, left, left_length);
    if (separator) bytes[left_length] = '/';
    memcpy(bytes + left_length + (separator ? 1 : 0), right, right_length);
    bytes[length] = 0;
    output->allocator = allocator;
    output->data = bytes;
    output->length = length;
    return true;
}

static bool mmay_cli_init_append(
    MarimayAllocator allocator,
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length,
    MarimayOwnedBytes *output
) {
    size_t length = 0;
    size_t allocation_size = 0;
    unsigned char *bytes;
    memset(output, 0, sizeof(*output));
    if (!mmay_size_add(left_length, right_length, &length)
        || length > MMAY_CLI_INIT_MAX_PATH
        || !mmay_size_add(length, 1, &allocation_size)) return false;
    bytes = mmay_allocate(&allocator, allocation_size);
    if (bytes == NULL) return false;
    memcpy(bytes, left, left_length);
    memcpy(bytes + left_length, right, right_length);
    bytes[length] = 0;
    output->allocator = allocator;
    output->data = bytes;
    output->length = length;
    return true;
}

static bool mmay_cli_init_destination(
    MarimayAllocator allocator,
    const char *input,
    MarimayOwnedBytes *destination
) {
    MarimayOwnedBytes current = {0};
    MarimayOwnedBytes joined = {0};
    MarimayPath normalized;
    MarimayFileStatus current_status;
    MarimayPathStatus path_status;
#if defined(_WIN32)
    MarimayPathFlavor flavor = MMAY_PATH_WINDOWS;
#else
    MarimayPathFlavor flavor = MMAY_PATH_POSIX;
#endif
    memset(&normalized, 0, sizeof(normalized));
    path_status = mmay_path_normalize(
        &normalized, allocator, flavor, input, strlen(input)
    );
    if (path_status != MMAY_PATH_OK) return false;
    if (normalized.absolute) {
        if (normalized.text.length > MMAY_CLI_INIT_MAX_PATH) {
            mmay_path_dispose(&normalized);
            return false;
        }
        *destination = normalized.text;
        memset(&normalized.text, 0, sizeof(normalized.text));
        mmay_path_dispose(&normalized);
        return true;
    }
    mmay_path_dispose(&normalized);
    memset(&normalized, 0, sizeof(normalized));
    current_status = mmay_directory_current(
        allocator, MMAY_CLI_INIT_MAX_PATH, &current
    );
    if (current_status != MMAY_FILE_OK) return false;
    if (!mmay_cli_init_join(
            allocator, (const char *)current.data, current.length,
            input, strlen(input), &joined
        )) goto cleanup;
    path_status = mmay_path_normalize(
        &normalized, allocator, flavor,
        (const char *)joined.data, joined.length
    );
    if (path_status != MMAY_PATH_OK || !normalized.absolute
        || normalized.text.length > MMAY_CLI_INIT_MAX_PATH) goto cleanup;
    *destination = normalized.text;
    memset(&normalized.text, 0, sizeof(normalized.text));
    mmay_path_dispose(&normalized);
    mmay_owned_bytes_dispose(&joined);
    mmay_owned_bytes_dispose(&current);
    return true;
cleanup:
    mmay_path_dispose(&normalized);
    mmay_owned_bytes_dispose(&joined);
    mmay_owned_bytes_dispose(&current);
    return false;
}

static bool mmay_cli_init_module_name(
    MarimayAllocator allocator,
    const MarimayOwnedBytes *destination,
    MarimayOwnedBytes *name
) {
    size_t start = destination->length;
    size_t length;
    size_t index;
    bool useful = false;
    while (start != 0 && destination->data[start - 1] != '/') start -= 1;
    length = destination->length - start;
    if (length == 0 || !mmay_owned_bytes_copy(
            name, allocator, destination->data + start, length
        )) return false;
    for (index = 0; index < name->length; index += 1) {
        unsigned char byte = name->data[index];
        bool allowed = (byte >= 'a' && byte <= 'z')
            || (byte >= 'A' && byte <= 'Z')
            || (byte >= '0' && byte <= '9')
            || byte == '_' || byte == '-' || byte == '.';
        if (!allowed) name->data[index] = '-';
        if (name->data[index] != '-' && name->data[index] != '.') useful = true;
    }
    if (!useful) {
        mmay_owned_bytes_dispose(name);
        return mmay_owned_bytes_copy(name, allocator, "app", 3);
    }
    return true;
}

static bool mmay_cli_init_manifest(
    MarimayAllocator allocator,
    const MarimayOwnedBytes *name,
    MarimayOwnedBytes *manifest
) {
    static const char prefix[] = "module \"local/";
    static const char suffix[] = "\"\n\nlanguage \"0.1\"\n";
    MarimayOwnedBytes partial = {0};
    bool ok = mmay_cli_init_append(
        allocator, prefix, sizeof(prefix) - 1,
        (const char *)name->data, name->length, &partial
    );
    if (ok) {
        ok = mmay_cli_init_append(
            allocator, (const char *)partial.data, partial.length,
            suffix, sizeof(suffix) - 1, manifest
        );
    }
    mmay_owned_bytes_dispose(&partial);
    return ok;
}

static MarimayFileStatus mmay_cli_init_write(
    MarimayAllocator allocator,
    const MarimayOwnedBytes *directory,
    const char *name,
    const unsigned char *bytes,
    size_t length,
    MarimayOwnedBytes *path
) {
    MarimayOwnedBytes temporary = {0};
    MarimayFileStatus status;
    if (!mmay_cli_init_join(
            allocator, (const char *)directory->data, directory->length,
            name, strlen(name), path
        ) || !mmay_cli_init_append(
            allocator, (const char *)path->data, path->length,
            ".writing", 8, &temporary
        )) {
        mmay_owned_bytes_dispose(path);
        return MMAY_FILE_ALLOCATION_FAILED;
    }
    status = mmay_file_replace_atomic(
        allocator, (const char *)temporary.data, temporary.length,
        (const char *)path->data, path->length, bytes, length
    );
    mmay_owned_bytes_dispose(&temporary);
    return status;
}

static void mmay_cli_init_cleanup(
    MarimayAllocator allocator,
    const MarimayOwnedBytes *temporary,
    MarimayOwnedBytes files[3],
    bool created
) {
    size_t index;
    if (created) {
        MarimayFileOps ops = mmay_platform_file_ops();
        for (index = 0; index < 3; index += 1) {
            if (files[index].data != NULL) {
                ops.remove(
                    ops.context, allocator,
                    (const char *)files[index].data, files[index].length
                );
            }
        }
        (void)mmay_directory_remove_empty(
            allocator, (const char *)temporary->data, temporary->length
        );
    }
}

int mmay_cli_init_project(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    const char *input = options->target != NULL
        ? options->target : "marimay-app";
    MarimayOwnedBytes destination = {0};
    MarimayOwnedBytes temporary = {0};
    MarimayOwnedBytes module_name = {0};
    MarimayOwnedBytes manifest = {0};
    MarimayOwnedBytes files[3] = {0};
    MarimayDirectoryEntryKind kind = MMAY_DIRECTORY_ENTRY_MISSING;
    MarimayFileStatus status = MMAY_FILE_IO_FAILED;
    bool temporary_created = false;
    bool published = false;
    size_t staging_attempt = 0;
    int exit_code = MMAY_EXIT_SOURCE_FAILURE;
    if (input[0] == '\0' || strlen(input) > MMAY_CLI_INIT_MAX_PATH) goto fail;
    status = MMAY_FILE_ALLOCATION_FAILED;
    if (!mmay_cli_init_destination(
            session->allocator, input, &destination
        )) goto fail;
    status = mmay_directory_kind(
        session->allocator, (const char *)destination.data,
        destination.length, &kind
    );
    if (status != MMAY_FILE_OK || kind != MMAY_DIRECTORY_ENTRY_MISSING) goto fail;
    for (staging_attempt = 0; staging_attempt < 128; staging_attempt += 1) {
        char suffix[48];
        int suffix_length = snprintf(
            suffix, sizeof(suffix), ".mmay-init-tmp-%zu", staging_attempt
        );
        if (suffix_length < 0 || (size_t)suffix_length >= sizeof(suffix)) {
            status = MMAY_FILE_LIMIT_EXCEEDED;
            goto fail;
        }
        mmay_owned_bytes_dispose(&temporary);
        status = MMAY_FILE_ALLOCATION_FAILED;
        if (!mmay_cli_init_append(
                session->allocator, (const char *)destination.data,
                destination.length, suffix, (size_t)suffix_length, &temporary
            )) goto fail;
        status = mmay_directory_create_exclusive(
            session->allocator, (const char *)temporary.data, temporary.length
        );
        if (status == MMAY_FILE_OK) {
            temporary_created = true;
            break;
        }
        if (status != MMAY_FILE_ALREADY_EXISTS) goto fail;
    }
    if (!temporary_created) goto fail;
    status = MMAY_FILE_ALLOCATION_FAILED;
    if (!mmay_cli_init_module_name(
            session->allocator, &destination, &module_name
        ) || !mmay_cli_init_manifest(
            session->allocator, &module_name, &manifest
        )) goto fail;
    status = mmay_cli_init_write(
        session->allocator, &temporary, "mmay.mod",
        manifest.data, manifest.length, &files[0]
    );
    if (status != MMAY_FILE_OK) goto fail;
    status = mmay_cli_init_write(
        session->allocator, &temporary, "main.my",
        mmay_cli_init_main, sizeof(mmay_cli_init_main) - 1, &files[1]
    );
    if (status != MMAY_FILE_OK) goto fail;
    status = mmay_cli_init_write(
        session->allocator, &temporary, "main_test.my",
        mmay_cli_init_test, sizeof(mmay_cli_init_test) - 1, &files[2]
    );
    if (status != MMAY_FILE_OK) goto fail;
    status = mmay_directory_publish(
        session->allocator,
        (const char *)temporary.data, temporary.length,
        (const char *)destination.data, destination.length
    );
    if (status != MMAY_FILE_OK) goto fail;
    published = true;
    if (!options->quiet && (!mmay_writer_write_cstr(
            standard_output, "initialized "
        ) || !mmay_writer_write(
            standard_output, (const char *)destination.data,
            destination.length
        ) || !mmay_writer_write_cstr(standard_output, "\n"))) {
        exit_code = MMAY_EXIT_INTERNAL;
        goto cleanup;
    }
    exit_code = MMAY_EXIT_SUCCESS;
    goto cleanup;
fail:
    if (status == MMAY_FILE_ALLOCATION_FAILED) {
        exit_code = mmay_cli_plain_error(
            standard_error, options->diagnostic_style,
            MMAY_DIAGNOSTIC_RUNTIME_FAILURE,
            "project initialization allocation failed",
            MMAY_EXIT_INTERNAL
        );
    } else {
        exit_code = mmay_cli_plain_error(
            standard_error, options->diagnostic_style,
            MMAY_DIAGNOSTIC_PROJECT_INIT_IO,
            kind != MMAY_DIRECTORY_ENTRY_MISSING
                || status == MMAY_FILE_ALREADY_EXISTS
                ? "project destination already exists"
                : "cannot atomically create the initialized project",
            MMAY_EXIT_SOURCE_FAILURE
        );
    }
cleanup:
    if (!published) mmay_cli_init_cleanup(
        session->allocator, &temporary, files, temporary_created
    );
    mmay_owned_bytes_dispose(&files[0]);
    mmay_owned_bytes_dispose(&files[1]);
    mmay_owned_bytes_dispose(&files[2]);
    mmay_owned_bytes_dispose(&manifest);
    mmay_owned_bytes_dispose(&module_name);
    mmay_owned_bytes_dispose(&temporary);
    mmay_owned_bytes_dispose(&destination);
    return exit_code;
}
