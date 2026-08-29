#include "test_checker_imports_support.h"

#include <string.h>

#include "mmay/bytecode/codec.h"
#include "mmay/bytecode/mir_lower.h"

typedef struct MarimayCp34PipelineFixture {
    MarimayCheckerImportFixture source;
    MarimayCheckResult checked;
} MarimayCp34PipelineFixture;

static const char mmay_cp34_path_source[] =
    "public enum Error { InvalidCharacter, UnsupportedPrefix, EscapesRoot, TooLong { limit: int, }, }\n"
    "extern \"c\" fn mmayPathNormalize(text: string): Result<string, Error>;\n"
    "extern \"c\" fn mmayPathIsAbsolute(text: string): bool;\n"
    "public fn exercise(text: string): void {\n"
    "  const _ = mmayPathNormalize(text);\n"
    "  const _ = mmayPathIsAbsolute(text);\n"
    "}\n";

static const char mmay_cp34_fs_source[] =
    "public enum Error {\n"
    "  CapabilityDenied { capability: string, path: string, },\n"
    "  InvalidLimit { limit: int, }, LimitExceeded { limit: int, },\n"
    "  NotFound, NotFile, NotDirectory, Symlink, Closed, OpenFailed,\n"
    "  ReadFailed, WriteFailed, SyncFailed, CloseFailed, ReplaceFailed,\n"
    "  InvalidUtf8 { offset: int, }, TooManyEntries { limit: int, },\n"
    "  DirectoryFailed,\n"
    "}\n"
    "public enum Kind { File, Directory, }\n"
    "public enum EntryKind { File, Directory, Symlink, Other, }\n"
    "public type Metadata { public kind: Kind, public size: int, }\n"
    "public type Entry { public name: string, public kind: EntryKind, }\n"
    "extern \"c\" fn mmayFsOpen(text: string): Result<int, Error>;\n"
    "extern \"c\" fn mmayFsRead(handle: int, limit: int): Result<[]byte, Error>;\n"
    "extern \"c\" fn mmayFsClose(handle: int): Result<void, Error>;\n"
    "extern \"c\" fn mmayFsCloseBestEffort(handle: int): void;\n"
    "extern \"c\" fn mmayFsReadAll(text: string, limit: int): Result<[]byte, Error>;\n"
    "extern \"c\" fn mmayFsWriteAtomic(text: string, values: []byte): Result<void, Error>;\n"
    "extern \"c\" fn mmayFsMetadata(text: string): Result<Metadata, Error>;\n"
    "extern \"c\" fn mmayFsReadDirectory(text: string, limit: int): Result<[]Entry, Error>;\n"
    "public fn exercise(text: string, values: []byte, handle: int, limit: int): void {\n"
    "  const _ = mmayFsOpen(text); const _ = mmayFsRead(handle, limit);\n"
    "  const _ = mmayFsClose(handle); mmayFsCloseBestEffort(handle);\n"
    "  const _ = mmayFsReadAll(text, limit);\n"
    "  const _ = mmayFsWriteAtomic(text, values);\n"
    "  const _ = mmayFsMetadata(text);\n"
    "  const _ = mmayFsReadDirectory(text, limit);\n"
    "}\n";

static const char mmay_cp34_application_source[] =
    "import { exercise as exerciseFs } from \"std/fs\";\n"
    "import { exercise as exercisePath } from \"std/path\";\n"
    "fn main(): void { exercisePath(\"sample\"); exerciseFs(\"sample\", [1], 1, 2); }\n";

static bool mmay_cp34_pipeline_init(MarimayCp34PipelineFixture *fixture) {
    const MarimayTestImportSource sources[] = {
        {"test/cp34", "app/main.my", mmay_cp34_application_source},
        {"std/path", "std/path/path.my", mmay_cp34_path_source},
        {"std/fs", "std/fs/fs.my", mmay_cp34_fs_source}
    };
    memset(fixture, 0, sizeof(*fixture));
    return mmay_checker_import_fixture_init(
            &fixture->source, sources,
            sizeof(sources) / sizeof(sources[0])
        ) && mmay_checker_import_fixture_check(
            &fixture->source, "test/cp34", mmay_system_allocator(),
            mmay_check_graph_default_options(), &fixture->checked
        ) == MMAY_CHECK_OK
        && fixture->checked.diagnostics.length == 0;
}

static void mmay_cp34_pipeline_dispose(
    MarimayCp34PipelineFixture *fixture
) {
    mmay_check_result_dispose(&fixture->checked);
    mmay_checker_import_fixture_dispose(&fixture->source);
    memset(fixture, 0, sizeof(*fixture));
}

