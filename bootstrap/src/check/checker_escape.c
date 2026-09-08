#include "checker_escape_api.h"
#include "checker_core_api.h"

#include <string.h>

/* D103: flow-insensitive identities, field-sensitive containment, followed by
 * monotone propagation of the shallowest possible retaining scope. Everything
 * stays in this owner, outside published MIR, and is discarded on all exits. */
typedef struct EscapeRoot { size_t value, depth; } EscapeRoot;
typedef struct EscapeOrigin { size_t value, depth; MarimayMirSpan span; } EscapeOrigin;
typedef struct EscapeNode { size_t parent, rank, fields, depth; bool queued; } EscapeNode;
typedef enum EscapeSlot { RECORD_SLOT, ENUM_SLOT, ELEMENT_SLOT, KEY_SLOT, VALUE_SLOT, ENV_SLOT } EscapeSlot;
typedef struct EscapeField { EscapeSlot kind; size_t variant, index, value, next; } EscapeField;
typedef struct EscapePair { size_t a, b; } EscapePair;
typedef struct MarimayEscapeFacts {
    MarimayVector roots, origins, nodes, fields, pairs, queue;
    size_t work;
} MarimayEscapeFacts;

static bool tick(MarimayFunctionChecker *f) {
    if (f->escape->work >= f->checker->options.mir_options.max_verify_work) {
        mmay_checker_fail(f->checker, MMAY_CHECK_LIMIT_REACHED); return false;
    }
    f->escape->work += 1;
    return true;
}

static bool push(MarimayFunctionChecker *f, MarimayVector *v, const void *item) {
    if (!tick(f)) return false;
    if (mmay_vector_push(v, item)) return true;
    mmay_checker_fail(f->checker, MMAY_CHECK_ALLOCATION_FAILED); return false;
}

static bool prepare(MarimayFunctionChecker *f) {
    if (f->escape != NULL) return true;
    MarimayAllocator allocator = f->checker->result->allocator;
    f->escape = mmay_allocate(&allocator, sizeof(*f->escape));
    if (f->escape == NULL) {
        mmay_checker_fail(f->checker, MMAY_CHECK_ALLOCATION_FAILED); return false;
    }
    memset(f->escape, 0, sizeof(*f->escape));
    MarimayEscapeFacts *e = f->escape;
    if (!mmay_vector_init(&e->roots, allocator, sizeof(EscapeRoot))
        || !mmay_vector_init(&e->origins, allocator, sizeof(EscapeOrigin))
        || !mmay_vector_init(&e->nodes, allocator, sizeof(EscapeNode))
        || !mmay_vector_init(&e->fields, allocator, sizeof(EscapeField))
        || !mmay_vector_init(&e->pairs, allocator, sizeof(EscapePair))
        || !mmay_vector_init(&e->queue, allocator, sizeof(size_t))) {
        mmay_checker_fail(f->checker, MMAY_CHECK_ALLOCATION_FAILED); return false;
    }
    return true;
}

void mmay_escape_dispose(MarimayFunctionChecker *f) {
    MarimayEscapeFacts *e = f->escape;
    if (e == NULL) return;
    mmay_vector_dispose(&e->roots); mmay_vector_dispose(&e->origins);
    mmay_vector_dispose(&e->nodes); mmay_vector_dispose(&e->fields);
    mmay_vector_dispose(&e->pairs); mmay_vector_dispose(&e->queue);
    mmay_deallocate(&f->checker->result->allocator, e, sizeof(*e));
    f->escape = NULL;
}

bool mmay_escape_local(MarimayFunctionChecker *f, const MarimayCheckLocal *local) {
    if (!f->capture_scan_needed) return true;
    if (!prepare(f)) return false;
    if (local->scope_depth == SIZE_MAX) {
        mmay_checker_fail(f->checker, MMAY_CHECK_LIMIT_REACHED); return false;
    }
    EscapeRoot root = {local->register_id, local->scope_depth + 1};
    if (!push(f, &f->escape->roots, &root)) return false;
    if (local->cell_type != MMAY_TYPE_INVALID) {
        root.value = local->cell_register;
        if (!push(f, &f->escape->roots, &root)) return false;
    }
    return true;
}

