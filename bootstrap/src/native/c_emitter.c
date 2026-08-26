#include "c_emitter_internal.h"

#include <stdlib.h>
#include <string.h>

#include "mmay/base/f64.h"

static bool mmay_native_c_emit_integer_entry(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *entry
) {
    const char *type = NULL;
    const char *label = NULL;
    const char *format = NULL;
    const char *cast = NULL;
    switch (entry->result_type) {
        case MMAY_TYPE_I8: type = "int8_t"; label = "i8"; format = "d"; cast = "int"; break;
        case MMAY_TYPE_I16: type = "int16_t"; label = "i16"; format = "d"; cast = "int"; break;
        case MMAY_TYPE_I32: type = "int32_t"; label = "i32"; format = "d"; cast = "int"; break;
        case MMAY_TYPE_U8: type = "uint8_t"; label = "u8"; format = "u"; cast = "unsigned int"; break;
        case MMAY_TYPE_U16: type = "uint16_t"; label = "u16"; format = "u"; cast = "unsigned int"; break;
        case MMAY_TYPE_U32: type = "uint32_t"; label = "u32"; format = "u"; cast = "unsigned int"; break;
        case MMAY_TYPE_U64: type = "uint64_t"; label = "u64"; format = "llu"; cast = "unsigned long long"; break;
        default: return false;
    }
    return mmay_native_c_write(buffer, "%s mmay_g_result = 0; mmay_g_s = ", type)
        && mmay_native_c_symbol(buffer, entry->symbol)
        && mmay_native_c_write(
            buffer,
            "(&mmay_g_x, &mmay_g_p, &mmay_g_result, &mmay_g_start, &mmay_g_end); if (mmay_g_s == 4) { mmay_g_print_panic(&mmay_g_p); mmay_g_execution_dispose(&mmay_g_x); return 1; } if (mmay_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", mmay_g_s, mmay_g_start, mmay_g_end); mmay_g_execution_dispose(&mmay_g_x); return 1; } printf(\"ok:%s:%%%s\\n\", (%s)mmay_g_result); mmay_g_execution_dispose(&mmay_g_x); return 0; }\n",
            label, format, cast
        );
}

bool mmay_native_c_emit_symbol_and_strings(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function
) {
    size_t index = 0;
    if (!mmay_native_c_write(buffer, "static const unsigned char ")
        || !mmay_native_c_symbol_data(buffer, function->symbol)
        || !mmay_native_c_write(buffer, "[16] = {")
        || !mmay_native_c_write(buffer, "0x%02x", (unsigned int)function->symbol.bytes[0])) {
        return false;
    }
    for (index = 1; index < sizeof(function->symbol.bytes); index += 1) {
        if (!mmay_native_c_write(
                buffer,
                ",0x%02x",
                (unsigned int)function->symbol.bytes[index]
            )) {
            return false;
        }
    }
    if (!mmay_native_c_write(buffer, "};\n")) {
        return false;
    }
    for (index = 0; index < function->string_constants.length; index += 1) {
        const MarimayMirStringConstant *constant = mmay_vector_at_const(
            &function->string_constants,
            index
        );
        size_t byte_index = 0;
        if (constant == NULL || !mmay_native_c_write(buffer, "static const unsigned char ")
            || !mmay_native_c_string_data(buffer, function->symbol, index)
            || !mmay_native_c_write(
                buffer,
                "[%zu] = {",
                constant->value.length == 0 ? 1 : constant->value.length
            )) {
            return false;
        }
        if (constant->value.length == 0) {
            if (!mmay_native_c_write(buffer, "0")) {
                return false;
            }
        } else {
            for (byte_index = 0;
                byte_index < constant->value.length;
                byte_index += 1) {
                if (!mmay_native_c_write(
                        buffer,
                        byte_index == 0
                            ? "0x%02x"
                            : (byte_index % 16 == 0 ? ",\n0x%02x" : ",0x%02x"),
                        (unsigned int)constant->value.data[byte_index]
                    )) {
                    return false;
                }
            }
        }
        if (!mmay_native_c_write(buffer, "};\n")) {
            return false;
        }
    }
    return true;
}

