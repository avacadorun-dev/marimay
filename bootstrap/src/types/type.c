#include "mmay/types/type.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/hash.h"

static const MarimayTypeKind mmay_primitive_type_kinds[] = {
    MMAY_TYPE_KIND_INVALID,
    MMAY_TYPE_KIND_BOOL,
    MMAY_TYPE_KIND_I8,
    MMAY_TYPE_KIND_I16,
    MMAY_TYPE_KIND_I32,
    MMAY_TYPE_KIND_I64,
    MMAY_TYPE_KIND_U8,
    MMAY_TYPE_KIND_U16,
    MMAY_TYPE_KIND_U32,
    MMAY_TYPE_KIND_U64,
    MMAY_TYPE_KIND_F32,
    MMAY_TYPE_KIND_F64,
    MMAY_TYPE_KIND_RUNE,
    MMAY_TYPE_KIND_STRING,
    MMAY_TYPE_KIND_VOID,
    MMAY_TYPE_KIND_NEVER
};

static void mmay_type_write_u32(unsigned char bytes[4], uint32_t value) {
    size_t index = 0;
    for (index = 0; index < 4; index += 1) {
        bytes[index] = (unsigned char)(value >> (index * 8));
    }
}

static void mmay_type_write_u64(unsigned char bytes[8], uint64_t value) {
    size_t index = 0;
    for (index = 0; index < 8; index += 1) {
        bytes[index] = (unsigned char)(value >> (index * 8));
    }
}

static bool mmay_type_canonical_digest(
    MarimayTypeKind kind,
    MarimaySymbolId symbol,
    size_t parameter_count,
    const MarimayTypeId *arguments,
    size_t argument_count,
    unsigned char digest[32]
) {
    MarimaySha256 hash;
    unsigned char kind_bytes[4];
    unsigned char parameter_count_bytes[8];
    unsigned char argument_count_bytes[8];
    size_t index = 0;
    mmay_type_write_u32(kind_bytes, (uint32_t)kind);
    mmay_type_write_u64(
        parameter_count_bytes,
        (uint64_t)parameter_count
    );
    mmay_type_write_u64(argument_count_bytes, (uint64_t)argument_count);
    mmay_sha256_init(&hash);
    if (!mmay_sha256_update(&hash, kind_bytes, sizeof(kind_bytes))
        || !mmay_sha256_update(&hash, symbol.bytes, sizeof(symbol.bytes))
        || !mmay_sha256_update(
            &hash,
            parameter_count_bytes,
            sizeof(parameter_count_bytes)
        ) || !mmay_sha256_update(
            &hash,
            argument_count_bytes,
            sizeof(argument_count_bytes)
        )) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        unsigned char type_bytes[4];
        mmay_type_write_u32(type_bytes, arguments[index]);
        if (!mmay_sha256_update(&hash, type_bytes, sizeof(type_bytes))) {
            return false;
        }
    }
    mmay_sha256_final(&hash, digest);
    return true;
}

static bool mmay_symbol_hash_bytes(
    MarimaySha256 *hash,
    const void *bytes,
    size_t length
) {
    unsigned char length_bytes[8];
    size_t index = 0;
    if (hash == NULL || (bytes == NULL && length != 0)
        || (uintmax_t)length > UINT64_MAX) {
        return false;
    }
    for (index = 0; index < sizeof(length_bytes); index += 1) {
        length_bytes[index] = (unsigned char)((uint64_t)length >> (index * 8));
    }
    return mmay_sha256_update(hash, length_bytes, sizeof(length_bytes))
        && mmay_sha256_update(hash, bytes, length);
}

MarimayTypeTableOptions mmay_type_table_default_options(void) {
    MarimayTypeTableOptions options;
    options.max_types = 1000000;
    options.max_type_arguments = 1024;
    return options;
}

bool mmay_symbol_id_from_bytes(
    const void *package_name,
    size_t package_name_length,
    const void *source_path,
    size_t source_path_length,
    const void *declaration_name,
    size_t declaration_name_length,
    uint32_t discriminator,
    MarimaySymbolId *result
) {
    MarimaySha256 hash;
    unsigned char discriminator_bytes[4];
    unsigned char digest[32];
    size_t index = 0;
    if (result == NULL || (package_name == NULL && package_name_length != 0)
        || (source_path == NULL && source_path_length != 0)
        || (declaration_name == NULL && declaration_name_length != 0)) {
        return false;
    }
    for (index = 0; index < sizeof(discriminator_bytes); index += 1) {
        discriminator_bytes[index] = (unsigned char)(discriminator >> (index * 8));
    }
    mmay_sha256_init(&hash);
    if (!mmay_symbol_hash_bytes(&hash, package_name, package_name_length)
        || !mmay_symbol_hash_bytes(&hash, source_path, source_path_length)
        || !mmay_symbol_hash_bytes(
            &hash,
            declaration_name,
            declaration_name_length
        )
        || !mmay_sha256_update(
            &hash,
            discriminator_bytes,
            sizeof(discriminator_bytes)
        )) {
        return false;
    }
    mmay_sha256_final(&hash, digest);
    memcpy(result->bytes, digest, sizeof(result->bytes));
    return true;
}

