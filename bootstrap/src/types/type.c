#include "ruda/types/type.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/hash.h"

static const RudaTypeKind ruda_primitive_type_kinds[] = {
    RUDA_TYPE_KIND_INVALID,
    RUDA_TYPE_KIND_BOOL,
    RUDA_TYPE_KIND_I8,
    RUDA_TYPE_KIND_I16,
    RUDA_TYPE_KIND_I32,
    RUDA_TYPE_KIND_I64,
    RUDA_TYPE_KIND_U8,
    RUDA_TYPE_KIND_U16,
    RUDA_TYPE_KIND_U32,
    RUDA_TYPE_KIND_U64,
    RUDA_TYPE_KIND_F32,
    RUDA_TYPE_KIND_F64,
    RUDA_TYPE_KIND_RUNE,
    RUDA_TYPE_KIND_STRING,
    RUDA_TYPE_KIND_VOID,
    RUDA_TYPE_KIND_NEVER
};

static void ruda_type_write_u32(unsigned char bytes[4], uint32_t value) {
    size_t index = 0;
    for (index = 0; index < 4; index += 1) {
        bytes[index] = (unsigned char)(value >> (index * 8));
    }
}

static void ruda_type_write_u64(unsigned char bytes[8], uint64_t value) {
    size_t index = 0;
    for (index = 0; index < 8; index += 1) {
        bytes[index] = (unsigned char)(value >> (index * 8));
    }
}

static bool ruda_type_canonical_digest(
    RudaTypeKind kind,
    RudaSymbolId symbol,
    size_t parameter_count,
    const RudaTypeId *arguments,
    size_t argument_count,
    unsigned char digest[32]
) {
    RudaSha256 hash;
    unsigned char kind_bytes[4];
    unsigned char parameter_count_bytes[8];
    unsigned char argument_count_bytes[8];
    size_t index = 0;
    ruda_type_write_u32(kind_bytes, (uint32_t)kind);
    ruda_type_write_u64(
        parameter_count_bytes,
        (uint64_t)parameter_count
    );
    ruda_type_write_u64(argument_count_bytes, (uint64_t)argument_count);
    ruda_sha256_init(&hash);
    if (!ruda_sha256_update(&hash, kind_bytes, sizeof(kind_bytes))
        || !ruda_sha256_update(&hash, symbol.bytes, sizeof(symbol.bytes))
        || !ruda_sha256_update(
            &hash,
            parameter_count_bytes,
            sizeof(parameter_count_bytes)
        ) || !ruda_sha256_update(
            &hash,
            argument_count_bytes,
            sizeof(argument_count_bytes)
        )) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        unsigned char type_bytes[4];
        ruda_type_write_u32(type_bytes, arguments[index]);
        if (!ruda_sha256_update(&hash, type_bytes, sizeof(type_bytes))) {
            return false;
        }
    }
    ruda_sha256_final(&hash, digest);
    return true;
}

static bool ruda_symbol_hash_part(RudaSha256 *hash, const char *text) {
    unsigned char length_bytes[8];
    size_t length = 0;
    size_t index = 0;
    if (text == NULL) {
        return false;
    }
    length = strlen(text);
    for (index = 0; index < sizeof(length_bytes); index += 1) {
        length_bytes[index] = (unsigned char)((uint64_t)length >> (index * 8));
    }
    return ruda_sha256_update(hash, length_bytes, sizeof(length_bytes))
        && ruda_sha256_update(hash, text, length);
}

RudaTypeTableOptions ruda_type_table_default_options(void) {
    RudaTypeTableOptions options;
    options.max_types = 1000000;
    options.max_type_arguments = 1024;
    return options;
}