static bool mmay_cp34_mir_instruction_is_fs(
    const MarimayMirInstruction *instruction
) {
    return instruction != NULL && (instruction->kind == MMAY_MIR_PATH
        || instruction->kind == MMAY_MIR_FS_UNARY
        || instruction->kind == MMAY_MIR_FS_BINARY);
}

static size_t mmay_cp34_verify_hostile_mir(MarimayMirFunction *function) {
    size_t block_index = 0;
    size_t count = 0;
    MMAY_EXPECT(mmay_mir_verify(function) == MMAY_MIR_VERIFY_OK);
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1U) {
        MarimayMirBlock *block = mmay_vector_at(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0; block != NULL
            && instruction_index < block->instructions.length;
            instruction_index += 1U) {
            MarimayMirInstruction *instruction = mmay_vector_at(
                &block->instructions, instruction_index
            );
            MarimayMirRegister saved_target;
            MarimayMirFsOperation saved_operation;
            if (!mmay_cp34_mir_instruction_is_fs(instruction)) continue;
            saved_target = instruction->target;
            instruction->target = MMAY_MIR_NO_REGISTER;
            MMAY_EXPECT(mmay_mir_verify(function)
                == MMAY_MIR_VERIFY_INVALID_REGISTER);
            instruction->target = saved_target;
            saved_operation = instruction->fs_operation;
            instruction->fs_operation = (MarimayMirFsOperation)99;
            MMAY_EXPECT(mmay_mir_verify(function)
                == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
            instruction->fs_operation = saved_operation;
            count += 1U;
        }
    }
    MMAY_EXPECT(mmay_mir_verify(function) == MMAY_MIR_VERIFY_OK);
    return count;
}

static size_t mmay_cp34_verify_hostile_bytecode(
    MarimayBytecodeFunction *function
) {
    MarimayOwnedBytes encoded = {0};
    MarimayBytecodeFunction decoded;
    size_t block_index = 0;
    size_t count = 0;
    memset(&decoded, 0, sizeof(decoded));
    MMAY_EXPECT(mmay_bytecode_verify(function) == MMAY_BYTECODE_VERIFY_OK);
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1U) {
        MarimayBytecodeBlock *block = mmay_vector_at(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0; block != NULL
            && instruction_index < block->instructions.length;
            instruction_index += 1U) {
            MarimayBytecodeInstruction *instruction = mmay_vector_at(
                &block->instructions, instruction_index
            );
            uint64_t saved_target;
            if (instruction == NULL
                || instruction->opcode < MMAY_OPCODE_PATH_NORMALIZE
                || instruction->opcode > MMAY_OPCODE_FS_READ_DIRECTORY) {
                continue;
            }
            saved_target = instruction->operands[0].unsigned_value;
            instruction->operands[0].unsigned_value =
                function->register_types.length;
            MMAY_EXPECT(mmay_bytecode_verify(function)
                == MMAY_BYTECODE_VERIFY_INVALID_REGISTER);
            instruction->operands[0].unsigned_value = saved_target;
            count += 1U;
        }
    }
    MMAY_EXPECT(mmay_bytecode_verify(function) == MMAY_BYTECODE_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_encode_function(
        function, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_function(
        encoded.data, encoded.length, mmay_system_allocator(),
        mmay_bytecode_function_default_options(), &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_verify(&decoded) == MMAY_BYTECODE_VERIFY_OK);
    mmay_bytecode_function_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    return count;
}

void mmay_test_cp34_fs_pipeline(void) {
    MarimayCp34PipelineFixture fixture;
    size_t function_index = 0;
    size_t mir_count = 0;
    size_t bytecode_count = 0;
    MMAY_EXPECT(mmay_cp34_pipeline_init(&fixture));
    for (function_index = 0; function_index < fixture.checked.functions.length;
        function_index += 1U) {
        MarimayCheckedFunction *checked = mmay_vector_at(
            &fixture.checked.functions, function_index
        );
        MarimayBytecodeFunction lowered;
        size_t function_mir_count;
        if (checked == NULL || checked->is_external) continue;
        function_mir_count = mmay_cp34_verify_hostile_mir(&checked->mir);
        mir_count += function_mir_count;
        if (function_mir_count == 0) continue;
        memset(&lowered, 0, sizeof(lowered));
        MMAY_EXPECT(mmay_mir_lower_bytecode_function(
            &checked->mir, mmay_system_allocator(),
            mmay_mir_bytecode_lower_default_options(), &lowered
        ) == MMAY_MIR_BYTECODE_LOWER_OK);
        bytecode_count += mmay_cp34_verify_hostile_bytecode(&lowered);
        mmay_bytecode_function_dispose(&lowered);
    }
    MMAY_EXPECT(mir_count == 10);
    MMAY_EXPECT(bytecode_count == 10);
    mmay_cp34_pipeline_dispose(&fixture);
}
