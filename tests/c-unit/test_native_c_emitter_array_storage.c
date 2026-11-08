#include "test_native_c_emitter_support.h"

#include <stdio.h>
#include <string.h>

typedef struct MarimayTestNativeStorageCase {
    MarimayTypeId element_type;
    const char *storage;
    const char *stride;
    bool has_value;
} MarimayTestNativeStorageCase;

static bool mmay_test_native_storage_build(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    const MarimayTestNativeStorageCase *cases,
    size_t count
) {
    MarimayMirRegister elements[16];
    MarimayMirRegister arrays[16];
    MarimayMirArrayId layouts[16];
    MarimayMirArrayConstructionId constructions[16];
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    size_t index = 0;
    memset(function, 0, sizeof(*function));
    if (count > sizeof(arrays) / sizeof(arrays[0])) {
        return false;
    }
    if (!mmay_mir_function_init(
            function, mmay_system_allocator(), symbol, NULL, 0,
            MMAY_TYPE_I64, mmay_mir_function_default_options()
        ) || !mmay_mir_add_register(function, MMAY_TYPE_I64, &result)) {
        goto failure;
    }
    for (index = 0; index < count; index += 1) {
        MarimayMirArrayArgument argument;
        MarimayTypeId array_type =
            MMAY_TYPE_FIRST_DYNAMIC + 200 + (MarimayTypeId)index;
        elements[index] = MMAY_MIR_NO_REGISTER;
        arrays[index] = MMAY_MIR_NO_REGISTER;
        layouts[index] = MMAY_MIR_NO_ARRAY;
        constructions[index] = MMAY_MIR_NO_ARRAY_CONSTRUCTION;
        if (!mmay_mir_add_array(
                function, array_type, cases[index].element_type,
                &layouts[index]
            ) || (cases[index].has_value && !mmay_mir_add_register(
                function, cases[index].element_type, &elements[index]
            )) || !mmay_mir_add_register(
                function, array_type, &arrays[index]
            )) {
            goto failure;
        }
        argument.register_id = elements[index];
        argument.type = cases[index].element_type;
        if (!mmay_mir_add_array_construction(
                function, layouts[index],
                cases[index].has_value ? &argument : NULL,
                cases[index].has_value ? 1 : 0,
                &constructions[index]
            )) goto failure;
    }
    if (!mmay_mir_add_block(function, &block)) {
        goto failure;
    }
    for (index = 0; index < count; index += 1) {
        if (cases[index].has_value) {
            MarimayMirInstructionKind kind = cases[index].element_type
                    == MMAY_TYPE_BOOL
                ? MMAY_MIR_CONSTANT_BOOL
                : cases[index].element_type == MMAY_TYPE_I64
                    ? MMAY_MIR_CONSTANT_I64
                    : cases[index].element_type == MMAY_TYPE_F64
                        ? MMAY_MIR_CONSTANT_F64
                        : MMAY_MIR_CONSTANT_INTEGER;
            instruction = mmay_test_native_instruction(
                kind, elements[index], MMAY_MIR_NO_REGISTER,
                MMAY_MIR_NO_REGISTER, 1, true, 5900 + index * 2
            );
            if (kind == MMAY_MIR_CONSTANT_INTEGER) {
                instruction.integer_bits = 1;
            } else if (kind == MMAY_MIR_CONSTANT_F64) {
                instruction.f64_bits = UINT64_C(0x3ff0000000000000);
            }
            if (!mmay_mir_append_instruction(function, block, &instruction)) {
                goto failure;
            }
        }
        instruction = mmay_test_native_instruction(
            MMAY_MIR_ARRAY_NEW, arrays[index], MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_REGISTER, 0, false, 5901 + index * 2
        );
        instruction.array = layouts[index];
        instruction.array_construction = constructions[index];
        if (!mmay_mir_append_instruction(function, block, &instruction)) {
            goto failure;
        }
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_I64, result, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 24, false, 6000
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(
            function, block, mmay_test_native_terminator(
                MMAY_MIR_RETURN, result, MMAY_MIR_NO_BLOCK,
                MMAY_MIR_NO_BLOCK, 6001
            )
        )) {
        goto failure;
    }
    return true;

failure:
    mmay_mir_function_dispose(function);
    return false;
}