bool ruda_symbol_id_from_parts(
    const char *package_name,
    const char *source_path,
    const char *declaration_name,
    uint32_t discriminator,
    RudaSymbolId *result
) {
    RudaSha256 hash;
    unsigned char discriminator_bytes[4];
    unsigned char digest[32];
    size_t index = 0;
    if (result == NULL || package_name == NULL || source_path == NULL
        || declaration_name == NULL) {
        return false;
    }
    for (index = 0; index < sizeof(discriminator_bytes); index += 1) {
        discriminator_bytes[index] = (unsigned char)(discriminator >> (index * 8));
    }
    ruda_sha256_init(&hash);
    if (!ruda_symbol_hash_part(&hash, package_name)
        || !ruda_symbol_hash_part(&hash, source_path)
        || !ruda_symbol_hash_part(&hash, declaration_name)
        || !ruda_sha256_update(
            &hash,
            discriminator_bytes,
            sizeof(discriminator_bytes)
        )) {
        return false;
    }
    ruda_sha256_final(&hash, digest);
    memcpy(result->bytes, digest, sizeof(result->bytes));
    return true;
}

bool ruda_symbol_id_equal(RudaSymbolId left, RudaSymbolId right) {
    return memcmp(left.bytes, right.bytes, sizeof(left.bytes)) == 0;
}

bool ruda_type_table_init(
    RudaTypeTable *table,
    RudaAllocator allocator,
    RudaTypeTableOptions options
) {
    static const uint64_t canonical_key0 = UINT64_C(0x727564612d747970);
    static const uint64_t canonical_key1 = UINT64_C(0x652d696e7465726e);
    bool descriptors_initialized = false;
    bool canonical_initialized = false;
    if (table == NULL || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL
        || options.max_types < RUDA_TYPE_FIRST_DYNAMIC
        || options.max_types > UINT32_MAX
        || options.max_type_arguments == 0) {
        return false;
    }
    memset(table, 0, sizeof(*table));
    descriptors_initialized = ruda_vector_init(
            &table->descriptors,
            allocator,
            sizeof(RudaTypeDescriptor *)
        );
    canonical_initialized = ruda_map_init(
        &table->canonical,
        allocator,
        sizeof(RudaTypeId),
        canonical_key0,
        canonical_key1
    );
    if (!descriptors_initialized || !canonical_initialized) {
        if (canonical_initialized) {
            ruda_map_dispose(&table->canonical);
        }
        if (descriptors_initialized) {
            ruda_vector_dispose(&table->descriptors);
        }
        memset(table, 0, sizeof(*table));
        return false;
    }
    table->allocator = allocator;
    table->options = options;
    table->initialized = true;
    return true;
}

void ruda_type_table_dispose(RudaTypeTable *table) {
    size_t index = 0;
    if (table == NULL || !table->initialized) {
        return;
    }
    for (index = 0; index < table->descriptors.length; index += 1) {
        RudaTypeDescriptor *const *slot = ruda_vector_at(
            &table->descriptors,
            index
        );
        if (slot != NULL && *slot != NULL) {
            ruda_deallocate(
                &table->allocator,
                *slot,
                (*slot)->allocation_size
            );
        }
    }
    ruda_map_dispose(&table->canonical);
    ruda_vector_dispose(&table->descriptors);
    memset(table, 0, sizeof(*table));
}

RudaTypeKind ruda_type_kind(const RudaTypeTable *table, RudaTypeId type) {
    const RudaTypeDescriptor *descriptor = NULL;
    if (type < RUDA_TYPE_FIRST_DYNAMIC) {
        return type < sizeof(ruda_primitive_type_kinds)
                / sizeof(ruda_primitive_type_kinds[0])
            ? ruda_primitive_type_kinds[type]
            : RUDA_TYPE_KIND_INVALID;
    }
    descriptor = ruda_type_descriptor(table, type);
    return descriptor != NULL ? descriptor->kind : RUDA_TYPE_KIND_INVALID;
}

