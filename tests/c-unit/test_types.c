#include "test_support.h"

#include <stddef.h>

#include "ruda/base/allocator.h"
#include "ruda/types/type.h"

static void ruda_test_type_interner_scale(void) {
    enum { TYPE_COUNT = 512 };
    RudaTypeTable table;
    RudaTypeTableOptions options = ruda_type_table_default_options();
    RudaSymbolId symbols[TYPE_COUNT];
    RudaTypeId types[TYPE_COUNT];
    size_t index = 0;
    options.max_types = RUDA_TYPE_FIRST_DYNAMIC + TYPE_COUNT;
    RUDA_EXPECT(ruda_type_table_init(
        &table,
        ruda_system_allocator(),
        options
    ));
    for (index = 0; index < TYPE_COUNT; index += 1) {
        RUDA_EXPECT(ruda_symbol_id_from_parts(
            "example.test/generated",
            "src/generated.ruda",
            "Generated",
            (uint32_t)index,
            &symbols[index]
        ));
        RUDA_EXPECT(ruda_type_intern_nominal(
            &table,
            RUDA_TYPE_KIND_NOMINAL,
            symbols[index],
            NULL,
            0,
            &types[index]
        ));
    }
    RUDA_EXPECT(table.descriptors.length == TYPE_COUNT);
    RUDA_EXPECT(table.canonical.length == TYPE_COUNT);
    for (index = TYPE_COUNT; index != 0; index -= 1) {
        RudaTypeId repeated = RUDA_TYPE_INVALID;
        RUDA_EXPECT(ruda_type_intern_nominal(
            &table,
            RUDA_TYPE_KIND_NOMINAL,
            symbols[index - 1],
            NULL,
            0,
            &repeated
        ));
        RUDA_EXPECT(repeated == types[index - 1]);
    }
    RUDA_EXPECT(table.descriptors.length == TYPE_COUNT);
    RUDA_EXPECT(table.canonical.length == TYPE_COUNT);
    ruda_type_table_dispose(&table);
}

