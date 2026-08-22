#include "test_mir_support.h"

#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/bytecode/mir_lower.h"
#include "mmay/native/c_emitter.h"

enum {
    MMAY_TEST_MIR_RECORD_TYPE = MMAY_TYPE_FIRST_DYNAMIC,
    MMAY_TEST_MIR_ENUM_TYPE,
    MMAY_TEST_MIR_INTERFACE_TYPE,
    MMAY_TEST_MIR_ARRAY_TYPE
};

typedef struct MarimayTestMirInterfaceFixture {
    MarimayMirFunction function;
    MarimayMirBlockId block;
    MarimayMirRegister payload;
    MarimayMirRegister argument;
    MarimayMirRegister interface_value;
    MarimayMirRegister result;
    MarimayMirRegister array_value;
    MarimayMirInterfaceId interface_id;
    MarimayMirInterfaceConstructionId construction_id;
    MarimayMirCallId call_id;
} MarimayTestMirInterfaceFixture;

typedef enum MarimayTestMirInterfaceMalformed {
    MMAY_TEST_MIR_INTERFACE_LAYOUT_CAP,
    MMAY_TEST_MIR_INTERFACE_CONSTRUCTION_CAP,
    MMAY_TEST_MIR_INTERFACE_METHOD_CAP,
    MMAY_TEST_MIR_INTERFACE_LAYOUT_SCALAR_TYPE,
    MMAY_TEST_MIR_INTERFACE_LAYOUT_DUPLICATE_TYPE,
    MMAY_TEST_MIR_INTERFACE_CONSTRUCTION_OOB,
    MMAY_TEST_MIR_INTERFACE_CONSTRUCTION_LAYOUT_OOB,
    MMAY_TEST_MIR_INTERFACE_METHOD_START,
    MMAY_TEST_MIR_INTERFACE_METHOD_COUNT,
    MMAY_TEST_MIR_INTERFACE_METHOD_RANGE,
    MMAY_TEST_MIR_INTERFACE_PAYLOAD_OOB,
    MMAY_TEST_MIR_INTERFACE_PAYLOAD_DECLARED_TYPE,
    MMAY_TEST_MIR_INTERFACE_PAYLOAD_SCALAR,
    MMAY_TEST_MIR_INTERFACE_PAYLOAD_ARRAY,
    MMAY_TEST_MIR_INTERFACE_PAYLOAD_UNINITIALIZED,
    MMAY_TEST_MIR_INTERFACE_TARGET_OOB,
    MMAY_TEST_MIR_INTERFACE_TARGET_TYPE,
    MMAY_TEST_MIR_INTERFACE_CALL_RANGE,
    MMAY_TEST_MIR_INTERFACE_CALL_EMPTY,
    MMAY_TEST_MIR_INTERFACE_CALL_RECEIVER_RECORD,
    MMAY_TEST_MIR_INTERFACE_CALL_SLOT_UNSET,
    MMAY_TEST_MIR_INTERFACE_CALL_SLOT_OOB,
    MMAY_TEST_MIR_INTERFACE_CALL_ARGUMENT_OOB,
    MMAY_TEST_MIR_INTERFACE_CALL_ARGUMENT_TYPE,
    MMAY_TEST_MIR_INTERFACE_CALL_RESULT_TYPE,
    MMAY_TEST_MIR_INTERFACE_CALL_RECEIVER_UNINITIALIZED,
    MMAY_TEST_MIR_INTERFACE_CALL_ARGUMENT_UNINITIALIZED,
    MMAY_TEST_MIR_INTERFACE_DIRECT_CALL_WITH_SLOT
} MarimayTestMirInterfaceMalformed;

typedef struct MarimayTestMirInterfaceMalformedCase {
    MarimayTestMirInterfaceMalformed kind;
    MarimayMirVerifyStatus expected;
} MarimayTestMirInterfaceMalformedCase;

static bool mmay_test_mir_interface_symbol(
    const char *name,
    uint32_t discriminator,
    MarimaySymbolId *result
) {
    return mmay_symbol_id_from_parts(
        "example.test/mir-interfaces",
        "src/interfaces.my",
        name,
        discriminator,
        result
    );
}