bool mmay_symbol_id_from_parts(
    const char *package_name,
    const char *source_path,
    const char *declaration_name,
    uint32_t discriminator,
    MarimaySymbolId *result
) {
    if (package_name == NULL || source_path == NULL
        || declaration_name == NULL) {
        return false;
    }
    return mmay_symbol_id_from_bytes(
        package_name,
        strlen(package_name),
        source_path,
        strlen(source_path),
        declaration_name,
        strlen(declaration_name),
        discriminator,
        result
    );
}

bool mmay_symbol_id_from_bytes_with_receiver(
    const void *package_name,
    size_t package_name_length,
    const void *source_path,
    size_t source_path_length,
    const void *declaration_name,
    size_t declaration_name_length,
    MarimaySymbolId receiver,
    MarimaySymbolId *result
) {
    static const unsigned char domain[] = {
        'r', 'e', 'c', 'e', 'i', 'v', 'e', 'r'
    };
    MarimaySha256 hash;
    unsigned char digest[32];
    if (result == NULL || (package_name == NULL && package_name_length != 0)
        || (source_path == NULL && source_path_length != 0)
        || (declaration_name == NULL && declaration_name_length != 0)) {
        return false;
    }
    mmay_sha256_init(&hash);
    if (!mmay_symbol_hash_bytes(&hash, package_name, package_name_length)
        || !mmay_symbol_hash_bytes(&hash, source_path, source_path_length)
        || !mmay_symbol_hash_bytes(
            &hash,
            declaration_name,
            declaration_name_length
        )
        || !mmay_sha256_update(&hash, domain, sizeof(domain))
        || !mmay_sha256_update(
            &hash,
            receiver.bytes,
            sizeof(receiver.bytes)
        )) {
        return false;
    }
    mmay_sha256_final(&hash, digest);
    memcpy(result->bytes, digest, sizeof(result->bytes));
    return true;
}

bool mmay_symbol_id_from_parts_with_receiver(
    const char *package_name,
    const char *source_path,
    const char *declaration_name,
    MarimaySymbolId receiver,
    MarimaySymbolId *result
) {
    if (package_name == NULL || source_path == NULL
        || declaration_name == NULL) {
        return false;
    }
    return mmay_symbol_id_from_bytes_with_receiver(
        package_name,
        strlen(package_name),
        source_path,
        strlen(source_path),
        declaration_name,
        strlen(declaration_name),
        receiver,
        result
    );
}

bool mmay_symbol_id_equal(MarimaySymbolId left, MarimaySymbolId right) {
    return memcmp(left.bytes, right.bytes, sizeof(left.bytes)) == 0;
}

bool mmay_type_table_init(
    MarimayTypeTable *table,
    MarimayAllocator allocator,
    MarimayTypeTableOptions options
) {
    static const uint64_t canonical_key0 = UINT64_C(0x727564612d747970);
    static const uint64_t canonical_key1 = UINT64_C(0x652d696e7465726e);
    bool descriptors_initialized = false;
    bool canonical_initialized = false;
    if (table == NULL || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL
        || options.max_types < MMAY_TYPE_FIRST_DYNAMIC
        || options.max_types > UINT32_MAX
        || options.max_type_arguments == 0) {
        return false;
    }
    memset(table, 0, sizeof(*table));
    descriptors_initialized = mmay_vector_init(
            &table->descriptors,
            allocator,
            sizeof(MarimayTypeDescriptor *)
        );
    canonical_initialized = mmay_map_init(
        &table->canonical,
        allocator,
        sizeof(MarimayTypeId),
        canonical_key0,
        canonical_key1
    );
    if (!descriptors_initialized || !canonical_initialized) {
        if (canonical_initialized) {
            mmay_map_dispose(&table->canonical);
        }
        if (descriptors_initialized) {
            mmay_vector_dispose(&table->descriptors);
        }
        memset(table, 0, sizeof(*table));
        return false;
    }
    table->allocator = allocator;
    table->options = options;
    table->initialized = true;
    return true;
}