bool mmay_native_c_emit_runtime(
    MarimayNativeCBuffer *buffer,
    size_t max_frames,
    size_t max_suppressed
) {
    return mmay_native_c_write(
            buffer,
            "#define MMAY_G_MAX_FRAMES %zu\n#define MMAY_G_MAX_SUPPRESSED %zu\n\n",
            max_frames, max_suppressed
        )
        && mmay_native_c_write(buffer,
        "typedef struct { MarimayAllocator delegate; size_t attempts; size_t failure_index; } mmay_g_fail_allocator;\n"
        "typedef struct { MarimayAllocator allocator; MarimayManagedHeap heap; MarimayTestOutput test_output; mmay_g_fail_allocator fail_allocator; } mmay_g_execution;\n"
        "static void mmay_g_execution_dispose(mmay_g_execution *x) { if (x == NULL) return; mmay_test_output_dispose(&x->test_output); mmay_managed_heap_dispose(&x->heap); }\n")
        && mmay_native_c_write(buffer,
            "#ifndef MMAY_G_COLLECT_BEFORE_ALLOCATION\n#define MMAY_G_COLLECT_BEFORE_ALLOCATION 0\n#endif\n"
            "#ifndef MMAY_G_FAIL_ALLOCATION_AT\n#define MMAY_G_FAIL_ALLOCATION_AT SIZE_MAX\n#endif\n"
            "#ifndef MMAY_G_MAX_OBJECTS\n#define MMAY_G_MAX_OBJECTS SIZE_MAX\n#endif\n")
        && mmay_native_c_write(buffer,
            "#if defined(__clang__) || defined(__GNUC__)\n"
            "#define MMAY_G_ALWAYS_INLINE inline __attribute__((always_inline))\n"
            "#else\n#define MMAY_G_ALWAYS_INLINE inline\n#endif\n")
        && mmay_native_c_write(buffer,
        "typedef struct { const unsigned char *data; size_t length; MarimayManagedReference managed; } mmay_g_string;\n")
        && mmay_native_c_write(buffer,
            "typedef struct { const unsigned char *symbol; size_t start; size_t end; } mmay_g_frame;\n")
        && mmay_native_c_write(buffer,
            "typedef struct { const char *code; mmay_g_string message; unsigned char message_storage[256]; const unsigned char *symbol; size_t start; size_t end; mmay_g_frame frames[MMAY_G_MAX_FRAMES]; size_t frame_count; } mmay_g_suppressed_panic;\n"
            "typedef struct { const char *code; mmay_g_string message; unsigned char message_storage[256]; const unsigned char *symbol; size_t start; size_t end; mmay_g_frame *frames; size_t frame_count; size_t frame_capacity; mmay_g_suppressed_panic *suppressed; size_t suppressed_count; size_t suppressed_capacity; } mmay_g_panic;\n")
        && mmay_native_c_write(buffer,
            "static bool mmay_g_string_equal(mmay_g_string a, mmay_g_string b) { return a.length == b.length && (a.length == 0 || memcmp(a.data, b.data, a.length) == 0); }\n")
        && mmay_native_c_write(buffer,
            "static bool mmay_g_append_frame(mmay_g_panic *p, const unsigned char *symbol, size_t start, size_t end) { if (p == NULL || p->frame_count >= p->frame_capacity) return false; p->frames[p->frame_count].symbol = symbol; p->frames[p->frame_count].start = start; p->frames[p->frame_count].end = end; p->frame_count += 1; return true; }\n")
        && mmay_native_c_write(buffer,
            "static bool mmay_g_append_suppressed_record(mmay_g_panic *p, const mmay_g_suppressed_panic *source) { mmay_g_suppressed_panic *target; if (p == NULL || source == NULL || p->suppressed_count >= p->suppressed_capacity || source->frame_count > MMAY_G_MAX_FRAMES) return false; target = &p->suppressed[p->suppressed_count++]; *target = *source; if (source->message.data == source->message_storage) { if (source->message.length > sizeof(target->message_storage)) return false; memcpy(target->message_storage, source->message_storage, source->message.length); target->message.data = target->message_storage; } return true; }\n"
            "static bool mmay_g_append_suppressed_panic(mmay_g_panic *p, const mmay_g_panic *source) { mmay_g_suppressed_panic observed; size_t i; if (p == NULL || source == NULL || source->frame_count > MMAY_G_MAX_FRAMES) return false; memset(&observed, 0, sizeof(observed)); observed.code = source->code; observed.message = source->message; observed.symbol = source->symbol; observed.start = source->start; observed.end = source->end; observed.frame_count = source->frame_count; if (source->frame_count != 0) memcpy(observed.frames, source->frames, source->frame_count * sizeof(*source->frames)); if (source->message.data == source->message_storage) { if (source->message.length > sizeof(observed.message_storage)) return false; memcpy(observed.message_storage, source->message_storage, source->message.length); observed.message.data = observed.message_storage; } if (!mmay_g_append_suppressed_record(p, &observed)) return false; for (i = 0; i < source->suppressed_count; i += 1) if (!mmay_g_append_suppressed_record(p, &source->suppressed[i])) return false; return true; }\n"
            "static int mmay_g_begin_panic(mmay_g_panic *p, mmay_g_string message, const unsigned char *symbol, size_t start, size_t end) { if (p == NULL) return 4; p->code = \"G4000\"; p->message = message; p->symbol = symbol; p->start = start; p->end = end; p->frame_count = 0; p->suppressed_count = 0; return mmay_g_append_frame(p, symbol, start, end) ? 4 : 5; }\n")
        && mmay_native_c_write(buffer,
            "static void *mmay_g_fail_allocate(void *context, size_t size) { mmay_g_fail_allocator *state = context; if (state == NULL) return NULL; if (state->attempts++ == state->failure_index) return NULL; return state->delegate.allocate(state->delegate.context, size); }\n"
            "static void *mmay_g_fail_resize(void *context, void *pointer, size_t old_size, size_t new_size) { mmay_g_fail_allocator *state = context; if (state == NULL) return NULL; if (state->attempts++ == state->failure_index) return NULL; return state->delegate.resize(state->delegate.context, pointer, old_size, new_size); }\n"
            "static void mmay_g_fail_deallocate(void *context, void *pointer, size_t old_size) { mmay_g_fail_allocator *state = context; if (state != NULL) state->delegate.deallocate(state->delegate.context, pointer, old_size); }\n"
            "static MarimayAllocator mmay_g_make_allocator(mmay_g_fail_allocator *state) { MarimayAllocator result; state->delegate = mmay_system_allocator(); state->attempts = 0; state->failure_index = (size_t)MMAY_G_FAIL_ALLOCATION_AT; result.context = state; result.allocate = mmay_g_fail_allocate; result.resize = mmay_g_fail_resize; result.deallocate = mmay_g_fail_deallocate; return result; }\n")
        && mmay_native_c_write(buffer,
            "static size_t mmay_g_decimal_u64(unsigned char *out, size_t at, uint64_t value) { unsigned char reversed[20]; size_t count = 0, i; do { reversed[count++] = (unsigned char)('0' + value %% UINT64_C(10)); value /= UINT64_C(10); } while (value != 0); for (i = count; i != 0; i -= 1) out[at++] = reversed[i - 1]; return at; }\n")
        && mmay_native_c_write(buffer,
            "static size_t mmay_g_decimal_i64(unsigned char *out, size_t at, int64_t value) { uint64_t magnitude = (uint64_t)value; if (value < 0) { out[at++] = '-'; magnitude = UINT64_C(0) - magnitude; } return mmay_g_decimal_u64(out, at, magnitude); }\n")
        && mmay_native_c_write(buffer,
            "static int mmay_g_begin_bounds(mmay_g_panic *p, int64_t index, size_t length, const unsigned char *symbol, size_t start, size_t end) { static const unsigned char prefix[] = \"array index \"; static const unsigned char suffix[] = \" is outside length \"; size_t at = 0; if (p == NULL) return 4; memcpy(p->message_storage + at, prefix, sizeof(prefix) - 1); at += sizeof(prefix) - 1; at = mmay_g_decimal_i64(p->message_storage, at, index); memcpy(p->message_storage + at, suffix, sizeof(suffix) - 1); at += sizeof(suffix) - 1; at = mmay_g_decimal_u64(p->message_storage, at, (uint64_t)length); p->code = \"G4002\"; p->message.data = p->message_storage; p->message.length = at; p->symbol = symbol; p->start = start; p->end = end; p->frame_count = 0; p->suppressed_count = 0; return mmay_g_append_frame(p, symbol, start, end) ? 4 : 5; }\n")
        && mmay_native_c_write(buffer,
            "static int mmay_g_managed_status(MarimayManagedStatus status) { if (status == MMAY_MANAGED_OK) return 0; if (status == MMAY_MANAGED_ALLOCATION_FAILED) return 5; if (status == MMAY_MANAGED_LIMIT_REACHED) return 6; return 7; }\n")
        && mmay_native_c_emit_array_mutation_runtime(buffer)
        && mmay_native_c_emit_format_runtime(buffer)
        && mmay_native_c_write(buffer,
            "static int mmay_g_text_array_bytes(mmay_g_execution *, MarimayManagedReference, unsigned char **, size_t *);\n"
        )
        && mmay_native_c_emit_io_runtime(buffer)
        && mmay_native_c_emit_text_runtime(buffer)
        && mmay_native_c_write(buffer,
            "static void mmay_g_print_hex(const unsigned char *data, size_t length) { size_t i; for (i = 0; i < length; i += 1) printf(\"%%02x\", (unsigned int)data[i]); }\n")
        && mmay_native_c_write(buffer,
            "static void mmay_g_print_panic(const mmay_g_panic *p) { size_t i, j; printf(\"panic:%%s:\", p->code); mmay_g_print_hex(p->symbol, 16); printf(\":%%zu:%%zu:%%zu:\", p->start, p->end, p->message.length); mmay_g_print_hex(p->message.data, p->message.length); printf(\":%%zu\", p->frame_count); for (i = 0; i < p->frame_count; i += 1) { putchar(':'); mmay_g_print_hex(p->frames[i].symbol, 16); printf(\":%%zu:%%zu\", p->frames[i].start, p->frames[i].end); } if (p->suppressed_count != 0) { printf(\":suppressed:%%zu\", p->suppressed_count); for (i = 0; i < p->suppressed_count; i += 1) { const mmay_g_suppressed_panic *s = &p->suppressed[i]; printf(\":%%s:\", s->code); mmay_g_print_hex(s->symbol, 16); printf(\":%%zu:%%zu:%%zu:\", s->start, s->end, s->message.length); mmay_g_print_hex(s->message.data, s->message.length); printf(\":%%zu\", s->frame_count); for (j = 0; j < s->frame_count; j += 1) { putchar(':'); mmay_g_print_hex(s->frames[j].symbol, 16); printf(\":%%zu:%%zu\", s->frames[j].start, s->frames[j].end); } } } putchar('\\n'); }\n");
}

