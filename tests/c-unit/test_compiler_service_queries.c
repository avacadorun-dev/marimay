#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/check/service.h"

static const char mmay_query_store_source[] =
    "public type User { public id: int, }\n"
    "public type Order { public id: int, }\n"
    "public fn label(user: User): int { return user.id; }\n"
    "public fn score(user: User): int { return user.id; }\n"
    "fn secret(user: User): int { return user.id; }\n"
    "public fn score(order: Order): int { return order.id; }\n";

static const char mmay_query_type_source[] =
    "import { User } from \"acme/store\";\n"
    "fn typeOnly(user: User): int {\n"
    "  return user.label() + user.score();\n"
    "}\n";

static const char mmay_query_named_source[] =
    "import { User as NamedUser, score as freeScore } from \"acme/store\";\n"
    "fn named(user: NamedUser): int {\n"
    "  return user.label() + user.score() + freeScore(user);\n"
    "}\n";

static const char mmay_query_named_type_only_source[] =
    "import { User as NamedUser } from \"acme/store\";\n"
    "fn named(user: NamedUser): int {\n"
    "  return user.label() + user.score();\n"
    "}\n";

static const char mmay_query_namespace_source[] =
    "import * as store from \"acme/store\";\n"
    "fn namespaced(user: store.User): int {\n"
    "  return user.label() + user.score() + store.score(user);\n"
    "}\n";

typedef struct MarimayQueryFixture {
    MarimayCompilerService service;
    MarimayFileId store;
    MarimayFileId type_only;
    MarimayFileId named;
    MarimayFileId namespaced;
} MarimayQueryFixture;

typedef struct MarimayQueryCancel {
    size_t polls;
    size_t cancel_at;
} MarimayQueryCancel;

static bool mmay_query_cancel_poll(void *context) {
    MarimayQueryCancel *cancel = context;
    if (cancel == NULL) {
        return false;
    }
    cancel->polls += 1;
    return cancel->polls >= cancel->cancel_at;
}

static bool mmay_query_bytes_equal(
    const MarimayOwnedBytes *bytes,
    const char *text
) {
    size_t length = text == NULL ? 0 : strlen(text);
    return bytes != NULL && text != NULL && bytes->length == length
        && (length == 0 || memcmp(bytes->data, text, length) == 0);
}

static bool mmay_query_owned_bytes_equal(
    const MarimayOwnedBytes *left,
    const MarimayOwnedBytes *right
) {
    return left != NULL && right != NULL && left->length == right->length
        && (left->length == 0
            || memcmp(left->data, right->data, left->length) == 0);
}

static bool mmay_query_fixture_init(
    MarimayQueryFixture *fixture,
    MarimayAllocator allocator,
    MarimayCompilerOptions options
) {
    memset(fixture, 0, sizeof(*fixture));
    if (mmay_compiler_service_init(
            &fixture->service,
            allocator,
            options
        ) != MMAY_COMPILER_OK) {
        return false;
    }
    if (mmay_compiler_service_add_document(
            &fixture->service,
            "root/ns.my",
            strlen("root/ns.my"),
            1,
            mmay_query_namespace_source,
            strlen(mmay_query_namespace_source),
            &fixture->namespaced
        ) != MMAY_COMPILER_OK || mmay_compiler_service_add_document(
            &fixture->service,
            "store/store.my",
            strlen("store/store.my"),
            1,
            mmay_query_store_source,
            strlen(mmay_query_store_source),
            &fixture->store
        ) != MMAY_COMPILER_OK || mmay_compiler_service_add_document(
            &fixture->service,
            "root/type.my",
            strlen("root/type.my"),
            1,
            mmay_query_type_source,
            strlen(mmay_query_type_source),
            &fixture->type_only
        ) != MMAY_COMPILER_OK || mmay_compiler_service_add_document(
            &fixture->service,
            "root/named.my",
            strlen("root/named.my"),
            1,
            mmay_query_named_source,
            strlen(mmay_query_named_source),
            &fixture->named
        ) != MMAY_COMPILER_OK) {
        mmay_compiler_service_dispose(&fixture->service);
        memset(fixture, 0, sizeof(*fixture));
        return false;
    }
    return true;
}