void mmay_type_table_dispose(MarimayTypeTable *table) {
    size_t index = 0;
    if (table == NULL || !table->initialized) {
        return;
    }
    for (index = 0; index < table->descriptors.length; index += 1) {
        MarimayTypeDescriptor *const *slot = mmay_vector_at(
            &table->descriptors,
            index
        );
        if (slot != NULL && *slot != NULL) {
            mmay_deallocate(
                &table->allocator,
                *slot,
                (*slot)->allocation_size
            );
        }
    }
    mmay_map_dispose(&table->canonical);
    mmay_vector_dispose(&table->descriptors);
    memset(table, 0, sizeof(*table));
}

MarimayTypeKind mmay_type_kind(const MarimayTypeTable *table, MarimayTypeId type) {
    const MarimayTypeDescriptor *descriptor = NULL;
    if (type < MMAY_TYPE_FIRST_DYNAMIC) {
        return type < sizeof(mmay_primitive_type_kinds)
                / sizeof(mmay_primitive_type_kinds[0])
            ? mmay_primitive_type_kinds[type]
            : MMAY_TYPE_KIND_INVALID;
    }
    descriptor = mmay_type_descriptor(table, type);
    return descriptor != NULL ? descriptor->kind : MMAY_TYPE_KIND_INVALID;
}

const MarimayTypeDescriptor *mmay_type_descriptor(
    const MarimayTypeTable *table,
    MarimayTypeId type
) {
    const MarimayTypeDescriptor *const *slot = NULL;
    size_t index = 0;
    if (table == NULL || !table->initialized || type < MMAY_TYPE_FIRST_DYNAMIC) {
        return NULL;
    }
    index = (size_t)(type - MMAY_TYPE_FIRST_DYNAMIC);
    slot = mmay_vector_at_const(&table->descriptors, index);
    return slot != NULL ? *slot : NULL;
}

static bool mmay_type_arguments_valid(
    const MarimayTypeTable *table,
    const MarimayTypeId *arguments,
    size_t argument_count
) {
    size_t index = 0;
    if ((arguments == NULL && argument_count != 0)
        || argument_count > table->options.max_type_arguments) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        if (mmay_type_kind(table, arguments[index]) == MMAY_TYPE_KIND_INVALID) {
            return false;
        }
    }
    return true;
}

static bool mmay_type_descriptor_matches(
    const MarimayTypeDescriptor *descriptor,
    MarimayTypeKind kind,
    MarimaySymbolId symbol,
    size_t parameter_count,
    const MarimayTypeId *arguments,
    size_t argument_count
) {
    return descriptor != NULL && descriptor->kind == kind
        && mmay_symbol_id_equal(descriptor->symbol, symbol)
        && descriptor->parameter_count == parameter_count
        && descriptor->argument_count == argument_count
        && (argument_count == 0 || memcmp(
            descriptor->arguments,
            arguments,
            argument_count * sizeof(MarimayTypeId)
        ) == 0);
}

