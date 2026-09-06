#include "test_native_c_emitter_support.h"

#include <string.h>

static bool mmay_test_entry_leaf(
    MarimayMirFunction *function, MarimayAllocator allocator,
    unsigned char symbol, bool takes_arguments
) {
    MarimayTypeId type = MMAY_TYPE_FIRST_DYNAMIC + 251;
    MarimayMirArrayId array = MMAY_MIR_NO_ARRAY;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    return mmay_mir_function_init(function, allocator, (MarimaySymbolId){{symbol}},
            takes_arguments ? &type : NULL, takes_arguments ? 1 : 0,
            MMAY_TYPE_VOID, mmay_mir_function_default_options())
        && (!takes_arguments || mmay_mir_add_array(function, type, MMAY_TYPE_STRING, &array))
        && mmay_mir_add_block(function, &block)
        && mmay_mir_set_terminator(function, block, mmay_test_native_terminator(
            MMAY_MIR_RETURN, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 0));
}

static void mmay_test_entry_invalid(
    const MarimayMirFunction *const *functions, MarimayNativeCEmitOptions options
) {
    MarimaySymbolId table[3] = {functions[1]->symbol, functions[0]->symbol, {{99}}};
    MarimayNativeCEmitOptions invalid = options;
    invalid.test_entries = NULL;
    mmay_test_native_emit(functions, 2, table[0], invalid, MMAY_NATIVE_C_EMIT_INVALID_ARGUMENT);
    invalid = options;
    invalid.test_entry_count = 0;
    mmay_test_native_emit(functions, 2, table[0], invalid, MMAY_NATIVE_C_EMIT_INVALID_ARGUMENT);
    invalid = options;
    invalid.executable_entry = false;
    mmay_test_native_emit(functions, 2, table[0], invalid, MMAY_NATIVE_C_EMIT_INVALID_ARGUMENT);
    invalid = options;
    invalid.executable_takes_arguments = true;
    mmay_test_native_emit(functions, 2, table[0], invalid, MMAY_NATIVE_C_EMIT_INVALID_ARGUMENT);
    invalid = options;
    invalid.executable_returns_status = true;
    mmay_test_native_emit(functions, 2, table[0], invalid, MMAY_NATIVE_C_EMIT_INVALID_ARGUMENT);
    invalid = options;
    invalid.executable_returns_result = true;
    mmay_test_native_emit(functions, 2, table[0], invalid, MMAY_NATIVE_C_EMIT_INVALID_ARGUMENT);
    mmay_test_native_emit(functions, 2, table[1], options, MMAY_NATIVE_C_EMIT_INVALID_MIR);
    invalid = options;
    invalid.test_entries = table;
    invalid.test_entry_count = 3;
    mmay_test_native_emit(functions, 2, table[0], invalid, MMAY_NATIVE_C_EMIT_INVALID_ARGUMENT);
    invalid.test_entry_count = 2;
    table[1] = table[0];
    mmay_test_native_emit(functions, 2, table[0], invalid, MMAY_NATIVE_C_EMIT_INVALID_MIR);
    table[1] = table[2];
    mmay_test_native_emit(functions, 2, table[0], invalid, MMAY_NATIVE_C_EMIT_INVALID_MIR);
    table[0] = functions[0]->symbol;
    table[1] = functions[1]->symbol;
    mmay_test_native_emit(functions, 2, table[0], invalid, MMAY_NATIVE_C_EMIT_INVALID_MIR);
    /* A valid MIR function can still have an invalid public test signature. */
    {
        MarimayMirFunction wrong = {0};
        const MarimayMirFunction *bad[] = {functions[1], &wrong};
        MarimayTypeId parameters[] = {MMAY_TYPE_I64, MMAY_TYPE_I64};
        MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
        size_t count;
        table[0] = functions[1]->symbol;
        table[1] = functions[0]->symbol;
        for (count = 1; count <= 2; count += 1) {
            MMAY_EXPECT(mmay_mir_function_init(&wrong, mmay_system_allocator(), table[1],
                parameters, count, MMAY_TYPE_VOID, mmay_mir_function_default_options()));
            MMAY_EXPECT(mmay_mir_add_block(&wrong, &block));
            MMAY_EXPECT(mmay_mir_set_terminator(&wrong, block, mmay_test_native_terminator(
                MMAY_MIR_RETURN, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 0)));
            mmay_test_native_emit(bad, 2, table[0], invalid, MMAY_NATIVE_C_EMIT_UNSUPPORTED_TYPE);
            mmay_mir_function_dispose(&wrong);
        }
    }
}

static void mmay_test_entry_allocation_failures(void) {
    size_t failure;
    bool success = false;
    for (failure = 0; failure < 10000; failure += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayMirFunction first = {0};
        MarimayMirFunction second = {0};
        const MarimayMirFunction *functions[] = {&second, &first};
        MarimaySymbolId table[] = {{{1}}, {{2}}};
        MarimayNativeCProgram program = {functions, 2, {{1}}};
        MarimayNativeCEmitOptions options = mmay_native_c_emit_default_options();
        MarimayNativeCEmitStatus status;
        MarimayOwnedBytes output = {0};
        bool built;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        allocator = mmay_fail_allocator(&state);
        built = mmay_test_entry_leaf(&first, allocator, 1, false)
            && mmay_test_entry_leaf(&second, allocator, 2, true);
        MMAY_EXPECT(built);
        options.executable_entry = true;
        options.test_entries = table;
        options.test_entry_count = 2;
        state.attempts = 0;
        state.failure_index = failure;
        status = built ? mmay_native_c_emit_program(&program, allocator, options, &output)
            : MMAY_NATIVE_C_EMIT_INVALID_MIR;
        success = status == MMAY_NATIVE_C_EMIT_OK;
        if (success) MMAY_EXPECT(failure >= state.attempts && output.length != 0);
        else MMAY_EXPECT(state.attempts > failure && output.data == NULL && output.length == 0);
        mmay_owned_bytes_dispose(&output);
        mmay_mir_function_dispose(&second);
        mmay_mir_function_dispose(&first);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        if (success || !built) break;
    }
    MMAY_EXPECT(success);
}

