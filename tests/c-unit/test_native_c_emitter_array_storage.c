#include "test_native_c_emitter_support.h"

#include <stdio.h>
#include <string.h>

typedef struct RudaTestNativeStorageCase {
    RudaTypeId element_type;
    const char *storage;
    const char *stride;
    bool has_value;
} RudaTestNativeStorageCase;

static bool ruda_test_native_storage_build(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    const RudaTestNativeStorageCase *cases,
    size_t count
) {
    RudaMirRegister elements[16];
    RudaMirRegister arrays[16];
    RudaMirArrayId layouts[16];
    RudaMirArrayConstructionId constructions[16];
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    size_t index = 0;
    memset(function, 0, sizeof(*function));
    if (count > sizeof(arrays) / sizeof(arrays[0])) {
        return false;
    }
    if (!ruda_mir_function_init(
            function, ruda_system_allocator(), symbol, NULL, 0,
            RUDA_TYPE_I64, ruda_mir_function_default_options()
        ) || !ruda_mir_add_register(function, RUDA_TYPE_I64, &result)) {
        goto failure;
    }
    for (index = 0; index < count; index += 1) {
        RudaMirArrayArgument argument;
        RudaTypeId array_type =
            RUDA_TYPE_FIRST_DYNAMIC + 200 + (RudaTypeId)index;
        elements[index] = RUDA_MIR_NO_REGISTER;
        arrays[index] = RUDA_MIR_NO_REGISTER;
        layouts[index] = RUDA_MIR_NO_ARRAY;
        constructions[index] = RUDA_MIR_NO_ARRAY_CONSTRUCTION;
        if (!ruda_mir_add_array(
                function, array_type, cases[index].element_type,
                &layouts[index]
            ) || (cases[index].has_value && !ruda_mir_add_register(
                function, cases[index].element_type, &elements[index]
            )) || !ruda_mir_add_register(
                function, array_type, &arrays[index]
            )) {
            goto failure;
        }
        argument.register_id = elements[index];
        argument.type = cases[index].element_type;
        if (!ruda_mir_add_array_construction(
                function, layouts[index],
                cases[index].has_value ? &argument : NULL,
                cases[index].has_value ? 1 : 0,
                &constructions[index]
            )) goto failure;
    }
    if (!ruda_mir_add_block(function, &block)) {
        goto failure;
    }
    for (index = 0; index < count; index += 1) {
        if (cases[index].has_value) {
            RudaMirInstructionKind kind = cases[index].element_type
                    == RUDA_TYPE_BOOL
                ? RUDA_MIR_CONSTANT_BOOL
                : cases[index].element_type == RUDA_TYPE_I64
                    ? RUDA_MIR_CONSTANT_I64
                    : cases[index].element_type == RUDA_TYPE_F64
                        ? RUDA_MIR_CONSTANT_F64
                        : RUDA_MIR_CONSTANT_INTEGER;
            instruction = ruda_test_native_instruction(
                kind, elements[index], RUDA_MIR_NO_REGISTER,
                RUDA_MIR_NO_REGISTER, 1, true, 5900 + index * 2
            );
            if (kind == RUDA_MIR_CONSTANT_INTEGER) {
                instruction.integer_bits = 1;
            } else if (kind == RUDA_MIR_CONSTANT_F64) {
                instruction.f64_bits = UINT64_C(0x3ff0000000000000);
            }
            if (!ruda_mir_append_instruction(function, block, &instruction)) {
                goto failure;
            }
        }
        instruction = ruda_test_native_instruction(
            RUDA_MIR_ARRAY_NEW, arrays[index], RUDA_MIR_NO_REGISTER,
            RUDA_MIR_NO_REGISTER, 0, false, 5901 + index * 2
        );
        instruction.array = layouts[index];
        instruction.array_construction = constructions[index];
        if (!ruda_mir_append_instruction(function, block, &instruction)) {
            goto failure;
        }
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_I64, result, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 24, false, 6000
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(
            function, block, ruda_test_native_terminator(
                RUDA_MIR_RETURN, result, RUDA_MIR_NO_BLOCK,
                RUDA_MIR_NO_BLOCK, 6001
            )
        )) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}

