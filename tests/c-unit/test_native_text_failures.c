#include "test_native_c_emitter_support.h"

#include <stdio.h>
#include <string.h>

/* Owns all-index teardown replay of real generated text helpers, including
 * NULL-backed managed empty strings, independently at -O0 and -O2. */
void mmay_test_native_text_failures(
    const MarimayOwnedBytes *source,
    MarimaySymbolId entry,
    size_t expected_length
) {
#if !defined(_WIN32)
    static const char prefix[] = "#define main mmay_text_original_main\n";
    static const char harness[] =
        "\n#undef main\n"
        "static int text_allocation_run(size_t failure, size_t *attempts) {\n"
        "  MarimayFailAllocator state; mmay_g_execution x = {0};\n"
        "  MarimayManagedHeapOptions options = mmay_managed_heap_default_options();\n"
        "  mmay_g_panic panic = {0}; mmay_g_frame frames[MMAY_G_MAX_FRAMES];\n"
        "  mmay_g_suppressed_panic suppressed[MMAY_G_MAX_SUPPRESSED];\n"
        "  size_t start = 0, end = 0; mmay_g_string value = {0}; int status = 5;\n"
        "  mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);\n"
        "  x.allocator = mmay_fail_allocator(&state);\n"
        "  options.collect_before_allocation = true;\n"
        "  panic.frames = frames; panic.frame_capacity = MMAY_G_MAX_FRAMES;\n"
        "  panic.suppressed = suppressed; panic.suppressed_capacity = MMAY_G_MAX_SUPPRESSED;\n"
        "  if (mmay_managed_heap_init(&x.heap, x.allocator, options)) {\n"
        "    status = mmay_g_managed_status(mmay_g_register_immediate_enums(&x.heap));\n"
        "    if (status == 0) status = mmay_g_f_%s(&x, &panic, &value, &start, &end);\n"
        "  }\n"
        "  mmay_g_execution_dispose(&x); *attempts = state.attempts;\n"
        "  if (state.live_allocations != 0 || state.live_bytes != 0) return 2;\n"
        "  return status == 0 ? (value.length == %zu ? 0 : 3) : (status == 5 ? 1 : 4);\n"
        "}\n"
        "int main(void) {\n"
        "  size_t attempts = 0, observed = 0, index;\n"
        "  (void)&mmay_text_original_main; mmay_g_keep_assets();\n"
        "  if (text_allocation_run(SIZE_MAX, &attempts) != 0 || attempts == 0) return 1;\n"
        "  for (index = 0; index < attempts; index += 1) {\n"
        "    int status = text_allocation_run(index, &observed);\n"
        "    if (status > 1) { printf(\"text allocation failure %%zu: %%d\\n\", index, status); return 1; }\n"
        "  }\n"
        "  puts(\"text allocation sweep: ok\"); return 0;\n"
        "}\n";
    char symbol[33];
    char suffix[4096];
    MarimayOwnedBytes generated = {0};
    size_t length = 0;
    size_t bytes = 0;
    int written;
    MMAY_EXPECT(mmay_test_native_hex_bytes(
        entry.bytes, sizeof(entry.bytes), symbol, sizeof(symbol)
    ));
    written = snprintf(suffix, sizeof(suffix), harness, symbol, expected_length);
    MMAY_EXPECT(written > 0 && (size_t)written < sizeof(suffix));
    if (written <= 0 || (size_t)written >= sizeof(suffix)) return;
    MMAY_EXPECT(mmay_size_add(sizeof(prefix) - 1, source->length, &length)
        && mmay_size_add(length, (size_t)written, &length)
        && mmay_size_add(length, 1, &bytes));
    if (bytes == 0) return;
    generated.allocator = mmay_system_allocator();
    generated.data = mmay_allocate(&generated.allocator, bytes);
    MMAY_EXPECT(generated.data != NULL);
    if (generated.data == NULL) return;
    generated.length = length;
    memcpy(generated.data, prefix, sizeof(prefix) - 1);
    memcpy(generated.data + sizeof(prefix) - 1, source->data, source->length);
    memcpy(generated.data + sizeof(prefix) - 1 + source->length,
        suffix, (size_t)written + 1);
    mmay_test_native_compile_and_run(&generated, "text allocation sweep: ok\n", 0);
    mmay_owned_bytes_dispose(&generated);
#else
    (void)source;
    (void)entry;
    (void)expected_length;
#endif
}