const RudaTypeDescriptor *ruda_type_descriptor(
    const RudaTypeTable *table,
    RudaTypeId type
) {
    const RudaTypeDescriptor *const *slot = NULL;
    size_t index = 0;
    if (table == NULL || !table->initialized || type < RUDA_TYPE_FIRST_DYNAMIC) {
        return NULL;
    }
    index = (size_t)(type - RUDA_TYPE_FIRST_DYNAMIC);
    slot = ruda_vector_at_const(&table->descriptors, index);
    return slot != NULL ? *slot : NULL;
}

static bool ruda_type_arguments_valid(
    const RudaTypeTable *table,
    const RudaTypeId *arguments,
    size_t argument_count
) {
    size_t index = 0;
    if ((arguments == NULL && argument_count != 0)
        || argument_count > table->options.max_type_arguments) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        if (ruda_type_kind(table, arguments[index]) == RUDA_TYPE_KIND_INVALID) {
            return false;
        }
    }
    return true;
}

static bool ruda_type_descriptor_matches(
    const RudaTypeDescriptor *descriptor,
    RudaTypeKind kind,
    RudaSymbolId symbol,
    size_t parameter_count,
    const RudaTypeId *arguments,
    size_t argument_count
) {
    return descriptor != NULL && descriptor->kind == kind
        && ruda_symbol_id_equal(descriptor->symbol, symbol)
        && descriptor->parameter_count == parameter_count
        && descriptor->argument_count == argument_count
        && (argument_count == 0 || memcmp(
            descriptor->arguments,
            arguments,
            argument_count * sizeof(RudaTypeId)
        ) == 0);
}

static bool ruda_type_intern(
    RudaTypeTable *table,
    RudaTypeKind kind,
    RudaSymbolId symbol,
    size_t parameter_count,
    const RudaTypeId *arguments,
    size_t argument_count,
    RudaTypeId *result
) {
    unsigned char digest[32];
    const RudaTypeId *existing = NULL;
    size_t argument_bytes = 0;
    size_t allocation_size = 0;
    RudaTypeDescriptor *descriptor = NULL;
    RudaTypeId type_id = RUDA_TYPE_INVALID;
    if (table == NULL || !table->initialized || result == NULL
        || !ruda_type_arguments_valid(table, arguments, argument_count)) {
        return false;
    }
    if (!ruda_type_canonical_digest(
            kind,
            symbol,
            parameter_count,
            arguments,
            argument_count,
            digest
        )) {
        return false;
    }
    existing = ruda_map_get_const(
        &table->canonical,
        digest,
        sizeof(digest)
    );
    if (existing != NULL) {
        const RudaTypeDescriptor *canonical = ruda_type_descriptor(
            table,
            *existing
        );
        if (!ruda_type_descriptor_matches(
                canonical,
                kind,
                symbol,
                parameter_count,
                arguments,
                argument_count
            )) {
            return false;
        }
        *result = *existing;
        return true;
    }
    if (table->descriptors.length
            >= table->options.max_types - RUDA_TYPE_FIRST_DYNAMIC
        || table->descriptors.length > UINT32_MAX - RUDA_TYPE_FIRST_DYNAMIC) {
        return false;
    }
    if (!ruda_size_multiply(
            argument_count,
            sizeof(RudaTypeId),
            &argument_bytes
        ) || !ruda_size_add(
            offsetof(RudaTypeDescriptor, arguments),
            argument_bytes,
            &allocation_size
        )) {
        return false;
    }
    descriptor = ruda_allocate(&table->allocator, allocation_size);
    if (descriptor == NULL) {
        return false;
    }
    descriptor->kind = kind;
    descriptor->symbol = symbol;
    descriptor->parameter_count = parameter_count;
    descriptor->argument_count = argument_count;
    descriptor->allocation_size = allocation_size;
    if (argument_count != 0) {
        memcpy(descriptor->arguments, arguments, argument_bytes);
    }
    if (!ruda_vector_push(&table->descriptors, &descriptor)) {
        ruda_deallocate(&table->allocator, descriptor, allocation_size);
        return false;
    }
    type_id = (RudaTypeId)(RUDA_TYPE_FIRST_DYNAMIC
        + table->descriptors.length - 1);
    if (!ruda_map_put(
            &table->canonical,
            digest,
            sizeof(digest),
            &type_id
        )) {
        table->descriptors.length -= 1;
        ruda_deallocate(&table->allocator, descriptor, allocation_size);
        return false;
    }
    *result = type_id;
    return true;
}