static MarimayCompilerStatus mmay_query_check_graph(
    const MarimayQueryFixture *fixture,
    bool reverse,
    const MarimayCompilerCheckRequest *request,
    MarimayCompilerResult *result
) {
    MarimayFileId store_files[1];
    MarimayFileId root_files[3];
    MarimayCompilerPackage packages[2];
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
    return mmay_compiler_service_check_graph_request(
        &fixture->service,
        packages,
        2,
        "app/root",
        strlen("app/root"),
        request,
        result
    );
}

static void mmay_query_expect_invalid_graphs(
    const MarimayQueryFixture *fixture
) {
    MarimayFileId store_files[1] = {fixture->store};
    MarimayFileId duplicate_root_files[3] = {
        fixture->type_only,
        fixture->named,
        fixture->named
    };
    MarimayFileId incomplete_root_files[2] = {
        fixture->type_only,
        fixture->named
    };
    MarimayCompilerPackage packages[2];
    MarimayCompilerResult result;
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
    MMAY_EXPECT(mmay_compiler_service_check_graph(
        &fixture->service,
        packages,
        2,
        "app/root",
        strlen("app/root"),
        &result
    ) == MMAY_COMPILER_INVALID_ARGUMENT);
    MMAY_EXPECT(!result.initialized);
    packages[1].file_ids = incomplete_root_files;
    packages[1].file_count = 2;
    MMAY_EXPECT(mmay_compiler_service_check_graph(
        &fixture->service,
        packages,
        2,
        "app/root",
        strlen("app/root"),
        &result
    ) == MMAY_COMPILER_INVALID_ARGUMENT);
    MMAY_EXPECT(!result.initialized);
}

static const MarimayCompilerUnit *mmay_query_unit(
    const MarimayCompilerResult *result,
    const char *path
) {
    size_t index = 0;
    for (index = 0; result != NULL && index < result->units.length;
        index += 1) {
        const MarimayCompilerUnit *unit = mmay_compiler_result_unit(
            result,
            index
        );
        if (unit != NULL && strcmp(
                mmay_source_path(unit->snapshot),
                path
            ) == 0) {
            return unit;
        }
    }
    return NULL;
}