static bool mmay_test_mir_interface_fixture_init(
    MarimayTestMirInterfaceFixture *fixture,
    MarimayAllocator allocator,
    MarimaySymbolId symbol,
    bool enum_payload,
    size_t method_count
) {
    MarimayTypeId payload_type = enum_payload
        ? MMAY_TEST_MIR_ENUM_TYPE
        : MMAY_TEST_MIR_RECORD_TYPE;
    MarimayTypeId parameters[2] = {payload_type, MMAY_TYPE_I64};
    MarimayMirRecordId record_id = MMAY_MIR_NO_RECORD;
    MarimayMirEnumId enum_id = MMAY_MIR_NO_ENUM;
    MarimayMirArrayId array_id = MMAY_MIR_NO_ARRAY;
    MarimayMirEnumVariant enum_variant;
    MarimayMirCallArgument arguments[2];
    MarimaySymbolId methods[2];
    MarimayMirInstruction instruction;
    size_t method_index = 0;
    bool ok = false;

    if (fixture == NULL || method_count == 0 || method_count > 2) {
        return false;
    }
    memset(fixture, 0, sizeof(*fixture));
    fixture->block = MMAY_MIR_NO_BLOCK;
    fixture->payload = MMAY_MIR_NO_REGISTER;
    fixture->argument = MMAY_MIR_NO_REGISTER;
    fixture->interface_value = MMAY_MIR_NO_REGISTER;
    fixture->result = MMAY_MIR_NO_REGISTER;
    fixture->array_value = MMAY_MIR_NO_REGISTER;
    fixture->interface_id = MMAY_MIR_NO_INTERFACE;
    fixture->construction_id = MMAY_MIR_NO_INTERFACE_CONSTRUCTION;
    fixture->call_id = MMAY_MIR_NO_CALL;

    for (method_index = 0; method_index < method_count; method_index += 1) {
        if (!mmay_test_mir_interface_symbol(
                method_index == 0 ? "method-a" : "method-b",
                (uint32_t)(method_index + 1),
                &methods[method_index]
            )) {
            return false;
        }
    }
    if (!mmay_mir_function_init(
            &fixture->function,
            allocator,
            symbol,
            parameters,
            2,
            MMAY_TYPE_I64,
            mmay_mir_function_default_options()
        )) {
        return false;
    }
    fixture->payload = 0;
    fixture->argument = 1;
    if (enum_payload) {
        enum_variant.kind = MMAY_MIR_ENUM_VARIANT_UNIT;
        enum_variant.payload_start = 0;
        enum_variant.payload_count = 0;
        ok = mmay_mir_add_enum(
            &fixture->function,
            payload_type,
            &enum_variant,
            1,
            NULL,
            0,
            &enum_id
        );
    } else {
        ok = mmay_mir_add_record(
            &fixture->function,
            payload_type,
            NULL,
            0,
            &record_id
        );
    }
    ok = ok && mmay_mir_add_array(
        &fixture->function,
        MMAY_TEST_MIR_ARRAY_TYPE,
        MMAY_TYPE_I64,
        &array_id
    ) && mmay_mir_add_interface(
        &fixture->function,
        MMAY_TEST_MIR_INTERFACE_TYPE,
        method_count,
        &fixture->interface_id
    ) && mmay_mir_add_register(
        &fixture->function,
        MMAY_TEST_MIR_INTERFACE_TYPE,
        &fixture->interface_value
    ) && mmay_mir_add_register(
        &fixture->function,
        MMAY_TYPE_I64,
        &fixture->result
    ) && mmay_mir_add_register(
        &fixture->function,
        MMAY_TEST_MIR_ARRAY_TYPE,
        &fixture->array_value
    ) && mmay_mir_add_block(
        &fixture->function,
        &fixture->block
    ) && mmay_mir_add_interface_construction(
        &fixture->function,
        fixture->interface_id,
        fixture->payload,
        payload_type,
        methods,
        method_count,
        &fixture->construction_id
    );
    if (!ok) {
        mmay_mir_function_dispose(&fixture->function);
        return false;
    }

    arguments[0].register_id = fixture->interface_value;
    arguments[0].type = MMAY_TEST_MIR_INTERFACE_TYPE;
    arguments[1].register_id = fixture->argument;
    arguments[1].type = MMAY_TYPE_I64;
    if (!mmay_mir_add_call(
            &fixture->function,
            symbol,
            MMAY_TYPE_I64,
            arguments,
            2,
            &fixture->call_id
        ) || !mmay_mir_set_call_interface_slot(
            &fixture->function,
            fixture->call_id,
            0
        )) {
        mmay_mir_function_dispose(&fixture->function);
        return false;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_INTERFACE_NEW;
    instruction.target = fixture->interface_value;
    instruction.interface_construction = fixture->construction_id;
    if (!mmay_mir_append_instruction(
            &fixture->function,
            fixture->block,
            &instruction
        )) {
        mmay_mir_function_dispose(&fixture->function);
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CALL_INTERFACE;
    instruction.target = fixture->result;
    instruction.call = fixture->call_id;
    if (!mmay_mir_append_instruction(
            &fixture->function,
            fixture->block,
            &instruction
        ) || !mmay_mir_set_terminator(
            &fixture->function,
            fixture->block,
            mmay_test_mir_return(fixture->result)
        )) {
        mmay_mir_function_dispose(&fixture->function);
        return false;
    }
    return true;
}

static MarimayMirInstruction *mmay_test_mir_interface_instruction(
    MarimayTestMirInterfaceFixture *fixture,
    size_t index
) {
    MarimayMirBlock *block = mmay_vector_at(
        &fixture->function.blocks,
        fixture->block
    );
    return block != NULL ? mmay_vector_at(&block->instructions, index) : NULL;
}

static bool mmay_test_mir_interface_mutate(
    MarimayTestMirInterfaceFixture *fixture,
    MarimayTestMirInterfaceMalformed kind
) {
    MarimayMirInterface *layout = mmay_vector_at(
        &fixture->function.interfaces,
        fixture->interface_id
    );
    MarimayMirInterfaceConstruction *construction = mmay_vector_at(
        &fixture->function.interface_constructions,
        fixture->construction_id
    );
    MarimayMirCall *call = mmay_vector_at(
        &fixture->function.calls,
        fixture->call_id
    );
    MarimayMirCallArgument *receiver = call != NULL
        ? mmay_vector_at(
            &fixture->function.call_arguments,
            call->argument_start
        )
        : NULL;
    MarimayMirCallArgument *argument = call != NULL
        ? mmay_vector_at(
            &fixture->function.call_arguments,
            call->argument_start + 1
        )
        : NULL;
    MarimayMirInstruction *interface_new =
        mmay_test_mir_interface_instruction(fixture, 0);
    MarimayMirInstruction *interface_call =
        mmay_test_mir_interface_instruction(fixture, 1);
    MarimayMirRegister extra = MMAY_MIR_NO_REGISTER;

    if (layout == NULL || construction == NULL || call == NULL
        || receiver == NULL || argument == NULL || interface_new == NULL
        || interface_call == NULL) {
        return false;
    }
    switch (kind) {
        case MMAY_TEST_MIR_INTERFACE_LAYOUT_CAP:
            fixture->function.options.max_interface_layouts = 0;
            return true;
        case MMAY_TEST_MIR_INTERFACE_CONSTRUCTION_CAP:
            fixture->function.options.max_interface_constructions = 0;
            return true;
        case MMAY_TEST_MIR_INTERFACE_METHOD_CAP:
            fixture->function.options.max_interface_methods = 0;
            return true;
        case MMAY_TEST_MIR_INTERFACE_LAYOUT_SCALAR_TYPE:
            layout->type = MMAY_TYPE_I64;
            return true;
        case MMAY_TEST_MIR_INTERFACE_LAYOUT_DUPLICATE_TYPE:
            layout->type = MMAY_TEST_MIR_RECORD_TYPE;
            return true;
        case MMAY_TEST_MIR_INTERFACE_CONSTRUCTION_OOB:
            interface_new->interface_construction =
                (MarimayMirInterfaceConstructionId)
                    fixture->function.interface_constructions.length;
            return true;
        case MMAY_TEST_MIR_INTERFACE_CONSTRUCTION_LAYOUT_OOB:
            construction->interface_id = (MarimayMirInterfaceId)
                fixture->function.interfaces.length;
            return true;
        case MMAY_TEST_MIR_INTERFACE_METHOD_START:
            construction->method_start = 1;
            return true;
        case MMAY_TEST_MIR_INTERFACE_METHOD_COUNT:
            construction->method_count = 0;
            return true;
        case MMAY_TEST_MIR_INTERFACE_METHOD_RANGE:
            fixture->function.interface_methods.length = 0;
            return true;
        case MMAY_TEST_MIR_INTERFACE_PAYLOAD_OOB:
            construction->payload = (MarimayMirRegister)
                fixture->function.register_types.length;
            return true;
        case MMAY_TEST_MIR_INTERFACE_PAYLOAD_DECLARED_TYPE:
            construction->payload_type = MMAY_TYPE_I64;
            return true;
        case MMAY_TEST_MIR_INTERFACE_PAYLOAD_SCALAR:
            construction->payload = fixture->argument;
            construction->payload_type = MMAY_TYPE_I64;
            return true;
        case MMAY_TEST_MIR_INTERFACE_PAYLOAD_ARRAY:
            construction->payload = fixture->array_value;
            construction->payload_type = MMAY_TEST_MIR_ARRAY_TYPE;
            return true;
        case MMAY_TEST_MIR_INTERFACE_PAYLOAD_UNINITIALIZED:
            if (!mmay_mir_add_register(
                    &fixture->function,
                    MMAY_TEST_MIR_RECORD_TYPE,
                    &extra
                )) {
                return false;
            }
            construction->payload = extra;
            return true;
        case MMAY_TEST_MIR_INTERFACE_TARGET_OOB:
            interface_new->target = (MarimayMirRegister)
                fixture->function.register_types.length;
            return true;
        case MMAY_TEST_MIR_INTERFACE_TARGET_TYPE:
            interface_new->target = fixture->payload;
            return true;
        case MMAY_TEST_MIR_INTERFACE_CALL_RANGE:
            call->argument_start = fixture->function.call_arguments.length;
            return true;
        case MMAY_TEST_MIR_INTERFACE_CALL_EMPTY:
            call->argument_count = 0;
            return true;
        case MMAY_TEST_MIR_INTERFACE_CALL_RECEIVER_RECORD:
            receiver->register_id = fixture->payload;
            receiver->type = MMAY_TEST_MIR_RECORD_TYPE;
            return true;
        case MMAY_TEST_MIR_INTERFACE_CALL_SLOT_UNSET:
            call->interface_slot = UINT32_MAX;
            return true;
        case MMAY_TEST_MIR_INTERFACE_CALL_SLOT_OOB:
            call->interface_slot = (uint32_t)layout->method_count;
            return true;
        case MMAY_TEST_MIR_INTERFACE_CALL_ARGUMENT_OOB:
            argument->register_id = (MarimayMirRegister)
                fixture->function.register_types.length;
            return true;
        case MMAY_TEST_MIR_INTERFACE_CALL_ARGUMENT_TYPE:
            argument->type = MMAY_TYPE_BOOL;
            return true;
        case MMAY_TEST_MIR_INTERFACE_CALL_RESULT_TYPE:
            call->result_type = MMAY_TYPE_BOOL;
            return true;
        case MMAY_TEST_MIR_INTERFACE_CALL_RECEIVER_UNINITIALIZED:
            if (!mmay_mir_add_register(
                    &fixture->function,
                    MMAY_TEST_MIR_INTERFACE_TYPE,
                    &extra
                )) {
                return false;
            }
            receiver->register_id = extra;
            return true;
        case MMAY_TEST_MIR_INTERFACE_CALL_ARGUMENT_UNINITIALIZED:
            if (!mmay_mir_add_register(
                    &fixture->function,
                    MMAY_TYPE_I64,
                    &extra
                )) {
                return false;
            }
            argument->register_id = extra;
            return true;
        case MMAY_TEST_MIR_INTERFACE_DIRECT_CALL_WITH_SLOT:
            interface_call->kind = MMAY_MIR_CALL;
            return true;
    }
    return false;
}

static void mmay_test_mir_interface_malformed_matrix(MarimaySymbolId symbol) {
    static const MarimayTestMirInterfaceMalformedCase cases[] = {
        {MMAY_TEST_MIR_INTERFACE_LAYOUT_CAP, MMAY_MIR_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_MIR_INTERFACE_CONSTRUCTION_CAP, MMAY_MIR_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_MIR_INTERFACE_METHOD_CAP, MMAY_MIR_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_MIR_INTERFACE_LAYOUT_SCALAR_TYPE, MMAY_MIR_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_MIR_INTERFACE_LAYOUT_DUPLICATE_TYPE, MMAY_MIR_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_MIR_INTERFACE_CONSTRUCTION_OOB, MMAY_MIR_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_MIR_INTERFACE_CONSTRUCTION_LAYOUT_OOB, MMAY_MIR_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_MIR_INTERFACE_METHOD_START, MMAY_MIR_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_MIR_INTERFACE_METHOD_COUNT, MMAY_MIR_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_MIR_INTERFACE_METHOD_RANGE, MMAY_MIR_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_MIR_INTERFACE_PAYLOAD_OOB, MMAY_MIR_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_MIR_INTERFACE_PAYLOAD_DECLARED_TYPE, MMAY_MIR_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_MIR_INTERFACE_PAYLOAD_SCALAR, MMAY_MIR_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_MIR_INTERFACE_PAYLOAD_ARRAY, MMAY_MIR_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_MIR_INTERFACE_PAYLOAD_UNINITIALIZED, MMAY_MIR_VERIFY_UNINITIALIZED_REGISTER},
        {MMAY_TEST_MIR_INTERFACE_TARGET_OOB, MMAY_MIR_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_MIR_INTERFACE_TARGET_TYPE, MMAY_MIR_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_MIR_INTERFACE_CALL_RANGE, MMAY_MIR_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_MIR_INTERFACE_CALL_EMPTY, MMAY_MIR_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_MIR_INTERFACE_CALL_RECEIVER_RECORD, MMAY_MIR_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_MIR_INTERFACE_CALL_SLOT_UNSET, MMAY_MIR_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_MIR_INTERFACE_CALL_SLOT_OOB, MMAY_MIR_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_MIR_INTERFACE_CALL_ARGUMENT_OOB, MMAY_MIR_VERIFY_INVALID_REGISTER},
        {MMAY_TEST_MIR_INTERFACE_CALL_ARGUMENT_TYPE, MMAY_MIR_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_MIR_INTERFACE_CALL_RESULT_TYPE, MMAY_MIR_VERIFY_TYPE_MISMATCH},
        {MMAY_TEST_MIR_INTERFACE_CALL_RECEIVER_UNINITIALIZED, MMAY_MIR_VERIFY_UNINITIALIZED_REGISTER},
        {MMAY_TEST_MIR_INTERFACE_CALL_ARGUMENT_UNINITIALIZED, MMAY_MIR_VERIFY_UNINITIALIZED_REGISTER},
        {MMAY_TEST_MIR_INTERFACE_DIRECT_CALL_WITH_SLOT, MMAY_MIR_VERIFY_INVALID_ARGUMENT}
    };
    size_t index = 0;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        MarimayTestMirInterfaceFixture fixture;
        MarimayMirVerifyStatus status = MMAY_MIR_VERIFY_OK;
        MMAY_EXPECT(mmay_test_mir_interface_fixture_init(
            &fixture,
            mmay_system_allocator(),
            symbol,
            false,
            1
        ));
        MMAY_EXPECT(mmay_test_mir_interface_mutate(
            &fixture,
            cases[index].kind
        ));
        status = mmay_mir_verify(&fixture.function);
        MMAY_EXPECT(status == cases[index].expected);
        mmay_mir_function_dispose(&fixture.function);
    }
}

static size_t mmay_test_mir_interface_minimum_work(
    MarimaySymbolId symbol,
    size_t method_count
) {
    MarimayTestMirInterfaceFixture fixture;
    size_t limit = 0;
    MMAY_EXPECT(mmay_test_mir_interface_fixture_init(
        &fixture,
        mmay_system_allocator(),
        symbol,
        false,
        method_count
    ));
    for (limit = 1; limit < 10000; limit += 1) {
        MarimayMirVerifyStatus status;
        fixture.function.options.max_verify_work = limit;
        status = mmay_mir_verify(&fixture.function);
        if (status == MMAY_MIR_VERIFY_OK) {
            break;
        }
        MMAY_EXPECT(status == MMAY_MIR_VERIFY_LIMIT_REACHED);
        if (status != MMAY_MIR_VERIFY_LIMIT_REACHED) {
            limit = 0;
            break;
        }
    }
    MMAY_EXPECT(limit != 0 && limit < 10000);
    mmay_mir_function_dispose(&fixture.function);
    return limit;
}

static size_t mmay_test_mir_interface_verify_allocations(
    MarimaySymbolId symbol
) {
    MarimayFailAllocator state;
    MarimayTestMirInterfaceFixture fixture;
    size_t attempts_before = 0;
    size_t attempts = 0;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_test_mir_interface_fixture_init(
        &fixture,
        mmay_fail_allocator(&state),
        symbol,
        false,
        1
    ));
    attempts_before = state.attempts;
    MMAY_EXPECT(mmay_mir_verify(&fixture.function) == MMAY_MIR_VERIFY_OK);
    attempts = state.attempts - attempts_before;
    MMAY_EXPECT(attempts != 0);
    mmay_mir_function_dispose(&fixture.function);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
    return attempts;
}