#if !defined(_WIN32)
static void mmay_test_entry_host(const MarimayOwnedBytes *source) {
    static const char prefix[] =
        "#include \"mmay/base/allocator.h\"\n"
        "static MarimayFailAllocator test_state;\n"
        "static MarimayAllocator test_allocator(void) { return mmay_fail_allocator(&test_state); }\n"
        "#define mmay_system_allocator test_allocator\n#define main test_entry_main\n";
    static const char suffix[] =
        "\n#undef main\n#undef mmay_system_allocator\n"
        "static int clean(void) { return test_state.live_allocations == 0 && test_state.live_bytes == 0; }\n"
        "int main(void) {\n"
        "  char *args[] = {\"test\", \"1\", \"\", \"Мир\", \"--\", \"two words\", NULL};\n"
        "  char *bad[] = {\"\", \"00\", \"+0\", \"-1\", \"2\", \"999999999999999999999999999999999999999999\", \"x\", \"١\"};\n"
        "  static char *maximum[4099]; static char large[262145];\n"
        "  size_t i, entry, failure; int code, reached;\n"
        "  if (freopen(\"/dev/null\", \"w\", stderr) == NULL) return 80;\n"
        "  mmay_fail_allocator_init(&test_state, mmay_system_allocator(), SIZE_MAX);\n"
        "  if (test_entry_main(1, args) != 70 || test_entry_main(0, NULL) != 70 || !clean() || test_state.attempts != 0) return 81;\n"
        "  for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) { args[1] = bad[i]; if (test_entry_main(2, args) != 70 || !clean() || test_state.attempts != 0) return 82; }\n"
        "  for (entry = 0; entry < 2; entry++) {\n"
        "    args[1] = entry == 0 ? \"0\" : \"1\"; reached = 0;\n"
        "    for (failure = 0; failure < 10000; failure++) {\n"
        "      mmay_fail_allocator_init(&test_state, mmay_system_allocator(), failure); code = test_entry_main(6, args);\n"
        "      if (!clean() || (code != 0 && code != 70)) return 83;\n"
        "      if (code == 0) { if (failure < test_state.attempts) return 84; reached = 1; break; }\n"
        "      if (test_state.attempts <= failure) return 85;\n"
        "    }\n"
        "    if (!reached) return 86;\n"
        "  }\n"
        "  mmay_fail_allocator_init(&test_state, mmay_system_allocator(), SIZE_MAX);\n"
        "  maximum[0] = \"test\"; maximum[1] = \"1\"; for (i = 2; i < 4099; i++) maximum[i] = \"\";\n"
        "  if (test_entry_main(4098, maximum) != 0 || !clean()) return 87;\n"
        "  if (test_entry_main(4099, maximum) != 1 || !clean()) return 88;\n"
        "  memset(large, 'a', sizeof(large) - 1); for (i = 2; i < 6; i++) maximum[i] = large;\n"
        "  if (test_entry_main(6, maximum) != 0 || !clean()) return 89;\n"
        "  maximum[6] = \"a\"; if (test_entry_main(7, maximum) != 1 || !clean()) return 90;\n"
        "  args[2] = \"\\xff\"; if (test_entry_main(6, args) != 1 || !clean()) return 91;\n"
        "  return 0;\n}\n";
    MarimayVector wrapped = {0};
    MarimayOwnedBytes view = {0};
    size_t total = sizeof(prefix) - 1 + source->length + sizeof(suffix) - 1;
    MMAY_EXPECT(mmay_vector_init(&wrapped, mmay_system_allocator(), 1));
    if (mmay_vector_reserve(&wrapped, total)) {
        memcpy(wrapped.data, prefix, sizeof(prefix) - 1);
        memcpy((unsigned char *)wrapped.data + sizeof(prefix) - 1, source->data, source->length);
        memcpy((unsigned char *)wrapped.data + sizeof(prefix) - 1 + source->length, suffix, sizeof(suffix) - 1);
        view.data = wrapped.data;
        view.length = total;
        mmay_test_native_compile_and_run(&view, "", 0);
    } else MMAY_EXPECT(false);
    mmay_vector_dispose(&wrapped);
}
#endif

void mmay_test_native_c_emitter_test_entry(void) {
    MarimayMirFunction first = {0};
    MarimayMirFunction second = {0};
    const MarimayMirFunction *functions[] = {&second, &first};
    MarimaySymbolId table[] = {{{1}}, {{2}}};
    MarimayNativeCProgram program = {functions, 2, {{1}}};
    MarimayNativeCEmitOptions options = mmay_native_c_emit_default_options();
    MarimayOwnedBytes source = {0};
    bool built = mmay_test_entry_leaf(&first, mmay_system_allocator(), 1, false)
        && mmay_test_entry_leaf(&second, mmay_system_allocator(), 2, true);
    MMAY_EXPECT(built);
    options.executable_entry = true;
    options.test_entries = table;
    options.test_entry_count = 2;
    if (built) {
        mmay_test_entry_invalid(functions, options);
        MMAY_EXPECT(mmay_native_c_emit_program(&program, mmay_system_allocator(), options, &source)
            == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
        if (source.data != NULL) mmay_test_entry_host(&source);
#endif
    }
    mmay_owned_bytes_dispose(&source);
    mmay_mir_function_dispose(&second);
    mmay_mir_function_dispose(&first);
    mmay_test_entry_allocation_failures();
}