static bool mmay_type_intern(
    MarimayTypeTable *table,
    MarimayTypeKind kind,
    MarimaySymbolId symbol,
    size_t parameter_count,
    const MarimayTypeId *arguments,
    size_t argument_count,
    MarimayTypeId *result
) {
    unsigned char digest[32];
    const MarimayTypeId *existing = NULL;
    size_t argument_bytes = 0;
    size_t allocation_size = 0;
    MarimayTypeDescriptor *descriptor = NULL;
    MarimayTypeId type_id = MMAY_TYPE_INVALID;
    if (table == NULL || !table->initialized || result == NULL
        || !mmay_type_arguments_valid(table, arguments, argument_count)) {
        return false;
    }
    if (!mmay_type_canonical_digest(
            kind,
            symbol,
            parameter_count,
            arguments,
            argument_count,
            digest
        )) {
        return false;
    }
    existing = mmay_map_get_const(
        &table->canonical,
        digest,
        sizeof(digest)
    );
    if (existing != NULL) {
        const MarimayTypeDescriptor *canonical = mmay_type_descriptor(
            table,
            *existing
        );
        if (!mmay_type_descriptor_matches(
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
            >= table->options.max_types - MMAY_TYPE_FIRST_DYNAMIC
        || table->descriptors.length > UINT32_MAX - MMAY_TYPE_FIRST_DYNAMIC) {
        return false;
    }
    if (!mmay_size_multiply(
            argument_count,
            sizeof(MarimayTypeId),
            &argument_bytes
        ) || !mmay_size_add(
            offsetof(MarimayTypeDescriptor, arguments),
            argument_bytes,
            &allocation_size
        )) {
        return false;
    }
    descriptor = mmay_allocate(&table->allocator, allocation_size);
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
    if (!mmay_vector_push(&table->descriptors, &descriptor)) {
        mmay_deallocate(&table->allocator, descriptor, allocation_size);
        return false;
    }
    type_id = (MarimayTypeId)(MMAY_TYPE_FIRST_DYNAMIC
        + table->descriptors.length - 1);
    if (!mmay_map_put(
            &table->canonical,
            digest,
            sizeof(digest),
            &type_id
        )) {
        table->descriptors.length -= 1;
        mmay_deallocate(&table->allocator, descriptor, allocation_size);
        return false;
    }
    *result = type_id;
    return true;
}

bool mmay_type_intern_nominal(
    MarimayTypeTable *table,
    MarimayTypeKind kind,
    MarimaySymbolId symbol,
    const MarimayTypeId *arguments,
    size_t argument_count,
    MarimayTypeId *result
) {
    if (kind != MMAY_TYPE_KIND_NOMINAL && kind != MMAY_TYPE_KIND_INTERFACE
        && kind != MMAY_TYPE_KIND_TYPE_PARAMETER) {
        return false;
    }
    return mmay_type_intern(
        table,
        kind,
        symbol,
        0,
        arguments,
        argument_count,
        result
    );
}

bool mmay_type_intern_array(
    MarimayTypeTable *table,
    MarimayTypeId element,
    MarimayTypeId *result
) {
    MarimaySymbolId symbol;
    memset(&symbol, 0, sizeof(symbol));
    if (table == NULL || !table->initialized || result == NULL
        || mmay_type_kind(table, element) == MMAY_TYPE_KIND_INVALID) {
        return false;
    }
    return mmay_type_intern(
        table,
        MMAY_TYPE_KIND_ARRAY,
        symbol,
        0,
        &element,
        1,
        result
    );
}

bool mmay_type_array_element(
    const MarimayTypeTable *table,
    MarimayTypeId array,
    MarimayTypeId *element
) {
    const MarimayTypeDescriptor *descriptor = mmay_type_descriptor(table, array);
    if (descriptor == NULL || descriptor->kind != MMAY_TYPE_KIND_ARRAY
        || descriptor->parameter_count != 0
        || descriptor->argument_count != 1 || element == NULL) {
        return false;
    }
    *element = descriptor->arguments[0];
    return true;
}

bool mmay_type_intern_tuple(
    MarimayTypeTable *table,
    const MarimayTypeId *elements,
    size_t element_count,
    MarimayTypeId *result
) {
    MarimaySymbolId symbol;
    memset(&symbol, 0, sizeof(symbol));
    if (element_count == 0) {
        return false;
    }
    return mmay_type_intern(
        table,
        MMAY_TYPE_KIND_TUPLE,
        symbol,
        0,
        elements,
        element_count,
        result
    );
}

bool mmay_type_intern_function(
    MarimayTypeTable *table,
    const MarimayTypeId *parameters,
    size_t parameter_count,
    MarimayTypeId return_type,
    MarimayTypeId *result
) {
    MarimaySymbolId symbol;
    MarimayTypeId *arguments = NULL;
    size_t argument_count = 0;
    size_t allocation_size = 0;
    bool interned = false;
    memset(&symbol, 0, sizeof(symbol));
    if (table == NULL || !table->initialized
        || (parameters == NULL && parameter_count != 0)
        || mmay_type_kind(table, return_type) == MMAY_TYPE_KIND_INVALID
        || !mmay_size_add(parameter_count, 1, &argument_count)
        || argument_count > table->options.max_type_arguments
        || !mmay_size_multiply(
            argument_count,
            sizeof(MarimayTypeId),
            &allocation_size
        )) {
        return false;
    }
    arguments = mmay_allocate(&table->allocator, allocation_size);
    if (arguments == NULL) {
        return false;
    }
    if (parameter_count != 0) {
        memcpy(arguments, parameters, parameter_count * sizeof(MarimayTypeId));
    }
    arguments[parameter_count] = return_type;
    interned = mmay_type_intern(
        table,
        MMAY_TYPE_KIND_FUNCTION,
        symbol,
        parameter_count,
        arguments,
        argument_count,
        result
    );
    mmay_deallocate(&table->allocator, arguments, allocation_size);
    return interned;
}

bool mmay_inference_context_init(
    MarimayInferenceContext *context,
    MarimayAllocator allocator,
    size_t max_variables
) {
    if (context == NULL || allocator.allocate == NULL
        || allocator.deallocate == NULL || max_variables == 0
        || max_variables > UINT32_MAX) {
        return false;
    }
    memset(context, 0, sizeof(*context));
    if (!mmay_vector_init(
            &context->variables,
            allocator,
            sizeof(MarimayInferenceVariable)
        )) {
        return false;
    }
    context->allocator = allocator;
    context->max_variables = max_variables;
    context->initialized = true;
    return true;
}

void mmay_inference_context_dispose(MarimayInferenceContext *context) {
    if (context == NULL || !context->initialized) {
        return;
    }
    mmay_vector_dispose(&context->variables);
    memset(context, 0, sizeof(*context));
}

bool mmay_inference_new(MarimayInferenceContext *context, MarimayInferenceId *result) {
    MarimayInferenceVariable variable;
    if (context == NULL || !context->initialized || result == NULL
        || context->variables.length >= context->max_variables
        || context->variables.length >= UINT32_MAX) {
        return false;
    }
    variable.parent = (MarimayInferenceId)context->variables.length;
    variable.rank = 0;
    variable.binding = MMAY_TYPE_INVALID;
    if (!mmay_vector_push(&context->variables, &variable)) {
        return false;
    }
    *result = variable.parent;
    return true;
}

static MarimayInferenceId mmay_inference_find(
    MarimayInferenceContext *context,
    MarimayInferenceId variable
) {
    MarimayInferenceVariable *entry = NULL;
    MarimayInferenceId root = variable;
    if (context == NULL || !context->initialized
        || variable >= context->variables.length) {
        return MMAY_INFERENCE_INVALID;
    }
    for (;;) {
        entry = mmay_vector_at(&context->variables, root);
        if (entry == NULL || entry->parent >= context->variables.length) {
            return MMAY_INFERENCE_INVALID;
        }
        if (entry->parent == root) {
            break;
        }
        root = entry->parent;
    }
    while (variable != root) {
        MarimayInferenceId parent = 0;
        entry = mmay_vector_at(&context->variables, variable);
        if (entry == NULL) {
            return MMAY_INFERENCE_INVALID;
        }
        parent = entry->parent;
        entry->parent = root;
        variable = parent;
    }
    return root;
}

bool mmay_inference_unify_variables(
    MarimayInferenceContext *context,
    MarimayInferenceId left,
    MarimayInferenceId right
) {
    MarimayInferenceId left_root = mmay_inference_find(context, left);
    MarimayInferenceId right_root = mmay_inference_find(context, right);
    MarimayInferenceVariable *left_entry = NULL;
    MarimayInferenceVariable *right_entry = NULL;
    if (left_root == MMAY_INFERENCE_INVALID
        || right_root == MMAY_INFERENCE_INVALID) {
        return false;
    }
    if (left_root == right_root) {
        return true;
    }
    left_entry = mmay_vector_at(&context->variables, left_root);
    right_entry = mmay_vector_at(&context->variables, right_root);
    if (left_entry == NULL || right_entry == NULL
        || (left_entry->binding != MMAY_TYPE_INVALID
            && right_entry->binding != MMAY_TYPE_INVALID
            && left_entry->binding != right_entry->binding)) {
        return false;
    }
    if (left_entry->rank < right_entry->rank) {
        MarimayInferenceVariable *temporary_entry = left_entry;
        left_root = right_root;
        left_entry = right_entry;
        right_entry = temporary_entry;
    }
    right_entry->parent = left_root;
    if (left_entry->binding == MMAY_TYPE_INVALID) {
        left_entry->binding = right_entry->binding;
    }
    if (left_entry->rank == right_entry->rank) {
        left_entry->rank += 1;
    }
    return true;
}

bool mmay_inference_bind(
    MarimayInferenceContext *context,
    MarimayInferenceId variable,
    MarimayTypeId type
) {
    MarimayInferenceId root = mmay_inference_find(context, variable);
    MarimayInferenceVariable *entry = NULL;
    if (root == MMAY_INFERENCE_INVALID || type == MMAY_TYPE_INVALID) {
        return false;
    }
    entry = mmay_vector_at(&context->variables, root);
    if (entry == NULL || (entry->binding != MMAY_TYPE_INVALID
            && entry->binding != type)) {
        return false;
    }
    entry->binding = type;
    return true;
}

MarimayTypeId mmay_inference_resolve(
    MarimayInferenceContext *context,
    MarimayInferenceId variable
) {
    MarimayInferenceId root = mmay_inference_find(context, variable);
    const MarimayInferenceVariable *entry = NULL;
    if (root == MMAY_INFERENCE_INVALID) {
        return MMAY_TYPE_INVALID;
    }
    entry = mmay_vector_at_const(&context->variables, root);
    return entry != NULL ? entry->binding : MMAY_TYPE_INVALID;
}