bool mmay_escape_origin(MarimayFunctionChecker *f, MarimayMirRegister value,
    size_t depth, MarimayMirSpan span) {
    if (depth == 0) return true;
    if (!prepare(f)) return false;
    EscapeOrigin origin = {value, depth, span};
    return push(f, &f->escape->origins, &origin);
}

static EscapeNode *node(MarimayFunctionChecker *f, size_t id) {
    return mmay_vector_at(&f->escape->nodes, id);
}

static size_t representative(MarimayFunctionChecker *f, size_t id) {
    if (id >= f->escape->nodes.length) {
        mmay_checker_fail(f->checker, MMAY_CHECK_INTERNAL_ERROR); return SIZE_MAX;
    }
    while (node(f, id)->parent != id) {
        if (!tick(f)) return SIZE_MAX;
        node(f, id)->parent = node(f, node(f, id)->parent)->parent;
        id = node(f, id)->parent;
    }
    return id;
}

static size_t new_node(MarimayFunctionChecker *f) {
    size_t id = f->escape->nodes.length;
    EscapeNode n = {id, 0, SIZE_MAX, SIZE_MAX, false};
    return push(f, &f->escape->nodes, &n) ? id : SIZE_MAX;
}

static bool same_slot(EscapeField a, EscapeField b) {
    return a.kind == b.kind && a.variant == b.variant && a.index == b.index;
}

static size_t find_field(MarimayFunctionChecker *f, size_t owner, EscapeField key) {
    for (size_t i = node(f, owner)->fields; i != SIZE_MAX;) {
        if (!tick(f)) return SIZE_MAX;
        const EscapeField *field = mmay_vector_at_const(&f->escape->fields, i);
        if (same_slot(*field, key)) return i;
        i = field->next;
    }
    return SIZE_MAX;
}

static size_t slot(MarimayFunctionChecker *f, size_t owner, EscapeSlot kind,
    size_t variant, size_t index) {
    owner = representative(f, owner);
    if (owner == SIZE_MAX) return SIZE_MAX;
    EscapeField field = {kind, variant, index, SIZE_MAX, node(f, owner)->fields};
    size_t existing = find_field(f, owner, field);
    if (existing != SIZE_MAX)
        return ((const EscapeField *)mmay_vector_at_const(&f->escape->fields, existing))->value;
    if (mmay_checker_has_failed(f->checker)) return SIZE_MAX;
    field.value = new_node(f);
    if (field.value == SIZE_MAX || !push(f, &f->escape->fields, &field)) return SIZE_MAX;
    node(f, owner)->fields = f->escape->fields.length - 1;
    return field.value;
}

/* Equal identities merge equal field slots. A work stack, rather than C
 * recursion, handles cyclic records and aliases created by loops. */
