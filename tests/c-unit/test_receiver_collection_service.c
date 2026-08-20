#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/check/service.h"

static const char ruda_receiver_service_store[] =
    "public type Stack {}\n"
    "public fn push(value: Stack, item: int): void {}\n"
    "public fn pop(value: Stack): int { return 1; }\n"
    "public fn length(value: Stack): int { return 2; }\n"
    "public fn clear(value: Stack): void {}\n";

static const char ruda_receiver_service_type_only[] =
    "import { Stack } from \"acme/store\";\n"
    "fn typeOnly(stack: Stack): int {\n"
    "  stack.push(1); const taken = stack.pop(); stack.clear();\n"
    "  return taken + stack.length();\n"
    "}\n";

static const char ruda_receiver_service_named[] =
    "import { Stack, clear as reset, length as size, pop as take, push as put } "
    "from \"acme/store\";\n"
    "fn named(stack: Stack): int {\n"
    "  put(stack, 1); stack.push(2);\n"
    "  const taken = take(stack) + stack.pop();\n"
    "  reset(stack); return taken + size(stack) + stack.length();\n"
    "}\n";

static const char ruda_receiver_service_namespace[] =
    "import * as store from \"acme/store\";\n"
    "fn namespaced(stack: store.Stack): int {\n"
    "  store.push(stack, 1); stack.push(2);\n"
    "  const taken = store.pop(stack) + stack.pop();\n"
    "  store.clear(stack);\n"
    "  return taken + store.length(stack) + stack.length();\n"
    "}\n";

typedef struct RudaReceiverServiceFixture {
    RudaCompilerService service;
    RudaFileId store;
    RudaFileId type_only;
    RudaFileId named;
    RudaFileId namespaced;
} RudaReceiverServiceFixture;

static bool ruda_receiver_service_bytes_equal(
    const RudaOwnedBytes *bytes,
    const char *text
) {
    size_t length = text == NULL ? 0 : strlen(text);
    return bytes != NULL && text != NULL && bytes->length == length
        && (length == 0 || memcmp(bytes->data, text, length) == 0);
}

static bool ruda_receiver_service_init(
    RudaReceiverServiceFixture *fixture,
    RudaAllocator allocator
) {
    memset(fixture, 0, sizeof(*fixture));
    if (ruda_compiler_service_init(
            &fixture->service,
            allocator,
            ruda_compiler_default_options()
        ) != RUDA_COMPILER_OK) {
        return false;
    }
    if (ruda_compiler_service_add_document(
            &fixture->service,
            "store/api.ruda",
            strlen("store/api.ruda"),
            1,
            ruda_receiver_service_store,
            strlen(ruda_receiver_service_store),
            &fixture->store
        ) != RUDA_COMPILER_OK || ruda_compiler_service_add_document(
            &fixture->service,
            "root/type.ruda",
            strlen("root/type.ruda"),
            1,
            ruda_receiver_service_type_only,
            strlen(ruda_receiver_service_type_only),
            &fixture->type_only
        ) != RUDA_COMPILER_OK || ruda_compiler_service_add_document(
            &fixture->service,
            "root/named.ruda",
            strlen("root/named.ruda"),
            1,
            ruda_receiver_service_named,
            strlen(ruda_receiver_service_named),
            &fixture->named
        ) != RUDA_COMPILER_OK || ruda_compiler_service_add_document(
            &fixture->service,
            "root/namespace.ruda",
            strlen("root/namespace.ruda"),
            1,
            ruda_receiver_service_namespace,
            strlen(ruda_receiver_service_namespace),
            &fixture->namespaced
        ) != RUDA_COMPILER_OK) {
        ruda_compiler_service_dispose(&fixture->service);
        memset(fixture, 0, sizeof(*fixture));
        return false;
    }
    return true;
}

static RudaCompilerStatus ruda_receiver_service_check(
    RudaReceiverServiceFixture *fixture,
    RudaCompilerResult *result
) {
    RudaFileId store_files[1] = {fixture->store};
    RudaFileId root_files[3] = {
        fixture->type_only,
        fixture->named,
        fixture->namespaced
    };
    RudaCompilerPackage packages[2];
    memset(packages, 0, sizeof(packages));
    packages[0].import_path = "acme/store";
    packages[0].import_path_length = strlen("acme/store");
    packages[0].file_ids = store_files;
    packages[0].file_count = 1;
    packages[1].import_path = "app/root";
    packages[1].import_path_length = strlen("app/root");
    packages[1].file_ids = root_files;
    packages[1].file_count = 3;
    return ruda_compiler_service_check_graph(
        &fixture->service,
        packages,
        2,
        "app/root",
        strlen("app/root"),
        result
    );
}

