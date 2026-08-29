#include "mmay/check/type_spelling.h"

#include <string.h>

#include "mmay/base/vector.h"

typedef struct MarimayTypeSpellingContext {
    const MarimayCheckResult *result;
    MarimayVector bytes;
    size_t max_depth;
    size_t max_bytes;
    MarimayTypeSpellingStatus status;
} MarimayTypeSpellingContext;

static bool mmay_type_spelling_append(
    MarimayTypeSpellingContext *context,
    const void *bytes,
    size_t length
) {
    size_t new_length = 0;
    if (context == NULL || (bytes == NULL && length != 0)) {
        return false;
    }
    if (!mmay_size_add(context->bytes.length, length, &new_length)
        || new_length > context->max_bytes) {
        context->status = MMAY_TYPE_SPELLING_LIMIT_REACHED;
        return false;
    }
    if (!mmay_vector_append(&context->bytes, bytes, length)) {
        context->status = MMAY_TYPE_SPELLING_ALLOCATION_FAILED;
        return false;
    }
    return true;
}

static const char *mmay_type_spelling_primitive(MarimayTypeKind kind) {
    switch (kind) {
        case MMAY_TYPE_KIND_BOOL: return "bool";
        case MMAY_TYPE_KIND_I8: return "i8";
        case MMAY_TYPE_KIND_I16: return "i16";
        case MMAY_TYPE_KIND_I32: return "i32";
        case MMAY_TYPE_KIND_I64: return "int";
        case MMAY_TYPE_KIND_U8: return "byte";
        case MMAY_TYPE_KIND_U16: return "u16";
        case MMAY_TYPE_KIND_U32: return "u32";
        case MMAY_TYPE_KIND_U64: return "uint";
        case MMAY_TYPE_KIND_F32: return "f32";
        case MMAY_TYPE_KIND_F64: return "float";
        case MMAY_TYPE_KIND_RUNE: return "rune";
        case MMAY_TYPE_KIND_STRING: return "string";
        case MMAY_TYPE_KIND_VOID: return "void";
        case MMAY_TYPE_KIND_NEVER: return "never";
        default: return NULL;
    }
}

static const char *mmay_type_spelling_prelude_name(MarimaySymbolId symbol) {
    static const char *const names[] = {
        "Option", "Result", "ArrayGrowthError", "ArrayTruncateError",
        "CollectionGrowthError"
    };
    size_t index = 0;
    for (index = 0; index < sizeof(names) / sizeof(names[0]); index += 1) {
        MarimaySymbolId candidate;
        const size_t length = strlen(names[index]);
        if (mmay_symbol_id_from_bytes(
                "std", 3, "prelude", 7, names[index], length, 0, &candidate
            ) && mmay_symbol_id_equal(symbol, candidate)) {
            return names[index];
        }
    }
    return NULL;
}

static bool mmay_type_spelling_nominal_name(
    MarimayTypeSpellingContext *context,
    MarimaySymbolId symbol
) {
    const MarimayCheckedSymbol *checked = mmay_checked_symbol_by_id(
        context->result, symbol
    );
    const char *prelude = NULL;
    if (checked != NULL) {
        return mmay_type_spelling_append(
            context, checked->name.data, checked->name.length
        );
    }
    prelude = mmay_type_spelling_prelude_name(symbol);
    if (prelude != NULL) {
        return mmay_type_spelling_append(context, prelude, strlen(prelude));
    }
    context->status = MMAY_TYPE_SPELLING_UNAVAILABLE;
    return false;
}

static bool mmay_type_spelling_render(
    MarimayTypeSpellingContext *context,
    MarimayTypeId type,
    size_t depth
);

static bool mmay_type_spelling_arguments(
    MarimayTypeSpellingContext *context,
    const MarimayTypeDescriptor *descriptor,
    size_t depth,
    const char *open,
    const char *close
) {
    size_t index = 0;
    if (!mmay_type_spelling_append(context, open, strlen(open))) {
        return false;
    }
    for (index = 0; index < descriptor->argument_count; index += 1) {
        if (index != 0
            && !mmay_type_spelling_append(context, ", ", 2)) {
            return false;
        }
        if (!mmay_type_spelling_render(
                context, descriptor->arguments[index], depth + 1
            )) {
            return false;
        }
    }
    return mmay_type_spelling_append(context, close, strlen(close));
}

static bool mmay_type_spelling_function(
    MarimayTypeSpellingContext *context,
    const MarimayTypeDescriptor *descriptor,
    size_t depth
) {
    size_t index = 0;
    if (descriptor->argument_count != descriptor->parameter_count + 1) {
        context->status = MMAY_TYPE_SPELLING_UNAVAILABLE;
        return false;
    }
    if (!mmay_type_spelling_append(context, "(", 1)) {
        return false;
    }
    for (index = 0; index < descriptor->parameter_count; index += 1) {
        if ((index != 0
                && !mmay_type_spelling_append(context, ", ", 2))
            || !mmay_type_spelling_render(
                context, descriptor->arguments[index], depth + 1
            )) {
            return false;
        }
    }
    return mmay_type_spelling_append(context, ") => ", 5)
        && mmay_type_spelling_render(
            context,
            descriptor->arguments[descriptor->parameter_count],
            depth + 1
        );
}

