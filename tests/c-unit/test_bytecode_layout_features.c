#include "test_bytecode_support.h"

/* Owner: bytecode codec feature closure over retained layout tables. */
void mmay_test_bytecode_layout_features(void) {
    MarimayBytecodeFunction function;
    MarimayBytecodeFunction decoded;
    MarimayOwnedBytes encoded;
    uint32_t layout = MMAY_BYTECODE_NO_LAYOUT;
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    MMAY_EXPECT(mmay_test_bytecode_build_constant(
        &function, mmay_system_allocator(), 42
    ));
    /* Imported signatures can retain layouts without live array registers. */
    MMAY_EXPECT(mmay_bytecode_add_array_type(
        &function, MMAY_BYTECODE_TYPE_STRING, MMAY_BYTECODE_NO_LAYOUT, &layout
    ));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_encode_function(
        &function, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_function(
        encoded.data, encoded.length, mmay_system_allocator(),
        mmay_bytecode_function_default_options(), &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    if (decoded.initialized) {
        MMAY_EXPECT(decoded.array_types.length == 1);
        MMAY_EXPECT(mmay_bytecode_verify(&decoded) == MMAY_BYTECODE_VERIFY_OK);
    }
    mmay_bytecode_function_dispose(&decoded);
    {
        size_t index = 0;
        bool succeeded = false;
        for (index = 0; index < 4096; index += 1) {
            MarimayFailAllocator state;
            MarimayBytecodeCodecStatus status;
            mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
            status = mmay_bytecode_decode_function(
                encoded.data, encoded.length, mmay_fail_allocator(&state),
                mmay_bytecode_function_default_options(), &decoded
            );
            succeeded = status == MMAY_BYTECODE_CODEC_OK;
            MMAY_EXPECT(succeeded
                || status == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED);
            mmay_bytecode_function_dispose(&decoded);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
            if (succeeded) break;
        }
        MMAY_EXPECT(succeeded);
    }
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_function_dispose(&function);
}