bool mmay_native_c_emit_keep_assets(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *const *functions,
    size_t count
) {
    size_t function_index = 0;
    if (!mmay_native_c_write(buffer, "static void mmay_g_keep_assets(void) {\n")) {
        return false;
    }
    for (function_index = 0; function_index < count; function_index += 1) {
        const MarimayMirFunction *function = functions[function_index];
        size_t string_index = 0;
        if (!mmay_native_c_write(buffer, "  (void)")
            || !mmay_native_c_symbol_data(buffer, function->symbol)
            || !mmay_native_c_write(buffer, ";\n")) {
            return false;
        }
        for (string_index = 0;
            string_index < function->string_constants.length;
            string_index += 1) {
            if (!mmay_native_c_write(buffer, "  (void)")
                || !mmay_native_c_string_data(
                    buffer,
                    function->symbol,
                    string_index
                )
                || !mmay_native_c_write(buffer, ";\n")) {
                return false;
            }
        }
    }
    return mmay_native_c_write(
        buffer,
        "  (void)&mmay_g_string_equal;\n  (void)&mmay_g_begin_panic;\n  (void)&mmay_g_begin_bounds;\n  (void)&mmay_g_begin_array_iterator_mutation;\n  (void)&mmay_g_append_suppressed_panic;\n  (void)&mmay_g_managed_status;\n  (void)&mmay_g_io_println;\n  (void)&mmay_g_test_output_begin;\n  (void)&mmay_g_test_output_end;\n  (void)&mmay_g_print_panic;\n  mmay_g_keep_managed();\n}\n\n"
    );
}

