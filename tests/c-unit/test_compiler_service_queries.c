#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/check/service.h"

static const char ruda_query_store_source[] =
    "public type User { public id: int; }\n"
    "public type Order { public id: int; }\n"
    "public fn label(user: User): int { return user.id; }\n"
    "public fn score(user: User): int { return user.id; }\n"
    "fn secret(user: User): int { return user.id; }\n"
    "public fn score(order: Order): int { return order.id; }\n";

static const char ruda_query_type_source[] =
    "import { User } from \"acme/store\";\n"
    "fn typeOnly(user: User): int {\n"
    "  return user.label() + user.score();\n"
    "}\n";

static const char ruda_query_named_source[] =
    "import { User as NamedUser, score as freeScore } from \"acme/store\";\n"
    "fn named(user: NamedUser): int {\n"
    "  return user.label() + user.score() + freeScore(user);\n"
    "}\n";

static const char ruda_query_named_type_only_source[] =
    "import { User as NamedUser } from \"acme/store\";\n"
    "fn named(user: NamedUser): int {\n"
    "  return user.label() + user.score();\n"
    "}\n";

static const char ruda_query_namespace_source[] =
    "import * as store from \"acme/store\";\n"
    "fn namespaced(user: store.User): int {\n"
    "  return user.label() + user.score() + store.score(user);\n"
    "}\n";

typedef struct RudaQueryFixture {
    RudaCompilerService service;
    RudaFileId store;
    RudaFileId type_only;
    RudaFileId named;
    RudaFileId namespaced;
} RudaQueryFixture;

typedef struct RudaQueryCancel {
    size_t polls;
    size_t cancel_at;
} RudaQueryCancel;

static bool ruda_query_cancel_poll(void *context) {
    RudaQueryCancel *cancel = context;
    if (cancel == NULL) {
        return false;
    }
    cancel->polls += 1;
    return cancel->polls >= cancel->cancel_at;
}

static bool ruda_query_bytes_equal(
    const RudaOwnedBytes *bytes,
    const char *text
) {
    size_t length = text == NULL ? 0 : strlen(text);
    return bytes != NULL && text != NULL && bytes->length == length
        && (length == 0 || memcmp(bytes->data, text, length) == 0);
}

static bool ruda_query_owned_bytes_equal(
    const RudaOwnedBytes *left,
    const RudaOwnedBytes *right
) {
    return left != NULL && right != NULL && left->length == right->length
        && (left->length == 0
            || memcmp(left->data, right->data, left->length) == 0);
}

static bool ruda_query_fixture_init(
    RudaQueryFixture *fixture,
    RudaAllocator allocator,
    RudaCompilerOptions options
) {
    memset(fixture, 0, sizeof(*fixture));
    if (ruda_compiler_service_init(
            &fixture->service,
            allocator,
            options
        ) != RUDA_COMPILER_OK) {
        return false;
    }
    if (ruda_compiler_service_add_document(
            &fixture->service,
            "root/ns.ruda",
            strlen("root/ns.ruda"),
            1,
            ruda_query_namespace_source,
            strlen(ruda_query_namespace_source),
            &fixture->namespaced
        ) != RUDA_COMPILER_OK || ruda_compiler_service_add_document(
            &fixture->service,
            "store/store.ruda",
            strlen("store/store.ruda"),
            1,
            ruda_query_store_source,
            strlen(ruda_query_store_source),
            &fixture->store
        ) != RUDA_COMPILER_OK || ruda_compiler_service_add_document(
            &fixture->service,
            "root/type.ruda",
            strlen("root/type.ruda"),
            1,
            ruda_query_type_source,
            strlen(ruda_query_type_source),
            &fixture->type_only
        ) != RUDA_COMPILER_OK || ruda_compiler_service_add_document(
            &fixture->service,
            "root/named.ruda",
            strlen("root/named.ruda"),
            1,
            ruda_query_named_source,
            strlen(ruda_query_named_source),
            &fixture->named
        ) != RUDA_COMPILER_OK) {
        ruda_compiler_service_dispose(&fixture->service);
        memset(fixture, 0, sizeof(*fixture));
        return false;
    }
    return true;
}

