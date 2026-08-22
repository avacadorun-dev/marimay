#include "test_support.h"

#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/cli.h"
#include "mmay/session.h"

static int mmay_cli_test_run(
    MarimaySession *session,
    int count,
    const char *const arguments[],
    MarimayTestBuffer *output,
    MarimayTestBuffer *error
) {
    mmay_test_buffer_init(output);
    mmay_test_buffer_init(error);
    return mmay_cli_run(
        session, count, arguments, (MarimayReadStream){0},
        mmay_test_buffer_writer(output), mmay_test_buffer_writer(error)
    );
}

static bool mmay_cli_test_names_ordered(const char *text) {
    static const char *const names[] = {
        "sourceOrderFirst",
        "recordsOptionsResultsAndMatch",
        "exactOutputAndCleanup",
        "packagePrivateBoundary",
        "integrationUsesPublicSurface"
    };
    const char *cursor = text;
    size_t index = 0;
    for (index = 0; cursor != NULL
        && index < sizeof(names) / sizeof(names[0]); index += 1) {
        cursor = strstr(cursor, names[index]);
        if (cursor != NULL) cursor += strlen(names[index]);
    }
    return cursor != NULL;
}

static void mmay_cli_test_acceptance(MarimaySession *session) {
    const char *vm[] = {
        "mmay", "test", "checkpoint11-tree", "--engine", "vm"
    };
    const char *both[] = {
        "mmay", "test", "checkpoint11-tree", "--engine", "both"
    };
    const char *native[] = {
        "mmay", "test", "checkpoint11-tree", "--engine", "native"
    };
    const char *mismatch[] = {
        "mmay", "test", "checkpoint11-mismatch", "--engine", "both"
    };
    const char *private_import[] = {
        "mmay", "test", "checkpoint11-private", "--engine", "vm"
    };
    const char *invalid_engine[] = {
        "mmay", "test", "checkpoint11-tree", "--engine", "jit"
    };
    const char *empty[] = {
        "mmay", "test", "checkpoint11-alloc", "--engine", "vm"
    };
    const char *filter[] = {
        "mmay", "test", "checkpoint11-tree", "--filter=source"
    };
    const char *watch[] = {
        "mmay", "test", "checkpoint11-tree", "--watch"
    };
    const char *cache[] = {
        "mmay", "test", "checkpoint11-tree", "--no-cache"
    };
    const char *invalid_expect[] = {
        "mmay", "test", "checkpoint11-invalid-expect", "--engine", "vm"
    };
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    int status = mmay_cli_test_run(
        session, 5, vm, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(strstr(output.bytes, "5 passed; 0 failed") != NULL);
    MMAY_EXPECT(mmay_cli_test_names_ordered(output.bytes));
    MMAY_EXPECT(error.length == 0);

    status = mmay_cli_test_run(session, 5, both, &output, &error);
    MMAY_EXPECT(status == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(strstr(output.bytes, "5 passed; 0 failed") != NULL);
    MMAY_EXPECT(error.length == 0);

    status = mmay_cli_test_run(session, 5, native, &output, &error);
    MMAY_EXPECT(status == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(strstr(output.bytes, "5 passed; 0 failed") != NULL);
    MMAY_EXPECT(error.length == 0);

    status = mmay_cli_test_run(session, 5, mismatch, &output, &error);
    MMAY_EXPECT(status == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(strstr(output.bytes, "panic[G4004]") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "panic[G4000]: boom") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "expected 9 bytes") != NULL);
    MMAY_EXPECT(strstr(error.bytes, "B0007") == NULL);
    MMAY_EXPECT(error.length == 0);

    status = mmay_cli_test_run(
        session, 5, private_import, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT(strstr(error.bytes, "error[R0011]") != NULL);

    status = mmay_cli_test_run(
        session, 5, invalid_engine, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_USAGE);
    MMAY_EXPECT(strstr(error.bytes, "error[U0005]") != NULL);

    status = mmay_cli_test_run(session, 5, empty, &output, &error);
    MMAY_EXPECT(status == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(strstr(output.bytes, "0 passed; 0 failed") != NULL);

    status = mmay_cli_test_run(session, 4, filter, &output, &error);
    MMAY_EXPECT(status == MMAY_EXIT_USAGE);
    MMAY_EXPECT(strstr(error.bytes, "error[U0005]") != NULL);
    status = mmay_cli_test_run(session, 4, watch, &output, &error);
    MMAY_EXPECT(status == MMAY_EXIT_USAGE);
    MMAY_EXPECT(strstr(error.bytes, "error[U0005]") != NULL);
    status = mmay_cli_test_run(session, 4, cache, &output, &error);
    MMAY_EXPECT(status == MMAY_EXIT_USAGE);
    MMAY_EXPECT(strstr(error.bytes, "error[U0005]") != NULL);

    status = mmay_cli_test_run(
        session, 5, invalid_expect, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(strstr(error.bytes, "error[R0007]") != NULL);
    MMAY_EXPECT(strstr(error.bytes, "error[T0002]") != NULL);
    MMAY_EXPECT(strstr(error.bytes, "error[T0009]") != NULL);
}

void mmay_test_cli_test(void) {
    MarimaySession session;
    mmay_session_init(&session, mmay_system_allocator());
    mmay_cli_test_acceptance(&session);
}
