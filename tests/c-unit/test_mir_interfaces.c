#include "test_mir_support.h"

#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/bytecode/mir_lower.h"
#include "ruda/native/c_emitter.h"

enum {
    RUDA_TEST_MIR_RECORD_TYPE = RUDA_TYPE_FIRST_DYNAMIC,
    RUDA_TEST_MIR_ENUM_TYPE,
    RUDA_TEST_MIR_INTERFACE_TYPE,
    RUDA_TEST_MIR_ARRAY_TYPE
};

typedef struct RudaTestMirInterfaceFixture {
    RudaMirFunction function;
    RudaMirBlockId block;
    RudaMirRegister payload;
    RudaMirRegister argument;
    RudaMirRegister interface_value;
    RudaMirRegister result;
    RudaMirRegister array_value;
    RudaMirInterfaceId interface_id;
    RudaMirInterfaceConstructionId construction_id;
    RudaMirCallId call_id;
} RudaTestMirInterfaceFixture;

typedef enum RudaTestMirInterfaceMalformed {
    RUDA_TEST_MIR_INTERFACE_LAYOUT_CAP,
    RUDA_TEST_MIR_INTERFACE_CONSTRUCTION_CAP,
    RUDA_TEST_MIR_INTERFACE_METHOD_CAP,
    RUDA_TEST_MIR_INTERFACE_LAYOUT_SCALAR_TYPE,
    RUDA_TEST_MIR_INTERFACE_LAYOUT_DUPLICATE_TYPE,
    RUDA_TEST_MIR_INTERFACE_CONSTRUCTION_OOB,
    RUDA_TEST_MIR_INTERFACE_CONSTRUCTION_LAYOUT_OOB,
    RUDA_TEST_MIR_INTERFACE_METHOD_START,
    RUDA_TEST_MIR_INTERFACE_METHOD_COUNT,
    RUDA_TEST_MIR_INTERFACE_METHOD_RANGE,
    RUDA_TEST_MIR_INTERFACE_PAYLOAD_OOB,
    RUDA_TEST_MIR_INTERFACE_PAYLOAD_DECLARED_TYPE,
    RUDA_TEST_MIR_INTERFACE_PAYLOAD_SCALAR,
    RUDA_TEST_MIR_INTERFACE_PAYLOAD_ARRAY,
    RUDA_TEST_MIR_INTERFACE_PAYLOAD_UNINITIALIZED,
    RUDA_TEST_MIR_INTERFACE_TARGET_OOB,
    RUDA_TEST_MIR_INTERFACE_TARGET_TYPE,
    RUDA_TEST_MIR_INTERFACE_CALL_RANGE,
    RUDA_TEST_MIR_INTERFACE_CALL_EMPTY,
    RUDA_TEST_MIR_INTERFACE_CALL_RECEIVER_RECORD,
    RUDA_TEST_MIR_INTERFACE_CALL_SLOT_UNSET,
    RUDA_TEST_MIR_INTERFACE_CALL_SLOT_OOB,
    RUDA_TEST_MIR_INTERFACE_CALL_ARGUMENT_OOB,
    RUDA_TEST_MIR_INTERFACE_CALL_ARGUMENT_TYPE,
    RUDA_TEST_MIR_INTERFACE_CALL_RESULT_TYPE,
    RUDA_TEST_MIR_INTERFACE_CALL_RECEIVER_UNINITIALIZED,
    RUDA_TEST_MIR_INTERFACE_CALL_ARGUMENT_UNINITIALIZED,
    RUDA_TEST_MIR_INTERFACE_DIRECT_CALL_WITH_SLOT
} RudaTestMirInterfaceMalformed;

typedef struct RudaTestMirInterfaceMalformedCase {
    RudaTestMirInterfaceMalformed kind;
    RudaMirVerifyStatus expected;
} RudaTestMirInterfaceMalformedCase;

static bool ruda_test_mir_interface_symbol(
    const char *name,
    uint32_t discriminator,
    RudaSymbolId *result
) {
    return ruda_symbol_id_from_parts(
        "example.test/mir-interfaces",
        "src/interfaces.ruda",
        name,
        discriminator,
        result
    );
}

