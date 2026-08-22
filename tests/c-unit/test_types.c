#include "test_support.h"

#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/types/type.h"

static void mmay_test_receiver_symbol_identity(void) {
    MarimaySymbolId first_receiver;
    MarimaySymbolId second_receiver;
    MarimaySymbolId first_function;
    MarimaySymbolId repeated_function;
    MarimaySymbolId second_function;
    MMAY_EXPECT(mmay_symbol_id_from_parts(
        "example.test/owned",
        "src/model.my",
        "First",
        0,
        &first_receiver
    ));
    MMAY_EXPECT(mmay_symbol_id_from_parts(
        "example.test/owned",
        "src/model.my",
        "Second",
        0,
        &second_receiver
    ));
    MMAY_EXPECT(mmay_symbol_id_from_parts_with_receiver(
        "example.test/owned",
        "src/behavior.my",
        "string",
        first_receiver,
        &first_function
    ));
    MMAY_EXPECT(mmay_symbol_id_from_parts_with_receiver(
        "example.test/owned",
        "src/behavior.my",
        "string",
        first_receiver,
        &repeated_function
    ));
    MMAY_EXPECT(mmay_symbol_id_from_parts_with_receiver(
        "example.test/owned",
        "src/behavior.my",
        "string",
        second_receiver,
        &second_function
    ));
    MMAY_EXPECT(mmay_symbol_id_equal(first_function, repeated_function));
    MMAY_EXPECT(!mmay_symbol_id_equal(first_function, second_function));
}

static void mmay_test_type_interner_scale(void) {
    enum { TYPE_COUNT = 512 };
    MarimayTypeTable table;
    MarimayTypeTableOptions options = mmay_type_table_default_options();
    MarimaySymbolId symbols[TYPE_COUNT];
    MarimayTypeId types[TYPE_COUNT];
    size_t index = 0;
    options.max_types = MMAY_TYPE_FIRST_DYNAMIC + TYPE_COUNT;
    MMAY_EXPECT(mmay_type_table_init(
        &table,
        mmay_system_allocator(),
        options
    ));
    for (index = 0; index < TYPE_COUNT; index += 1) {
        MMAY_EXPECT(mmay_symbol_id_from_parts(
            "example.test/generated",
            "src/generated.my",
            "Generated",
            (uint32_t)index,
            &symbols[index]
        ));
        MMAY_EXPECT(mmay_type_intern_nominal(
            &table,
            MMAY_TYPE_KIND_NOMINAL,
            symbols[index],
            NULL,
            0,
            &types[index]
        ));
    }
    MMAY_EXPECT(table.descriptors.length == TYPE_COUNT);
    MMAY_EXPECT(table.canonical.length == TYPE_COUNT);
    for (index = TYPE_COUNT; index != 0; index -= 1) {
        MarimayTypeId repeated = MMAY_TYPE_INVALID;
        MMAY_EXPECT(mmay_type_intern_nominal(
            &table,
            MMAY_TYPE_KIND_NOMINAL,
            symbols[index - 1],
            NULL,
            0,
            &repeated
        ));
        MMAY_EXPECT(repeated == types[index - 1]);
    }
    MMAY_EXPECT(table.descriptors.length == TYPE_COUNT);
    MMAY_EXPECT(table.canonical.length == TYPE_COUNT);
    mmay_type_table_dispose(&table);
}