static const RudaCompilerUnit *ruda_receiver_service_unit(
    const RudaCompilerResult *result,
    const char *path
) {
    size_t index = 0;
    for (index = 0; result != NULL && index < result->units.length;
        index += 1) {
        const RudaCompilerUnit *unit = ruda_compiler_result_unit(result, index);
        if (unit != NULL && strcmp(ruda_source_path(unit->snapshot), path) == 0) {
            return unit;
        }
    }
    return NULL;
}

static const RudaCheckedSymbol *ruda_receiver_service_stack(
    const RudaCheckResult *result
) {
    size_t index = 0;
    for (index = 0; result != NULL && index < result->symbols.length;
        index += 1) {
        const RudaCheckedSymbol *symbol = ruda_checked_symbol(result, index);
        if (symbol != NULL && symbol->kind == RUDA_CHECKED_SYMBOL_RECORD
            && ruda_receiver_service_bytes_equal(
                &symbol->package_name, "acme/store"
            ) && ruda_receiver_service_bytes_equal(&symbol->name, "Stack")) {
            return symbol;
        }
    }
    return NULL;
}

static size_t ruda_receiver_service_find(
    const RudaSourceSnapshot *snapshot,
    const char *needle
) {
    const unsigned char *bytes = ruda_source_bytes(snapshot);
    size_t length = ruda_source_length(snapshot);
    size_t needle_length = strlen(needle);
    size_t index = 0;
    for (index = 0; needle_length <= length
        && index <= length - needle_length; index += 1) {
        if (memcmp(bytes + index, needle, needle_length) == 0) {
            return index;
        }
    }
    return SIZE_MAX;
}

static const RudaCheckedCompletion *ruda_receiver_service_completion(
    const RudaCheckResult *result,
    const RudaSourceSnapshot *snapshot,
    RudaSymbolId receiver,
    const char *name
) {
    size_t index = 0;
    size_t count = ruda_checked_completion_count(result, snapshot, receiver);
    for (index = 0; index < count; index += 1) {
        const RudaCheckedCompletion *completion = ruda_checked_completion(
            result, snapshot, receiver, index
        );
        if (completion != NULL && ruda_receiver_service_bytes_equal(
                &completion->name, name
            )) {
            return completion;
        }
    }
    return NULL;
}

