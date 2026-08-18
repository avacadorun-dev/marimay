#include "c_emitter_internal.h"

#include <stdlib.h>
#include <string.h>

#include "ruda/base/f64.h"

static bool ruda_native_c_emit_integer_entry(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *entry
) {
    const char *type = NULL;
    const char *label = NULL;
    const char *format = NULL;
    const char *cast = NULL;
    switch (entry->result_type) {
        case RUDA_TYPE_I8: type = "int8_t"; label = "i8"; format = "d"; cast = "int"; break;
        case RUDA_TYPE_I16: type = "int16_t"; label = "i16"; format = "d"; cast = "int"; break;
        case RUDA_TYPE_I32: type = "int32_t"; label = "i32"; format = "d"; cast = "int"; break;
        case RUDA_TYPE_U8: type = "uint8_t"; label = "u8"; format = "u"; cast = "unsigned int"; break;
        case RUDA_TYPE_U16: type = "uint16_t"; label = "u16"; format = "u"; cast = "unsigned int"; break;
        case RUDA_TYPE_U32: type = "uint32_t"; label = "u32"; format = "u"; cast = "unsigned int"; break;
        case RUDA_TYPE_U64: type = "uint64_t"; label = "u64"; format = "llu"; cast = "unsigned long long"; break;
        default: return false;
    }
    return ruda_native_c_write(buffer, "%s ruda_g_result = 0; ruda_g_s = ", type)
        && ruda_native_c_symbol(buffer, entry->symbol)
        && ruda_native_c_write(
            buffer,
            "(&ruda_g_x, &ruda_g_p, &ruda_g_result, &ruda_g_start, &ruda_g_end); if (ruda_g_s == 4) { ruda_g_print_panic(&ruda_g_p); ruda_g_execution_dispose(&ruda_g_x); return 1; } if (ruda_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", ruda_g_s, ruda_g_start, ruda_g_end); ruda_g_execution_dispose(&ruda_g_x); return 1; } printf(\"ok:%s:%%%s\\n\", (%s)ruda_g_result); ruda_g_execution_dispose(&ruda_g_x); return 0; }\n",
            label, format, cast
        );
}

