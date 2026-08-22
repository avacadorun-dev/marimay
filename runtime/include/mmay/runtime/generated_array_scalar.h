#ifndef MMAY_RUNTIME_GENERATED_ARRAY_SCALAR_H
#define MMAY_RUNTIME_GENERATED_ARRAY_SCALAR_H

/* Included after MarimayManagedArray by generated_array.h. */
#if defined(__clang__) || defined(__GNUC__)
#define MMAY_G_RESTRICT __restrict
#else
#define MMAY_G_RESTRICT restrict
#endif

static inline uint64_t mmay_managed_array_generated_get_scalar(
    const MarimayManagedArray *MMAY_G_RESTRICT array,
    size_t index,
    size_t stride
) {
    const unsigned char *MMAY_G_RESTRICT source =
        array->storage + index * stride;
    if (stride == sizeof(uint8_t)) {
        uint8_t value = 0; memcpy(&value, source, sizeof(value)); return value;
    }
    if (stride == sizeof(uint16_t)) {
        uint16_t value = 0; memcpy(&value, source, sizeof(value)); return value;
    }
    if (stride == sizeof(uint32_t)) {
        uint32_t value = 0; memcpy(&value, source, sizeof(value)); return value;
    }
    {
        uint64_t value = 0; memcpy(&value, source, sizeof(value)); return value;
    }
}

static inline bool mmay_managed_array_generated_set_scalar(
    MarimayManagedArray *MMAY_G_RESTRICT array,
    size_t index,
    size_t stride,
    uint64_t bits
) {
    unsigned char *MMAY_G_RESTRICT target = array->storage + index * stride;
    if (array->mutation_generation == UINT64_MAX) return false;
    if (stride == sizeof(uint8_t)) {
        uint8_t value = (uint8_t)bits; memcpy(target, &value, sizeof(value));
    } else if (stride == sizeof(uint16_t)) {
        uint16_t value = (uint16_t)bits; memcpy(target, &value, sizeof(value));
    } else if (stride == sizeof(uint32_t)) {
        uint32_t value = (uint32_t)bits; memcpy(target, &value, sizeof(value));
    } else {
        memcpy(target, &bits, sizeof(bits));
    }
    array->mutation_generation += UINT64_C(1);
    return true;
}

static inline void mmay_managed_array_generated_set_scalar_untracked(
    MarimayManagedArray *MMAY_G_RESTRICT array,
    size_t index,
    size_t stride,
    uint64_t bits
) {
    unsigned char *MMAY_G_RESTRICT target = array->storage + index * stride;
    if (stride == sizeof(uint8_t)) {
        uint8_t value = (uint8_t)bits; memcpy(target, &value, sizeof(value));
    } else if (stride == sizeof(uint16_t)) {
        uint16_t value = (uint16_t)bits; memcpy(target, &value, sizeof(value));
    } else if (stride == sizeof(uint32_t)) {
        uint32_t value = (uint32_t)bits; memcpy(target, &value, sizeof(value));
    } else {
        memcpy(target, &bits, sizeof(bits));
    }
}

#undef MMAY_G_RESTRICT

#endif
