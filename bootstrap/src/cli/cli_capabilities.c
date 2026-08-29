#include "cli_internal.h"

#include <string.h>

#include "mmay/generated/diagnostic_codes.h"

bool mmay_cli_fs_capabilities(
    const MarimayCliOptions *options,
    MarimayCliFsCapabilities *capabilities
) {
    size_t index = 0;
    if (options == NULL || capabilities == NULL
        || options->allow_read_count > MMAY_CLI_MAX_FS_ROOTS
        || options->allow_write_count > MMAY_CLI_MAX_FS_ROOTS) return false;
    memset(capabilities, 0, sizeof(*capabilities));
    capabilities->options = mmay_fs_default_options();
    for (index = 0; index < options->allow_read_count; index += 1) {
        size_t length = options->allow_read[index] != NULL
            ? strlen(options->allow_read[index]) : 0;
        if (length == 0 || length > MMAY_FS_MAX_PATH_BYTES) return false;
        capabilities->read_roots[index] = (MarimayFsPathView){
            options->allow_read[index], length
        };
    }
    for (index = 0; index < options->allow_write_count; index += 1) {
        size_t length = options->allow_write[index] != NULL
            ? strlen(options->allow_write[index]) : 0;
        if (length == 0 || length > MMAY_FS_MAX_PATH_BYTES) return false;
        capabilities->write_roots[index] = (MarimayFsPathView){
            options->allow_write[index], length
        };
    }
    capabilities->options.read_roots = capabilities->read_roots;
    capabilities->options.read_root_count = options->allow_read_count;
    capabilities->options.write_roots = capabilities->write_roots;
    capabilities->options.write_root_count = options->allow_write_count;
    return true;
}

static int mmay_cli_validate_fs_root(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayFsPathView root,
    size_t argument_index,
    bool write,
    MarimayWriter error_writer
) {
    MarimayFsOptions fs_options = mmay_fs_default_options();
    MarimayFsContext context;
    MarimayFsStatus status;
    memset(&context, 0, sizeof(context));
    if (write) {
        fs_options.write_roots = &root;
        fs_options.write_root_count = 1;
    } else {
        fs_options.read_roots = &root;
        fs_options.read_root_count = 1;
    }
    status = mmay_fs_init_status(&context, session->allocator, &fs_options);
    if (status == MMAY_FS_OK) {
        mmay_fs_dispose(&context);
        return MMAY_EXIT_SUCCESS;
    }
    if (status == MMAY_FS_INVALID_ARGUMENT) {
        return mmay_cli_option_value_error(
            error_writer, options->diagnostic_style,
            MMAY_DIAGNOSTIC_INVALID_OPTION_VALUE, argument_index,
            "filesystem capability root is not a valid normalized host path"
        );
    }
    return MMAY_EXIT_INTERNAL;
}

int mmay_cli_validate_fs_capabilities(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayWriter error_writer
) {
    MarimayCliFsCapabilities capabilities;
    size_t read_index = 0;
    size_t write_index = 0;
    if (session == NULL || options == NULL
        || !mmay_cli_fs_capabilities(options, &capabilities)) {
        return MMAY_EXIT_INTERNAL;
    }
    while (read_index < options->allow_read_count
        || write_index < options->allow_write_count) {
        bool read = write_index >= options->allow_write_count
            || (read_index < options->allow_read_count
                && options->allow_read_index[read_index]
                    < options->allow_write_index[write_index]);
        int status;
        if (read) {
            status = mmay_cli_validate_fs_root(
                session, options, capabilities.read_roots[read_index],
                options->allow_read_index[read_index], false, error_writer
            );
            read_index += 1U;
        } else {
            status = mmay_cli_validate_fs_root(
                session, options, capabilities.write_roots[write_index],
                options->allow_write_index[write_index], true, error_writer
            );
            write_index += 1U;
        }
        if (status != MMAY_EXIT_SUCCESS) return status;
    }
    return MMAY_EXIT_SUCCESS;
}