bool ruda_native_c_emit_symbol_and_strings(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function
) {
    size_t index = 0;
    if (!ruda_native_c_write(buffer, "static const unsigned char ")
        || !ruda_native_c_symbol_data(buffer, function->symbol)
        || !ruda_native_c_write(buffer, "[16] = {")
        || !ruda_native_c_write(buffer, "0x%02x", (unsigned int)function->symbol.bytes[0])) {
        return false;
    }
    for (index = 1; index < sizeof(function->symbol.bytes); index += 1) {
        if (!ruda_native_c_write(
                buffer,
                ",0x%02x",
                (unsigned int)function->symbol.bytes[index]
            )) {
            return false;
        }
    }
    if (!ruda_native_c_write(buffer, "};\n")) {
        return false;
    }
    for (index = 0; index < function->string_constants.length; index += 1) {
        const RudaMirStringConstant *constant = ruda_vector_at_const(
            &function->string_constants,
            index
        );
        size_t byte_index = 0;
        if (constant == NULL || !ruda_native_c_write(buffer, "static const unsigned char ")
            || !ruda_native_c_string_data(buffer, function->symbol, index)
            || !ruda_native_c_write(
                buffer,
                "[%zu] = {",
                constant->value.length == 0 ? 1 : constant->value.length
            )) {
            return false;
        }
        if (constant->value.length == 0) {
            if (!ruda_native_c_write(buffer, "0")) {
                return false;
            }
        } else {
            for (byte_index = 0;
                byte_index < constant->value.length;
                byte_index += 1) {
                if (!ruda_native_c_write(
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
        if (!ruda_native_c_write(buffer, "};\n")) {
            return false;
        }
    }
    return true;
}

bool ruda_native_c_emit_runtime(
    RudaNativeCBuffer *buffer,
    size_t max_frames,
    size_t max_suppressed
) {
    return ruda_native_c_write(
            buffer,
            "#define RUDA_G_MAX_FRAMES %zu\n#define RUDA_G_MAX_SUPPRESSED %zu\n\n",
            max_frames, max_suppressed
        )
        && ruda_native_c_write(buffer,
        "typedef struct { RudaAllocator delegate; size_t attempts; size_t failure_index; } ruda_g_fail_allocator;\n"
        "typedef struct { RudaAllocator allocator; RudaManagedHeap heap; RudaTestOutput test_output; ruda_g_fail_allocator fail_allocator; } ruda_g_execution;\n"
        "static void ruda_g_execution_dispose(ruda_g_execution *x) { if (x == NULL) return; ruda_test_output_dispose(&x->test_output); ruda_managed_heap_dispose(&x->heap); }\n")
        && ruda_native_c_write(buffer,
            "#ifndef RUDA_G_COLLECT_BEFORE_ALLOCATION\n#define RUDA_G_COLLECT_BEFORE_ALLOCATION 0\n#endif\n"
            "#ifndef RUDA_G_FAIL_ALLOCATION_AT\n#define RUDA_G_FAIL_ALLOCATION_AT SIZE_MAX\n#endif\n"
            "#ifndef RUDA_G_MAX_OBJECTS\n#define RUDA_G_MAX_OBJECTS SIZE_MAX\n#endif\n")
        && ruda_native_c_write(buffer,
        "typedef struct { const unsigned char *data; size_t length; RudaManagedReference managed; } ruda_g_string;\n")
        && ruda_native_c_write(buffer,
            "typedef struct { const unsigned char *symbol; size_t start; size_t end; } ruda_g_frame;\n")
        && ruda_native_c_write(buffer,
            "typedef struct { const char *code; ruda_g_string message; unsigned char message_storage[256]; const unsigned char *symbol; size_t start; size_t end; ruda_g_frame frames[RUDA_G_MAX_FRAMES]; size_t frame_count; } ruda_g_suppressed_panic;\n"
            "typedef struct { const char *code; ruda_g_string message; unsigned char message_storage[256]; const unsigned char *symbol; size_t start; size_t end; ruda_g_frame *frames; size_t frame_count; size_t frame_capacity; ruda_g_suppressed_panic *suppressed; size_t suppressed_count; size_t suppressed_capacity; } ruda_g_panic;\n")
        && ruda_native_c_write(buffer,
            "static bool ruda_g_string_equal(ruda_g_string a, ruda_g_string b) { return a.length == b.length && (a.length == 0 || memcmp(a.data, b.data, a.length) == 0); }\n")
        && ruda_native_c_write(buffer,
            "static bool ruda_g_append_frame(ruda_g_panic *p, const unsigned char *symbol, size_t start, size_t end) { if (p == NULL || p->frame_count >= p->frame_capacity) return false; p->frames[p->frame_count].symbol = symbol; p->frames[p->frame_count].start = start; p->frames[p->frame_count].end = end; p->frame_count += 1; return true; }\n")
        && ruda_native_c_write(buffer,
            "static bool ruda_g_append_suppressed_record(ruda_g_panic *p, const ruda_g_suppressed_panic *source) { ruda_g_suppressed_panic *target; if (p == NULL || source == NULL || p->suppressed_count >= p->suppressed_capacity || source->frame_count > RUDA_G_MAX_FRAMES) return false; target = &p->suppressed[p->suppressed_count++]; *target = *source; if (source->message.data == source->message_storage) { if (source->message.length > sizeof(target->message_storage)) return false; memcpy(target->message_storage, source->message_storage, source->message.length); target->message.data = target->message_storage; } return true; }\n"
            "static bool ruda_g_append_suppressed_panic(ruda_g_panic *p, const ruda_g_panic *source) { ruda_g_suppressed_panic observed; size_t i; if (p == NULL || source == NULL || source->frame_count > RUDA_G_MAX_FRAMES) return false; memset(&observed, 0, sizeof(observed)); observed.code = source->code; observed.message = source->message; observed.symbol = source->symbol; observed.start = source->start; observed.end = source->end; observed.frame_count = source->frame_count; if (source->frame_count != 0) memcpy(observed.frames, source->frames, source->frame_count * sizeof(*source->frames)); if (source->message.data == source->message_storage) { if (source->message.length > sizeof(observed.message_storage)) return false; memcpy(observed.message_storage, source->message_storage, source->message.length); observed.message.data = observed.message_storage; } if (!ruda_g_append_suppressed_record(p, &observed)) return false; for (i = 0; i < source->suppressed_count; i += 1) if (!ruda_g_append_suppressed_record(p, &source->suppressed[i])) return false; return true; }\n"
            "static int ruda_g_begin_panic(ruda_g_panic *p, ruda_g_string message, const unsigned char *symbol, size_t start, size_t end) { if (p == NULL) return 4; p->code = \"G4000\"; p->message = message; p->symbol = symbol; p->start = start; p->end = end; p->frame_count = 0; p->suppressed_count = 0; return ruda_g_append_frame(p, symbol, start, end) ? 4 : 5; }\n")
        && ruda_native_c_write(buffer,
            "static void *ruda_g_fail_allocate(void *context, size_t size) { ruda_g_fail_allocator *state = context; if (state == NULL) return NULL; if (state->attempts++ == state->failure_index) return NULL; return state->delegate.allocate(state->delegate.context, size); }\n"
            "static void *ruda_g_fail_resize(void *context, void *pointer, size_t old_size, size_t new_size) { ruda_g_fail_allocator *state = context; if (state == NULL) return NULL; if (state->attempts++ == state->failure_index) return NULL; return state->delegate.resize(state->delegate.context, pointer, old_size, new_size); }\n"
            "static void ruda_g_fail_deallocate(void *context, void *pointer, size_t old_size) { ruda_g_fail_allocator *state = context; if (state != NULL) state->delegate.deallocate(state->delegate.context, pointer, old_size); }\n"
            "static RudaAllocator ruda_g_make_allocator(ruda_g_fail_allocator *state) { RudaAllocator result; state->delegate = ruda_system_allocator(); state->attempts = 0; state->failure_index = (size_t)RUDA_G_FAIL_ALLOCATION_AT; result.context = state; result.allocate = ruda_g_fail_allocate; result.resize = ruda_g_fail_resize; result.deallocate = ruda_g_fail_deallocate; return result; }\n")
        && ruda_native_c_write(buffer,
            "static size_t ruda_g_decimal_u64(unsigned char *out, size_t at, uint64_t value) { unsigned char reversed[20]; size_t count = 0, i; do { reversed[count++] = (unsigned char)('0' + value %% UINT64_C(10)); value /= UINT64_C(10); } while (value != 0); for (i = count; i != 0; i -= 1) out[at++] = reversed[i - 1]; return at; }\n")
        && ruda_native_c_write(buffer,
            "static size_t ruda_g_decimal_i64(unsigned char *out, size_t at, int64_t value) { uint64_t magnitude = (uint64_t)value; if (value < 0) { out[at++] = '-'; magnitude = UINT64_C(0) - magnitude; } return ruda_g_decimal_u64(out, at, magnitude); }\n")
        && ruda_native_c_write(buffer,
            "static int ruda_g_begin_bounds(ruda_g_panic *p, int64_t index, size_t length, const unsigned char *symbol, size_t start, size_t end) { static const unsigned char prefix[] = \"array index \"; static const unsigned char suffix[] = \" is outside length \"; size_t at = 0; if (p == NULL) return 4; memcpy(p->message_storage + at, prefix, sizeof(prefix) - 1); at += sizeof(prefix) - 1; at = ruda_g_decimal_i64(p->message_storage, at, index); memcpy(p->message_storage + at, suffix, sizeof(suffix) - 1); at += sizeof(suffix) - 1; at = ruda_g_decimal_u64(p->message_storage, at, (uint64_t)length); p->code = \"G4002\"; p->message.data = p->message_storage; p->message.length = at; p->symbol = symbol; p->start = start; p->end = end; p->frame_count = 0; p->suppressed_count = 0; return ruda_g_append_frame(p, symbol, start, end) ? 4 : 5; }\n")
        && ruda_native_c_write(buffer,
            "static int ruda_g_managed_status(RudaManagedStatus status) { if (status == RUDA_MANAGED_OK) return 0; if (status == RUDA_MANAGED_ALLOCATION_FAILED) return 5; if (status == RUDA_MANAGED_LIMIT_REACHED || status == RUDA_MANAGED_LENGTH_LIMIT) return 6; if (status == RUDA_MANAGED_INVALID_REFERENCE || status == RUDA_MANAGED_TYPE_MISMATCH) return 7; return 5; }\n")
        && ruda_native_c_write(buffer,
            "static RudaManagedStatus ruda_g_array_check(const RudaManagedHeap *heap, RudaManagedReference array, const RudaManagedArrayDescriptor *expected, int64_t index, size_t *length) { const RudaManagedArrayDescriptor *actual = NULL; RudaManagedStatus status = ruda_managed_array_describe(heap, array, &actual); if (status != RUDA_MANAGED_OK) return status; if (actual != expected) return RUDA_MANAGED_TYPE_MISMATCH; status = ruda_managed_array_length(heap, array, length); if (status != RUDA_MANAGED_OK) return status; return index < 0 || (uint64_t)index >= (uint64_t)*length ? RUDA_MANAGED_BOUNDS : RUDA_MANAGED_OK; }\n")
        && ruda_native_c_emit_io_runtime(buffer)
        && ruda_native_c_write(buffer,
            "static void ruda_g_print_hex(const unsigned char *data, size_t length) { size_t i; for (i = 0; i < length; i += 1) printf(\"%%02x\", (unsigned int)data[i]); }\n")
        && ruda_native_c_write(buffer,
            "static void ruda_g_print_panic(const ruda_g_panic *p) { size_t i, j; printf(\"panic:%%s:\", p->code); ruda_g_print_hex(p->symbol, 16); printf(\":%%zu:%%zu:%%zu:\", p->start, p->end, p->message.length); ruda_g_print_hex(p->message.data, p->message.length); printf(\":%%zu\", p->frame_count); for (i = 0; i < p->frame_count; i += 1) { putchar(':'); ruda_g_print_hex(p->frames[i].symbol, 16); printf(\":%%zu:%%zu\", p->frames[i].start, p->frames[i].end); } if (p->suppressed_count != 0) { printf(\":suppressed:%%zu\", p->suppressed_count); for (i = 0; i < p->suppressed_count; i += 1) { const ruda_g_suppressed_panic *s = &p->suppressed[i]; printf(\":%%s:\", s->code); ruda_g_print_hex(s->symbol, 16); printf(\":%%zu:%%zu:%%zu:\", s->start, s->end, s->message.length); ruda_g_print_hex(s->message.data, s->message.length); printf(\":%%zu\", s->frame_count); for (j = 0; j < s->frame_count; j += 1) { putchar(':'); ruda_g_print_hex(s->frames[j].symbol, 16); printf(\":%%zu:%%zu\", s->frames[j].start, s->frames[j].end); } } } putchar('\\n'); }\n");
}

bool ruda_native_c_emit_keep_assets(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *const *functions,
    size_t count
) {
    size_t function_index = 0;
    if (!ruda_native_c_write(buffer, "static void ruda_g_keep_assets(void) {\n")) {
        return false;
    }
    for (function_index = 0; function_index < count; function_index += 1) {
        const RudaMirFunction *function = functions[function_index];
        size_t string_index = 0;
        if (!ruda_native_c_write(buffer, "  (void)")
            || !ruda_native_c_symbol_data(buffer, function->symbol)
            || !ruda_native_c_write(buffer, ";\n")) {
            return false;
        }
        for (string_index = 0;
            string_index < function->string_constants.length;
            string_index += 1) {
            if (!ruda_native_c_write(buffer, "  (void)")
                || !ruda_native_c_string_data(
                    buffer,
                    function->symbol,
                    string_index
                )
                || !ruda_native_c_write(buffer, ";\n")) {
                return false;
            }
        }
    }
    return ruda_native_c_write(
        buffer,
        "  (void)&ruda_g_string_equal;\n  (void)&ruda_g_begin_panic;\n  (void)&ruda_g_begin_bounds;\n  (void)&ruda_g_append_suppressed_panic;\n  (void)&ruda_g_managed_status;\n  (void)&ruda_g_array_check;\n  (void)&ruda_g_io_println;\n  (void)&ruda_g_test_output_begin;\n  (void)&ruda_g_test_output_end;\n  (void)&ruda_g_print_panic;\n  ruda_g_keep_managed();\n}\n\n"
    );
}

RudaNativeCEmitOptions ruda_native_c_emit_default_options(void) {
    RudaNativeCEmitOptions options;
    memset(&options, 0, sizeof(options));
    options.max_functions = 100000;
    options.max_generated_bytes = 64U * 1024U * 1024U;
    return options;
}

RudaNativeCEmitStatus ruda_native_c_emit_program(
    const RudaNativeCProgram *program, RudaAllocator allocator,
    RudaNativeCEmitOptions options, RudaOwnedBytes *output
) {
    const RudaMirFunction **ordered = NULL;
    unsigned char *states = NULL;
    RudaNativeCCycleFrame *cycle_stack = NULL;
    RudaNativeCBuffer buffer;
    RudaNativeCEmitStatus status = RUDA_NATIVE_C_EMIT_OK;
    size_t bytes = 0;
    size_t cycle_stack_bytes = 0;
    size_t max_suppressed = 1;
    size_t index = 0;
    const RudaMirFunction *entry = NULL;
    bool uses_f64 = false;
    if (output == NULL) return RUDA_NATIVE_C_EMIT_INVALID_ARGUMENT;
    memset(output, 0, sizeof(*output));
    if (program == NULL || program->functions == NULL || program->function_count == 0
        || options.max_functions == 0
        || options.max_generated_bytes == 0 || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL) {
        return RUDA_NATIVE_C_EMIT_INVALID_ARGUMENT;
    }
    if (program->function_count > options.max_functions) {
        return RUDA_NATIVE_C_EMIT_LIMIT_REACHED;
    }
    if (!ruda_size_multiply(program->function_count, sizeof(*ordered), &bytes)
        || !ruda_size_multiply(
            program->function_count,
            sizeof(*cycle_stack),
            &cycle_stack_bytes
        )) {
        return RUDA_NATIVE_C_EMIT_LIMIT_REACHED;
    }
    ordered = ruda_allocate(&allocator, bytes);
    states = ruda_allocate(&allocator, program->function_count);
    cycle_stack = ruda_allocate(&allocator, cycle_stack_bytes);
    if (ordered == NULL || states == NULL || cycle_stack == NULL) {
        status = RUDA_NATIVE_C_EMIT_ALLOCATION_FAILED;
        goto cleanup;
    }
    memset(states, 0, program->function_count);
    for (index = 0; index < program->function_count; index += 1) {
        if (program->functions[index] == NULL) {
            status = RUDA_NATIVE_C_EMIT_INVALID_MIR;
            goto cleanup;
        }
        ordered[index] = program->functions[index];
    }
    qsort(
        ordered,
        program->function_count,
        sizeof(*ordered),
        ruda_native_c_function_pointer_compare
    );
    if (!ruda_native_c_managed_layouts_valid(
            ordered, program->function_count
        )) {
        status = RUDA_NATIVE_C_EMIT_INVALID_MIR;
        goto cleanup;
    }
    for (index = 0; index < program->function_count; index += 1) {
        if (index != 0 && ruda_native_c_symbol_compare(
                ordered[index - 1]->symbol, ordered[index]->symbol
            ) == 0) {
            status = RUDA_NATIVE_C_EMIT_INVALID_MIR;
            goto cleanup;
        }
        status = ruda_native_c_validate_function(
            ordered[index], ordered, program->function_count
        );
        if (status != RUDA_NATIVE_C_EMIT_OK) {
            goto cleanup;
        }
        if (!ruda_size_add(
                max_suppressed,
                ordered[index]->calls.length,
                &max_suppressed
            )) {
            status = RUDA_NATIVE_C_EMIT_LIMIT_REACHED;
            goto cleanup;
        }
        if (ordered[index]->result_type == RUDA_TYPE_F64) {
            uses_f64 = true;
        }
        {
            size_t type_index = 0;
            for (type_index = 0;
                type_index < ordered[index]->register_types.length;
                type_index += 1) {
                const RudaTypeId *type = ruda_vector_at_const(
                    &ordered[index]->register_types, type_index
                );
                if (type != NULL && *type == RUDA_TYPE_F64) {
                    uses_f64 = true;
                }
            }
        }
        if (ruda_native_c_symbol_compare(
                ordered[index]->symbol, program->entry_symbol
            ) == 0) {
            entry = ordered[index];
        }
    }
    if (entry == NULL || entry->parameter_count != 0) {
        status = RUDA_NATIVE_C_EMIT_INVALID_MIR;
        goto cleanup;
    }
    if (options.executable_entry) {
        status = ruda_native_c_validate_executable_entry(
            entry, ordered, program->function_count, &options
        );
        if (status != RUDA_NATIVE_C_EMIT_OK) goto cleanup;
    }
    if (ruda_native_c_array_kind(entry, entry->result_type)
            == RUDA_NATIVE_C_ARRAY_F64
        || ruda_native_c_array_kind(entry, entry->result_type)
            == RUDA_NATIVE_C_ARRAY_ARRAY_F64) {
        status = RUDA_NATIVE_C_EMIT_UNSUPPORTED_TYPE;
        goto cleanup;
    }
    if (!options.executable_entry && entry->result_type != RUDA_TYPE_VOID
        && entry->result_type != RUDA_TYPE_NEVER
        && entry->result_type != RUDA_TYPE_I8
        && entry->result_type != RUDA_TYPE_I16
        && entry->result_type != RUDA_TYPE_I32
        && entry->result_type != RUDA_TYPE_I64
        && entry->result_type != RUDA_TYPE_U8
        && entry->result_type != RUDA_TYPE_U16
        && entry->result_type != RUDA_TYPE_U32
        && entry->result_type != RUDA_TYPE_U64
        && entry->result_type != RUDA_TYPE_F64
        && entry->result_type != RUDA_TYPE_BOOL
        && entry->result_type != RUDA_TYPE_STRING) {
        status = RUDA_NATIVE_C_EMIT_UNSUPPORTED_TYPE;
        goto cleanup;
    }
    if (uses_f64 && !ruda_f64_binary64_supported()) {
        status = RUDA_NATIVE_C_EMIT_UNSUPPORTED_HOST;
        goto cleanup;
    }
    if (ruda_native_c_has_cycle(
            ordered,
            program->function_count,
            states,
            cycle_stack
        )) {
        status = RUDA_NATIVE_C_EMIT_UNSUPPORTED_CALL;
        goto cleanup;
    }
    memset(&buffer, 0, sizeof(buffer)); buffer.limit = options.max_generated_bytes;
    if (!ruda_vector_init(&buffer.bytes, allocator, sizeof(char))) { status = RUDA_NATIVE_C_EMIT_ALLOCATION_FAILED; goto cleanup; }
    if (!ruda_native_c_write(&buffer, "#include <stdbool.h>\n#include <stdint.h>\n#include <stddef.h>\n#include <stdio.h>\n#include <string.h>\n#include \"ruda/base/checked_i64.h\"\n#include \"ruda/base/allocator.h\"\n#include \"ruda/runtime/managed.h\"\n#include \"ruda/runtime/string.h\"\n#include \"ruda/runtime/test_output.h\"\n")
        || !ruda_native_c_write(&buffer, "#include \"ruda/base/checked_integer.h\"\n")
        || !ruda_native_c_write(&buffer, "#include \"ruda/base/number_format.h\"\n")
        || (uses_f64 && !ruda_native_c_write(
            &buffer,
            "#include \"ruda/base/f64.h\"\n"
        ))
        || !ruda_native_c_write(&buffer, "\n")
        || !ruda_native_c_emit_runtime(
            &buffer, program->function_count, max_suppressed
        )) goto emit_fail;
    for (index = 0; index < program->function_count; index += 1) {
        if (!ruda_native_c_emit_symbol_and_strings(&buffer, ordered[index])) goto emit_fail;
    }
    if (!ruda_native_c_write(&buffer, "\n")
        || !ruda_native_c_emit_managed_descriptors(
            &buffer, ordered, program->function_count
        )
        || !ruda_native_c_emit_keep_assets(
            &buffer,
            ordered,
            program->function_count
        )) goto emit_fail;
    for (index = 0; index < program->function_count; index += 1) if (!ruda_native_c_emit_signature(&buffer, ordered[index], true)) goto emit_fail;
    if (!ruda_native_c_write(&buffer, "\n")) goto emit_fail;
    for (index = 0; index < program->function_count; index += 1) if (!ruda_native_c_emit_function(&buffer, ordered[index], ordered, program->function_count)) goto emit_fail;
    if (options.executable_entry
        && !ruda_native_c_emit_public_helpers(&buffer)) goto emit_fail;
    if (!ruda_native_c_write(&buffer, "int main(void) { size_t ruda_g_start = 0, ruda_g_end = 0; int ruda_g_s; static ruda_g_frame ruda_g_frames[RUDA_G_MAX_FRAMES]; static ruda_g_suppressed_panic ruda_g_suppressed[RUDA_G_MAX_SUPPRESSED]; ruda_g_panic ruda_g_p = {0}; ruda_g_execution ruda_g_x = {0}; RudaManagedHeapOptions ruda_g_heap_options; ")) goto emit_fail;
    for (index = 0; index < program->function_count; index += 1) {
        if (!ruda_native_c_write(&buffer, "(void)&")
            || !ruda_native_c_symbol(&buffer, ordered[index]->symbol)
            || !ruda_native_c_write(&buffer, "; ")) goto emit_fail;
    }
    if (uses_f64 && !ruda_native_c_write(&buffer, options.executable_entry
            ? "if (!ruda_f64_binary64_supported()) { fputs(\"error[I0002]: native target does not provide strict binary64\\n\", stderr); return 70; } "
            : "if (!ruda_f64_binary64_supported()) { fputs(\"unsupported:f64\\n\", stderr); return 2; } ")) goto emit_fail;
    if (!ruda_native_c_write(&buffer, "ruda_g_keep_assets(); ruda_g_p.frames = ruda_g_frames; ruda_g_p.frame_capacity = RUDA_G_MAX_FRAMES; ruda_g_p.suppressed = ruda_g_suppressed; ruda_g_p.suppressed_capacity = RUDA_G_MAX_SUPPRESSED; ruda_g_x.allocator = ruda_g_make_allocator(&ruda_g_x.fail_allocator); ruda_g_heap_options = ruda_managed_heap_default_options(); ruda_g_heap_options.collect_before_allocation = RUDA_G_COLLECT_BEFORE_ALLOCATION != 0; if ((size_t)RUDA_G_MAX_OBJECTS < ruda_g_heap_options.max_objects) ruda_g_heap_options.max_objects = (size_t)RUDA_G_MAX_OBJECTS; if (!ruda_managed_heap_init(&ruda_g_x.heap, ruda_g_x.allocator, ruda_g_heap_options)) { ")) goto emit_fail;
    if (!ruda_native_c_write(&buffer, options.executable_entry
            ? "fputs(\"error[I0002]: native runtime allocation failed\\n\", stderr); return 70; } "
            : "fputs(\"trap:5:0:0\\n\", stdout); return 1; } ")) goto emit_fail;
    if (options.executable_entry) {
        if (!ruda_native_c_emit_executable_entry(
                &buffer, entry, ordered, program->function_count, &options
            )) goto emit_fail;
    }
    else if (entry->result_type >= RUDA_TYPE_I8
        && entry->result_type <= RUDA_TYPE_U64
        && entry->result_type != RUDA_TYPE_I64) {
        if (!ruda_native_c_emit_integer_entry(&buffer, entry)) goto emit_fail;
    }
    else if (entry->result_type == RUDA_TYPE_I64) { if (!ruda_native_c_write(&buffer, "int64_t ruda_g_result = 0; ruda_g_s = ") || !ruda_native_c_symbol(&buffer, entry->symbol) || !ruda_native_c_write(&buffer, "(&ruda_g_x, &ruda_g_p, &ruda_g_result, &ruda_g_start, &ruda_g_end); if (ruda_g_s == 4) { ruda_g_print_panic(&ruda_g_p); ruda_g_execution_dispose(&ruda_g_x); return 1; } if (ruda_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", ruda_g_s, ruda_g_start, ruda_g_end); ruda_g_execution_dispose(&ruda_g_x); return 1; } printf(\"ok:i64:%%lld\\n\", (long long)ruda_g_result); ruda_g_execution_dispose(&ruda_g_x); return 0; }\n")) goto emit_fail; }
    else if (entry->result_type == RUDA_TYPE_F64) { if (!ruda_native_c_write(&buffer, "uint64_t ruda_g_result = UINT64_C(0); ruda_g_s = ") || !ruda_native_c_symbol(&buffer, entry->symbol) || !ruda_native_c_write(&buffer, "(&ruda_g_x, &ruda_g_p, &ruda_g_result, &ruda_g_start, &ruda_g_end); if (ruda_g_s == 4) { ruda_g_print_panic(&ruda_g_p); ruda_g_execution_dispose(&ruda_g_x); return 1; } if (ruda_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", ruda_g_s, ruda_g_start, ruda_g_end); ruda_g_execution_dispose(&ruda_g_x); return 1; } printf(\"ok:f64:%%016llx\\n\", (unsigned long long)ruda_g_result); ruda_g_execution_dispose(&ruda_g_x); return 0; }\n")) goto emit_fail; }
    else if (entry->result_type == RUDA_TYPE_BOOL) { if (!ruda_native_c_write(&buffer, "bool ruda_g_result = false; ruda_g_s = ") || !ruda_native_c_symbol(&buffer, entry->symbol) || !ruda_native_c_write(&buffer, "(&ruda_g_x, &ruda_g_p, &ruda_g_result, &ruda_g_start, &ruda_g_end); if (ruda_g_s == 4) { ruda_g_print_panic(&ruda_g_p); ruda_g_execution_dispose(&ruda_g_x); return 1; } if (ruda_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", ruda_g_s, ruda_g_start, ruda_g_end); ruda_g_execution_dispose(&ruda_g_x); return 1; } printf(\"ok:bool:%%s\\n\", ruda_g_result ? \"true\" : \"false\"); ruda_g_execution_dispose(&ruda_g_x); return 0; }\n")) goto emit_fail; }
    else if (entry->result_type == RUDA_TYPE_STRING) { if (!ruda_native_c_write(&buffer, "ruda_g_string ruda_g_result = {0}; ruda_g_s = ") || !ruda_native_c_symbol(&buffer, entry->symbol) || !ruda_native_c_write(&buffer, "(&ruda_g_x, &ruda_g_p, &ruda_g_result, &ruda_g_start, &ruda_g_end); if (ruda_g_s == 4) { ruda_g_print_panic(&ruda_g_p); ruda_g_execution_dispose(&ruda_g_x); return 1; } if (ruda_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", ruda_g_s, ruda_g_start, ruda_g_end); ruda_g_execution_dispose(&ruda_g_x); return 1; } printf(\"ok:string:%%zu:\", ruda_g_result.length); ruda_g_print_hex(ruda_g_result.data, ruda_g_result.length); putchar('\\n'); ruda_g_execution_dispose(&ruda_g_x); return 0; }\n")) goto emit_fail; }
    else { if (!ruda_native_c_write(&buffer, "ruda_g_s = ") || !ruda_native_c_symbol(&buffer, entry->symbol) || !ruda_native_c_write(&buffer, "(&ruda_g_x, &ruda_g_p, &ruda_g_start, &ruda_g_end); if (ruda_g_s == 4) { ruda_g_print_panic(&ruda_g_p); ruda_g_execution_dispose(&ruda_g_x); return 1; } if (ruda_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", ruda_g_s, ruda_g_start, ruda_g_end); ruda_g_execution_dispose(&ruda_g_x); return 1; } puts(\"ok:void\"); ruda_g_execution_dispose(&ruda_g_x); return 0; }\n")) goto emit_fail; }
    if (!ruda_owned_bytes_copy(output, allocator, buffer.bytes.data, buffer.bytes.length)) { status = RUDA_NATIVE_C_EMIT_ALLOCATION_FAILED; goto cleanup_buffer; }
    goto cleanup_buffer;
emit_fail:
    status = buffer.limited ? RUDA_NATIVE_C_EMIT_LIMIT_REACHED : RUDA_NATIVE_C_EMIT_ALLOCATION_FAILED;
cleanup_buffer:
    ruda_vector_dispose(&buffer.bytes);
cleanup:
    ruda_deallocate(&allocator, states, program != NULL ? program->function_count : 0);
    ruda_deallocate(&allocator, cycle_stack, cycle_stack_bytes);
    ruda_deallocate(&allocator, ordered, bytes);
    if (status != RUDA_NATIVE_C_EMIT_OK) ruda_owned_bytes_dispose(output);
    return status;
}