static size_t mmay_query_find(
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

static const MarimayCheckedSymbol *mmay_query_find_symbol(
    const MarimayCheckResult *result,
    const char *package_name,
    const char *name,
    MarimayCheckedSymbolKind kind
) {
    size_t index = 0;
    for (index = 0; result != NULL && index < result->symbols.length;
        index += 1) {
        const MarimayCheckedSymbol *symbol = mmay_checked_symbol(result, index);
        if (symbol != NULL && symbol->kind == kind
            && mmay_query_bytes_equal(&symbol->package_name, package_name)
            && mmay_query_bytes_equal(&symbol->name, name)) {
            return symbol;
        }
    }
    return NULL;
}

static const MarimayCheckedCompletion *mmay_query_find_completion(
    const MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot,
    MarimaySymbolId receiver,
    const char *name
) {
    size_t index = 0;
    size_t count = mmay_checked_completion_count(
        result,
        snapshot,
        receiver
    );
    for (index = 0; index < count; index += 1) {
        const MarimayCheckedCompletion *completion = mmay_checked_completion(
            result,
            snapshot,
            receiver,
            index
        );
        if (completion != NULL
            && mmay_query_bytes_equal(&completion->name, name)) {
            return completion;
        }
    }
    return NULL;
}

static void mmay_query_expect_scope(
    const MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot,
    MarimaySymbolId receiver,
    const char *label_free,
    const char *score_free
) {
    const MarimayCheckedCompletion *label = mmay_query_find_completion(
        result,
        snapshot,
        receiver,
        "label"
    );
    const MarimayCheckedCompletion *score = mmay_query_find_completion(
        result,
        snapshot,
        receiver,
        "score"
    );
    const MarimayCheckedSymbol *definition = NULL;
    size_t label_offset = mmay_query_find(snapshot, "user.label");
    size_t score_offset = mmay_query_find(snapshot, "user.score");
    MMAY_EXPECT(mmay_checked_completion_count(result, snapshot, receiver) == 2);
    MMAY_EXPECT(label != NULL && score != NULL);
    if (label == NULL || score == NULL) {
        return;
    }
    MMAY_EXPECT(mmay_query_bytes_equal(&label->package_name, "acme/store"));
    MMAY_EXPECT(mmay_query_bytes_equal(&score->package_name, "acme/store"));
    MMAY_EXPECT(mmay_query_bytes_equal(&label->free_spelling, label_free));
    MMAY_EXPECT(mmay_query_bytes_equal(&score->free_spelling, score_free));
    MMAY_EXPECT(label_offset != SIZE_MAX && score_offset != SIZE_MAX);
    if (label_offset == SIZE_MAX || score_offset == SIZE_MAX) {
        return;
    }
    label_offset += 5;
    score_offset += 5;
    definition = mmay_checked_definition_from_reference(
        result,
        snapshot,
        label_offset
    );
    MMAY_EXPECT(definition != NULL);
    if (definition != NULL) {
        MMAY_EXPECT(mmay_symbol_id_equal(definition->symbol, label->symbol));
    }
    definition = mmay_checked_definition_from_reference(
        result,
        snapshot,
        score_offset
    );
    MMAY_EXPECT(definition != NULL);
    if (definition != NULL) {
        MMAY_EXPECT(mmay_symbol_id_equal(definition->symbol, score->symbol));
    }
}

static void mmay_query_expect_deterministic(
    const MarimayCheckResult *left,
    const MarimayCheckResult *right
) {
    size_t index = 0;
    MMAY_EXPECT(left->occurrences.length == right->occurrences.length);
    MMAY_EXPECT(left->completions.length == right->completions.length);
    for (index = 0; index < left->occurrences.length
        && index < right->occurrences.length; index += 1) {
        const MarimayCheckedOccurrence *left_item = mmay_checked_occurrence(
            left,
            index
        );
        const MarimayCheckedOccurrence *right_item = mmay_checked_occurrence(
            right,
            index
        );
        MMAY_EXPECT(left_item != NULL && right_item != NULL);
        if (left_item != NULL && right_item != NULL) {
            MMAY_EXPECT(left_item->snapshot == right_item->snapshot);
            MMAY_EXPECT(left_item->start == right_item->start);
            MMAY_EXPECT(left_item->end == right_item->end);
            MMAY_EXPECT(mmay_symbol_id_equal(
                left_item->symbol,
                right_item->symbol
            ));
        }
    }
    for (index = 0; index < left->completions.length
        && index < right->completions.length; index += 1) {
        const MarimayCheckedCompletion *left_item = mmay_vector_at_const(
            &left->completions,
            index
        );
        const MarimayCheckedCompletion *right_item = mmay_vector_at_const(
            &right->completions,
            index
        );
        MMAY_EXPECT(left_item != NULL && right_item != NULL);
        if (left_item != NULL && right_item != NULL) {
            MMAY_EXPECT(left_item->snapshot == right_item->snapshot);
            MMAY_EXPECT(mmay_query_owned_bytes_equal(
                &left_item->name,
                &right_item->name
            ));
            MMAY_EXPECT(mmay_query_owned_bytes_equal(
                &left_item->package_name,
                &right_item->package_name
            ));
            MMAY_EXPECT(mmay_query_owned_bytes_equal(
                &left_item->free_spelling,
                &right_item->free_spelling
            ));
            MMAY_EXPECT(mmay_symbol_id_equal(
                left_item->symbol,
                right_item->symbol
            ));
            MMAY_EXPECT(mmay_symbol_id_equal(
                left_item->receiver_symbol,
                right_item->receiver_symbol
            ));
        }
    }
}

static void mmay_test_compiler_queries_import_matrix(void) {
    MarimayQueryFixture fixture;
    MarimayCompilerResult result;
    MarimayCompilerResult reversed;
    MarimayCompilerResult updated;
    MarimayCompilerResult private_updated;
    MarimayCompilerResult invalid;
    MarimayCompilerResult cancelled;
    const MarimayCheckedSymbol *user = NULL;
    const MarimayCheckedSymbol *definition = NULL;
    const MarimayCheckedCompletion *old_score = NULL;
    const MarimayCheckedCompletion *new_score = NULL;
    const MarimayCheckedSymbol *free_definition = NULL;
    const MarimayCompilerUnit *type_unit = NULL;
    const MarimayCompilerUnit *named_unit = NULL;
    const MarimayCompilerUnit *namespace_unit = NULL;
    const MarimaySourceSnapshot *old_named_snapshot = NULL;
    size_t count = 0;
    size_t offset = 0;
    MarimayQueryCancel cancel;
    MarimayCompilerCheckRequest request;
    char private_change_store[sizeof(mmay_query_store_source)];
    char private_store[sizeof(mmay_query_store_source)];
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
    MMAY_EXPECT(mmay_query_fixture_init(
        &fixture,
        mmay_system_allocator(),
        mmay_compiler_default_options()
    ));
    mmay_query_expect_invalid_graphs(&fixture);
    MMAY_EXPECT(mmay_query_check_graph(&fixture, false, NULL, &result)
        == MMAY_COMPILER_OK);
    MMAY_EXPECT(result.initialized && !mmay_compiler_result_has_errors(&result));
    MMAY_EXPECT(mmay_compiler_result_is_current(&fixture.service, &result));
    user = mmay_query_find_symbol(
        &result.checked,
        "acme/store",
        "User",
        MMAY_CHECKED_SYMBOL_RECORD
    );
    type_unit = mmay_query_unit(&result, "root/type.my");
    named_unit = mmay_query_unit(&result, "root/named.my");
    namespace_unit = mmay_query_unit(&result, "root/ns.my");
    old_named_snapshot = named_unit == NULL ? NULL : named_unit->snapshot;
    MMAY_EXPECT(user != NULL && type_unit != NULL && named_unit != NULL
        && namespace_unit != NULL);
    if (user != NULL && type_unit != NULL && named_unit != NULL
        && namespace_unit != NULL) {
        mmay_query_expect_scope(
            &result.checked,
            type_unit->snapshot,
            user->symbol,
            "",
            ""
        );
        mmay_query_expect_scope(
            &result.checked,
            named_unit->snapshot,
            user->symbol,
            "",
            "freeScore"
        );
        mmay_query_expect_scope(
            &result.checked,
            namespace_unit->snapshot,
            user->symbol,
            "store.label",
            "store.score"
        );
        old_score = mmay_query_find_completion(
            &result.checked,
            named_unit->snapshot,
            user->symbol,
            "score"
        );
        offset = mmay_query_find(named_unit->snapshot, "freeScore(");
        free_definition = mmay_checked_definition_from_reference(
            &result.checked,
            named_unit->snapshot,
            offset
        );
        MMAY_EXPECT(old_score != NULL && free_definition != NULL);
        if (old_score != NULL && free_definition != NULL) {
            MMAY_EXPECT(mmay_symbol_id_equal(
                old_score->symbol,
                free_definition->symbol
            ));
        }
        offset = mmay_query_find(namespace_unit->snapshot, "store.score");
        MMAY_EXPECT(offset != SIZE_MAX);
        if (offset != SIZE_MAX) {
            offset += 6;
        }
        free_definition = mmay_checked_definition_from_reference(
            &result.checked,
            namespace_unit->snapshot,
            offset
        );
        MMAY_EXPECT(old_score != NULL && free_definition != NULL);
        if (old_score != NULL && free_definition != NULL) {
            MMAY_EXPECT(mmay_symbol_id_equal(
                old_score->symbol,
                free_definition->symbol
            ));
        }
        offset = mmay_query_find(type_unit->snapshot, "user.score");
        MMAY_EXPECT(offset != SIZE_MAX);
        if (offset != SIZE_MAX) {
            offset += 5;
        }
        MMAY_EXPECT(mmay_compiler_service_definition_from_reference(
            &fixture.service,
            &result,
            type_unit->snapshot,
            offset,
            &definition
        ) == MMAY_COMPILER_OK);
        MMAY_EXPECT(definition != NULL
            && mmay_query_bytes_equal(&definition->name, "score"));
        MMAY_EXPECT(mmay_compiler_service_completion_count(
            &fixture.service,
            &result,
            type_unit->snapshot,
            user->symbol,
            &count
        ) == MMAY_COMPILER_OK);
        MMAY_EXPECT(count == 2);
    }
    MMAY_EXPECT(mmay_checked_symbol_search_count(
        &result.checked,
        "s",
        1
    ) == 3);
    MMAY_EXPECT(mmay_compiler_service_symbol_search_count(
        &fixture.service,
        &result,
        "score",
        strlen("score"),
        &count
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(count == 2);

    MMAY_EXPECT(mmay_query_check_graph(&fixture, true, NULL, &reversed)
        == MMAY_COMPILER_OK);
    mmay_query_expect_deterministic(&result.checked, &reversed.checked);

    old_score = named_unit == NULL || user == NULL ? NULL
        : mmay_query_find_completion(
            &result.checked,
            named_unit->snapshot,
            user->symbol,
            "score"
        );
    MMAY_EXPECT(old_score != NULL
        && mmay_query_bytes_equal(&old_score->free_spelling, "freeScore"));
    MMAY_EXPECT(mmay_compiler_service_overlay(
        &fixture.service,
        fixture.named,
        2,
        mmay_query_named_type_only_source,
        strlen(mmay_query_named_type_only_source)
    ) == MMAY_COMPILER_OK);
    definition = (const MarimayCheckedSymbol *)(uintptr_t)1;
    MMAY_EXPECT(mmay_compiler_service_definition_from_reference(
        &fixture.service,
        &result,
        type_unit->snapshot,
        offset,
        &definition
    ) == MMAY_COMPILER_STALE_RESULT);
    MMAY_EXPECT(definition == NULL);
    MMAY_EXPECT(mmay_query_check_graph(&fixture, false, NULL, &updated)
        == MMAY_COMPILER_OK);
    named_unit = mmay_query_unit(&updated, "root/named.my");
    new_score = named_unit == NULL || user == NULL ? NULL
        : mmay_query_find_completion(
            &updated.checked,
            named_unit->snapshot,
            user->symbol,
            "score"
        );
    MMAY_EXPECT(new_score != NULL
        && mmay_query_bytes_equal(&new_score->free_spelling, ""));
    MMAY_EXPECT(old_score != NULL
        && mmay_query_bytes_equal(&old_score->free_spelling, "freeScore"));
    if (old_named_snapshot != NULL && user != NULL) {
        count = SIZE_MAX;
        MMAY_EXPECT(mmay_compiler_service_completion_count(
            &fixture.service,
            &updated,
            old_named_snapshot,
            user->symbol,
            &count
        ) == MMAY_COMPILER_STALE_RESULT);
        MMAY_EXPECT(count == 0);
        definition = (const MarimayCheckedSymbol *)(uintptr_t)1;
        MMAY_EXPECT(mmay_compiler_service_definition_from_reference(
            &fixture.service,
            &updated,
            old_named_snapshot,
            0,
            &definition
        ) == MMAY_COMPILER_STALE_RESULT);
        MMAY_EXPECT(definition == NULL);
    }

    memcpy(private_change_store, mmay_query_store_source,
        sizeof(mmay_query_store_source));
    secret_name = strstr(private_change_store, "secret");
    MMAY_EXPECT(secret_name != NULL);
    if (secret_name != NULL) {
        memcpy(secret_name, "hidden", strlen("hidden"));
    }
    MMAY_EXPECT(mmay_compiler_service_overlay(
        &fixture.service,
        fixture.store,
        2,
        private_change_store,
        strlen(private_change_store)
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_query_check_graph(
        &fixture,
        false,
        NULL,
        &private_updated
    ) == MMAY_COMPILER_OK);
    type_unit = mmay_query_unit(&private_updated, "root/type.my");
    MMAY_EXPECT(type_unit != NULL && user != NULL);
    if (type_unit != NULL && user != NULL) {
        mmay_query_expect_scope(
            &private_updated.checked,
            type_unit->snapshot,
            user->symbol,
            "",
            ""
        );
    }

    cancel.cancel_at = 1;
    request.cancel_poll = mmay_query_cancel_poll;
    request.cancel_context = &cancel;
    MMAY_EXPECT(mmay_query_check_graph(
        &fixture,
        false,
        &request,
        &cancelled
    ) == MMAY_COMPILER_CANCELLED);
    MMAY_EXPECT(!cancelled.initialized);

    memcpy(private_store, mmay_query_store_source,
        sizeof(mmay_query_store_source));
    public_label = strstr(private_store, "public fn label");
    MMAY_EXPECT(public_label != NULL);
    if (public_label != NULL) {
        memset(public_label, ' ', strlen("public "));
    }
    MMAY_EXPECT(mmay_compiler_service_overlay(
        &fixture.service,
        fixture.store,
        3,
        private_store,
        strlen(private_store)
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_query_check_graph(&fixture, false, NULL, &invalid)
        == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_result_has_errors(&invalid));
    MMAY_EXPECT(invalid.checked.symbols.length == 0);
    MMAY_EXPECT(invalid.checked.occurrences.length == 0);
    MMAY_EXPECT(invalid.checked.completions.length == 0);

    mmay_compiler_result_dispose(&invalid);
    mmay_compiler_result_dispose(&private_updated);
    mmay_compiler_result_dispose(&updated);
    mmay_compiler_result_dispose(&reversed);
    mmay_compiler_result_dispose(&result);
    mmay_compiler_service_dispose(&fixture.service);
}

static const char mmay_query_limit_source[] =
    "type User {}\n"
    "fn first(user: User): int { return 1; }\n"
    "fn second(user: User): int { return 2; }\n"
    "fn use(user: User): int { return user.first() + user.second(); }\n";

static MarimayCompilerStatus mmay_query_check_single_graph(
    const MarimayCompilerService *service,
    MarimayFileId file,
    MarimayCompilerResult *result
) {
    MarimayCompilerPackage package;
    memset(&package, 0, sizeof(package));
    package.import_path = "limits/root";
    package.import_path_length = strlen("limits/root");
    package.file_ids = &file;
    package.file_count = 1;
    return mmay_compiler_service_check_graph(
        service,
        &package,
        1,
        "limits/root",
        strlen("limits/root"),
        result
    );
}

static void mmay_test_compiler_queries_limits(void) {
    MarimayCompilerOptions options = mmay_compiler_default_options();
    MarimayFailAllocator state;
    MarimayAllocator allocator;
    MarimayCompilerService service;
    MarimayCompilerResult result;
    MarimayFileId file = 0;
    memset(&service, 0, sizeof(service));
    memset(&result, 0, sizeof(result));
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    allocator = mmay_fail_allocator(&state);
    options.check_options.max_completions = 2;
    MMAY_EXPECT(mmay_compiler_service_init(
        &service,
        allocator,
        options
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service,
        "limits.my",
        strlen("limits.my"),
        1,
        mmay_query_limit_source,
        strlen(mmay_query_limit_source),
        &file
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_query_check_single_graph(&service, file, &result)
        == MMAY_COMPILER_LIMIT_REACHED);
    MMAY_EXPECT(!result.initialized);
    mmay_compiler_service_dispose(&service);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);

    options = mmay_compiler_default_options();
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    allocator = mmay_fail_allocator(&state);
    options.check_options.max_occurrences = 1;
    MMAY_EXPECT(mmay_compiler_service_init(
        &service,
        allocator,
        options
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_compiler_service_add_document(
        &service,
        "limits.my",
        strlen("limits.my"),
        1,
        mmay_query_limit_source,
        strlen(mmay_query_limit_source),
        &file
    ) == MMAY_COMPILER_OK);
    MMAY_EXPECT(mmay_query_check_single_graph(&service, file, &result)
        == MMAY_COMPILER_LIMIT_REACHED);
    MMAY_EXPECT(!result.initialized);
    mmay_compiler_service_dispose(&service);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
}

static void mmay_test_compiler_queries_allocation_failures(void) {
    size_t relative_failure = 0;
    bool reached_success = false;
    for (relative_failure = 0; relative_failure < 1024;
        relative_failure += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayQueryFixture fixture;
        MarimayCompilerResult result;
        MarimayCompilerStatus status;
        memset(&fixture, 0, sizeof(fixture));
        memset(&result, 0, sizeof(result));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_query_fixture_init(
            &fixture,
            allocator,
            mmay_compiler_default_options()
        ));
        state.failure_index = state.attempts + relative_failure;
        status = mmay_query_check_graph(&fixture, false, NULL, &result);
        if (status == MMAY_COMPILER_OK) {
            reached_success = true;
            MMAY_EXPECT(result.checked.completions.length == 13);
            MMAY_EXPECT(result.checked.occurrences.length != 0);
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

void mmay_test_compiler_service_queries(void) {
    mmay_test_compiler_queries_import_matrix();
    mmay_test_compiler_queries_limits();
    mmay_test_compiler_queries_allocation_failures();
}