static bool unify(MarimayFunctionChecker *f, size_t a, size_t b) {
    if (a == SIZE_MAX || b == SIZE_MAX) return false;
    EscapePair pair = {a, b};
    if (!push(f, &f->escape->pairs, &pair)) return false;
    while (f->escape->pairs.length != 0) {
        pair = *(const EscapePair *)mmay_vector_at_const(&f->escape->pairs, f->escape->pairs.length - 1);
        f->escape->pairs.length -= 1;
        a = representative(f, pair.a); b = representative(f, pair.b);
        if (a == SIZE_MAX || b == SIZE_MAX) return false;
        if (a == b) continue;
        if (node(f, a)->rank < node(f, b)->rank) { size_t swap = a; a = b; b = swap; }
        node(f, b)->parent = a;
        if (node(f, a)->rank == node(f, b)->rank) node(f, a)->rank += 1;
        if (node(f, b)->depth < node(f, a)->depth) node(f, a)->depth = node(f, b)->depth;
        size_t i = node(f, b)->fields;
        node(f, b)->fields = SIZE_MAX;
        while (i != SIZE_MAX) {
            if (!tick(f)) return false;
            EscapeField field = *(const EscapeField *)mmay_vector_at_const(&f->escape->fields, i);
            size_t existing = find_field(f, a, field);
            if (mmay_checker_has_failed(f->checker)) return false;
            if (existing == SIZE_MAX) {
                ((EscapeField *)mmay_vector_at(&f->escape->fields, i))->next = node(f, a)->fields;
                node(f, a)->fields = i;
            } else {
                pair = (EscapePair){field.value,
                    ((const EscapeField *)mmay_vector_at_const(&f->escape->fields, existing))->value};
                if (!push(f, &f->escape->pairs, &pair)) return false;
            }
            i = field.next;
        }
    }
    return true;
}

static bool retain(MarimayFunctionChecker *f, size_t value, size_t depth) {
    if (value == MMAY_MIR_NO_REGISTER) return true;
    value = representative(f, value);
    if (value == SIZE_MAX) return false;
    if (node(f, value)->depth > depth) node(f, value)->depth = depth;
    return true;
}

static bool field_value(MarimayFunctionChecker *f, size_t owner, EscapeSlot kind,
    size_t variant, size_t index, size_t value) {
    return unify(f, slot(f, owner, kind, variant, index), value);
}

static bool call_roots(MarimayFunctionChecker *f, MarimayMirCallId id, bool interface_call,
    MarimayMirRegister target) {
    const MarimayMirCall *call = mmay_vector_at_const(&f->mir.calls, id);
    if (call == NULL) { mmay_checker_fail(f->checker, MMAY_CHECK_INTERNAL_ERROR); return false; }
    for (size_t i = 0; i < call->argument_count; i += 1) {
        const MarimayMirCallArgument *arg = mmay_vector_at_const(&f->mir.call_arguments, call->argument_start + i);
        if (!tick(f)) return false;
        if (i == 0 && interface_call
            && mmay_type_kind(&f->checker->result->types, arg->type) == MMAY_TYPE_KIND_FUNCTION) {
            /* Invocation does not retain the callable itself. Its body can
             * expose values captured in its environment (including closures). */
            size_t environment = slot(f, arg->register_id, ENV_SLOT, 0, 0);
            if (environment == SIZE_MAX || !retain(f, environment, 0)) return false;
        } else if (!retain(f, arg->register_id, 0)) return false;
    }
    /* An opaque result can alias storage owned/retained by the callee. Scalar
     * results have no containment, so rooting them cannot warn about inputs. */
    return retain(f, target, 0);
}

