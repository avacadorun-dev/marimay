#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/check/service.h"

static const char mmay_receiver_service_store[] =
    "public type Stack {}\n"
    "public fn push(value: Stack, item: int): void {}\n"
    "public fn pop(value: Stack): int { return 1; }\n"
    "public fn length(value: Stack): int { return 2; }\n"
    "public fn clear(value: Stack): void {}\n";

static const char mmay_receiver_service_type_only[] =
    "import { Stack } from \"acme/store\";\n"
    "fn typeOnly(stack: Stack): int {\n"
    "  const _ = stack.push(1); const taken = stack.pop(); const _ = stack.clear();\n"
    "  return taken + stack.length();\n"
    "}\n";

static const char mmay_receiver_service_named[] =
    "import { Stack, clear as reset, length as size, pop as take, push as put } from \"acme/store\";\n"
    "fn named(stack: Stack): int {\n"
    "  put(stack, 1); const _ = stack.push(2);\n"
    "  const taken = take(stack) + stack.pop();\n"
    "  reset(stack); return taken + size(stack) + stack.length();\n"
    "}\n";

static const char mmay_receiver_service_namespace[] =
    "import * as store from \"acme/store\";\n"
    "fn namespaced(stack: store.Stack): int {\n"
    "  const _ = store.push(stack, 1); const _ = stack.push(2);\n"
    "  const taken = store.pop(stack) + stack.pop();\n"
    "  const _ = store.clear(stack);\n"
    "  return taken + store.length(stack) + stack.length();\n"
    "}\n";

typedef struct MarimayReceiverServiceFixture {
    MarimayCompilerService service;
    MarimayFileId store;
    MarimayFileId type_only;
    MarimayFileId named;
    MarimayFileId namespaced;
} MarimayReceiverServiceFixture;

static bool mmay_receiver_service_bytes_equal(
    const MarimayOwnedBytes *bytes,
    const char *text
) {
    size_t length = text == NULL ? 0 : strlen(text);
    return bytes != NULL && text != NULL && bytes->length == length
        && (length == 0 || memcmp(bytes->data, text, length) == 0);
}

static bool mmay_receiver_service_init(
    MarimayReceiverServiceFixture *fixture,
    MarimayAllocator allocator
) {
    memset(fixture, 0, sizeof(*fixture));
    if (mmay_compiler_service_init(
            &fixture->service,
            allocator,
            mmay_compiler_default_options()
        ) != MMAY_COMPILER_OK) {
        return false;
    }
    if (mmay_compiler_service_add_document(
            &fixture->service,
            "store/api.my",
            strlen("store/api.my"),
            1,
            mmay_receiver_service_store,
            strlen(mmay_receiver_service_store),
            &fixture->store
        ) != MMAY_COMPILER_OK || mmay_compiler_service_add_document(
            &fixture->service,
            "root/type.my",
            strlen("root/type.my"),
            1,
            mmay_receiver_service_type_only,
            strlen(mmay_receiver_service_type_only),
            &fixture->type_only
        ) != MMAY_COMPILER_OK || mmay_compiler_service_add_document(
            &fixture->service,
            "root/named.my",
            strlen("root/named.my"),
            1,
            mmay_receiver_service_named,
            strlen(mmay_receiver_service_named),
            &fixture->named
        ) != MMAY_COMPILER_OK || mmay_compiler_service_add_document(
            &fixture->service,
            "root/namespace.my",
            strlen("root/namespace.my"),
            1,
            mmay_receiver_service_namespace,
            strlen(mmay_receiver_service_namespace),
            &fixture->namespaced
        ) != MMAY_COMPILER_OK) {
        mmay_compiler_service_dispose(&fixture->service);
        memset(fixture, 0, sizeof(*fixture));
        return false;
    }
    return true;
}

static MarimayCompilerStatus mmay_receiver_service_check(
    MarimayReceiverServiceFixture *fixture,
    MarimayCompilerResult *result
) {
    MarimayFileId store_files[1] = {fixture->store};
    MarimayFileId root_files[3] = {
        fixture->type_only,
        fixture->named,
        fixture->namespaced
    };
    MarimayCompilerPackage packages[2];
    memset(packages, 0, sizeof(packages));
    packages[0].import_path = "acme/store";
    packages[0].import_path_length = strlen("acme/store");
    packages[0].file_ids = store_files;
    packages[0].file_count = 1;
    packages[1].import_path = "app/root";
    packages[1].import_path_length = strlen("app/root");
    packages[1].file_ids = root_files;
    packages[1].file_count = 3;
    return mmay_compiler_service_check_graph(
        &fixture->service,
        packages,
        2,
        "app/root",
        strlen("app/root"),
        result
    );
}