static void ruda_test_type_interner_rollback(void) {
    RudaSymbolId first_symbol;
    RudaSymbolId second_symbol;
    size_t failure_offset = 0;
    bool reached_success = false;
    RUDA_EXPECT(ruda_symbol_id_from_parts(
        "example.test/rollback",
        "src/types.ruda",
        "First",
        0,
        &first_symbol
    ));
    RUDA_EXPECT(ruda_symbol_id_from_parts(
        "example.test/rollback",
        "src/types.ruda",
        "Second",
        0,
        &second_symbol
    ));
    for (failure_offset = 0; failure_offset < 16; failure_offset += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaTypeTable table;
        RudaTypeId first = RUDA_TYPE_INVALID;
        RudaTypeId repeated = RUDA_TYPE_INVALID;
        RudaTypeId second = RUDA_TYPE_INVALID;
        bool interned = false;
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            SIZE_MAX
        );
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_type_table_init(
            &table,
            allocator,
            ruda_type_table_default_options()
        ));
        RUDA_EXPECT(ruda_type_intern_nominal(
            &table,
            RUDA_TYPE_KIND_NOMINAL,
            first_symbol,
            NULL,
            0,
            &first
        ));
        state.failure_index = state.attempts + failure_offset;
        interned = ruda_type_intern_nominal(
            &table,
            RUDA_TYPE_KIND_NOMINAL,
            second_symbol,
            NULL,
            0,
            &second
        );
        if (interned) {
            reached_success = true;
            RUDA_EXPECT(table.descriptors.length == 2);
            RUDA_EXPECT(table.canonical.length == 2);
        } else {
            RUDA_EXPECT(table.descriptors.length == 1);
            RUDA_EXPECT(table.canonical.length == 1);
            RUDA_EXPECT(ruda_type_intern_nominal(
                &table,
                RUDA_TYPE_KIND_NOMINAL,
                first_symbol,
                NULL,
                0,
                &repeated
            ));
            RUDA_EXPECT(repeated == first);
        }
        ruda_type_table_dispose(&table);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_array_type_interner_rollback(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 16; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaTypeTable table;
        RudaTypeId array = RUDA_TYPE_INVALID;
        RudaTypeId repeated = RUDA_TYPE_INVALID;
        bool initialized = false;
        bool interned = false;
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            failure_index
        );
        allocator = ruda_fail_allocator(&state);
        initialized = ruda_type_table_init(
            &table,
            allocator,
            ruda_type_table_default_options()
        );
        if (initialized) {
            interned = ruda_type_intern_array(
                &table,
                RUDA_TYPE_F64,
                &array
            );
            if (interned) {
                reached_success = true;
                RUDA_EXPECT(ruda_type_intern_array(
                    &table,
                    RUDA_TYPE_F64,
                    &repeated
                ));
                RUDA_EXPECT(repeated == array);
            } else {
                RUDA_EXPECT(table.descriptors.length == 0);
                RUDA_EXPECT(table.canonical.length == 0);
            }
            ruda_type_table_dispose(&table);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_types(void) {
    RudaTypeTable table;
    RudaTypeTableOptions options = ruda_type_table_default_options();
    RudaSymbolId user_symbol;
    RudaSymbolId other_symbol;
    RudaTypeId user = RUDA_TYPE_INVALID;
    RudaTypeId same_user = RUDA_TYPE_INVALID;
    RudaTypeId other_user = RUDA_TYPE_INVALID;
    RudaTypeId tuple = RUDA_TYPE_INVALID;
    RudaTypeId function = RUDA_TYPE_INVALID;
    RudaTypeId array_f64 = RUDA_TYPE_INVALID;
    RudaTypeId same_array_f64 = RUDA_TYPE_INVALID;
    RudaTypeId array_i64 = RUDA_TYPE_INVALID;
    RudaTypeId nested_array = RUDA_TYPE_INVALID;
    RudaTypeId element = RUDA_TYPE_INVALID;
    RudaTypeId tuple_elements[] = {RUDA_TYPE_STRING, RUDA_TYPE_I64};
    RudaTypeId parameters[] = {RUDA_TYPE_I64, RUDA_TYPE_BOOL};
    const RudaTypeDescriptor *descriptor = NULL;
    RudaInferenceContext inference;
    RudaInferenceId first = RUDA_INFERENCE_INVALID;
    RudaInferenceId second = RUDA_INFERENCE_INVALID;

    options.max_types = 64;
    RUDA_EXPECT(ruda_type_table_init(
        &table,
        ruda_system_allocator(),
        options
    ));
    RUDA_EXPECT(ruda_type_kind(&table, RUDA_TYPE_I64) == RUDA_TYPE_KIND_I64);
    RUDA_EXPECT(ruda_type_kind(&table, RUDA_TYPE_INVALID) == RUDA_TYPE_KIND_INVALID);
    RUDA_EXPECT(ruda_symbol_id_from_parts(
        "example.test/core",
        "src/user.ruda",
        "User",
        0,
        &user_symbol
    ));
    RUDA_EXPECT(ruda_symbol_id_from_parts(
        "example.test/core",
        "src/other.ruda",
        "User",
        0,
        &other_symbol
    ));
    RUDA_EXPECT(!ruda_symbol_id_equal(user_symbol, other_symbol));
    RUDA_EXPECT(ruda_type_intern_nominal(
        &table,
        RUDA_TYPE_KIND_NOMINAL,
        user_symbol,
        NULL,
        0,
        &user
    ));
    RUDA_EXPECT(ruda_type_intern_nominal(
        &table,
        RUDA_TYPE_KIND_NOMINAL,
        user_symbol,
        NULL,
        0,
        &same_user
    ));
    RUDA_EXPECT(user == same_user);
    RUDA_EXPECT(ruda_type_intern_nominal(
        &table,
        RUDA_TYPE_KIND_NOMINAL,
        other_symbol,
        NULL,
        0,
        &other_user
    ));
    RUDA_EXPECT(user != other_user);
    RUDA_EXPECT(ruda_type_intern_tuple(
        &table,
        tuple_elements,
        2,
        &tuple
    ));
    descriptor = ruda_type_descriptor(&table, tuple);
    RUDA_EXPECT(descriptor != NULL);
    RUDA_EXPECT(descriptor != NULL && descriptor->kind == RUDA_TYPE_KIND_TUPLE);
    RUDA_EXPECT(descriptor != NULL && descriptor->argument_count == 2);
    RUDA_EXPECT(ruda_type_intern_function(
        &table,
        parameters,
        2,
        RUDA_TYPE_I64,
        &function
    ));
    descriptor = ruda_type_descriptor(&table, function);
    RUDA_EXPECT(descriptor != NULL && descriptor->kind == RUDA_TYPE_KIND_FUNCTION);
    RUDA_EXPECT(descriptor != NULL && descriptor->parameter_count == 2);
    RUDA_EXPECT(descriptor != NULL && descriptor->argument_count == 3);
    RUDA_EXPECT(ruda_type_intern_array(
        &table,
        RUDA_TYPE_F64,
        &array_f64
    ));
    RUDA_EXPECT(ruda_type_intern_array(
        &table,
        RUDA_TYPE_F64,
        &same_array_f64
    ));
    RUDA_EXPECT(array_f64 == same_array_f64);
    RUDA_EXPECT(ruda_type_kind(&table, array_f64) == RUDA_TYPE_KIND_ARRAY);
    RUDA_EXPECT(ruda_type_array_element(&table, array_f64, &element));
    RUDA_EXPECT(element == RUDA_TYPE_F64);
    RUDA_EXPECT(ruda_type_intern_array(
        &table,
        RUDA_TYPE_I64,
        &array_i64
    ));
    RUDA_EXPECT(array_i64 != array_f64);
    RUDA_EXPECT(ruda_type_intern_array(
        &table,
        array_f64,
        &nested_array
    ));
    RUDA_EXPECT(ruda_type_array_element(&table, nested_array, &element));
    RUDA_EXPECT(element == array_f64);
    RUDA_EXPECT(!ruda_type_intern_array(
        &table,
        RUDA_TYPE_INVALID,
        &element
    ));
    RUDA_EXPECT(!ruda_type_array_element(
        &table,
        RUDA_TYPE_F64,
        &element
    ));

    RUDA_EXPECT(ruda_inference_context_init(
        &inference,
        ruda_system_allocator(),
        8
    ));
    RUDA_EXPECT(ruda_inference_new(&inference, &first));
    RUDA_EXPECT(ruda_inference_new(&inference, &second));
    RUDA_EXPECT(ruda_inference_unify_variables(&inference, first, second));
    RUDA_EXPECT(ruda_inference_bind(&inference, first, user));
    RUDA_EXPECT(ruda_inference_resolve(&inference, second) == user);
    RUDA_EXPECT(!ruda_inference_bind(&inference, second, other_user));
    ruda_inference_context_dispose(&inference);
    ruda_type_table_dispose(&table);

    {
        size_t failure_index = 0;
        bool reached_success = false;
        for (failure_index = 0; failure_index < 8; failure_index += 1) {
            RudaFailAllocator state;
            RudaAllocator allocator;
            RudaTypeId allocated = RUDA_TYPE_INVALID;
            bool initialized = false;
            bool interned = false;
            ruda_fail_allocator_init(
                &state,
                ruda_system_allocator(),
                failure_index
            );
            allocator = ruda_fail_allocator(&state);
            initialized = ruda_type_table_init(
                &table,
                allocator,
                ruda_type_table_default_options()
            );
            if (initialized) {
                interned = ruda_type_intern_nominal(
                    &table,
                    RUDA_TYPE_KIND_NOMINAL,
                    user_symbol,
                    NULL,
                    0,
                    &allocated
                );
                if (interned) {
                    reached_success = true;
                    RUDA_EXPECT(allocated >= RUDA_TYPE_FIRST_DYNAMIC);
                }
                ruda_type_table_dispose(&table);
            }
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
            if (reached_success) {
                break;
            }
        }
        RUDA_EXPECT(reached_success);
    }

    ruda_test_type_interner_scale();
    ruda_test_type_interner_rollback();
    ruda_test_array_type_interner_rollback();
}