static bool instruction(MarimayFunctionChecker *f, const MarimayMirInstruction *in) {
    switch (in->kind) {
        case MMAY_MIR_COPY: case MMAY_MIR_ENUM_REQUIRE_VARIANT:
            return unify(f, in->target, in->first);
        case MMAY_MIR_RECORD_NEW: {
            const MarimayMirRecordConstruction *c = mmay_vector_at_const(&f->mir.record_constructions, in->record_construction);
            for (size_t i = 0; i < c->argument_count; i += 1) {
                const MarimayMirRecordArgument *arg = mmay_vector_at_const(&f->mir.record_arguments, c->argument_start + i);
                if (!field_value(f, in->target, RECORD_SLOT, 0, i, arg->register_id)) return false;
            }
            return true;
        }
        case MMAY_MIR_RECORD_GET:
            return field_value(f, in->first, RECORD_SLOT, 0, in->field, in->target);
        case MMAY_MIR_RECORD_SET:
            return field_value(f, in->first, RECORD_SLOT, 0, in->field, in->second)
                && unify(f, in->target, in->second);
        case MMAY_MIR_ENUM_NEW: {
            const MarimayMirEnumConstruction *c = mmay_vector_at_const(&f->mir.enum_constructions, in->enum_construction);
            for (size_t i = 0; i < c->argument_count; i += 1) {
                const MarimayMirEnumArgument *arg = mmay_vector_at_const(&f->mir.enum_arguments, c->argument_start + i);
                if (!field_value(f, in->target, ENUM_SLOT, c->variant, i, arg->register_id)) return false;
            }
            return true;
        }
        case MMAY_MIR_ENUM_GET_PAYLOAD:
            return field_value(f, in->first, ENUM_SLOT, in->variant, in->field, in->target);
        case MMAY_MIR_INTERFACE_NEW: {
            const MarimayMirInterfaceConstruction *c = mmay_vector_at_const(&f->mir.interface_constructions, in->interface_construction);
            return field_value(f, in->target, ENV_SLOT, 0, 0, c->payload);
        }
        case MMAY_MIR_ARRAY_NEW: {
            const MarimayMirArrayConstruction *c = mmay_vector_at_const(&f->mir.array_constructions, in->array_construction);
            for (size_t i = 0; i < c->argument_count; i += 1) {
                const MarimayMirArrayArgument *arg = mmay_vector_at_const(&f->mir.array_arguments, c->argument_start + i);
                if (!field_value(f, in->target, ELEMENT_SLOT, 0, 0, arg->register_id)) return false;
            }
            return true;
        }
        case MMAY_MIR_ARRAY_GET: case MMAY_MIR_ARRAY_GET_IN_BOUNDS: case MMAY_MIR_ARRAY_POP:
            return field_value(f, in->first, ELEMENT_SLOT, 0, 0, in->target);
        case MMAY_MIR_ARRAY_SET: case MMAY_MIR_ARRAY_SET_IN_BOUNDS:
            return field_value(f, in->first, ELEMENT_SLOT, 0, 0, in->third)
                && unify(f, in->target, in->third);
        case MMAY_MIR_ARRAY_PUSH:
            return field_value(f, in->first, ELEMENT_SLOT, 0, 0, in->second)
                && unify(f, in->target, in->first);
        case MMAY_MIR_COLLECTION_REQUIRE_GENERATION:
        case MMAY_MIR_ARRAY_REQUIRE_INDEX: case MMAY_MIR_ARRAY_ASSUME_INDEX:
        case MMAY_MIR_ARRAY_REMOVE_AT: case MMAY_MIR_ARRAY_TRUNCATE:
        case MMAY_MIR_ARRAY_CLEAR: case MMAY_MIR_ARRAY_RESERVE: case MMAY_MIR_ARRAY_REQUIRE_GENERATION:
            return unify(f, in->target, in->first);
        case MMAY_MIR_ARRAY_COPY:
            return unify(f, slot(f, in->target, ELEMENT_SLOT, 0, 0), slot(f, in->first, ELEMENT_SLOT, 0, 0));
        case MMAY_MIR_COLLECTION_PUT: case MMAY_MIR_COLLECTION_PUT_RESULT:
            return field_value(f, in->first, KEY_SLOT, 0, 0, in->second)
                && field_value(f, in->first, VALUE_SLOT, 0, 0, in->third);
        case MMAY_MIR_COLLECTION_ADD: case MMAY_MIR_COLLECTION_ADD_RESULT:
            return field_value(f, in->first, KEY_SLOT, 0, 0, in->second);
        case MMAY_MIR_COLLECTION_GET: case MMAY_MIR_COLLECTION_ENTRY_VALUE:
        case MMAY_MIR_COLLECTION_CURSOR_VALUE:
            return field_value(f, in->first, VALUE_SLOT, 0, 0, in->target);
        case MMAY_MIR_COLLECTION_ENTRY_KEY: case MMAY_MIR_COLLECTION_CURSOR_KEY:
            return field_value(f, in->first, KEY_SLOT, 0, 0, in->target);
        case MMAY_MIR_COLLECTION_GET_OPTION: case MMAY_MIR_COLLECTION_REMOVE_OPTION:
            return unify(f, slot(f, in->first, VALUE_SLOT, 0, 0), slot(f, in->target, ENUM_SLOT, 0, 0));
        case MMAY_MIR_DEFER_CALL:
            return call_roots(f, in->call, false, MMAY_MIR_NO_REGISTER);
        case MMAY_MIR_CALL: case MMAY_MIR_CALL_INTERFACE:
            return call_roots(f, in->call, in->kind == MMAY_MIR_CALL_INTERFACE, in->target);
        default: return true; /* Scalar operations and removals introduce no aliases. */
    }
}