MarimayNativeCEmitOptions mmay_native_c_emit_default_options(void) {
    MarimayNativeCEmitOptions options;
    memset(&options, 0, sizeof(options));
    options.max_functions = 100000;
    options.max_generated_bytes = 64U * 1024U * 1024U;
    options.max_heap_bytes = 64U * 1024U * 1024U;
    return options;
}

MarimayNativeCEmitStatus mmay_native_c_emit_program(
    const MarimayNativeCProgram *program, MarimayAllocator allocator,
    MarimayNativeCEmitOptions options, MarimayOwnedBytes *output
) {
    const MarimayMirFunction **ordered = NULL;
    unsigned char *states = NULL;
    MarimayNativeCCycleFrame *cycle_stack = NULL;
    MarimayNativeCBuffer buffer;
    MarimayNativeCEmitStatus status = MMAY_NATIVE_C_EMIT_OK;
    size_t bytes = 0;
    size_t cycle_stack_bytes = 0;
    size_t max_suppressed = 1;
    size_t index = 0;
    unsigned char *root_registers = NULL;
    size_t root_register_bytes = 0;
    size_t root_count = 0;
    const MarimayMirFunction *entry = NULL;
    bool uses_f64 = false;
    if (output == NULL) return MMAY_NATIVE_C_EMIT_INVALID_ARGUMENT;
    memset(output, 0, sizeof(*output));
    if (program == NULL || program->functions == NULL || program->function_count == 0
        || options.max_functions == 0
        || options.max_generated_bytes == 0 || options.max_heap_bytes == 0
        || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL) {
        return MMAY_NATIVE_C_EMIT_INVALID_ARGUMENT;
    }
    if (program->function_count > options.max_functions) {
        return MMAY_NATIVE_C_EMIT_LIMIT_REACHED;
    }
    if (!mmay_size_multiply(program->function_count, sizeof(*ordered), &bytes)
        || !mmay_size_multiply(
            program->function_count,
            sizeof(*cycle_stack),
            &cycle_stack_bytes
        )) {
        return MMAY_NATIVE_C_EMIT_LIMIT_REACHED;
    }
    ordered = mmay_allocate(&allocator, bytes);
    states = mmay_allocate(&allocator, program->function_count);
    cycle_stack = mmay_allocate(&allocator, cycle_stack_bytes);
    if (ordered == NULL || states == NULL || cycle_stack == NULL) {
        status = MMAY_NATIVE_C_EMIT_ALLOCATION_FAILED;
        goto cleanup;
    }
    memset(states, 0, program->function_count);
    for (index = 0; index < program->function_count; index += 1) {
        if (program->functions[index] == NULL) {
            status = MMAY_NATIVE_C_EMIT_INVALID_MIR;
            goto cleanup;
        }
        ordered[index] = program->functions[index];
    }
    qsort(
        ordered,
        program->function_count,
        sizeof(*ordered),
        mmay_native_c_function_pointer_compare
    );
    if (!mmay_native_c_managed_layouts_valid(
            ordered, program->function_count
        )) {
        status = MMAY_NATIVE_C_EMIT_INVALID_MIR;
        goto cleanup;
    }
    for (index = 0; index < program->function_count; index += 1) {
        if (index != 0 && mmay_native_c_symbol_compare(
                ordered[index - 1]->symbol, ordered[index]->symbol
            ) == 0) {
            status = MMAY_NATIVE_C_EMIT_INVALID_MIR;
            goto cleanup;
        }
        status = mmay_native_c_validate_function(
            ordered[index], ordered, program->function_count
        );
        if (status != MMAY_NATIVE_C_EMIT_OK) {
            goto cleanup;
        }
        if (!mmay_size_add(
                max_suppressed,
                ordered[index]->calls.length,
                &max_suppressed
            )) {
            status = MMAY_NATIVE_C_EMIT_LIMIT_REACHED;
            goto cleanup;
        }
        if (ordered[index]->result_type == MMAY_TYPE_F64) {
            uses_f64 = true;
        }
        {
            size_t type_index = 0;
            for (type_index = 0;
                type_index < ordered[index]->register_types.length;
                type_index += 1) {
                const MarimayTypeId *type = mmay_vector_at_const(
                    &ordered[index]->register_types, type_index
                );
                if (type != NULL && *type == MMAY_TYPE_F64) {
                    uses_f64 = true;
                }
            }
        }
        if (mmay_native_c_symbol_compare(
                ordered[index]->symbol, program->entry_symbol
            ) == 0) {
            entry = ordered[index];
        }
    }
    if (entry == NULL
        || (!options.executable_entry && entry->parameter_count != 0)) {
        status = MMAY_NATIVE_C_EMIT_INVALID_MIR;
        goto cleanup;
    }
    if (options.executable_entry) {
        status = mmay_native_c_validate_executable_entry(
            entry, ordered, program->function_count, &options
        );
        if (status != MMAY_NATIVE_C_EMIT_OK) goto cleanup;
    }
    if (mmay_native_c_array_kind(entry, entry->result_type)
            == MMAY_NATIVE_C_ARRAY_F64
        || mmay_native_c_array_kind(entry, entry->result_type)
            == MMAY_NATIVE_C_ARRAY_ARRAY_F64) {
        status = MMAY_NATIVE_C_EMIT_UNSUPPORTED_TYPE;
        goto cleanup;
    }
    if (!options.executable_entry && entry->result_type != MMAY_TYPE_VOID
        && entry->result_type != MMAY_TYPE_NEVER
        && entry->result_type != MMAY_TYPE_I8
        && entry->result_type != MMAY_TYPE_I16
        && entry->result_type != MMAY_TYPE_I32
        && entry->result_type != MMAY_TYPE_I64
        && entry->result_type != MMAY_TYPE_U8
        && entry->result_type != MMAY_TYPE_U16
        && entry->result_type != MMAY_TYPE_U32
        && entry->result_type != MMAY_TYPE_U64
        && entry->result_type != MMAY_TYPE_F64
        && entry->result_type != MMAY_TYPE_BOOL
        && entry->result_type != MMAY_TYPE_STRING) {
        status = MMAY_NATIVE_C_EMIT_UNSUPPORTED_TYPE;
        goto cleanup;
    }
    if (uses_f64 && !mmay_f64_binary64_supported()) {
        status = MMAY_NATIVE_C_EMIT_UNSUPPORTED_HOST;
        goto cleanup;
    }
    if (mmay_native_c_has_cycle(
            ordered,
            program->function_count,
            states,
            cycle_stack
        )) {
        status = MMAY_NATIVE_C_EMIT_UNSUPPORTED_CALL;
        goto cleanup;
    }
    memset(&buffer, 0, sizeof(buffer)); buffer.limit = options.max_generated_bytes;
    if (!mmay_vector_init(&buffer.bytes, allocator, sizeof(char))) { status = MMAY_NATIVE_C_EMIT_ALLOCATION_FAILED; goto cleanup; }
    if (!mmay_native_c_write(&buffer, "#include <stdbool.h>\n#include <errno.h>\n#include <stdint.h>\n#include <stddef.h>\n#include <signal.h>\n#include <stdio.h>\n#include <string.h>\n#include \"mmay/base/checked_i64_fast.h\"\n#include \"mmay/base/allocator.h\"\n#include \"mmay/base/text.h\"\n#include \"mmay/base/utf8.h\"\n#include \"mmay/runtime/io.h\"\n#include \"mmay/runtime/managed.h\"\n#include \"mmay/runtime/program_arguments.h\"\n#include \"mmay/runtime/string.h\"\n#include \"mmay/runtime/test_output.h\"\n")
        || !mmay_native_c_write(&buffer, "#include \"mmay/base/checked_integer.h\"\n")
        || !mmay_native_c_write(&buffer, "#include \"mmay/base/number_format.h\"\n#include \"mmay/base/integer_text.h\"\n")
        || (uses_f64 && !mmay_native_c_write(
            &buffer,
            "#include \"mmay/base/f64.h\"\n"
        ))
        || !mmay_native_c_write(&buffer, "\n")
        || !mmay_native_c_emit_runtime(
            &buffer, program->function_count, max_suppressed
        )) goto emit_fail;
    for (index = 0; index < program->function_count; index += 1) {
        if (!mmay_native_c_emit_symbol_and_strings(&buffer, ordered[index])) goto emit_fail;
    }
    if (!mmay_native_c_write(&buffer, "\n")
        || !mmay_native_c_emit_managed_descriptors(
            &buffer, ordered, program->function_count
        )
        || !mmay_native_c_emit_keep_assets(
            &buffer,
            ordered,
            program->function_count
        )) goto emit_fail;
    for (index = 0; index < program->function_count; index += 1) if (!mmay_native_c_emit_signature(&buffer, ordered[index], ordered, program->function_count, true)) goto emit_fail;
    if (!mmay_native_c_write(&buffer, "\n")) goto emit_fail;
    for (index = 0; index < program->function_count; index += 1) {
        if (!mmay_native_c_plan_root_registers(
                ordered[index], ordered, program->function_count, allocator,
                &root_registers, &root_register_bytes, &root_count
            ) || !mmay_native_c_emit_function(
                &buffer, ordered[index], ordered, program->function_count,
                root_registers, root_count
            )) goto emit_fail;
        mmay_deallocate(&allocator, root_registers, root_register_bytes);
        root_registers = NULL;
        root_register_bytes = 0;
    }
    if (options.executable_entry
        && !mmay_native_c_emit_public_helpers(&buffer)) goto emit_fail;
    if (options.executable_entry) {
        if (!mmay_native_c_emit_executable_host_open(&buffer)) goto emit_fail;
    } else if (!mmay_native_c_write(
            &buffer,
            "int main(void) { size_t mmay_g_start = 0, mmay_g_end = 0; int mmay_g_s; static mmay_g_frame mmay_g_frames[MMAY_G_MAX_FRAMES]; static mmay_g_suppressed_panic mmay_g_suppressed[MMAY_G_MAX_SUPPRESSED]; mmay_g_panic mmay_g_p = {0}; mmay_g_execution mmay_g_x = {0}; MarimayManagedHeapOptions mmay_g_heap_options; "
        )) goto emit_fail;
    for (index = 0; index < program->function_count; index += 1) {
        if (!mmay_native_c_write(&buffer, "(void)&")
            || !mmay_native_c_symbol(&buffer, ordered[index]->symbol)
            || !mmay_native_c_write(&buffer, "; ")) goto emit_fail;
    }
    if (uses_f64 && !mmay_native_c_write(&buffer, options.executable_entry
            ? "if (!mmay_f64_binary64_supported()) { fputs(\"error[I0002]: native target does not provide strict binary64\\n\", stderr); return 70; } "
            : "if (!mmay_f64_binary64_supported()) { fputs(\"unsupported:f64\\n\", stderr); return 2; } ")) goto emit_fail;
    if (!mmay_native_c_write(&buffer, "mmay_g_keep_assets(); mmay_g_p.frames = mmay_g_frames; mmay_g_p.frame_capacity = MMAY_G_MAX_FRAMES; mmay_g_p.suppressed = mmay_g_suppressed; mmay_g_p.suppressed_capacity = MMAY_G_MAX_SUPPRESSED; mmay_g_x.allocator = mmay_g_make_allocator(&mmay_g_x.fail_allocator); ")) goto emit_fail;
    if (options.executable_entry
        && !mmay_native_c_emit_executable_host_arguments(&buffer)) {
        goto emit_fail;
    }
    if (!mmay_native_c_write(&buffer, "mmay_g_heap_options = mmay_managed_heap_default_options(); mmay_g_heap_options.max_live_bytes = %zu; mmay_g_heap_options.collect_before_allocation = MMAY_G_COLLECT_BEFORE_ALLOCATION != 0; if ((size_t)MMAY_G_MAX_OBJECTS < mmay_g_heap_options.max_objects) mmay_g_heap_options.max_objects = (size_t)MMAY_G_MAX_OBJECTS; if (!mmay_managed_heap_init(&mmay_g_x.heap, mmay_g_x.allocator, mmay_g_heap_options)) { ", options.max_heap_bytes)) goto emit_fail;
    if (!mmay_native_c_write(&buffer, options.executable_entry
            ? "mmay_g_exit = mmay_g_public_message(\"error\", \"I0002\", (const unsigned char *)\"runtime host allocation failed\", sizeof(\"runtime host allocation failed\") - 1, 70); goto mmay_g_public_host_cleanup; } "
            : "fputs(\"trap:5:0:0\\n\", stdout); return 1; } ")) goto emit_fail;
    if (!mmay_native_c_write(&buffer, options.executable_entry
            ? "if (mmay_g_register_immediate_enums(&mmay_g_x.heap) != MMAY_MANAGED_OK) { mmay_g_exit = mmay_g_public_message(\"error\", \"I0002\", (const unsigned char *)\"runtime host allocation failed\", sizeof(\"runtime host allocation failed\") - 1, 70); goto mmay_g_public_cleanup; } "
            : "if (mmay_g_register_immediate_enums(&mmay_g_x.heap) != MMAY_MANAGED_OK) { mmay_g_execution_dispose(&mmay_g_x); fputs(\"trap:5:0:0\\n\", stdout); return 1; } ")) goto emit_fail;
    if (options.executable_entry) {
        if (options.executable_takes_arguments
            && !mmay_native_c_emit_executable_argument_value(
                &buffer, entry
            )) goto emit_fail;
        if (!mmay_native_c_emit_executable_entry(
                &buffer, entry, ordered, program->function_count, &options
            )) goto emit_fail;
        if (!mmay_native_c_emit_executable_host_close(&buffer)) goto emit_fail;
    }
    else if (entry->result_type >= MMAY_TYPE_I8
        && entry->result_type <= MMAY_TYPE_U64
        && entry->result_type != MMAY_TYPE_I64) {
        if (!mmay_native_c_emit_integer_entry(&buffer, entry)) goto emit_fail;
    }
    else if (entry->result_type == MMAY_TYPE_I64) { if (!mmay_native_c_write(&buffer, "int64_t mmay_g_result = 0; mmay_g_s = ") || !mmay_native_c_symbol(&buffer, entry->symbol) || !mmay_native_c_write(&buffer, "(&mmay_g_x, &mmay_g_p, &mmay_g_result, &mmay_g_start, &mmay_g_end); if (mmay_g_s == 4) { mmay_g_print_panic(&mmay_g_p); mmay_g_execution_dispose(&mmay_g_x); return 1; } if (mmay_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", mmay_g_s, mmay_g_start, mmay_g_end); mmay_g_execution_dispose(&mmay_g_x); return 1; } printf(\"ok:i64:%%lld\\n\", (long long)mmay_g_result); mmay_g_execution_dispose(&mmay_g_x); return 0; }\n")) goto emit_fail; }
    else if (entry->result_type == MMAY_TYPE_F64) { if (!mmay_native_c_write(&buffer, "uint64_t mmay_g_result = UINT64_C(0); mmay_g_s = ") || !mmay_native_c_symbol(&buffer, entry->symbol) || !mmay_native_c_write(&buffer, "(&mmay_g_x, &mmay_g_p, &mmay_g_result, &mmay_g_start, &mmay_g_end); if (mmay_g_s == 4) { mmay_g_print_panic(&mmay_g_p); mmay_g_execution_dispose(&mmay_g_x); return 1; } if (mmay_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", mmay_g_s, mmay_g_start, mmay_g_end); mmay_g_execution_dispose(&mmay_g_x); return 1; } printf(\"ok:f64:%%016llx\\n\", (unsigned long long)mmay_g_result); mmay_g_execution_dispose(&mmay_g_x); return 0; }\n")) goto emit_fail; }
    else if (entry->result_type == MMAY_TYPE_BOOL) { if (!mmay_native_c_write(&buffer, "bool mmay_g_result = false; mmay_g_s = ") || !mmay_native_c_symbol(&buffer, entry->symbol) || !mmay_native_c_write(&buffer, "(&mmay_g_x, &mmay_g_p, &mmay_g_result, &mmay_g_start, &mmay_g_end); if (mmay_g_s == 4) { mmay_g_print_panic(&mmay_g_p); mmay_g_execution_dispose(&mmay_g_x); return 1; } if (mmay_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", mmay_g_s, mmay_g_start, mmay_g_end); mmay_g_execution_dispose(&mmay_g_x); return 1; } printf(\"ok:bool:%%s\\n\", mmay_g_result ? \"true\" : \"false\"); mmay_g_execution_dispose(&mmay_g_x); return 0; }\n")) goto emit_fail; }
    else if (entry->result_type == MMAY_TYPE_STRING) { if (!mmay_native_c_write(&buffer, "mmay_g_string mmay_g_result = {0}; mmay_g_s = ") || !mmay_native_c_symbol(&buffer, entry->symbol) || !mmay_native_c_write(&buffer, "(&mmay_g_x, &mmay_g_p, &mmay_g_result, &mmay_g_start, &mmay_g_end); if (mmay_g_s == 4) { mmay_g_print_panic(&mmay_g_p); mmay_g_execution_dispose(&mmay_g_x); return 1; } if (mmay_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", mmay_g_s, mmay_g_start, mmay_g_end); mmay_g_execution_dispose(&mmay_g_x); return 1; } printf(\"ok:string:%%zu:\", mmay_g_result.length); mmay_g_print_hex(mmay_g_result.data, mmay_g_result.length); putchar('\\n'); mmay_g_execution_dispose(&mmay_g_x); return 0; }\n")) goto emit_fail; }
    else { if (!mmay_native_c_write(&buffer, "mmay_g_s = ") || !mmay_native_c_symbol(&buffer, entry->symbol) || !mmay_native_c_write(&buffer, "(&mmay_g_x, &mmay_g_p, &mmay_g_start, &mmay_g_end); if (mmay_g_s == 4) { mmay_g_print_panic(&mmay_g_p); mmay_g_execution_dispose(&mmay_g_x); return 1; } if (mmay_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", mmay_g_s, mmay_g_start, mmay_g_end); mmay_g_execution_dispose(&mmay_g_x); return 1; } puts(\"ok:void\"); mmay_g_execution_dispose(&mmay_g_x); return 0; }\n")) goto emit_fail; }
    if (!mmay_owned_bytes_copy(output, allocator, buffer.bytes.data, buffer.bytes.length)) { status = MMAY_NATIVE_C_EMIT_ALLOCATION_FAILED; goto cleanup_buffer; }
    goto cleanup_buffer;
emit_fail:
    status = buffer.limited ? MMAY_NATIVE_C_EMIT_LIMIT_REACHED : MMAY_NATIVE_C_EMIT_ALLOCATION_FAILED;
cleanup_buffer:
    mmay_deallocate(&allocator, root_registers, root_register_bytes);
    mmay_vector_dispose(&buffer.bytes);
cleanup:
    mmay_deallocate(&allocator, states, program != NULL ? program->function_count : 0);
    mmay_deallocate(&allocator, cycle_stack, cycle_stack_bytes);
    mmay_deallocate(&allocator, ordered, bytes);
    if (status != MMAY_NATIVE_C_EMIT_OK) mmay_owned_bytes_dispose(output);
    return status;
}