static void mmay_test_mir_interface_allocation_failures(
    MarimaySymbolId symbol
) {
    const size_t attempts = mmay_test_mir_interface_verify_allocations(symbol);
    size_t failure_index = 0;
    for (failure_index = 0; failure_index <= attempts; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayTestMirInterfaceFixture fixture;
        MarimayMirVerifyStatus status;
        size_t baseline_allocations = 0;
        size_t baseline_bytes = 0;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_test_mir_interface_fixture_init(
            &fixture,
            mmay_fail_allocator(&state),
            symbol,
            false,
            1
        ));
        baseline_allocations = state.live_allocations;
        baseline_bytes = state.live_bytes;
        state.failure_index = state.attempts + failure_index;
        status = mmay_mir_verify(&fixture.function);
        MMAY_EXPECT(status == (failure_index < attempts
            ? MMAY_MIR_VERIFY_ALLOCATION_FAILED
            : MMAY_MIR_VERIFY_OK));
        MMAY_EXPECT(state.live_allocations == baseline_allocations);
        MMAY_EXPECT(state.live_bytes == baseline_bytes);
        mmay_mir_function_dispose(&fixture.function);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
}

static void mmay_test_mir_interface_backend_preflight(MarimaySymbolId symbol) {
    MarimayTestMirInterfaceFixture fixture;
    MarimayBytecodeFunction bytecode;
    const MarimayMirFunction *functions[1];
    MarimayNativeCProgram program;
    MarimayOwnedBytes generated;
    memset(&bytecode, 0, sizeof(bytecode));
    memset(&generated, 0, sizeof(generated));
    MMAY_EXPECT(mmay_test_mir_interface_fixture_init(
        &fixture,
        mmay_system_allocator(),
        symbol,
        false,
        1
    ));
    MMAY_EXPECT(mmay_test_mir_interface_mutate(
        &fixture,
        MMAY_TEST_MIR_INTERFACE_CALL_SLOT_OOB
    ));
    MMAY_EXPECT(
        mmay_mir_verify(&fixture.function) == MMAY_MIR_VERIFY_INVALID_ARGUMENT
    );
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(
        &fixture.function,
        mmay_system_allocator(),
        mmay_mir_bytecode_lower_default_options(),
        &bytecode
    ) == MMAY_MIR_BYTECODE_LOWER_INVALID_MIR);
    MMAY_EXPECT(!bytecode.initialized);

    functions[0] = &fixture.function;
    program.functions = functions;
    program.function_count = 1;
    program.entry_symbol = symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &program,
        mmay_system_allocator(),
        mmay_native_c_emit_default_options(),
        &generated
    ) == MMAY_NATIVE_C_EMIT_INVALID_MIR);
    MMAY_EXPECT(generated.data == NULL && generated.length == 0);
    mmay_owned_bytes_dispose(&generated);
    mmay_bytecode_function_dispose(&bytecode);
    mmay_mir_function_dispose(&fixture.function);
}

