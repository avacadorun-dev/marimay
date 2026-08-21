#include "ruda/check/type_spelling.h"

#include <string.h>

#include "ruda/base/vector.h"

typedef struct RudaTypeSpellingContext {
    const RudaCheckResult *result;
    RudaVector bytes;
    size_t max_depth;
    size_t max_bytes;
    RudaTypeSpellingStatus status;
} RudaTypeSpellingContext;

static bool ruda_type_spelling_append(
    RudaTypeSpellingContext *context,
    const void *bytes,
    size_t length
) {
    size_t new_length = 0;
    if (context == NULL || (bytes == NULL && length != 0)) {
        return false;
    }
    if (!ruda_size_add(context->bytes.length, length, &new_length)
        || new_length > context->max_bytes) {
        context->status = RUDA_TYPE_SPELLING_LIMIT_REACHED;
        return false;
    }
    if (!ruda_vector_append(&context->bytes, bytes, length)) {
        context->status = RUDA_TYPE_SPELLING_ALLOCATION_FAILED;
        return false;
    }
    return true;
}

static const char *ruda_type_spelling_primitive(RudaTypeKind kind) {
    switch (kind) {
        case RUDA_TYPE_KIND_BOOL: return "bool";
        case RUDA_TYPE_KIND_I8: return "i8";
        case RUDA_TYPE_KIND_I16: return "i16";
        case RUDA_TYPE_KIND_I32: return "i32";
        case RUDA_TYPE_KIND_I64: return "int";
        case RUDA_TYPE_KIND_U8: return "byte";
        case RUDA_TYPE_KIND_U16: return "u16";
        case RUDA_TYPE_KIND_U32: return "u32";
        case RUDA_TYPE_KIND_U64: return "uint";
        case RUDA_TYPE_KIND_F32: return "f32";
        case RUDA_TYPE_KIND_F64: return "float";
        case RUDA_TYPE_KIND_RUNE: return "rune";
        case RUDA_TYPE_KIND_STRING: return "string";
        case RUDA_TYPE_KIND_VOID: return "void";
        case RUDA_TYPE_KIND_NEVER: return "never";
        default: return NULL;
    }
}

static const char *ruda_type_spelling_prelude_name(RudaSymbolId symbol) {
    static const char *const names[] = {
        "Option", "Result", "ArrayGrowthError", "ArrayTruncateError"
    };
    size_t index = 0;
    for (index = 0; index < sizeof(names) / sizeof(names[0]); index += 1) {
        RudaSymbolId candidate;
        const size_t length = strlen(names[index]);
        if (ruda_symbol_id_from_bytes(
                "std", 3, "prelude", 7, names[index], length, 0, &candidate
            ) && ruda_symbol_id_equal(symbol, candidate)) {
            return names[index];
        }
    }
    return NULL;
}

static bool ruda_type_spelling_nominal_name(
    RudaTypeSpellingContext *context,
    RudaSymbolId symbol
) {
    const RudaCheckedSymbol *checked = ruda_checked_symbol_by_id(
        context->result, symbol
    );
    const char *prelude = NULL;
    if (checked != NULL) {
        return ruda_type_spelling_append(
            context, checked->name.data, checked->name.length
        );
    }
    prelude = ruda_type_spelling_prelude_name(symbol);
    if (prelude != NULL) {
        return ruda_type_spelling_append(context, prelude, strlen(prelude));
    }
    context->status = RUDA_TYPE_SPELLING_UNAVAILABLE;
    return false;
}

static bool ruda_type_spelling_render(
    RudaTypeSpellingContext *context,
    RudaTypeId type,
    size_t depth
);

static bool ruda_type_spelling_arguments(
    RudaTypeSpellingContext *context,
    const RudaTypeDescriptor *descriptor,
    size_t depth,
    const char *open,
    const char *close
) {
    size_t index = 0;
    if (!ruda_type_spelling_append(context, open, strlen(open))) {
        return false;
    }
    for (index = 0; index < descriptor->argument_count; index += 1) {
        if (index != 0
            && !ruda_type_spelling_append(context, ", ", 2)) {
            return false;
        }
        if (!ruda_type_spelling_render(
                context, descriptor->arguments[index], depth + 1
            )) {
            return false;
        }
    }
    return ruda_type_spelling_append(context, close, strlen(close));
}