static RudaCompilerStatus ruda_query_check_graph(
    const RudaQueryFixture *fixture,
    bool reverse,
    const RudaCompilerCheckRequest *request,
    RudaCompilerResult *result
) {
    RudaFileId store_files[1];
    RudaFileId root_files[3];
    RudaCompilerPackage packages[2];
    memset(packages, 0, sizeof(packages));
    store_files[0] = fixture->store;
    root_files[0] = reverse ? fixture->type_only : fixture->namespaced;
    root_files[1] = fixture->named;
    root_files[2] = reverse ? fixture->namespaced : fixture->type_only;
    packages[reverse ? 1 : 0].import_path = "acme/store";
    packages[reverse ? 1 : 0].import_path_length = strlen("acme/store");
    packages[reverse ? 1 : 0].file_ids = store_files;
    packages[reverse ? 1 : 0].file_count = 1;
    packages[reverse ? 0 : 1].import_path = "app/root";
    packages[reverse ? 0 : 1].import_path_length = strlen("app/root");
    packages[reverse ? 0 : 1].file_ids = root_files;
    packages[reverse ? 0 : 1].file_count = 3;
    return ruda_compiler_service_check_graph_request(
        &fixture->service,
        packages,
        2,
        "app/root",
        strlen("app/root"),
        request,
        result
    );
}

static void ruda_query_expect_invalid_graphs(
    const RudaQueryFixture *fixture
) {
    RudaFileId store_files[1] = {fixture->store};
    RudaFileId duplicate_root_files[3] = {
        fixture->type_only,
        fixture->named,
        fixture->named
    };
    RudaFileId incomplete_root_files[2] = {
        fixture->type_only,
        fixture->named
    };
    RudaCompilerPackage packages[2];
    RudaCompilerResult result;
    memset(packages, 0, sizeof(packages));
    memset(&result, 0, sizeof(result));
    packages[0].import_path = "acme/store";
    packages[0].import_path_length = strlen("acme/store");
    packages[0].file_ids = store_files;
    packages[0].file_count = 1;
    packages[1].import_path = "app/root";
    packages[1].import_path_length = strlen("app/root");
    packages[1].file_ids = duplicate_root_files;
    packages[1].file_count = 3;
    RUDA_EXPECT(ruda_compiler_service_check_graph(
        &fixture->service,
        packages,
        2,
        "app/root",
        strlen("app/root"),
        &result
    ) == RUDA_COMPILER_INVALID_ARGUMENT);
    RUDA_EXPECT(!result.initialized);
    packages[1].file_ids = incomplete_root_files;
    packages[1].file_count = 2;
    RUDA_EXPECT(ruda_compiler_service_check_graph(
        &fixture->service,
        packages,
        2,
        "app/root",
        strlen("app/root"),
        &result
    ) == RUDA_COMPILER_INVALID_ARGUMENT);
    RUDA_EXPECT(!result.initialized);
}

static const RudaCompilerUnit *ruda_query_unit(
    const RudaCompilerResult *result,
    const char *path
) {
    size_t index = 0;
    for (index = 0; result != NULL && index < result->units.length;
        index += 1) {
        const RudaCompilerUnit *unit = ruda_compiler_result_unit(
            result,
            index
        );
        if (unit != NULL && strcmp(
                ruda_source_path(unit->snapshot),
                path
            ) == 0) {
            return unit;
        }
    }
    return NULL;
}