void mmay_test_mir_interfaces(void) {
    MarimaySymbolId symbol;
    MarimayTestMirInterfaceFixture fixture;
    size_t one_method_work = 0;
    size_t two_method_work = 0;
    MMAY_EXPECT(mmay_test_mir_interface_symbol("entry", 0, &symbol));

    MMAY_EXPECT(mmay_test_mir_interface_fixture_init(
        &fixture,
        mmay_system_allocator(),
        symbol,
        false,
        1
    ));
    MMAY_EXPECT(mmay_mir_verify(&fixture.function) == MMAY_MIR_VERIFY_OK);
    mmay_mir_function_dispose(&fixture.function);

    MMAY_EXPECT(mmay_test_mir_interface_fixture_init(
        &fixture,
        mmay_system_allocator(),
        symbol,
        true,
        1
    ));
    MMAY_EXPECT(mmay_mir_verify(&fixture.function) == MMAY_MIR_VERIFY_OK);
    mmay_mir_function_dispose(&fixture.function);

    mmay_test_mir_interface_malformed_matrix(symbol);
    one_method_work = mmay_test_mir_interface_minimum_work(symbol, 1);
    two_method_work = mmay_test_mir_interface_minimum_work(symbol, 2);
    MMAY_EXPECT(two_method_work > one_method_work);
    mmay_test_mir_interface_allocation_failures(symbol);
    mmay_test_mir_interface_backend_preflight(symbol);
}