static bool ruda_test_mir_interface_fixture_init(
    RudaTestMirInterfaceFixture *fixture,
    RudaAllocator allocator,
    RudaSymbolId symbol,
    bool enum_payload,
    size_t method_count
) {
    RudaTypeId payload_type = enum_payload
        ? RUDA_TEST_MIR_ENUM_TYPE
        : RUDA_TEST_MIR_RECORD_TYPE;
    RudaTypeId parameters[2] = {payload_type, RUDA_TYPE_I64};
    RudaMirRecordId record_id = RUDA_MIR_NO_RECORD;
    RudaMirEnumId enum_id = RUDA_MIR_NO_ENUM;
    RudaMirArrayId array_id = RUDA_MIR_NO_ARRAY;
    RudaMirEnumVariant enum_variant;
    RudaMirCallArgument arguments[2];
    RudaSymbolId methods[2];
    RudaMirInstruction instruction;
    size_t method_index = 0;
    bool ok = false;

    if (fixture == NULL || method_count == 0 || method_count > 2) {
        return false;
    }
    memset(fixture, 0, sizeof(*fixture));
    fixture->block = RUDA_MIR_NO_BLOCK;
    fixture->payload = RUDA_MIR_NO_REGISTER;
    fixture->argument = RUDA_MIR_NO_REGISTER;
    fixture->interface_value = RUDA_MIR_NO_REGISTER;
    fixture->result = RUDA_MIR_NO_REGISTER;
    fixture->array_value = RUDA_MIR_NO_REGISTER;
    fixture->interface_id = RUDA_MIR_NO_INTERFACE;
    fixture->construction_id = RUDA_MIR_NO_INTERFACE_CONSTRUCTION;
    fixture->call_id = RUDA_MIR_NO_CALL;

    for (method_index = 0; method_index < method_count; method_index += 1) {
        if (!ruda_test_mir_interface_symbol(
                method_index == 0 ? "method-a" : "method-b",
                (uint32_t)(method_index + 1),
                &methods[method_index]
            )) {
            return false;
        }
    }
    if (!ruda_mir_function_init(
            &fixture->function,
            allocator,
            symbol,
            parameters,
            2,
            RUDA_TYPE_I64,
            ruda_mir_function_default_options()
        )) {
        return false;
    }
    fixture->payload = 0;
    fixture->argument = 1;
    if (enum_payload) {
        enum_variant.kind = RUDA_MIR_ENUM_VARIANT_UNIT;
        enum_variant.payload_start = 0;
        enum_variant.payload_count = 0;
        ok = ruda_mir_add_enum(
            &fixture->function,
            payload_type,
            &enum_variant,
            1,
            NULL,
            0,
            &enum_id
        );
    } else {
        ok = ruda_mir_add_record(
            &fixture->function,
            payload_type,
            NULL,
            0,
            &record_id
        );
    }
    ok = ok && ruda_mir_add_array(
        &fixture->function,
        RUDA_TEST_MIR_ARRAY_TYPE,
        RUDA_TYPE_I64,
        &array_id
    ) && ruda_mir_add_interface(
        &fixture->function,
        RUDA_TEST_MIR_INTERFACE_TYPE,
        method_count,
        &fixture->interface_id
    ) && ruda_mir_add_register(
        &fixture->function,
        RUDA_TEST_MIR_INTERFACE_TYPE,
        &fixture->interface_value
    ) && ruda_mir_add_register(
        &fixture->function,
        RUDA_TYPE_I64,
        &fixture->result
    ) && ruda_mir_add_register(
        &fixture->function,
        RUDA_TEST_MIR_ARRAY_TYPE,
        &fixture->array_value
    ) && ruda_mir_add_block(
        &fixture->function,
        &fixture->block
    ) && ruda_mir_add_interface_construction(
        &fixture->function,
        fixture->interface_id,
        fixture->payload,
        payload_type,
        methods,
        method_count,
        &fixture->construction_id
    );
    if (!ok) {
        ruda_mir_function_dispose(&fixture->function);
        return false;
    }

    arguments[0].register_id = fixture->interface_value;
    arguments[0].type = RUDA_TEST_MIR_INTERFACE_TYPE;
    arguments[1].register_id = fixture->argument;
    arguments[1].type = RUDA_TYPE_I64;
    if (!ruda_mir_add_call(
            &fixture->function,
            symbol,
            RUDA_TYPE_I64,
            arguments,
            2,
            &fixture->call_id
        ) || !ruda_mir_set_call_interface_slot(
            &fixture->function,
            fixture->call_id,
            0
        )) {
        ruda_mir_function_dispose(&fixture->function);
        return false;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_INTERFACE_NEW;
    instruction.target = fixture->interface_value;
    instruction.interface_construction = fixture->construction_id;
    if (!ruda_mir_append_instruction(
            &fixture->function,
            fixture->block,
            &instruction
        )) {
        ruda_mir_function_dispose(&fixture->function);
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CALL_INTERFACE;
    instruction.target = fixture->result;
    instruction.call = fixture->call_id;
    if (!ruda_mir_append_instruction(
            &fixture->function,
            fixture->block,
            &instruction
        ) || !ruda_mir_set_terminator(
            &fixture->function,
            fixture->block,
            ruda_test_mir_return(fixture->result)
        )) {
        ruda_mir_function_dispose(&fixture->function);
        return false;
    }
    return true;
}

static RudaMirInstruction *ruda_test_mir_interface_instruction(
    RudaTestMirInterfaceFixture *fixture,
    size_t index
) {
    RudaMirBlock *block = ruda_vector_at(
        &fixture->function.blocks,
        fixture->block
    );
    return block != NULL ? ruda_vector_at(&block->instructions, index) : NULL;
}

static bool ruda_test_mir_interface_mutate(
    RudaTestMirInterfaceFixture *fixture,
    RudaTestMirInterfaceMalformed kind
) {
    RudaMirInterface *layout = ruda_vector_at(
        &fixture->function.interfaces,
        fixture->interface_id
    );
    RudaMirInterfaceConstruction *construction = ruda_vector_at(
        &fixture->function.interface_constructions,
        fixture->construction_id
    );
    RudaMirCall *call = ruda_vector_at(
        &fixture->function.calls,
        fixture->call_id
    );
    RudaMirCallArgument *receiver = call != NULL
        ? ruda_vector_at(
            &fixture->function.call_arguments,
            call->argument_start
        )
        : NULL;
    RudaMirCallArgument *argument = call != NULL
        ? ruda_vector_at(
            &fixture->function.call_arguments,
            call->argument_start + 1
        )
        : NULL;
    RudaMirInstruction *interface_new =
        ruda_test_mir_interface_instruction(fixture, 0);
    RudaMirInstruction *interface_call =
        ruda_test_mir_interface_instruction(fixture, 1);
    RudaMirRegister extra = RUDA_MIR_NO_REGISTER;

    if (layout == NULL || construction == NULL || call == NULL
        || receiver == NULL || argument == NULL || interface_new == NULL
        || interface_call == NULL) {
        return false;
    }
    switch (kind) {
        case RUDA_TEST_MIR_INTERFACE_LAYOUT_CAP:
            fixture->function.options.max_interface_layouts = 0;
            return true;
        case RUDA_TEST_MIR_INTERFACE_CONSTRUCTION_CAP:
            fixture->function.options.max_interface_constructions = 0;
            return true;
        case RUDA_TEST_MIR_INTERFACE_METHOD_CAP:
            fixture->function.options.max_interface_methods = 0;
            return true;
        case RUDA_TEST_MIR_INTERFACE_LAYOUT_SCALAR_TYPE:
            layout->type = RUDA_TYPE_I64;
            return true;
        case RUDA_TEST_MIR_INTERFACE_LAYOUT_DUPLICATE_TYPE:
            layout->type = RUDA_TEST_MIR_RECORD_TYPE;
            return true;
        case RUDA_TEST_MIR_INTERFACE_CONSTRUCTION_OOB:
            interface_new->interface_construction =
                (RudaMirInterfaceConstructionId)
                    fixture->function.interface_constructions.length;
            return true;
        case RUDA_TEST_MIR_INTERFACE_CONSTRUCTION_LAYOUT_OOB:
            construction->interface_id = (RudaMirInterfaceId)
                fixture->function.interfaces.length;
            return true;
        case RUDA_TEST_MIR_INTERFACE_METHOD_START:
            construction->method_start = 1;
            return true;
        case RUDA_TEST_MIR_INTERFACE_METHOD_COUNT:
            construction->method_count = 0;
            return true;
        case RUDA_TEST_MIR_INTERFACE_METHOD_RANGE:
            fixture->function.interface_methods.length = 0;
            return true;
        case RUDA_TEST_MIR_INTERFACE_PAYLOAD_OOB:
            construction->payload = (RudaMirRegister)
                fixture->function.register_types.length;
            return true;
        case RUDA_TEST_MIR_INTERFACE_PAYLOAD_DECLARED_TYPE:
            construction->payload_type = RUDA_TYPE_I64;
            return true;
        case RUDA_TEST_MIR_INTERFACE_PAYLOAD_SCALAR:
            construction->payload = fixture->argument;
            construction->payload_type = RUDA_TYPE_I64;
            return true;
        case RUDA_TEST_MIR_INTERFACE_PAYLOAD_ARRAY:
            construction->payload = fixture->array_value;
            construction->payload_type = RUDA_TEST_MIR_ARRAY_TYPE;
            return true;
        case RUDA_TEST_MIR_INTERFACE_PAYLOAD_UNINITIALIZED:
            if (!ruda_mir_add_register(
                    &fixture->function,
                    RUDA_TEST_MIR_RECORD_TYPE,
                    &extra
                )) {
                return false;
            }
            construction->payload = extra;
            return true;
        case RUDA_TEST_MIR_INTERFACE_TARGET_OOB:
            interface_new->target = (RudaMirRegister)
                fixture->function.register_types.length;
            return true;
        case RUDA_TEST_MIR_INTERFACE_TARGET_TYPE:
            interface_new->target = fixture->payload;
            return true;
        case RUDA_TEST_MIR_INTERFACE_CALL_RANGE:
            call->argument_start = fixture->function.call_arguments.length;
            return true;
        case RUDA_TEST_MIR_INTERFACE_CALL_EMPTY:
            call->argument_count = 0;
            return true;
        case RUDA_TEST_MIR_INTERFACE_CALL_RECEIVER_RECORD:
            receiver->register_id = fixture->payload;
            receiver->type = RUDA_TEST_MIR_RECORD_TYPE;
            return true;
        case RUDA_TEST_MIR_INTERFACE_CALL_SLOT_UNSET:
            call->interface_slot = UINT32_MAX;
            return true;
        case RUDA_TEST_MIR_INTERFACE_CALL_SLOT_OOB:
            call->interface_slot = (uint32_t)layout->method_count;
            return true;
        case RUDA_TEST_MIR_INTERFACE_CALL_ARGUMENT_OOB:
            argument->register_id = (RudaMirRegister)
                fixture->function.register_types.length;
            return true;
        case RUDA_TEST_MIR_INTERFACE_CALL_ARGUMENT_TYPE:
            argument->type = RUDA_TYPE_BOOL;
            return true;
        case RUDA_TEST_MIR_INTERFACE_CALL_RESULT_TYPE:
            call->result_type = RUDA_TYPE_BOOL;
            return true;
        case RUDA_TEST_MIR_INTERFACE_CALL_RECEIVER_UNINITIALIZED:
            if (!ruda_mir_add_register(
                    &fixture->function,
                    RUDA_TEST_MIR_INTERFACE_TYPE,
                    &extra
                )) {
                return false;
            }
            receiver->register_id = extra;
            return true;
        case RUDA_TEST_MIR_INTERFACE_CALL_ARGUMENT_UNINITIALIZED:
            if (!ruda_mir_add_register(
                    &fixture->function,
                    RUDA_TYPE_I64,
                    &extra
                )) {
                return false;
            }
            argument->register_id = extra;
            return true;
        case RUDA_TEST_MIR_INTERFACE_DIRECT_CALL_WITH_SLOT:
            interface_call->kind = RUDA_MIR_CALL;
            return true;
    }
    return false;
}

static void ruda_test_mir_interface_malformed_matrix(RudaSymbolId symbol) {
    static const RudaTestMirInterfaceMalformedCase cases[] = {
        {RUDA_TEST_MIR_INTERFACE_LAYOUT_CAP, RUDA_MIR_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_MIR_INTERFACE_CONSTRUCTION_CAP, RUDA_MIR_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_MIR_INTERFACE_METHOD_CAP, RUDA_MIR_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_MIR_INTERFACE_LAYOUT_SCALAR_TYPE, RUDA_MIR_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_MIR_INTERFACE_LAYOUT_DUPLICATE_TYPE, RUDA_MIR_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_MIR_INTERFACE_CONSTRUCTION_OOB, RUDA_MIR_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_MIR_INTERFACE_CONSTRUCTION_LAYOUT_OOB, RUDA_MIR_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_MIR_INTERFACE_METHOD_START, RUDA_MIR_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_MIR_INTERFACE_METHOD_COUNT, RUDA_MIR_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_MIR_INTERFACE_METHOD_RANGE, RUDA_MIR_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_MIR_INTERFACE_PAYLOAD_OOB, RUDA_MIR_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_MIR_INTERFACE_PAYLOAD_DECLARED_TYPE, RUDA_MIR_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_MIR_INTERFACE_PAYLOAD_SCALAR, RUDA_MIR_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_MIR_INTERFACE_PAYLOAD_ARRAY, RUDA_MIR_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_MIR_INTERFACE_PAYLOAD_UNINITIALIZED, RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER},
        {RUDA_TEST_MIR_INTERFACE_TARGET_OOB, RUDA_MIR_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_MIR_INTERFACE_TARGET_TYPE, RUDA_MIR_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_MIR_INTERFACE_CALL_RANGE, RUDA_MIR_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_MIR_INTERFACE_CALL_EMPTY, RUDA_MIR_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_MIR_INTERFACE_CALL_RECEIVER_RECORD, RUDA_MIR_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_MIR_INTERFACE_CALL_SLOT_UNSET, RUDA_MIR_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_MIR_INTERFACE_CALL_SLOT_OOB, RUDA_MIR_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_MIR_INTERFACE_CALL_ARGUMENT_OOB, RUDA_MIR_VERIFY_INVALID_REGISTER},
        {RUDA_TEST_MIR_INTERFACE_CALL_ARGUMENT_TYPE, RUDA_MIR_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_MIR_INTERFACE_CALL_RESULT_TYPE, RUDA_MIR_VERIFY_TYPE_MISMATCH},
        {RUDA_TEST_MIR_INTERFACE_CALL_RECEIVER_UNINITIALIZED, RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER},
        {RUDA_TEST_MIR_INTERFACE_CALL_ARGUMENT_UNINITIALIZED, RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER},
        {RUDA_TEST_MIR_INTERFACE_DIRECT_CALL_WITH_SLOT, RUDA_MIR_VERIFY_INVALID_ARGUMENT}
    };
    size_t index = 0;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        RudaTestMirInterfaceFixture fixture;
        RudaMirVerifyStatus status = RUDA_MIR_VERIFY_OK;
        RUDA_EXPECT(ruda_test_mir_interface_fixture_init(
            &fixture,
            ruda_system_allocator(),
            symbol,
            false,
            1
        ));
        RUDA_EXPECT(ruda_test_mir_interface_mutate(
            &fixture,
            cases[index].kind
        ));
        status = ruda_mir_verify(&fixture.function);
        RUDA_EXPECT(status == cases[index].expected);
        ruda_mir_function_dispose(&fixture.function);
    }
}

static size_t ruda_test_mir_interface_minimum_work(
    RudaSymbolId symbol,
    size_t method_count
) {
    RudaTestMirInterfaceFixture fixture;
    size_t limit = 0;
    RUDA_EXPECT(ruda_test_mir_interface_fixture_init(
        &fixture,
        ruda_system_allocator(),
        symbol,
        false,
        method_count
    ));
    for (limit = 1; limit < 10000; limit += 1) {
        RudaMirVerifyStatus status;
        fixture.function.options.max_verify_work = limit;
        status = ruda_mir_verify(&fixture.function);
        if (status == RUDA_MIR_VERIFY_OK) {
            break;
        }
        RUDA_EXPECT(status == RUDA_MIR_VERIFY_LIMIT_REACHED);
        if (status != RUDA_MIR_VERIFY_LIMIT_REACHED) {
            limit = 0;
            break;
        }
    }
    RUDA_EXPECT(limit != 0 && limit < 10000);
    ruda_mir_function_dispose(&fixture.function);
    return limit;
}

static size_t ruda_test_mir_interface_verify_allocations(
    RudaSymbolId symbol
) {
    RudaFailAllocator state;
    RudaTestMirInterfaceFixture fixture;
    size_t attempts_before = 0;
    size_t attempts = 0;
    ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
    RUDA_EXPECT(ruda_test_mir_interface_fixture_init(
        &fixture,
        ruda_fail_allocator(&state),
        symbol,
        false,
        1
    ));
    attempts_before = state.attempts;
    RUDA_EXPECT(ruda_mir_verify(&fixture.function) == RUDA_MIR_VERIFY_OK);
    attempts = state.attempts - attempts_before;
    RUDA_EXPECT(attempts != 0);
    ruda_mir_function_dispose(&fixture.function);
    RUDA_EXPECT(state.live_allocations == 0);
    RUDA_EXPECT(state.live_bytes == 0);
    return attempts;
}

static void ruda_test_mir_interface_allocation_failures(
    RudaSymbolId symbol
) {
    const size_t attempts = ruda_test_mir_interface_verify_allocations(symbol);
    size_t failure_index = 0;
    for (failure_index = 0; failure_index <= attempts; failure_index += 1) {
        RudaFailAllocator state;
        RudaTestMirInterfaceFixture fixture;
        RudaMirVerifyStatus status;
        size_t baseline_allocations = 0;
        size_t baseline_bytes = 0;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        RUDA_EXPECT(ruda_test_mir_interface_fixture_init(
            &fixture,
            ruda_fail_allocator(&state),
            symbol,
            false,
            1
        ));
        baseline_allocations = state.live_allocations;
        baseline_bytes = state.live_bytes;
        state.failure_index = state.attempts + failure_index;
        status = ruda_mir_verify(&fixture.function);
        RUDA_EXPECT(status == (failure_index < attempts
            ? RUDA_MIR_VERIFY_ALLOCATION_FAILED
            : RUDA_MIR_VERIFY_OK));
        RUDA_EXPECT(state.live_allocations == baseline_allocations);
        RUDA_EXPECT(state.live_bytes == baseline_bytes);
        ruda_mir_function_dispose(&fixture.function);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
}

static void ruda_test_mir_interface_backend_preflight(RudaSymbolId symbol) {
    RudaTestMirInterfaceFixture fixture;
    RudaBytecodeFunction bytecode;
    const RudaMirFunction *functions[1];
    RudaNativeCProgram program;
    RudaOwnedBytes generated;
    memset(&bytecode, 0, sizeof(bytecode));
    memset(&generated, 0, sizeof(generated));
    RUDA_EXPECT(ruda_test_mir_interface_fixture_init(
        &fixture,
        ruda_system_allocator(),
        symbol,
        false,
        1
    ));
    RUDA_EXPECT(ruda_test_mir_interface_mutate(
        &fixture,
        RUDA_TEST_MIR_INTERFACE_CALL_SLOT_OOB
    ));
    RUDA_EXPECT(
        ruda_mir_verify(&fixture.function) == RUDA_MIR_VERIFY_INVALID_ARGUMENT
    );
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(
        &fixture.function,
        ruda_system_allocator(),
        ruda_mir_bytecode_lower_default_options(),
        &bytecode
    ) == RUDA_MIR_BYTECODE_LOWER_INVALID_MIR);
    RUDA_EXPECT(!bytecode.initialized);

    functions[0] = &fixture.function;
    program.functions = functions;
    program.function_count = 1;
    program.entry_symbol = symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(
        &program,
        ruda_system_allocator(),
        ruda_native_c_emit_default_options(),
        &generated
    ) == RUDA_NATIVE_C_EMIT_INVALID_MIR);
    RUDA_EXPECT(generated.data == NULL && generated.length == 0);
    ruda_owned_bytes_dispose(&generated);
    ruda_bytecode_function_dispose(&bytecode);
    ruda_mir_function_dispose(&fixture.function);
}