static size_t ruda_query_find(
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

static const RudaCheckedSymbol *ruda_query_find_symbol(
    const RudaCheckResult *result,
    const char *package_name,
    const char *name,
    RudaCheckedSymbolKind kind
) {
    size_t index = 0;
    for (index = 0; result != NULL && index < result->symbols.length;
        index += 1) {
        const RudaCheckedSymbol *symbol = ruda_checked_symbol(result, index);
        if (symbol != NULL && symbol->kind == kind
            && ruda_query_bytes_equal(&symbol->package_name, package_name)
            && ruda_query_bytes_equal(&symbol->name, name)) {
            return symbol;
        }
    }
    return NULL;
}

static const RudaCheckedCompletion *ruda_query_find_completion(
    const RudaCheckResult *result,
    const RudaSourceSnapshot *snapshot,
    RudaSymbolId receiver,
    const char *name
) {
    size_t index = 0;
    size_t count = ruda_checked_completion_count(
        result,
        snapshot,
        receiver
    );
    for (index = 0; index < count; index += 1) {
        const RudaCheckedCompletion *completion = ruda_checked_completion(
            result,
            snapshot,
            receiver,
            index
        );
        if (completion != NULL
            && ruda_query_bytes_equal(&completion->name, name)) {
            return completion;
        }
    }
    return NULL;
}

static void ruda_query_expect_scope(
    const RudaCheckResult *result,
    const RudaSourceSnapshot *snapshot,
    RudaSymbolId receiver,
    const char *label_free,
    const char *score_free
) {
    const RudaCheckedCompletion *label = ruda_query_find_completion(
        result,
        snapshot,
        receiver,
        "label"
    );
    const RudaCheckedCompletion *score = ruda_query_find_completion(
        result,
        snapshot,
        receiver,
        "score"
    );
    const RudaCheckedSymbol *definition = NULL;
    size_t label_offset = ruda_query_find(snapshot, "user.label");
    size_t score_offset = ruda_query_find(snapshot, "user.score");
    RUDA_EXPECT(ruda_checked_completion_count(result, snapshot, receiver) == 2);
    RUDA_EXPECT(label != NULL && score != NULL);
    if (label == NULL || score == NULL) {
        return;
    }
    RUDA_EXPECT(ruda_query_bytes_equal(&label->package_name, "acme/store"));
    RUDA_EXPECT(ruda_query_bytes_equal(&score->package_name, "acme/store"));
    RUDA_EXPECT(ruda_query_bytes_equal(&label->free_spelling, label_free));
    RUDA_EXPECT(ruda_query_bytes_equal(&score->free_spelling, score_free));
    RUDA_EXPECT(label_offset != SIZE_MAX && score_offset != SIZE_MAX);
    if (label_offset == SIZE_MAX || score_offset == SIZE_MAX) {
        return;
    }
    label_offset += 5;
    score_offset += 5;
    definition = ruda_checked_definition_from_reference(
        result,
        snapshot,
        label_offset
    );
    RUDA_EXPECT(definition != NULL);
    if (definition != NULL) {
        RUDA_EXPECT(ruda_symbol_id_equal(definition->symbol, label->symbol));
    }
    definition = ruda_checked_definition_from_reference(
        result,
        snapshot,
        score_offset
    );
    RUDA_EXPECT(definition != NULL);
    if (definition != NULL) {
        RUDA_EXPECT(ruda_symbol_id_equal(definition->symbol, score->symbol));
    }
}

static void ruda_query_expect_deterministic(
    const RudaCheckResult *left,
    const RudaCheckResult *right
) {
    size_t index = 0;
    RUDA_EXPECT(left->occurrences.length == right->occurrences.length);
    RUDA_EXPECT(left->completions.length == right->completions.length);
    for (index = 0; index < left->occurrences.length
        && index < right->occurrences.length; index += 1) {
        const RudaCheckedOccurrence *left_item = ruda_checked_occurrence(
            left,
            index
        );
        const RudaCheckedOccurrence *right_item = ruda_checked_occurrence(
            right,
            index
        );
        RUDA_EXPECT(left_item != NULL && right_item != NULL);
        if (left_item != NULL && right_item != NULL) {
            RUDA_EXPECT(left_item->snapshot == right_item->snapshot);
            RUDA_EXPECT(left_item->start == right_item->start);
            RUDA_EXPECT(left_item->end == right_item->end);
            RUDA_EXPECT(ruda_symbol_id_equal(
                left_item->symbol,
                right_item->symbol
            ));
        }
    }
    for (index = 0; index < left->completions.length
        && index < right->completions.length; index += 1) {
        const RudaCheckedCompletion *left_item = ruda_vector_at_const(
            &left->completions,
            index
        );
        const RudaCheckedCompletion *right_item = ruda_vector_at_const(
            &right->completions,
            index
        );
        RUDA_EXPECT(left_item != NULL && right_item != NULL);
        if (left_item != NULL && right_item != NULL) {
            RUDA_EXPECT(left_item->snapshot == right_item->snapshot);
            RUDA_EXPECT(ruda_query_owned_bytes_equal(
                &left_item->name,
                &right_item->name
            ));
            RUDA_EXPECT(ruda_query_owned_bytes_equal(
                &left_item->package_name,
                &right_item->package_name
            ));
            RUDA_EXPECT(ruda_query_owned_bytes_equal(
                &left_item->free_spelling,
                &right_item->free_spelling
            ));
            RUDA_EXPECT(ruda_symbol_id_equal(
                left_item->symbol,
                right_item->symbol
            ));
            RUDA_EXPECT(ruda_symbol_id_equal(
                left_item->receiver_symbol,
                right_item->receiver_symbol
            ));
        }
    }
}

static void ruda_test_compiler_queries_import_matrix(void) {
    RudaQueryFixture fixture;
    RudaCompilerResult result;
    RudaCompilerResult reversed;
    RudaCompilerResult updated;
    RudaCompilerResult private_updated;
    RudaCompilerResult invalid;
    RudaCompilerResult cancelled;
    const RudaCheckedSymbol *user = NULL;
    const RudaCheckedSymbol *definition = NULL;
    const RudaCheckedCompletion *old_score = NULL;
    const RudaCheckedCompletion *new_score = NULL;
    const RudaCheckedSymbol *free_definition = NULL;
    const RudaCompilerUnit *type_unit = NULL;
    const RudaCompilerUnit *named_unit = NULL;
    const RudaCompilerUnit *namespace_unit = NULL;
    const RudaSourceSnapshot *old_named_snapshot = NULL;
    size_t count = 0;
    size_t offset = 0;
    RudaQueryCancel cancel;
    RudaCompilerCheckRequest request;
    char private_change_store[sizeof(ruda_query_store_source)];
    char private_store[sizeof(ruda_query_store_source)];
    char *secret_name = NULL;
    char *public_label = NULL;
    memset(&result, 0, sizeof(result));
    memset(&reversed, 0, sizeof(reversed));
    memset(&updated, 0, sizeof(updated));
    memset(&private_updated, 0, sizeof(private_updated));
    memset(&invalid, 0, sizeof(invalid));
    memset(&cancelled, 0, sizeof(cancelled));
    memset(&cancel, 0, sizeof(cancel));
    memset(&request, 0, sizeof(request));
    RUDA_EXPECT(ruda_query_fixture_init(
        &fixture,
        ruda_system_allocator(),
        ruda_compiler_default_options()
    ));
    ruda_query_expect_invalid_graphs(&fixture);
    RUDA_EXPECT(ruda_query_check_graph(&fixture, false, NULL, &result)
        == RUDA_COMPILER_OK);
    RUDA_EXPECT(result.initialized && !ruda_compiler_result_has_errors(&result));
    RUDA_EXPECT(ruda_compiler_result_is_current(&fixture.service, &result));
    user = ruda_query_find_symbol(
        &result.checked,
        "acme/store",
        "User",
        RUDA_CHECKED_SYMBOL_RECORD
    );
    type_unit = ruda_query_unit(&result, "root/type.ruda");
    named_unit = ruda_query_unit(&result, "root/named.ruda");
    namespace_unit = ruda_query_unit(&result, "root/ns.ruda");
    old_named_snapshot = named_unit == NULL ? NULL : named_unit->snapshot;
    RUDA_EXPECT(user != NULL && type_unit != NULL && named_unit != NULL
        && namespace_unit != NULL);
    if (user != NULL && type_unit != NULL && named_unit != NULL
        && namespace_unit != NULL) {
        ruda_query_expect_scope(
            &result.checked,
            type_unit->snapshot,
            user->symbol,
            "",
            ""
        );
        ruda_query_expect_scope(
            &result.checked,
            named_unit->snapshot,
            user->symbol,
            "",
            "freeScore"
        );
        ruda_query_expect_scope(
            &result.checked,
            namespace_unit->snapshot,
            user->symbol,
            "store.label",
            "store.score"
        );
        old_score = ruda_query_find_completion(
            &result.checked,
            named_unit->snapshot,
            user->symbol,
            "score"
        );
        offset = ruda_query_find(named_unit->snapshot, "freeScore(");
        free_definition = ruda_checked_definition_from_reference(
            &result.checked,
            named_unit->snapshot,
            offset
        );
        RUDA_EXPECT(old_score != NULL && free_definition != NULL);
        if (old_score != NULL && free_definition != NULL) {
            RUDA_EXPECT(ruda_symbol_id_equal(
                old_score->symbol,
                free_definition->symbol
            ));
        }
        offset = ruda_query_find(namespace_unit->snapshot, "store.score");
        RUDA_EXPECT(offset != SIZE_MAX);
        if (offset != SIZE_MAX) {
            offset += 6;
        }
        free_definition = ruda_checked_definition_from_reference(
            &result.checked,
            namespace_unit->snapshot,
            offset
        );
        RUDA_EXPECT(old_score != NULL && free_definition != NULL);
        if (old_score != NULL && free_definition != NULL) {
            RUDA_EXPECT(ruda_symbol_id_equal(
                old_score->symbol,
                free_definition->symbol
            ));
        }
        offset = ruda_query_find(type_unit->snapshot, "user.score");
        RUDA_EXPECT(offset != SIZE_MAX);
        if (offset != SIZE_MAX) {
            offset += 5;
        }
        RUDA_EXPECT(ruda_compiler_service_definition_from_reference(
            &fixture.service,
            &result,
            type_unit->snapshot,
            offset,
            &definition
        ) == RUDA_COMPILER_OK);
        RUDA_EXPECT(definition != NULL
            && ruda_query_bytes_equal(&definition->name, "score"));
        RUDA_EXPECT(ruda_compiler_service_completion_count(
            &fixture.service,
            &result,
            type_unit->snapshot,
            user->symbol,
            &count
        ) == RUDA_COMPILER_OK);
        RUDA_EXPECT(count == 2);
    }
    RUDA_EXPECT(ruda_checked_symbol_search_count(
        &result.checked,
        "s",
        1
    ) == 3);
    RUDA_EXPECT(ruda_compiler_service_symbol_search_count(
        &fixture.service,
        &result,
        "score",
        strlen("score"),
        &count
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(count == 2);

    RUDA_EXPECT(ruda_query_check_graph(&fixture, true, NULL, &reversed)
        == RUDA_COMPILER_OK);
    ruda_query_expect_deterministic(&result.checked, &reversed.checked);

    old_score = named_unit == NULL || user == NULL ? NULL
        : ruda_query_find_completion(
            &result.checked,
            named_unit->snapshot,
            user->symbol,
            "score"
        );
    RUDA_EXPECT(old_score != NULL
        && ruda_query_bytes_equal(&old_score->free_spelling, "freeScore"));
    RUDA_EXPECT(ruda_compiler_service_overlay(
        &fixture.service,
        fixture.named,
        2,
        ruda_query_named_type_only_source,
        strlen(ruda_query_named_type_only_source)
    ) == RUDA_COMPILER_OK);
    definition = (const RudaCheckedSymbol *)(uintptr_t)1;
    RUDA_EXPECT(ruda_compiler_service_definition_from_reference(
        &fixture.service,
        &result,
        type_unit->snapshot,
        offset,
        &definition
    ) == RUDA_COMPILER_STALE_RESULT);
    RUDA_EXPECT(definition == NULL);
    RUDA_EXPECT(ruda_query_check_graph(&fixture, false, NULL, &updated)
        == RUDA_COMPILER_OK);
    named_unit = ruda_query_unit(&updated, "root/named.ruda");
    new_score = named_unit == NULL || user == NULL ? NULL
        : ruda_query_find_completion(
            &updated.checked,
            named_unit->snapshot,
            user->symbol,
            "score"
        );
    RUDA_EXPECT(new_score != NULL
        && ruda_query_bytes_equal(&new_score->free_spelling, ""));
    RUDA_EXPECT(old_score != NULL
        && ruda_query_bytes_equal(&old_score->free_spelling, "freeScore"));
    if (old_named_snapshot != NULL && user != NULL) {
        count = SIZE_MAX;
        RUDA_EXPECT(ruda_compiler_service_completion_count(
            &fixture.service,
            &updated,
            old_named_snapshot,
            user->symbol,
            &count
        ) == RUDA_COMPILER_STALE_RESULT);
        RUDA_EXPECT(count == 0);
        definition = (const RudaCheckedSymbol *)(uintptr_t)1;
        RUDA_EXPECT(ruda_compiler_service_definition_from_reference(
            &fixture.service,
            &updated,
            old_named_snapshot,
            0,
            &definition
        ) == RUDA_COMPILER_STALE_RESULT);
        RUDA_EXPECT(definition == NULL);
    }

    memcpy(private_change_store, ruda_query_store_source,
        sizeof(ruda_query_store_source));
    secret_name = strstr(private_change_store, "secret");
    RUDA_EXPECT(secret_name != NULL);
    if (secret_name != NULL) {
        memcpy(secret_name, "hidden", strlen("hidden"));
    }
    RUDA_EXPECT(ruda_compiler_service_overlay(
        &fixture.service,
        fixture.store,
        2,
        private_change_store,
        strlen(private_change_store)
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_query_check_graph(
        &fixture,
        false,
        NULL,
        &private_updated
    ) == RUDA_COMPILER_OK);
    type_unit = ruda_query_unit(&private_updated, "root/type.ruda");
    RUDA_EXPECT(type_unit != NULL && user != NULL);
    if (type_unit != NULL && user != NULL) {
        ruda_query_expect_scope(
            &private_updated.checked,
            type_unit->snapshot,
            user->symbol,
            "",
            ""
        );
    }

    cancel.cancel_at = 1;
    request.cancel_poll = ruda_query_cancel_poll;
    request.cancel_context = &cancel;
    RUDA_EXPECT(ruda_query_check_graph(
        &fixture,
        false,
        &request,
        &cancelled
    ) == RUDA_COMPILER_CANCELLED);
    RUDA_EXPECT(!cancelled.initialized);

    memcpy(private_store, ruda_query_store_source,
        sizeof(ruda_query_store_source));
    public_label = strstr(private_store, "public fn label");
    RUDA_EXPECT(public_label != NULL);
    if (public_label != NULL) {
        memset(public_label, ' ', strlen("public "));
    }
    RUDA_EXPECT(ruda_compiler_service_overlay(
        &fixture.service,
        fixture.store,
        3,
        private_store,
        strlen(private_store)
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_query_check_graph(&fixture, false, NULL, &invalid)
        == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_result_has_errors(&invalid));
    RUDA_EXPECT(invalid.checked.symbols.length == 0);
    RUDA_EXPECT(invalid.checked.occurrences.length == 0);
    RUDA_EXPECT(invalid.checked.completions.length == 0);

    ruda_compiler_result_dispose(&invalid);
    ruda_compiler_result_dispose(&private_updated);
    ruda_compiler_result_dispose(&updated);
    ruda_compiler_result_dispose(&reversed);
    ruda_compiler_result_dispose(&result);
    ruda_compiler_service_dispose(&fixture.service);
}

static const char ruda_query_limit_source[] =
    "type User {}\n"
    "fn first(user: User): int { return 1; }\n"
    "fn second(user: User): int { return 2; }\n"
    "fn use(user: User): int { return user.first() + user.second(); }\n";

static RudaCompilerStatus ruda_query_check_single_graph(
    const RudaCompilerService *service,
    RudaFileId file,
    RudaCompilerResult *result
) {
    RudaCompilerPackage package;
    memset(&package, 0, sizeof(package));
    package.import_path = "limits/root";
    package.import_path_length = strlen("limits/root");
    package.file_ids = &file;
    package.file_count = 1;
    return ruda_compiler_service_check_graph(
        service,
        &package,
        1,
        "limits/root",
        strlen("limits/root"),
        result
    );
}

static void ruda_test_compiler_queries_limits(void) {
    RudaCompilerOptions options = ruda_compiler_default_options();
    RudaFailAllocator state;
    RudaAllocator allocator;
    RudaCompilerService service;
    RudaCompilerResult result;
    RudaFileId file = 0;
    memset(&service, 0, sizeof(service));
    memset(&result, 0, sizeof(result));
    ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
    allocator = ruda_fail_allocator(&state);
    options.check_options.max_completions = 2;
    RUDA_EXPECT(ruda_compiler_service_init(
        &service,
        allocator,
        options
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service,
        "limits.ruda",
        strlen("limits.ruda"),
        1,
        ruda_query_limit_source,
        strlen(ruda_query_limit_source),
        &file
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_query_check_single_graph(&service, file, &result)
        == RUDA_COMPILER_LIMIT_REACHED);
    RUDA_EXPECT(!result.initialized);
    ruda_compiler_service_dispose(&service);
    RUDA_EXPECT(state.live_allocations == 0);
    RUDA_EXPECT(state.live_bytes == 0);

    options = ruda_compiler_default_options();
    ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
    allocator = ruda_fail_allocator(&state);
    options.check_options.max_occurrences = 1;
    RUDA_EXPECT(ruda_compiler_service_init(
        &service,
        allocator,
        options
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_compiler_service_add_document(
        &service,
        "limits.ruda",
        strlen("limits.ruda"),
        1,
        ruda_query_limit_source,
        strlen(ruda_query_limit_source),
        &file
    ) == RUDA_COMPILER_OK);
    RUDA_EXPECT(ruda_query_check_single_graph(&service, file, &result)
        == RUDA_COMPILER_LIMIT_REACHED);
    RUDA_EXPECT(!result.initialized);
    ruda_compiler_service_dispose(&service);
    RUDA_EXPECT(state.live_allocations == 0);
    RUDA_EXPECT(state.live_bytes == 0);
}

static void ruda_test_compiler_queries_allocation_failures(void) {
    size_t relative_failure = 0;
    bool reached_success = false;
    for (relative_failure = 0; relative_failure < 1024;
        relative_failure += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaQueryFixture fixture;
        RudaCompilerResult result;
        RudaCompilerStatus status;
        memset(&fixture, 0, sizeof(fixture));
        memset(&result, 0, sizeof(result));
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_query_fixture_init(
            &fixture,
            allocator,
            ruda_compiler_default_options()
        ));
        state.failure_index = state.attempts + relative_failure;
        status = ruda_query_check_graph(&fixture, false, NULL, &result);
        if (status == RUDA_COMPILER_OK) {
            reached_success = true;
            RUDA_EXPECT(result.checked.completions.length == 13);
            RUDA_EXPECT(result.checked.occurrences.length != 0);
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

void ruda_test_compiler_service_queries(void) {
    ruda_test_compiler_queries_import_matrix();
    ruda_test_compiler_queries_limits();
    ruda_test_compiler_queries_allocation_failures();
}