static bool ruda_type_spelling_function(
    RudaTypeSpellingContext *context,
    const RudaTypeDescriptor *descriptor,
    size_t depth
) {
    size_t index = 0;
    if (descriptor->argument_count != descriptor->parameter_count + 1) {
        context->status = RUDA_TYPE_SPELLING_UNAVAILABLE;
        return false;
    }
    if (!ruda_type_spelling_append(context, "(", 1)) {
        return false;
    }
    for (index = 0; index < descriptor->parameter_count; index += 1) {
        if ((index != 0
                && !ruda_type_spelling_append(context, ", ", 2))
            || !ruda_type_spelling_render(
                context, descriptor->arguments[index], depth + 1
            )) {
            return false;
        }
    }
    return ruda_type_spelling_append(context, ") => ", 5)
        && ruda_type_spelling_render(
            context,
            descriptor->arguments[descriptor->parameter_count],
            depth + 1
        );
}

static bool ruda_type_spelling_render(
    RudaTypeSpellingContext *context,
    RudaTypeId type,
    size_t depth
) {
    const RudaTypeKind kind = ruda_type_kind(&context->result->types, type);
    const char *primitive = ruda_type_spelling_primitive(kind);
    const RudaTypeDescriptor *descriptor = NULL;
    if (depth > context->max_depth) {
        context->status = RUDA_TYPE_SPELLING_LIMIT_REACHED;
        return false;
    }
    if (primitive != NULL) {
        return ruda_type_spelling_append(
            context, primitive, strlen(primitive)
        );
    }
    descriptor = ruda_type_descriptor(&context->result->types, type);
    if (descriptor == NULL) {
        context->status = RUDA_TYPE_SPELLING_UNAVAILABLE;
        return false;
    }
    if (kind == RUDA_TYPE_KIND_ARRAY) {
        if (descriptor->argument_count != 1) {
            context->status = RUDA_TYPE_SPELLING_UNAVAILABLE;
            return false;
        }
        return ruda_type_spelling_append(context, "[]", 2)
            && ruda_type_spelling_render(
                context, descriptor->arguments[0], depth + 1
            );
    }
    if (kind == RUDA_TYPE_KIND_TUPLE) {
        return ruda_type_spelling_arguments(
            context, descriptor, depth, "[", "]"
        );
    }
    if (kind == RUDA_TYPE_KIND_FUNCTION) {
        return ruda_type_spelling_function(context, descriptor, depth);
    }
    if (kind == RUDA_TYPE_KIND_NOMINAL || kind == RUDA_TYPE_KIND_INTERFACE) {
        if (!ruda_type_spelling_nominal_name(context, descriptor->symbol)) {
            return false;
        }
        return descriptor->argument_count == 0
            || ruda_type_spelling_arguments(
                context, descriptor, depth, "<", ">"
            );
    }
    context->status = RUDA_TYPE_SPELLING_UNAVAILABLE;
    return false;
}

RudaTypeSpellingOptions ruda_type_spelling_default_options(void) {
    RudaTypeSpellingOptions options;
    options.max_depth = 256;
    options.max_bytes = 1024 * 1024;
    return options;
}

RudaTypeSpellingStatus ruda_checked_type_spelling(
    const RudaCheckResult *result,
    RudaTypeId type,
    RudaAllocator allocator,
    RudaTypeSpellingOptions options,
    RudaOwnedBytes *output
) {
    RudaTypeSpellingContext context;
    if (output != NULL) {
        memset(output, 0, sizeof(*output));
    }
    if (result == NULL || !result->initialized || type == RUDA_TYPE_INVALID
        || allocator.allocate == NULL || allocator.resize == NULL
        || allocator.deallocate == NULL || options.max_depth == 0
        || options.max_bytes == 0 || output == NULL) {
        return RUDA_TYPE_SPELLING_INVALID_ARGUMENT;
    }
    memset(&context, 0, sizeof(context));
    context.result = result;
    context.max_depth = options.max_depth;
    context.max_bytes = options.max_bytes;
    context.status = RUDA_TYPE_SPELLING_OK;
    if (!ruda_vector_init(&context.bytes, allocator, sizeof(unsigned char))) {
        return RUDA_TYPE_SPELLING_ALLOCATION_FAILED;
    }
    if (!ruda_type_spelling_render(&context, type, 1)) {
        ruda_vector_dispose(&context.bytes);
        return context.status;
    }
    if (!ruda_owned_bytes_copy(
            output, allocator, context.bytes.data, context.bytes.length
        )) {
        ruda_vector_dispose(&context.bytes);
        return RUDA_TYPE_SPELLING_ALLOCATION_FAILED;
    }
    ruda_vector_dispose(&context.bytes);
    return RUDA_TYPE_SPELLING_OK;
}