void ruda_test_mir_interfaces(void) {
    RudaSymbolId symbol;
    RudaTestMirInterfaceFixture fixture;
    size_t one_method_work = 0;
    size_t two_method_work = 0;
    RUDA_EXPECT(ruda_test_mir_interface_symbol("entry", 0, &symbol));

    RUDA_EXPECT(ruda_test_mir_interface_fixture_init(
        &fixture,
        ruda_system_allocator(),
        symbol,
        false,
        1
    ));
    RUDA_EXPECT(ruda_mir_verify(&fixture.function) == RUDA_MIR_VERIFY_OK);
    ruda_mir_function_dispose(&fixture.function);

    RUDA_EXPECT(ruda_test_mir_interface_fixture_init(
        &fixture,
        ruda_system_allocator(),
        symbol,
        true,
        1
    ));
    RUDA_EXPECT(ruda_mir_verify(&fixture.function) == RUDA_MIR_VERIFY_OK);
    ruda_mir_function_dispose(&fixture.function);

    ruda_test_mir_interface_malformed_matrix(symbol);
    one_method_work = ruda_test_mir_interface_minimum_work(symbol, 1);
    two_method_work = ruda_test_mir_interface_minimum_work(symbol, 2);
    RUDA_EXPECT(two_method_work > one_method_work);
    ruda_test_mir_interface_allocation_failures(symbol);
    ruda_test_mir_interface_backend_preflight(symbol);
}