void mmay_test_native_c_emitter_array_storage(void) {
    static const MarimayTestNativeStorageCase cases[] = {
        {MMAY_TYPE_BOOL, "MMAY_MANAGED_ARRAY_STORAGE_RAW8", "sizeof(uint8_t)", true},
        {MMAY_TYPE_I8, "MMAY_MANAGED_ARRAY_STORAGE_RAW8", "sizeof(uint8_t)", true},
        {MMAY_TYPE_U8, "MMAY_MANAGED_ARRAY_STORAGE_RAW8", "sizeof(uint8_t)", true},
        {MMAY_TYPE_I16, "MMAY_MANAGED_ARRAY_STORAGE_RAW16", "sizeof(uint16_t)", true},
        {MMAY_TYPE_U16, "MMAY_MANAGED_ARRAY_STORAGE_RAW16", "sizeof(uint16_t)", true},
        {MMAY_TYPE_I32, "MMAY_MANAGED_ARRAY_STORAGE_RAW32", "sizeof(uint32_t)", true},
        {MMAY_TYPE_U32, "MMAY_MANAGED_ARRAY_STORAGE_RAW32", "sizeof(uint32_t)", true},
        {MMAY_TYPE_F32, "MMAY_MANAGED_ARRAY_STORAGE_RAW32", "sizeof(uint32_t)", false},
        {MMAY_TYPE_I64, "MMAY_MANAGED_ARRAY_STORAGE_RAW64", "sizeof(uint64_t)", true},
        {MMAY_TYPE_U64, "MMAY_MANAGED_ARRAY_STORAGE_RAW64", "sizeof(uint64_t)", true},
        {MMAY_TYPE_F64, "MMAY_MANAGED_ARRAY_STORAGE_RAW64", "sizeof(uint64_t)", true},
        {MMAY_TYPE_STRING, "MMAY_MANAGED_ARRAY_STORAGE_CELL", "sizeof(MarimayManagedCell)", false}
    };
    MarimayMirFunction function;
    const MarimayMirFunction *functions[1];
    MarimayNativeCProgram program;
    MarimayOwnedBytes generated;
    MarimaySymbolId symbol;
    size_t index = 0;
    memset(&program, 0, sizeof(program));
    memset(&generated, 0, sizeof(generated));
    memset(&symbol, 0xd0, sizeof(symbol));
    MMAY_EXPECT(mmay_test_native_storage_build(
        &function, symbol, cases, sizeof(cases) / sizeof(cases[0])
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    functions[0] = &function;
    program.functions = functions;
    program.function_count = 1;
    program.entry_symbol = symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &program, mmay_system_allocator(), mmay_native_c_emit_default_options(),
        &generated
    ) == MMAY_NATIVE_C_EMIT_OK);
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        char expected[256];
        const char *cell = cases[index].element_type == MMAY_TYPE_STRING
            ? "MMAY_MANAGED_CELL_REFERENCE" : "MMAY_MANAGED_CELL_RAW64";
        const char *reference = cases[index].element_type == MMAY_TYPE_STRING
            ? "MMAY_MANAGED_OBJECT_STRING" : "MMAY_MANAGED_OBJECT_INVALID";
        if (cases[index].element_type == MMAY_TYPE_F64) {
            MMAY_EXPECT(mmay_test_native_contains(
                &generated, "&mmay_managed_array_f64_descriptor"
            ));
            continue;
        }
        int written = snprintf(
            expected, sizeof(expected),
            "const MarimayManagedArrayDescriptor mmay_g_ad_%u = {%s, %s, %s, %s, NULL};",
            (unsigned int)(MMAY_TYPE_FIRST_DYNAMIC + 200 + index),
            cell,
            cases[index].storage,
            cases[index].stride,
            reference
        );
        MMAY_EXPECT(written > 0 && (size_t)written < sizeof(expected));
        MMAY_EXPECT(mmay_test_native_contains(&generated, expected));
    }
#if !defined(_WIN32)
    mmay_test_native_compile_and_run(&generated, "ok:i64:24\n", 0);
#endif
    mmay_owned_bytes_dispose(&generated);
    mmay_mir_function_dispose(&function);
}