static void ruda_receiver_service_expect_reference(
    RudaReceiverServiceFixture *fixture,
    const RudaCompilerResult *result,
    const RudaSourceSnapshot *snapshot,
    const char *needle,
    size_t member_offset,
    RudaSymbolId expected
) {
    const RudaCheckedSymbol *definition = NULL;
    size_t offset = ruda_receiver_service_find(snapshot, needle);
    RUDA_EXPECT(offset != SIZE_MAX);
    if (offset == SIZE_MAX) {
        return;
    }
    RUDA_EXPECT(ruda_compiler_service_definition_from_reference(
        &fixture->service,
        result,
        snapshot,
        offset + member_offset,
        &definition
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(definition != NULL
        && ruda_symbol_id_equal(definition->symbol, expected));
}

static void ruda_receiver_service_expect_scope(
    const RudaCheckResult *result,
    const RudaSourceSnapshot *snapshot,
    RudaSymbolId stack,
    const char *push_spelling,
    const char *pop_spelling,
    const char *length_spelling,
    const char *clear_spelling
) {
    const RudaCheckedCompletion *push = ruda_receiver_service_completion(
        result, snapshot, stack, "push"
    );
    const RudaCheckedCompletion *pop = ruda_receiver_service_completion(
        result, snapshot, stack, "pop"
    );
    const RudaCheckedCompletion *length = ruda_receiver_service_completion(
        result, snapshot, stack, "length"
    );
    const RudaCheckedCompletion *clear = ruda_receiver_service_completion(
        result, snapshot, stack, "clear"
    );
    RUDA_EXPECT(ruda_checked_completion_count(result, snapshot, stack) == 4);
    RUDA_EXPECT(push != NULL && pop != NULL && length != NULL && clear != NULL);
    if (push != NULL && pop != NULL && length != NULL && clear != NULL) {
        RUDA_EXPECT(ruda_receiver_service_bytes_equal(
            &push->free_spelling, push_spelling
        ));
        RUDA_EXPECT(ruda_receiver_service_bytes_equal(
            &pop->free_spelling, pop_spelling
        ));
        RUDA_EXPECT(ruda_receiver_service_bytes_equal(
            &length->free_spelling, length_spelling
        ));
        RUDA_EXPECT(ruda_receiver_service_bytes_equal(
            &clear->free_spelling, clear_spelling
        ));
    }
}

static void ruda_test_receiver_collection_service_queries(void) {
    RudaReceiverServiceFixture fixture;
    RudaCompilerResult result;
    const RudaCheckedSymbol *stack = NULL;
    const RudaCheckedCompletion *push = NULL;
    const RudaCompilerUnit *type_only = NULL;
    const RudaCompilerUnit *named = NULL;
    const RudaCompilerUnit *namespaced = NULL;
    size_t count = 0;
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(ruda_receiver_service_init(
        &fixture, ruda_system_allocator()
    ));
    RUDA_EXPECT(ruda_receiver_service_check(&fixture, &result)
        == RUDA_COMPILER_OK);
    RUDA_EXPECT(result.initialized && !ruda_compiler_result_has_errors(&result));
    stack = ruda_receiver_service_stack(&result.checked);
    type_only = ruda_receiver_service_unit(&result, "root/type.ruda");
    named = ruda_receiver_service_unit(&result, "root/named.ruda");
    namespaced = ruda_receiver_service_unit(&result, "root/namespace.ruda");
    RUDA_EXPECT(stack != NULL && type_only != NULL && named != NULL
        && namespaced != NULL);
    if (stack != NULL && type_only != NULL && named != NULL
        && namespaced != NULL) {
        ruda_receiver_service_expect_scope(
            &result.checked,
            type_only->snapshot,
            stack->symbol,
            "",
            "",
            "",
            ""
        );
        ruda_receiver_service_expect_scope(
            &result.checked,
            named->snapshot,
            stack->symbol,
            "put",
            "take",
            "size",
            "reset"
        );
        ruda_receiver_service_expect_scope(
            &result.checked,
            namespaced->snapshot,
            stack->symbol,
            "store.push",
            "store.pop",
            "store.length",
            "store.clear"
        );
        push = ruda_receiver_service_completion(
            &result.checked, named->snapshot, stack->symbol, "push"
        );
        RUDA_EXPECT(push != NULL);
        if (push != NULL) {
            ruda_receiver_service_expect_reference(
                &fixture,
                &result,
                type_only->snapshot,
                "stack.push",
                6,
                push->symbol
            );
            ruda_receiver_service_expect_reference(
                &fixture,
                &result,
                named->snapshot,
                "put(stack",
                0,
                push->symbol
            );
            ruda_receiver_service_expect_reference(
                &fixture,
                &result,
                named->snapshot,
                "stack.push",
                6,
                push->symbol
            );
            ruda_receiver_service_expect_reference(
                &fixture,
                &result,
                namespaced->snapshot,
                "store.push",
                6,
                push->symbol
            );
        }
        RUDA_EXPECT(ruda_compiler_service_completion_count(
            &fixture.service,
            &result,
            type_only->snapshot,
            stack->symbol,
            &count
        ) == RUDA_COMPILER_OK);
        RUDA_EXPECT(count == 4);
    }
    ruda_compiler_result_dispose(&result);
    ruda_compiler_service_dispose(&fixture.service);
}

static void ruda_test_receiver_collection_service_allocations(void) {
    size_t relative_failure = 0;
    bool reached_success = false;
    for (relative_failure = 0; relative_failure < 1024;
        relative_failure += 1) {
        RudaFailAllocator state;
        RudaReceiverServiceFixture fixture;
        RudaCompilerResult result;
        RudaCompilerStatus status;
        memset(&fixture, 0, sizeof(fixture));
        memset(&result, 0, sizeof(result));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), SIZE_MAX
        );
        RUDA_EXPECT(ruda_receiver_service_init(
            &fixture, ruda_fail_allocator(&state)
        ));
        state.failure_index = state.attempts + relative_failure;
        status = ruda_receiver_service_check(&fixture, &result);
        if (status == RUDA_COMPILER_OK) {
            reached_success = true;
            RUDA_EXPECT(result.initialized
                && !ruda_compiler_result_has_errors(&result));
        } else {
            RUDA_EXPECT(status == RUDA_COMPILER_ALLOCATION_FAILED);
            RUDA_EXPECT(!result.initialized);
        }
        ruda_compiler_result_dispose(&result);
        ruda_compiler_service_dispose(&fixture.service);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_receiver_collection_service(void) {
    ruda_test_receiver_collection_service_queries();
    ruda_test_receiver_collection_service_allocations();
}