bool ruda_type_intern_nominal(
    RudaTypeTable *table,
    RudaTypeKind kind,
    RudaSymbolId symbol,
    const RudaTypeId *arguments,
    size_t argument_count,
    RudaTypeId *result
) {
    if (kind != RUDA_TYPE_KIND_NOMINAL && kind != RUDA_TYPE_KIND_INTERFACE
        && kind != RUDA_TYPE_KIND_TYPE_PARAMETER) {
        return false;
    }
    return ruda_type_intern(
        table,
        kind,
        symbol,
        0,
        arguments,
        argument_count,
        result
    );
}

bool ruda_type_intern_tuple(
    RudaTypeTable *table,
    const RudaTypeId *elements,
    size_t element_count,
    RudaTypeId *result
) {
    RudaSymbolId symbol;
    memset(&symbol, 0, sizeof(symbol));
    if (element_count == 0) {
        return false;
    }
    return ruda_type_intern(
        table,
        RUDA_TYPE_KIND_TUPLE,
        symbol,
        0,
        elements,
        element_count,
        result
    );
}

bool ruda_type_intern_function(
    RudaTypeTable *table,
    const RudaTypeId *parameters,
    size_t parameter_count,
    RudaTypeId return_type,
    RudaTypeId *result
) {
    RudaSymbolId symbol;
    RudaTypeId *arguments = NULL;
    size_t argument_count = 0;
    size_t allocation_size = 0;
    bool interned = false;
    memset(&symbol, 0, sizeof(symbol));
    if (table == NULL || !table->initialized
        || (parameters == NULL && parameter_count != 0)
        || ruda_type_kind(table, return_type) == RUDA_TYPE_KIND_INVALID
        || !ruda_size_add(parameter_count, 1, &argument_count)
        || argument_count > table->options.max_type_arguments
        || !ruda_size_multiply(
            argument_count,
            sizeof(RudaTypeId),
            &allocation_size
        )) {
        return false;
    }
    arguments = ruda_allocate(&table->allocator, allocation_size);
    if (arguments == NULL) {
        return false;
    }
    if (parameter_count != 0) {
        memcpy(arguments, parameters, parameter_count * sizeof(RudaTypeId));
    }
    arguments[parameter_count] = return_type;
    interned = ruda_type_intern(
        table,
        RUDA_TYPE_KIND_FUNCTION,
        symbol,
        parameter_count,
        arguments,
        argument_count,
        result
    );
    ruda_deallocate(&table->allocator, arguments, allocation_size);
    return interned;
}

bool ruda_inference_context_init(
    RudaInferenceContext *context,
    RudaAllocator allocator,
    size_t max_variables
) {
    if (context == NULL || allocator.allocate == NULL
        || allocator.deallocate == NULL || max_variables == 0
        || max_variables > UINT32_MAX) {
        return false;
    }
    memset(context, 0, sizeof(*context));
    if (!ruda_vector_init(
            &context->variables,
            allocator,
            sizeof(RudaInferenceVariable)
        )) {
        return false;
    }
    context->allocator = allocator;
    context->max_variables = max_variables;
    context->initialized = true;
    return true;
}

void ruda_inference_context_dispose(RudaInferenceContext *context) {
    if (context == NULL || !context->initialized) {
        return;
    }
    ruda_vector_dispose(&context->variables);
    memset(context, 0, sizeof(*context));
}