static const MarimayCompilerUnit *mmay_receiver_service_unit(
    const MarimayCompilerResult *result,
    const char *path
) {
    size_t index = 0;
    for (index = 0; result != NULL && index < result->units.length;
        index += 1) {
        const MarimayCompilerUnit *unit = mmay_compiler_result_unit(result, index);
        if (unit != NULL && strcmp(mmay_source_path(unit->snapshot), path) == 0) {
            return unit;
        }
    }
    return NULL;
}

static const MarimayCheckedSymbol *mmay_receiver_service_stack(
    const MarimayCheckResult *result
) {
    size_t index = 0;
    for (index = 0; result != NULL && index < result->symbols.length;
        index += 1) {
        const MarimayCheckedSymbol *symbol = mmay_checked_symbol(result, index);
        if (symbol != NULL && symbol->kind == MMAY_CHECKED_SYMBOL_RECORD
            && mmay_receiver_service_bytes_equal(
                &symbol->package_name, "acme/store"
            ) && mmay_receiver_service_bytes_equal(&symbol->name, "Stack")) {
            return symbol;
        }
    }
    return NULL;
}

static size_t mmay_receiver_service_find(
    const MarimaySourceSnapshot *snapshot,
    const char *needle
) {
    const unsigned char *bytes = mmay_source_bytes(snapshot);
    size_t length = mmay_source_length(snapshot);
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

static const MarimayCheckedCompletion *mmay_receiver_service_completion(
    const MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot,
    MarimaySymbolId receiver,
    const char *name
) {
    size_t index = 0;
    size_t count = mmay_checked_completion_count(result, snapshot, receiver);
    for (index = 0; index < count; index += 1) {
        const MarimayCheckedCompletion *completion = mmay_checked_completion(
            result, snapshot, receiver, index
        );
        if (completion != NULL && mmay_receiver_service_bytes_equal(
                &completion->name, name
            )) {
            return completion;
        }
    }
    return NULL;
}

static void mmay_receiver_service_expect_reference(
    MarimayReceiverServiceFixture *fixture,
    const MarimayCompilerResult *result,
    const MarimaySourceSnapshot *snapshot,
    const char *needle,
    size_t member_offset,
    MarimaySymbolId expected
) {
    const MarimayCheckedSymbol *definition = NULL;
    size_t offset = mmay_receiver_service_find(snapshot, needle);
    MMAY_EXPECT(offset != SIZE_MAX);
    if (offset == SIZE_MAX) {
        return;
    }
    MMAY_EXPECT(mmay_compiler_service_definition_from_reference(
        &fixture->service,
        result,
        snapshot,
        offset + member_offset,
        &definition
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(definition != NULL
        && mmay_symbol_id_equal(definition->symbol, expected));
}

static void mmay_receiver_service_expect_scope(
    const MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot,
    MarimaySymbolId stack,
    const char *push_spelling,
    const char *pop_spelling,
    const char *length_spelling,
    const char *clear_spelling
) {
    const MarimayCheckedCompletion *push = mmay_receiver_service_completion(
        result, snapshot, stack, "push"
    );
    const MarimayCheckedCompletion *pop = mmay_receiver_service_completion(
        result, snapshot, stack, "pop"
    );
    const MarimayCheckedCompletion *length = mmay_receiver_service_completion(
        result, snapshot, stack, "length"
    );
    const MarimayCheckedCompletion *clear = mmay_receiver_service_completion(
        result, snapshot, stack, "clear"
    );
    MMAY_EXPECT(mmay_checked_completion_count(result, snapshot, stack) == 4);
    MMAY_EXPECT(push != NULL && pop != NULL && length != NULL && clear != NULL);
    if (push != NULL && pop != NULL && length != NULL && clear != NULL) {
        MMAY_EXPECT(mmay_receiver_service_bytes_equal(
            &push->free_spelling, push_spelling
        ));
        MMAY_EXPECT(mmay_receiver_service_bytes_equal(
            &pop->free_spelling, pop_spelling
        ));
        MMAY_EXPECT(mmay_receiver_service_bytes_equal(
            &length->free_spelling, length_spelling
        ));
        MMAY_EXPECT(mmay_receiver_service_bytes_equal(
            &clear->free_spelling, clear_spelling
        ));
    }
}

static void mmay_test_receiver_collection_service_queries(void) {
    MarimayReceiverServiceFixture fixture;
    MarimayCompilerResult result;
    const MarimayCheckedSymbol *stack = NULL;
    const MarimayCheckedCompletion *push = NULL;
    const MarimayCompilerUnit *type_only = NULL;
    const MarimayCompilerUnit *named = NULL;
    const MarimayCompilerUnit *namespaced = NULL;
    size_t count = 0;
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_receiver_service_init(
        &fixture, mmay_system_allocator()
    ));
    MMAY_EXPECT(mmay_receiver_service_check(&fixture, &result)
        == MMAY_COMPILER_OK);
    MMAY_EXPECT(result.initialized && !mmay_compiler_result_has_errors(&result));
    stack = mmay_receiver_service_stack(&result.checked);
    type_only = mmay_receiver_service_unit(&result, "root/type.my");
    named = mmay_receiver_service_unit(&result, "root/named.my");
    namespaced = mmay_receiver_service_unit(&result, "root/namespace.my");
    MMAY_EXPECT(stack != NULL && type_only != NULL && named != NULL
        && namespaced != NULL);
    if (stack != NULL && type_only != NULL && named != NULL
        && namespaced != NULL) {
        mmay_receiver_service_expect_scope(
            &result.checked,
            type_only->snapshot,
            stack->symbol,
            "",
            "",
            "",
            ""
        );
        mmay_receiver_service_expect_scope(
            &result.checked,
            named->snapshot,
            stack->symbol,
            "put",
            "take",
            "size",
            "reset"
        );
        mmay_receiver_service_expect_scope(
            &result.checked,
            namespaced->snapshot,
            stack->symbol,
            "store.push",
            "store.pop",
            "store.length",
            "store.clear"
        );
        push = mmay_receiver_service_completion(
            &result.checked, named->snapshot, stack->symbol, "push"
        );
        MMAY_EXPECT(push != NULL);
        if (push != NULL) {
            mmay_receiver_service_expect_reference(
                &fixture,
                &result,
                type_only->snapshot,
                "stack.push",
                6,
                push->symbol
            );
            mmay_receiver_service_expect_reference(
                &fixture,
                &result,
                named->snapshot,
                "put(stack",
                0,
                push->symbol
            );
            mmay_receiver_service_expect_reference(
                &fixture,
                &result,
                named->snapshot,
                "stack.push",
                6,
                push->symbol
            );
            mmay_receiver_service_expect_reference(
                &fixture,
                &result,
                namespaced->snapshot,
                "store.push",
                6,
                push->symbol
            );
        }
        MMAY_EXPECT(mmay_compiler_service_completion_count(
            &fixture.service,
            &result,
            type_only->snapshot,
            stack->symbol,
            &count
        ) == MMAY_COMPILER_OK);
        MMAY_EXPECT(count == 4);
    }
    mmay_compiler_result_dispose(&result);
    mmay_compiler_service_dispose(&fixture.service);
}

static void mmay_test_receiver_collection_service_allocations(void) {
    size_t relative_failure = 0;
    bool reached_success = false;
    for (relative_failure = 0; relative_failure < 1024;
        relative_failure += 1) {
        MarimayFailAllocator state;
        MarimayReceiverServiceFixture fixture;
        MarimayCompilerResult result;
        MarimayCompilerStatus status;
        memset(&fixture, 0, sizeof(fixture));
        memset(&result, 0, sizeof(result));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), SIZE_MAX
        );
        MMAY_EXPECT(mmay_receiver_service_init(
            &fixture, mmay_fail_allocator(&state)
        ));
        state.failure_index = state.attempts + relative_failure;
        status = mmay_receiver_service_check(&fixture, &result);
        if (status == MMAY_COMPILER_OK) {
            reached_success = true;
            MMAY_EXPECT(result.initialized
                && !mmay_compiler_result_has_errors(&result));
        } else {
            MMAY_EXPECT(status == MMAY_COMPILER_ALLOCATION_FAILED);
            MMAY_EXPECT(!result.initialized);
        }
        mmay_compiler_result_dispose(&result);
        mmay_compiler_service_dispose(&fixture.service);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_receiver_collection_service(void) {
    mmay_test_receiver_collection_service_queries();
    mmay_test_receiver_collection_service_allocations();
}