static void mmay_test_type_interner_rollback(void) {
    MarimaySymbolId first_symbol;
    MarimaySymbolId second_symbol;
    size_t failure_offset = 0;
    bool reached_success = false;
    MMAY_EXPECT(mmay_symbol_id_from_parts(
        "example.test/rollback",
        "src/types.my",
        "First",
        0,
        &first_symbol
    ));
    MMAY_EXPECT(mmay_symbol_id_from_parts(
        "example.test/rollback",
        "src/types.my",
        "Second",
        0,
        &second_symbol
    ));
    for (failure_offset = 0; failure_offset < 16; failure_offset += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayTypeTable table;
        MarimayTypeId first = MMAY_TYPE_INVALID;
        MarimayTypeId repeated = MMAY_TYPE_INVALID;
        MarimayTypeId second = MMAY_TYPE_INVALID;
        bool interned = false;
        mmay_fail_allocator_init(
            &state,
            mmay_system_allocator(),
            SIZE_MAX
        );
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_type_table_init(
            &table,
            allocator,
            mmay_type_table_default_options()
        ));
        MMAY_EXPECT(mmay_type_intern_nominal(
            &table,
            MMAY_TYPE_KIND_NOMINAL,
            first_symbol,
            NULL,
            0,
            &first
        ));
        state.failure_index = state.attempts + failure_offset;
        interned = mmay_type_intern_nominal(
            &table,
            MMAY_TYPE_KIND_NOMINAL,
            second_symbol,
            NULL,
            0,
            &second
        );
        if (interned) {
            reached_success = true;
            MMAY_EXPECT(table.descriptors.length == 2);
            MMAY_EXPECT(table.canonical.length == 2);
        } else {
            MMAY_EXPECT(table.descriptors.length == 1);
            MMAY_EXPECT(table.canonical.length == 1);
            MMAY_EXPECT(mmay_type_intern_nominal(
                &table,
                MMAY_TYPE_KIND_NOMINAL,
                first_symbol,
                NULL,
                0,
                &repeated
            ));
            MMAY_EXPECT(repeated == first);
        }
        mmay_type_table_dispose(&table);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_array_type_interner_rollback(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 16; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayTypeTable table;
        MarimayTypeId array = MMAY_TYPE_INVALID;
        MarimayTypeId repeated = MMAY_TYPE_INVALID;
        bool initialized = false;
        bool interned = false;
        mmay_fail_allocator_init(
            &state,
            mmay_system_allocator(),
            failure_index
        );
        allocator = mmay_fail_allocator(&state);
        initialized = mmay_type_table_init(
            &table,
            allocator,
            mmay_type_table_default_options()
        );
        if (initialized) {
            interned = mmay_type_intern_array(
                &table,
                MMAY_TYPE_F64,
                &array
            );
            if (interned) {
                reached_success = true;
                MMAY_EXPECT(mmay_type_intern_array(
                    &table,
                    MMAY_TYPE_F64,
                    &repeated
                ));
                MMAY_EXPECT(repeated == array);
            } else {
                MMAY_EXPECT(table.descriptors.length == 0);
                MMAY_EXPECT(table.canonical.length == 0);
            }
            mmay_type_table_dispose(&table);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_types(void) {
    MarimayTypeTable table;
    MarimayTypeTableOptions options = mmay_type_table_default_options();
    MarimaySymbolId user_symbol;
    MarimaySymbolId other_symbol;
    MarimayTypeId user = MMAY_TYPE_INVALID;
    MarimayTypeId same_user = MMAY_TYPE_INVALID;
    MarimayTypeId other_user = MMAY_TYPE_INVALID;
    MarimayTypeId tuple = MMAY_TYPE_INVALID;
    MarimayTypeId function = MMAY_TYPE_INVALID;
    MarimayTypeId array_f64 = MMAY_TYPE_INVALID;
    MarimayTypeId same_array_f64 = MMAY_TYPE_INVALID;
    MarimayTypeId array_i64 = MMAY_TYPE_INVALID;
    MarimayTypeId nested_array = MMAY_TYPE_INVALID;
    MarimayTypeId element = MMAY_TYPE_INVALID;
    MarimayTypeId tuple_elements[] = {MMAY_TYPE_STRING, MMAY_TYPE_I64};
    MarimayTypeId parameters[] = {MMAY_TYPE_I64, MMAY_TYPE_BOOL};
    const MarimayTypeDescriptor *descriptor = NULL;
    MarimayInferenceContext inference;
    MarimayInferenceId first = MMAY_INFERENCE_INVALID;
    MarimayInferenceId second = MMAY_INFERENCE_INVALID;

    options.max_types = 64;
    MMAY_EXPECT(mmay_type_table_init(
        &table,
        mmay_system_allocator(),
        options
    ));
    MMAY_EXPECT(mmay_type_kind(&table, MMAY_TYPE_I64) == MMAY_TYPE_KIND_I64);
    MMAY_EXPECT(mmay_type_kind(&table, MMAY_TYPE_INVALID) == MMAY_TYPE_KIND_INVALID);
    MMAY_EXPECT(mmay_symbol_id_from_parts(
        "example.test/core",
        "src/user.my",
        "User",
        0,
        &user_symbol
    ));
    MMAY_EXPECT(mmay_symbol_id_from_parts(
        "example.test/core",
        "src/other.my",
        "User",
        0,
        &other_symbol
    ));
    MMAY_EXPECT(!mmay_symbol_id_equal(user_symbol, other_symbol));
    MMAY_EXPECT(mmay_type_intern_nominal(
        &table,
        MMAY_TYPE_KIND_NOMINAL,
        user_symbol,
        NULL,
        0,
        &user
    ));
    MMAY_EXPECT(mmay_type_intern_nominal(
        &table,
        MMAY_TYPE_KIND_NOMINAL,
        user_symbol,
        NULL,
        0,
        &same_user
    ));
    MMAY_EXPECT(user == same_user);
    MMAY_EXPECT(mmay_type_intern_nominal(
        &table,
        MMAY_TYPE_KIND_NOMINAL,
        other_symbol,
        NULL,
        0,
        &other_user
    ));
    MMAY_EXPECT(user != other_user);
    MMAY_EXPECT(mmay_type_intern_tuple(
        &table,
        tuple_elements,
        2,
        &tuple
    ));
    descriptor = mmay_type_descriptor(&table, tuple);
    MMAY_EXPECT(descriptor != NULL);
    MMAY_EXPECT(descriptor != NULL && descriptor->kind == MMAY_TYPE_KIND_TUPLE);
    MMAY_EXPECT(descriptor != NULL && descriptor->argument_count == 2);
    MMAY_EXPECT(mmay_type_intern_function(
        &table,
        parameters,
        2,
        MMAY_TYPE_I64,
        &function
    ));
    descriptor = mmay_type_descriptor(&table, function);
    MMAY_EXPECT(descriptor != NULL && descriptor->kind == MMAY_TYPE_KIND_FUNCTION);
    MMAY_EXPECT(descriptor != NULL && descriptor->parameter_count == 2);
    MMAY_EXPECT(descriptor != NULL && descriptor->argument_count == 3);
    MMAY_EXPECT(mmay_type_intern_array(
        &table,
        MMAY_TYPE_F64,
        &array_f64
    ));
    MMAY_EXPECT(mmay_type_intern_array(
        &table,
        MMAY_TYPE_F64,
        &same_array_f64
    ));
    MMAY_EXPECT(array_f64 == same_array_f64);
    MMAY_EXPECT(mmay_type_kind(&table, array_f64) == MMAY_TYPE_KIND_ARRAY);
    MMAY_EXPECT(mmay_type_array_element(&table, array_f64, &element));
    MMAY_EXPECT(element == MMAY_TYPE_F64);
    MMAY_EXPECT(mmay_type_intern_array(
        &table,
        MMAY_TYPE_I64,
        &array_i64
    ));
    MMAY_EXPECT(array_i64 != array_f64);
    MMAY_EXPECT(mmay_type_intern_array(
        &table,
        array_f64,
        &nested_array
    ));
    MMAY_EXPECT(mmay_type_array_element(&table, nested_array, &element));
    MMAY_EXPECT(element == array_f64);
    MMAY_EXPECT(!mmay_type_intern_array(
        &table,
        MMAY_TYPE_INVALID,
        &element
    ));
    MMAY_EXPECT(!mmay_type_array_element(
        &table,
        MMAY_TYPE_F64,
        &element
    ));

    MMAY_EXPECT(mmay_inference_context_init(
        &inference,
        mmay_system_allocator(),
        8
    ));
    MMAY_EXPECT(mmay_inference_new(&inference, &first));
    MMAY_EXPECT(mmay_inference_new(&inference, &second));
    MMAY_EXPECT(mmay_inference_unify_variables(&inference, first, second));
    MMAY_EXPECT(mmay_inference_bind(&inference, first, user));
    MMAY_EXPECT(mmay_inference_resolve(&inference, second) == user);
    MMAY_EXPECT(!mmay_inference_bind(&inference, second, other_user));
    mmay_inference_context_dispose(&inference);
    mmay_type_table_dispose(&table);

    {
        size_t failure_index = 0;
        bool reached_success = false;
        for (failure_index = 0; failure_index < 8; failure_index += 1) {
            MarimayFailAllocator state;
            MarimayAllocator allocator;
            MarimayTypeId allocated = MMAY_TYPE_INVALID;
            bool initialized = false;
            bool interned = false;
            mmay_fail_allocator_init(
                &state,
                mmay_system_allocator(),
                failure_index
            );
            allocator = mmay_fail_allocator(&state);
            initialized = mmay_type_table_init(
                &table,
                allocator,
                mmay_type_table_default_options()
            );
            if (initialized) {
                interned = mmay_type_intern_nominal(
                    &table,
                    MMAY_TYPE_KIND_NOMINAL,
                    user_symbol,
                    NULL,
                    0,
                    &allocated
                );
                if (interned) {
                    reached_success = true;
                    MMAY_EXPECT(allocated >= MMAY_TYPE_FIRST_DYNAMIC);
                }
                mmay_type_table_dispose(&table);
            }
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
            if (reached_success) {
                break;
            }
        }
        MMAY_EXPECT(reached_success);
    }

    mmay_test_receiver_symbol_identity();
    mmay_test_type_interner_scale();
    mmay_test_type_interner_rollback();
    mmay_test_array_type_interner_rollback();
}
