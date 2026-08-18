#include "test_support.h"

#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/cli.h"
#include "ruda/session.h"

static int ruda_cli_test_run(
    RudaSession *session,
    int count,
    const char *const arguments[],
    RudaTestBuffer *output,
    RudaTestBuffer *error
) {
    ruda_test_buffer_init(output);
    ruda_test_buffer_init(error);
    return ruda_cli_run(
        session, count, arguments, (RudaReadStream){0},
        ruda_test_buffer_writer(output), ruda_test_buffer_writer(error)
    );
}

static bool ruda_cli_test_names_ordered(const char *text) {
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

static void ruda_cli_test_acceptance(RudaSession *session) {
    const char *vm[] = {
        "ruda", "test", "checkpoint11-tree", "--engine", "vm"
    };
    const char *both[] = {
        "ruda", "test", "checkpoint11-tree", "--engine", "both"
    };
    const char *native[] = {
        "ruda", "test", "checkpoint11-tree", "--engine", "native"
    };
    const char *mismatch[] = {
        "ruda", "test", "checkpoint11-mismatch", "--engine", "both"
    };
    const char *private_import[] = {
        "ruda", "test", "checkpoint11-private", "--engine", "vm"
    };
    const char *invalid_engine[] = {
        "ruda", "test", "checkpoint11-tree", "--engine", "jit"
    };
    const char *empty[] = {
        "ruda", "test", "checkpoint11-alloc", "--engine", "vm"
    };
    const char *filter[] = {
        "ruda", "test", "checkpoint11-tree", "--filter=source"
    };
    const char *watch[] = {
        "ruda", "test", "checkpoint11-tree", "--watch"
    };
    const char *cache[] = {
        "ruda", "test", "checkpoint11-tree", "--no-cache"
    };
    const char *invalid_expect[] = {
        "ruda", "test", "checkpoint11-invalid-expect", "--engine", "vm"
    };
    RudaTestBuffer output;
    RudaTestBuffer error;
    int status = ruda_cli_test_run(
        session, 5, vm, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(strstr(output.bytes, "5 passed; 0 failed") != NULL);
    RUDA_EXPECT(ruda_cli_test_names_ordered(output.bytes));
    RUDA_EXPECT(error.length == 0);

    status = ruda_cli_test_run(session, 5, both, &output, &error);
    RUDA_EXPECT(status == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(strstr(output.bytes, "5 passed; 0 failed") != NULL);
    RUDA_EXPECT(error.length == 0);

    status = ruda_cli_test_run(session, 5, native, &output, &error);
    RUDA_EXPECT(status == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(strstr(output.bytes, "5 passed; 0 failed") != NULL);
    RUDA_EXPECT(error.length == 0);

    status = ruda_cli_test_run(session, 5, mismatch, &output, &error);
    RUDA_EXPECT(status == RUDA_EXIT_SOURCE_FAILURE);
    RUDA_EXPECT(strstr(output.bytes, "panic[G4004]") != NULL);
    RUDA_EXPECT(strstr(output.bytes, "panic[G4000]: boom") != NULL);
    RUDA_EXPECT(strstr(output.bytes, "expected 9 bytes") != NULL);
    RUDA_EXPECT(strstr(error.bytes, "B0007") == NULL);
    RUDA_EXPECT(error.length == 0);

    status = ruda_cli_test_run(
        session, 5, private_import, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_SOURCE_FAILURE);
    RUDA_EXPECT(output.length == 0);
    RUDA_EXPECT(strstr(error.bytes, "error[R0011]") != NULL);

    status = ruda_cli_test_run(
        session, 5, invalid_engine, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_USAGE);
    RUDA_EXPECT(strstr(error.bytes, "error[U0005]") != NULL);

    status = ruda_cli_test_run(session, 5, empty, &output, &error);
    RUDA_EXPECT(status == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(strstr(output.bytes, "0 passed; 0 failed") != NULL);

    status = ruda_cli_test_run(session, 4, filter, &output, &error);
    RUDA_EXPECT(status == RUDA_EXIT_USAGE);
    RUDA_EXPECT(strstr(error.bytes, "error[U0005]") != NULL);
    status = ruda_cli_test_run(session, 4, watch, &output, &error);
    RUDA_EXPECT(status == RUDA_EXIT_USAGE);
    RUDA_EXPECT(strstr(error.bytes, "error[U0005]") != NULL);
    status = ruda_cli_test_run(session, 4, cache, &output, &error);
    RUDA_EXPECT(status == RUDA_EXIT_USAGE);
    RUDA_EXPECT(strstr(error.bytes, "error[U0005]") != NULL);

    status = ruda_cli_test_run(
        session, 5, invalid_expect, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_SOURCE_FAILURE);
    RUDA_EXPECT(strstr(error.bytes, "error[R0007]") != NULL);
    RUDA_EXPECT(strstr(error.bytes, "error[T0002]") != NULL);
    RUDA_EXPECT(strstr(error.bytes, "error[T0009]") != NULL);
}

void ruda_test_cli_test(void) {
    RudaSession session;
    ruda_session_init(&session, ruda_system_allocator());
    ruda_cli_test_acceptance(&session);
}