void ruda_test_native_c_emitter_array_storage(void) {
    static const RudaTestNativeStorageCase cases[] = {
        {RUDA_TYPE_BOOL, "RUDA_MANAGED_ARRAY_STORAGE_RAW8", "sizeof(uint8_t)", true},
        {RUDA_TYPE_I8, "RUDA_MANAGED_ARRAY_STORAGE_RAW8", "sizeof(uint8_t)", true},
        {RUDA_TYPE_U8, "RUDA_MANAGED_ARRAY_STORAGE_RAW8", "sizeof(uint8_t)", true},
        {RUDA_TYPE_I16, "RUDA_MANAGED_ARRAY_STORAGE_RAW16", "sizeof(uint16_t)", true},
        {RUDA_TYPE_U16, "RUDA_MANAGED_ARRAY_STORAGE_RAW16", "sizeof(uint16_t)", true},
        {RUDA_TYPE_I32, "RUDA_MANAGED_ARRAY_STORAGE_RAW32", "sizeof(uint32_t)", true},
        {RUDA_TYPE_U32, "RUDA_MANAGED_ARRAY_STORAGE_RAW32", "sizeof(uint32_t)", true},
        {RUDA_TYPE_F32, "RUDA_MANAGED_ARRAY_STORAGE_RAW32", "sizeof(uint32_t)", false},
        {RUDA_TYPE_I64, "RUDA_MANAGED_ARRAY_STORAGE_RAW64", "sizeof(uint64_t)", true},
        {RUDA_TYPE_U64, "RUDA_MANAGED_ARRAY_STORAGE_RAW64", "sizeof(uint64_t)", true},
        {RUDA_TYPE_F64, "RUDA_MANAGED_ARRAY_STORAGE_RAW64", "sizeof(uint64_t)", true},
        {RUDA_TYPE_STRING, "RUDA_MANAGED_ARRAY_STORAGE_CELL", "sizeof(RudaManagedCell)", false}
    };
    RudaMirFunction function;
    const RudaMirFunction *functions[1];
    RudaNativeCProgram program;
    RudaOwnedBytes generated;
    RudaSymbolId symbol;
    size_t index = 0;
    memset(&program, 0, sizeof(program));
    memset(&generated, 0, sizeof(generated));
    memset(&symbol, 0xd0, sizeof(symbol));
    RUDA_EXPECT(ruda_test_native_storage_build(
        &function, symbol, cases, sizeof(cases) / sizeof(cases[0])
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    functions[0] = &function;
    program.functions = functions;
    program.function_count = 1;
    program.entry_symbol = symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(
        &program, ruda_system_allocator(), ruda_native_c_emit_default_options(),
        &generated
    ) == RUDA_NATIVE_C_EMIT_OK);
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        char expected[256];
        const char *cell = cases[index].element_type == RUDA_TYPE_STRING
            ? "RUDA_MANAGED_CELL_REFERENCE" : "RUDA_MANAGED_CELL_RAW64";
        const char *reference = cases[index].element_type == RUDA_TYPE_STRING
            ? "RUDA_MANAGED_OBJECT_STRING" : "RUDA_MANAGED_OBJECT_INVALID";
        if (cases[index].element_type == RUDA_TYPE_F64) {
            RUDA_EXPECT(ruda_test_native_contains(
                &generated, "&ruda_managed_array_f64_descriptor"
            ));
            continue;
        }
        int written = snprintf(
            expected, sizeof(expected),
            "static const RudaManagedArrayDescriptor ruda_g_ad_%u = {%s, %s, %s, %s, NULL};",
            (unsigned int)(RUDA_TYPE_FIRST_DYNAMIC + 200 + index),
            cell,
            cases[index].storage,
            cases[index].stride,
            reference
        );
        RUDA_EXPECT(written > 0 && (size_t)written < sizeof(expected));
        RUDA_EXPECT(ruda_test_native_contains(&generated, expected));
    }
#if !defined(_WIN32)
    ruda_test_native_compile_and_run(&generated, "ok:i64:24\n", 0);
#endif
    ruda_owned_bytes_dispose(&generated);
    ruda_mir_function_dispose(&function);
}