static bool mmay_type_spelling_render(
    MarimayTypeSpellingContext *context,
    MarimayTypeId type,
    size_t depth
) {
    const MarimayTypeKind kind = mmay_type_kind(&context->result->types, type);
    const char *primitive = mmay_type_spelling_primitive(kind);
    const MarimayTypeDescriptor *descriptor = NULL;
    if (depth > context->max_depth) {
        context->status = MMAY_TYPE_SPELLING_LIMIT_REACHED;
        return false;
    }
    if (primitive != NULL) {
        return mmay_type_spelling_append(
            context, primitive, strlen(primitive)
        );
    }
    descriptor = mmay_type_descriptor(&context->result->types, type);
    if (descriptor == NULL) {
        context->status = MMAY_TYPE_SPELLING_UNAVAILABLE;
        return false;
    }
    if (kind == MMAY_TYPE_KIND_ARRAY) {
        if (descriptor->argument_count != 1) {
            context->status = MMAY_TYPE_SPELLING_UNAVAILABLE;
            return false;
        }
        return mmay_type_spelling_append(context, "[]", 2)
            && mmay_type_spelling_render(
                context, descriptor->arguments[0], depth + 1
            );
    }
    if (kind == MMAY_TYPE_KIND_MAP || kind == MMAY_TYPE_KIND_SET) {
        const size_t expected = kind == MMAY_TYPE_KIND_MAP ? 2 : 1;
        const char *name = kind == MMAY_TYPE_KIND_MAP ? "Map" : "Set";
        if (descriptor->argument_count != expected
            || !mmay_type_spelling_append(context, name, strlen(name))) {
            context->status = descriptor->argument_count != expected
                ? MMAY_TYPE_SPELLING_UNAVAILABLE : context->status;
            return false;
        }
        return mmay_type_spelling_arguments(
            context, descriptor, depth, "<", ">"
        );
    }
    if (kind == MMAY_TYPE_KIND_TUPLE) {
        return mmay_type_spelling_arguments(
            context, descriptor, depth, "[", "]"
        );
    }
    if (kind == MMAY_TYPE_KIND_FUNCTION) {
        return mmay_type_spelling_function(context, descriptor, depth);
    }
    if (kind == MMAY_TYPE_KIND_NOMINAL || kind == MMAY_TYPE_KIND_INTERFACE) {
        if (!mmay_type_spelling_nominal_name(context, descriptor->symbol)) {
            return false;
        }
        return descriptor->argument_count == 0
            || mmay_type_spelling_arguments(
                context, descriptor, depth, "<", ">"
            );
    }
    context->status = MMAY_TYPE_SPELLING_UNAVAILABLE;
    return false;
}

MarimayTypeSpellingOptions mmay_type_spelling_default_options(void) {
    MarimayTypeSpellingOptions options;
    options.max_depth = 256;
    options.max_bytes = 1024 * 1024;
    return options;
}

MarimayTypeSpellingStatus mmay_checked_type_spelling(
    const MarimayCheckResult *result,
    MarimayTypeId type,
    MarimayAllocator allocator,
    MarimayTypeSpellingOptions options,
    MarimayOwnedBytes *output
) {
    MarimayTypeSpellingContext context;
    if (output != NULL) {
        memset(output, 0, sizeof(*output));
    }
    if (result == NULL || !result->initialized || type == MMAY_TYPE_INVALID
        || allocator.allocate == NULL || allocator.resize == NULL
        || allocator.deallocate == NULL || options.max_depth == 0
        || options.max_bytes == 0 || output == NULL) {
        return MMAY_TYPE_SPELLING_INVALID_ARGUMENT;
    }
    memset(&context, 0, sizeof(context));
    context.result = result;
    context.max_depth = options.max_depth;
    context.max_bytes = options.max_bytes;
    context.status = MMAY_TYPE_SPELLING_OK;
    if (!mmay_vector_init(&context.bytes, allocator, sizeof(unsigned char))) {
        return MMAY_TYPE_SPELLING_ALLOCATION_FAILED;
    }
    if (!mmay_type_spelling_render(&context, type, 1)) {
        mmay_vector_dispose(&context.bytes);
        return context.status;
    }
    if (!mmay_owned_bytes_copy(
            output, allocator, context.bytes.data, context.bytes.length
        )) {
        mmay_vector_dispose(&context.bytes);
        return MMAY_TYPE_SPELLING_ALLOCATION_FAILED;
    }
    mmay_vector_dispose(&context.bytes);
    return MMAY_TYPE_SPELLING_OK;
}