bool ruda_inference_new(RudaInferenceContext *context, RudaInferenceId *result) {
    RudaInferenceVariable variable;
    if (context == NULL || !context->initialized || result == NULL
        || context->variables.length >= context->max_variables
        || context->variables.length >= UINT32_MAX) {
        return false;
    }
    variable.parent = (RudaInferenceId)context->variables.length;
    variable.rank = 0;
    variable.binding = RUDA_TYPE_INVALID;
    if (!ruda_vector_push(&context->variables, &variable)) {
        return false;
    }
    *result = variable.parent;
    return true;
}

static RudaInferenceId ruda_inference_find(
    RudaInferenceContext *context,
    RudaInferenceId variable
) {
    RudaInferenceVariable *entry = NULL;
    RudaInferenceId root = variable;
    if (context == NULL || !context->initialized
        || variable >= context->variables.length) {
        return RUDA_INFERENCE_INVALID;
    }
    for (;;) {
        entry = ruda_vector_at(&context->variables, root);
        if (entry == NULL || entry->parent >= context->variables.length) {
            return RUDA_INFERENCE_INVALID;
        }
        if (entry->parent == root) {
            break;
        }
        root = entry->parent;
    }
    while (variable != root) {
        RudaInferenceId parent = 0;
        entry = ruda_vector_at(&context->variables, variable);
        if (entry == NULL) {
            return RUDA_INFERENCE_INVALID;
        }
        parent = entry->parent;
        entry->parent = root;
        variable = parent;
    }
    return root;
}

bool ruda_inference_unify_variables(
    RudaInferenceContext *context,
    RudaInferenceId left,
    RudaInferenceId right
) {
    RudaInferenceId left_root = ruda_inference_find(context, left);
    RudaInferenceId right_root = ruda_inference_find(context, right);
    RudaInferenceVariable *left_entry = NULL;
    RudaInferenceVariable *right_entry = NULL;
    if (left_root == RUDA_INFERENCE_INVALID
        || right_root == RUDA_INFERENCE_INVALID) {
        return false;
    }
    if (left_root == right_root) {
        return true;
    }
    left_entry = ruda_vector_at(&context->variables, left_root);
    right_entry = ruda_vector_at(&context->variables, right_root);
    if (left_entry == NULL || right_entry == NULL
        || (left_entry->binding != RUDA_TYPE_INVALID
            && right_entry->binding != RUDA_TYPE_INVALID
            && left_entry->binding != right_entry->binding)) {
        return false;
    }
    if (left_entry->rank < right_entry->rank) {
        RudaInferenceVariable *temporary_entry = left_entry;
        left_root = right_root;
        left_entry = right_entry;
        right_entry = temporary_entry;
    }
    right_entry->parent = left_root;
    if (left_entry->binding == RUDA_TYPE_INVALID) {
        left_entry->binding = right_entry->binding;
    }
    if (left_entry->rank == right_entry->rank) {
        left_entry->rank += 1;
    }
    return true;
}

bool ruda_inference_bind(
    RudaInferenceContext *context,
    RudaInferenceId variable,
    RudaTypeId type
) {
    RudaInferenceId root = ruda_inference_find(context, variable);
    RudaInferenceVariable *entry = NULL;
    if (root == RUDA_INFERENCE_INVALID || type == RUDA_TYPE_INVALID) {
        return false;
    }
    entry = ruda_vector_at(&context->variables, root);
    if (entry == NULL || (entry->binding != RUDA_TYPE_INVALID
            && entry->binding != type)) {
        return false;
    }
    entry->binding = type;
    return true;
}

RudaTypeId ruda_inference_resolve(
    RudaInferenceContext *context,
    RudaInferenceId variable
) {
    RudaInferenceId root = ruda_inference_find(context, variable);
    const RudaInferenceVariable *entry = NULL;
    if (root == RUDA_INFERENCE_INVALID) {
        return RUDA_TYPE_INVALID;
    }
    entry = ruda_vector_at_const(&context->variables, root);
    return entry != NULL ? entry->binding : RUDA_TYPE_INVALID;
}