static bool enqueue(MarimayFunctionChecker *f, size_t id) {
    if (node(f, id)->queued) return true;
    if (!push(f, &f->escape->queue, &id)) return false;
    node(f, id)->queued = true; return true;
}

static bool propagate(MarimayFunctionChecker *f) {
    for (size_t i = 0; i < f->escape->nodes.length; i += 1) {
        if (!tick(f)) return false;
        if (node(f, i)->parent == i && node(f, i)->depth != SIZE_MAX && !enqueue(f, i)) return false;
    }
    while (f->escape->queue.length != 0) {
        size_t id = *(const size_t *)mmay_vector_at_const(&f->escape->queue, f->escape->queue.length - 1);
        f->escape->queue.length -= 1;
        node(f, id)->queued = false;
        for (size_t i = node(f, id)->fields; i != SIZE_MAX;) {
            if (!tick(f)) return false;
            const EscapeField *field = mmay_vector_at_const(&f->escape->fields, i);
            size_t child = representative(f, field->value);
            if (child == SIZE_MAX) return false;
            if (node(f, child)->depth > node(f, id)->depth) {
                node(f, child)->depth = node(f, id)->depth;
                if (!enqueue(f, child)) return false;
            }
            i = field->next;
        }
    }
    return true;
}

bool mmay_escape_check(MarimayFunctionChecker *f) {
    if (f->escape == NULL || f->escape->origins.length == 0) return true;
    for (size_t i = 0; i < f->mir.register_types.length; i += 1)
        if (new_node(f) == SIZE_MAX) return false;
    for (size_t i = 0; i < f->mir.parameter_count; i += 1)
        if (!tick(f) || !retain(f, i, 0)) return false;
    for (size_t i = 0; i < f->escape->roots.length; i += 1) {
        const EscapeRoot *root = mmay_vector_at_const(&f->escape->roots, i);
        if (!tick(f) || !retain(f, root->value, root->depth)) return false;
    }
    for (size_t b = 0; b < f->mir.blocks.length; b += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(&f->mir.blocks, b);
        if (!tick(f)) return false;
        for (size_t i = 0; i < block->instructions.length; i += 1)
            if (!tick(f) || !instruction(f, mmay_vector_at_const(&block->instructions, i))) return false;
        if (block->terminator.kind == MMAY_MIR_RETURN && !retain(f, block->terminator.value, 0)) return false;
        if (block->terminator.kind == MMAY_MIR_CALL_NEVER
            && !call_roots(f, block->terminator.call, false, MMAY_MIR_NO_REGISTER)) return false;
    }
    if (!propagate(f)) return false;
    for (size_t i = 0; i < f->escape->origins.length; i += 1) {
        const EscapeOrigin *origin = mmay_vector_at_const(&f->escape->origins, i);
        size_t value = representative(f, origin->value);
        if (!tick(f) || value == SIZE_MAX) return false;
        if (node(f, value)->depth < origin->depth
            && !mmay_checker_add_warning(f->checker, MMAY_DIAGNOSTIC_ESCAPING_MUTABLE_CAPTURE,
                "closure may outlive the block declaring a captured mutable local",
                origin->span.start, origin->span.end)) return false;
    }
    return true;
}
