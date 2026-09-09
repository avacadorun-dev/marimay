#include "cli_standard_packages.h"
#include "cli_standard_packages_internal.h"

#include <string.h>

static const unsigned char mmay_cli_std_convert_source[] =
    "public enum ParseError {\n"
    "  Empty,\n"
    "  InvalidDigit { offset: int, },\n"
    "  OutOfRange,\n"
    "  InvalidRadix,\n"
    "}\n\n"
    "public extern \"c\" fn parseInt(text: string): Result<int, ParseError>;\n"
    "public extern \"c\" fn parseIntRadix(text: string, radix: int): Result<int, ParseError>;\n"
    "public extern \"c\" fn formatF32(value: f32): string;\n"
    "public extern \"c\" fn formatF64(value: f64): string;\n"
    "public extern \"c\" fn formatBool(value: bool): string;\n"
    "public extern \"c\" fn formatI8(value: i8): string;\n"
    "public extern \"c\" fn formatI16(value: i16): string;\n"
    "public extern \"c\" fn formatI32(value: i32): string;\n"
    "public extern \"c\" fn formatI64(value: i64): string;\n"
    "public extern \"c\" fn formatU8(value: u8): string;\n"
    "public extern \"c\" fn formatU16(value: u16): string;\n"
    "public extern \"c\" fn formatU32(value: u32): string;\n"
    "public extern \"c\" fn formatU64(value: u64): string;\n";

static const unsigned char mmay_cli_std_io_source[] =
    "import { Bytes, fromArray, toArray } from \"std/bytes\";\n\n"
    "public enum Error {\n"
    "  CapabilityDenied { capability: string, },\n"
    "  ReadFailed,\n"
    "  WriteFailed,\n"
    "  FlushFailed,\n"
    "  InvalidUtf8,\n"
    "  LineTooLong { limit: int, },\n"
    "  InputTooLong { limit: int, },\n"
    "}\n\n"
    "public fn message(error: Error): string {\n"
    "  return match error {\n"
    "    CapabilityDenied { capability } => $\"capability {capability} is not granted\",\n"
    "    ReadFailed => \"standard input read failed\",\n"
    "    WriteFailed => \"standard stream write failed\",\n"
    "    FlushFailed => \"standard stream flush failed\",\n"
    "    InvalidUtf8 => \"standard input is not valid UTF-8\",\n"
    "    LineTooLong { limit } => $\"standard input line exceeds {limit}-byte limit\",\n"
    "    InputTooLong { limit } => $\"standard input exceeds {limit}-byte limit\",\n"
    "  };\n"
    "}\n\n"
    "public extern \"c\" fn readLine(limit: int): Result<Option<string>, Error>;\n"
    "public extern \"c\" fn print(text: string): Result<void, Error>;\n"
    "public extern \"c\" fn println(text: string): Result<void, Error>;\n"
    "public extern \"c\" fn eprint(text: string): Result<void, Error>;\n"
    "public extern \"c\" fn eprintln(text: string): Result<void, Error>;\n"
    "public extern \"c\" fn flushStdout(): Result<void, Error>;\n"
    "public extern \"c\" fn flushStderr(): Result<void, Error>;\n"
    "extern \"c\" fn mmayReadBytes(limit: int): Result<[]byte, Error>;\n"
    "extern \"c\" fn mmayWriteBytes(values: []byte): Result<void, Error>;\n"
    "extern \"c\" fn mmayEwriteBytes(values: []byte): Result<void, Error>;\n\n"
    "public fn readBytes(limit: int): Result<Bytes, Error> {\n"
    "  return match mmayReadBytes(limit) {\n"
    "    Ok(values) => Ok(fromArray(values)),\n"
    "    Err(error) => Err(error),\n"
    "  };\n"
    "}\n\n"
    "public fn writeBytes(value: Bytes): Result<void, Error> {\n"
    "  return mmayWriteBytes(toArray(value));\n"
    "}\n\n"
    "public fn ewriteBytes(value: Bytes): Result<void, Error> {\n"
    "  return mmayEwriteBytes(toArray(value));\n"
    "}\n";

static const unsigned char mmay_cli_std_bytes_source[] =
    "public type Bytes { values: []byte, }\n\n"
    "public type Iterator {\n"
    "  bytes: Bytes,\n"
    "  mutable index: int,\n"
    "  generation: u64,\n"
    "}\n\n"
    "public type Builder { value: Bytes, limit: int, }\n\n"
    "public enum SliceError {\n"
    "  OutOfBounds { start: int, end: int, length: int, },\n"
    "}\n\n"
    "public enum Utf8Error { Invalid { offset: int, }, }\n\n"
    "public enum BuildError {\n"
    "  InvalidLimit { limit: int, },\n"
    "  LimitExceeded { limit: int, required: int, },\n"
    "}\n\n"
    "extern \"c\" fn mmayEncodeUtf8(text: string): []byte;\n"
    "extern \"c\" fn mmayInvalidUtf8Offset(values: []byte): int;\n"
    "extern \"c\" fn mmayDecodeUtf8(values: []byte): string;\n"
    "extern \"c\" fn mmayGeneration(values: []byte): u64;\n"
    "extern \"c\" fn mmayRequireGeneration(values: []byte, generation: u64): void;\n\n"
    "public fn new(): Bytes { return Bytes { values: [] }; }\n\n"
    "public fn fromArray(values: []byte): Bytes {\n"
    "  const result = new();\n"
    "  const _ = result.values.reserve(values.length());\n"
    "  for value in values { const _ = result.values.push(value); }\n"
    "  return result;\n"
    "}\n\n"
    "public fn toArray(bytes: Bytes): []byte {\n"
    "  let result: []byte = [];\n"
    "  const _ = result.reserve(bytes.values.length());\n"
    "  for value in bytes.values { const _ = result.push(value); }\n"
    "  return result;\n"
    "}\n\n"
    "public fn length(bytes: Bytes): int { return bytes.values.length(); }\n"
    "public fn get(bytes: Bytes, index: int): byte { return bytes.values[index]; }\n"
    "public fn set(bytes: Bytes, index: int, value: byte): void { bytes.values[index] = value; }\n"
    "public fn push(bytes: Bytes, value: byte): Result<void, ArrayGrowthError> {\n"
    "  return bytes.values.push(value);\n"
    "}\n\n"
    "public fn reserve(bytes: Bytes, additional: int): Result<void, ArrayGrowthError> {\n"
    "  return bytes.values.reserve(additional);\n"
    "}\n\n"
    "public fn append(bytes: Bytes, suffix: Bytes): Result<void, ArrayGrowthError> {\n"
    "  let source = suffix;\n"
    "  if identity.same(bytes, suffix) { source = fromArray(suffix.values); }\n"
    "  bytes.values.reserve(source.values.length())?;\n"
    "  for value in source.values { bytes.values.push(value)?; }\n"
    "  return Ok();\n"
    "}\n\n"
    "public fn clear(bytes: Bytes): void { const _ = bytes.values.clear(); }\n"
    "public fn copy(bytes: Bytes): Bytes { return fromArray(bytes.values); }\n\n"
    "public fn slice(bytes: Bytes, start: int, end: int): Result<Bytes, SliceError> {\n"
    "  const count = bytes.values.length();\n"
    "  if start < 0 || end < start || end > count {\n"
    "    return Err(SliceError.OutOfBounds { start, end, length: count });\n"
    "  }\n"
    "  const result = new();\n"
    "  const _ = result.values.reserve(end - start);\n"
    "  for index in start..end { const _ = result.values.push(bytes.values[index]); }\n"
    "  return Ok(result);\n"
    "}\n\n"
    "public fn iterator(bytes: Bytes): Iterator {\n"
    "  return Iterator { bytes, index: 0, generation: mmayGeneration(bytes.values) };\n"
    "}\n\n"
    "public fn next(iterator: Iterator): Option<byte> {\n"
    "  mmayRequireGeneration(iterator.bytes.values, iterator.generation);\n"
    "  if iterator.index == iterator.bytes.values.length() { return None; }\n"
    "  const value = iterator.bytes.values[iterator.index];\n"
    "  iterator.index += 1;\n"
    "  return Some(value);\n"
    "}\n\n"
    "public fn encodeUtf8(text: string): Bytes {\n"
    "  return Bytes { values: mmayEncodeUtf8(text) };\n"
    "}\n\n"
    "public fn decodeUtf8(bytes: Bytes): Result<string, Utf8Error> {\n"
    "  const offset = mmayInvalidUtf8Offset(bytes.values);\n"
    "  if offset >= 0 { return Err(Utf8Error.Invalid { offset }); }\n"
    "  return Ok(mmayDecodeUtf8(bytes.values));\n"
    "}\n";

static const unsigned char mmay_cli_std_bytes_tail[] =
    "public fn newBuilder(limit: int): Result<Builder, BuildError> {\n"
    "  if limit < 0 || limit > 1048576 { return Err(BuildError.InvalidLimit { limit }); }\n"
    "  return Ok(Builder { value: new(), limit });\n"
    "}\n\n"
    "public fn appendBytes(builder: Builder, value: Bytes): Result<void, BuildError> {\n"
    "  const current = builder.value.values.length();\n"
    "  const amount = value.values.length();\n"
    "  if amount > builder.limit - current {\n"
    "    return Err(BuildError.LimitExceeded { limit: builder.limit, required: builder.limit + 1 });\n"
    "  }\n"
    "  const _ = append(builder.value, value);\n"
    "  return Ok();\n"
    "}\n\n"
    "public fn appendByte(builder: Builder, value: byte): Result<void, BuildError> {\n"
    "  const current = builder.value.values.length();\n"
    "  if current == builder.limit {\n"
    "    return Err(BuildError.LimitExceeded { limit: builder.limit, required: builder.limit + 1 });\n"
    "  }\n"
    "  const _ = builder.value.values.push(value);\n"
    "  return Ok();\n"
    "}\n\n"
    "public fn finish(builder: Builder): Bytes { return copy(builder.value); }\n";

static const unsigned char mmay_cli_std_text_source[] =
    "type TextOwner = string;\n"
    "public enum BoundaryError { OutOfBounds { start: int, end: int, length: int, }, NotRuneBoundary { offset: int, }, }\n"
    "public enum LimitError { InvalidLimit { limit: int, }, LimitExceeded { limit: int, required: int, }, }\n"
    "public enum SplitError { EmptySeparator, InvalidLimit { limit: int, }, LimitExceeded { limit: int, required: int, }, }\n"
    "public type Builder { chunks: []string, mutable length: int, limit: int, }\n\n"
    "extern \"c\" fn mmayByteLength(text: string): int;\n"
    "extern \"c\" fn mmayRuneCount(text: string): int;\n"
    "extern \"c\" fn mmayGraphemeCount(text: string): int;\n"
    "extern \"c\" fn mmayIsRuneBoundary(text: string, offset: int): bool;\n"
    "extern \"c\" fn mmaySubstring(text: string, start: int, end: int): string;\n"
    "extern \"c\" fn mmayDropPrefix(text: string, offset: int): string;\n"
    "extern \"c\" fn mmayTakePrefix(text: string, offset: int): string;\n"
    "extern \"c\" fn mmayContains(text: string, needle: string): bool;\n"
    "extern \"c\" fn mmayStartsWith(text: string, prefix: string): bool;\n"
    "extern \"c\" fn mmayEndsWith(text: string, suffix: string): bool;\n"
    "extern \"c\" fn mmayTrimAscii(text: string): string;\n"
    "extern \"c\" fn mmaySplitCount(text: string, separator: string): int;\n"
    "extern \"c\" fn mmaySplit(text: string, separator: string): []string;\n"
    "extern \"c\" fn mmayJoin(parts: []string, separator: string): string;\n"
    "extern \"c\" fn mmayRunes(text: string): []rune;\n"
    "extern \"c\" fn mmayGraphemes(text: string): []string;\n\n"
    "public fn byteLength(text: string): int { return mmayByteLength(text); }\n"
    "public fn runeCount(text: string): int { return mmayRuneCount(text); }\n"
    "public fn contains(text: string, needle: string): bool { return mmayContains(text, needle); }\n"
    "public fn startsWith(text: string, prefix: string): bool { return mmayStartsWith(text, prefix); }\n"
    "public fn endsWith(text: string, suffix: string): bool { return mmayEndsWith(text, suffix); }\n"
    "public fn trimAscii(text: string): string { return mmayTrimAscii(text); }\n\n"
    "public fn runes(text: string, limit: int): Result<[]rune, LimitError> { if limit < 0 || limit > 1048576 { return Err(LimitError.InvalidLimit { limit }); } const required = mmayRuneCount(text); if required > limit { return Err(LimitError.LimitExceeded { limit, required }); } return Ok(mmayRunes(text)); }\n"
    "public fn graphemes(text: string, limit: int): Result<[]string, LimitError> { if limit < 0 || limit > 1048576 { return Err(LimitError.InvalidLimit { limit }); } const required = mmayGraphemeCount(text); if required > limit { return Err(LimitError.LimitExceeded { limit, required }); } return Ok(mmayGraphemes(text)); }\n";

static const unsigned char mmay_cli_std_text_tail[] =
    "public fn substring(text: string, start: int, end: int): Result<string, BoundaryError> {\n"
    "  const count = mmayByteLength(text);\n"
    "  if start < 0 || end < start || end > count {\n"
    "    return Err(BoundaryError.OutOfBounds { start, end, length: count });\n"
    "  }\n"
    "  if !mmayIsRuneBoundary(text, start) { return Err(BoundaryError.NotRuneBoundary { offset: start }); }\n"
    "  if !mmayIsRuneBoundary(text, end) { return Err(BoundaryError.NotRuneBoundary { offset: end }); }\n"
    "  return Ok(mmaySubstring(text, start, end));\n"
    "}\n\n"
    "public fn split(text: string, separator: string, limit: int): Result<[]string, SplitError> {\n"
    "  if mmayByteLength(separator) == 0 { return Err(SplitError.EmptySeparator); }\n"
    "  if limit < 0 || limit > 65536 { return Err(SplitError.InvalidLimit { limit }); }\n"
    "  const required = mmaySplitCount(text, separator);\n"
    "  if required > limit { return Err(SplitError.LimitExceeded { limit, required }); }\n"
    "  return Ok(mmaySplit(text, separator));\n"
    "}\n\n"
    "public fn join(parts: []string, separator: string, limit: int): Result<string, LimitError> {\n"
    "  if limit < 0 || limit > 1048576 { return Err(LimitError.InvalidLimit { limit }); }\n"
    "  let required = 0;\n"
    "  let first = true;\n"
    "  for part in parts {\n"
    "    if !first {\n"
    "      const amount = mmayByteLength(separator);\n"
    "      if amount > limit - required {\n"
    "        return Err(LimitError.LimitExceeded { limit, required: limit + 1 });\n"
    "      }\n"
    "      required += amount;\n"
    "    }\n"
    "    const amount = mmayByteLength(part);\n"
    "    if amount > limit - required {\n"
    "      return Err(LimitError.LimitExceeded { limit, required: limit + 1 });\n"
    "    }\n"
    "    required += amount;\n"
    "    first = false;\n"
    "  }\n"
    "  return Ok(mmayJoin(parts, separator));\n"
    "}\n\n"
    "public fn newBuilder(limit: int): Result<Builder, LimitError> { if limit < 0 || limit > 1048576 { return Err(LimitError.InvalidLimit { limit }); } return Ok(Builder { chunks: [], length: 0, limit }); }\n"
    "public fn append(builder: Builder, text: string): Result<void, LimitError> { const amount = mmayByteLength(text); if amount > builder.limit - builder.length { return Err(LimitError.LimitExceeded { limit: builder.limit, required: builder.limit + 1 }); } const _ = builder.chunks.push(text); builder.length += amount; return Ok(); }\n"
    "public fn finish(builder: Builder): string { return mmayJoin(builder.chunks, \"\"); }\n";

static const unsigned char mmay_cli_std_path_source[] =
    "public enum Error {\n"
    "  InvalidCharacter,\n"
    "  UnsupportedPrefix,\n"
    "  EscapesRoot,\n"
    "  TooLong { limit: int, },\n"
    "}\n\n"
    "public type Path { text: string, absolute: bool, }\n\n"
    "extern \"c\" fn mmayPathNormalize(text: string): Result<string, Error>;\n"
    "extern \"c\" fn mmayPathIsAbsolute(text: string): bool;\n\n"
    "public fn fromString(text: string): Result<Path, Error> {\n"
    "  return match mmayPathNormalize(text) {\n"
    "    Ok(value) => Ok(Path { text: value, absolute: mmayPathIsAbsolute(value) }),\n"
    "    Err(error) => Err(error),\n"
    "  };\n"
    "}\n\n"
    "public fn string(path: Path): string { return path.text; }\n"
    "public fn isAbsolute(path: Path): bool { return path.absolute; }\n\n"
    "public fn join(base: Path, child: Path): Result<Path, Error> {\n"
    "  if child.absolute { return Ok(child); }\n"
    "  return fromString($\"{base.text}/{child.text}\");\n"
    "}\n";

static const unsigned char mmay_cli_std_fs_source[] =
    "import * as bytes from \"std/bytes\";\n"
    "import * as paths from \"std/path\";\n\n"
    "public enum Error {\n"
    "  CapabilityDenied { capability: string, path: string, },\n"
    "  InvalidLimit { limit: int, },\n"
    "  LimitExceeded { limit: int, },\n"
    "  NotFound,\n"
    "  NotFile,\n"
    "  NotDirectory,\n"
    "  Symlink,\n"
    "  Closed,\n"
    "  OpenFailed,\n"
    "  ReadFailed,\n"
    "  WriteFailed,\n"
    "  SyncFailed,\n"
    "  CloseFailed,\n"
    "  ReplaceFailed,\n"
    "  InvalidUtf8 { offset: int, },\n"
    "  TooManyEntries { limit: int, },\n"
    "  DirectoryFailed,\n"
    "}\n\n"
    "public enum Kind { File, Directory, }\n"
    "public enum EntryKind { File, Directory, Symlink, Other, }\n"
    "public type File { handle: int, mutable closed: bool, }\n"
    "public type Metadata { public kind: Kind, public size: int, }\n"
    "public type Entry { public name: string, public kind: EntryKind, }\n\n"
    "extern \"c\" fn mmayFsOpen(text: string): Result<int, Error>;\n"
    "extern \"c\" fn mmayFsRead(handle: int, limit: int): Result<[]byte, Error>;\n"
    "extern \"c\" fn mmayFsReadLine(handle: int, limit: int): Result<Option<string>, Error>;\n"
    "extern \"c\" fn mmayFsClose(handle: int): Result<void, Error>;\n"
    "extern \"c\" fn mmayFsCloseBestEffort(handle: int): void;\n"
    "extern \"c\" fn mmayFsReadAll(text: string, limit: int): Result<[]byte, Error>;\n"
    "extern \"c\" fn mmayFsWriteAtomic(text: string, values: []byte): Result<void, Error>;\n"
    "extern \"c\" fn mmayFsMetadata(text: string): Result<Metadata, Error>;\n"
    "extern \"c\" fn mmayFsReadDirectory(text: string, limit: int): Result<[]Entry, Error>;\n\n"
    "public fn message(error: Error): string {\n"
    "  return match error {\n"
    "    CapabilityDenied { capability, path } => $\"{capability} capability denied for {path}\",\n"
    "    InvalidLimit { limit } => $\"invalid filesystem limit {limit}\",\n"
    "    LimitExceeded { limit } => $\"file exceeds {limit}-byte limit\",\n"
    "    NotFound => \"filesystem path was not found\",\n"
    "    NotFile => \"filesystem path is not a file\",\n"
    "    NotDirectory => \"filesystem path is not a directory\",\n"
    "    Symlink => \"symbolic links are not followed\",\n"
    "    Closed => \"file is closed\",\n"
    "    OpenFailed => \"file open failed\",\n"
    "    ReadFailed => \"file read failed\",\n"
    "    WriteFailed => \"file write failed\",\n"
    "    SyncFailed => \"file sync failed\",\n"
    "    CloseFailed => \"file close failed\",\n"
    "    ReplaceFailed => \"atomic file replacement failed\",\n"
    "    InvalidUtf8 { offset } => $\"file is not valid UTF-8 at byte {offset}\",\n"
    "    TooManyEntries { limit } => $\"directory exceeds {limit}-entry limit\",\n"
    "    DirectoryFailed => \"directory enumeration failed\",\n"
    "  };\n"
    "}\n\n"
    "public fn open(filePath: paths.Path): Result<File, Error> {\n"
    "  return match mmayFsOpen(paths.string(filePath)) {\n"
    "    Ok(handle) => Ok(File { handle, closed: false }),\n"
    "    Err(error) => Err(error),\n"
    "  };\n"
    "}\n\n"
    "public fn readLine(file: File, limit: int): Result<Option<string>, Error> {\n"
    "  if file.closed { return Err(Error.Closed); }\n"
    "  return mmayFsReadLine(file.handle, limit);\n"
    "}\n\n"
    "public fn read(file: File, limit: int): Result<bytes.Bytes, Error> {\n"
    "  if file.closed { return Err(Error.Closed); }\n"
    "  return match mmayFsRead(file.handle, limit) {\n"
    "    Ok(values) => Ok(bytes.fromArray(values)),\n"
    "    Err(error) => Err(error),\n"
    "  };\n"
    "}\n\n"
    "public fn close(file: File): Result<void, Error> {\n"
    "  if file.closed { return Ok(); }\n"
    "  const result = mmayFsClose(file.handle);\n"
    "  file.closed = true;\n"
    "  return result;\n"
    "}\n\n"
    "public fn closeBestEffort(file: File): void {\n"
    "  if !file.closed { mmayFsCloseBestEffort(file.handle); file.closed = true; }\n"
    "}\n\n"
    "public fn readBytes(filePath: paths.Path, limit: int): Result<bytes.Bytes, Error> {\n"
    "  return match mmayFsReadAll(paths.string(filePath), limit) {\n"
    "    Ok(values) => Ok(bytes.fromArray(values)),\n"
    "    Err(error) => Err(error),\n"
    "  };\n"
    "}\n";

static const unsigned char mmay_cli_std_fs_tail[] =
    "import * as bytes from \"std/bytes\";\n"
    "import * as paths from \"std/path\";\n\n"
    "fn fromUtf8Error(error: bytes.Utf8Error): Error {\n"
    "  return match error {\n"
    "    bytes.Invalid { offset } => Error.InvalidUtf8 { offset },\n"
    "  };\n"
    "}\n\n"
    "public fn readText(filePath: paths.Path, limit: int): Result<string, Error> {\n"
    "  const value = readBytes(filePath, limit)?;\n"
    "  return bytes.decodeUtf8(value).mapError(fromUtf8Error);\n"
    "}\n\n"
    "public fn writeBytes(filePath: paths.Path, value: bytes.Bytes): Result<void, Error> {\n"
    "  return mmayFsWriteAtomic(paths.string(filePath), bytes.toArray(value));\n"
    "}\n"
    "public fn writeText(filePath: paths.Path, value: string): Result<void, Error> {\n"
    "  return writeBytes(filePath, bytes.encodeUtf8(value));\n"
    "}\n"
    "public fn metadata(filePath: paths.Path): Result<Metadata, Error> {\n"
    "  return mmayFsMetadata(paths.string(filePath));\n"
    "}\n"
    "public fn readDirectory(filePath: paths.Path, limit: int): Result<[]Entry, Error> {\n"
    "  return mmayFsReadDirectory(paths.string(filePath), limit);\n"
    "}\n";

MarimayCliCheckTargetStatus mmay_cli_standard_package_add(
    MarimayCliCheckTarget *target,
    const MarimayCliStandardSource *standard
) {
    MarimayCliCheckPackage package;
    MarimayFileId file_id = 0;
    MarimayFileId tail_id = 0;
    MarimayFileId third_id = 0;
    MarimayCompilerStatus compiler_status;
    memset(&package, 0, sizeof(package));
    compiler_status = mmay_compiler_service_add_document(
        &target->service,
        standard->source_path,
        strlen(standard->source_path),
        1,
        standard->source,
        standard->source_length,
        &file_id
    );
    if (compiler_status != MMAY_COMPILER_OK) {
        return compiler_status == MMAY_COMPILER_ALLOCATION_FAILED
            ? MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED
            : (compiler_status == MMAY_COMPILER_LIMIT_REACHED
                ? MMAY_CLI_CHECK_TARGET_LIMIT_REACHED
                : MMAY_CLI_CHECK_TARGET_INTERNAL_ERROR);
    }
    if (standard->tail != NULL) {
        compiler_status = mmay_compiler_service_add_document(
            &target->service,
            standard->tail_path,
            strlen(standard->tail_path),
            1,
            standard->tail,
            standard->tail_length,
            &tail_id
        );
        if (compiler_status != MMAY_COMPILER_OK) {
            return compiler_status == MMAY_COMPILER_ALLOCATION_FAILED
                ? MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED
                : (compiler_status == MMAY_COMPILER_LIMIT_REACHED
                    ? MMAY_CLI_CHECK_TARGET_LIMIT_REACHED
                    : MMAY_CLI_CHECK_TARGET_INTERNAL_ERROR);
        }
    }
    if (standard->third != NULL) {
        compiler_status = mmay_compiler_service_add_document(
            &target->service,
            standard->third_path,
            strlen(standard->third_path),
            1,
            standard->third,
            standard->third_length,
            &third_id
        );
        if (compiler_status != MMAY_COMPILER_OK) {
            return compiler_status == MMAY_COMPILER_ALLOCATION_FAILED
                ? MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED
                : (compiler_status == MMAY_COMPILER_LIMIT_REACHED
                    ? MMAY_CLI_CHECK_TARGET_LIMIT_REACHED
                    : MMAY_CLI_CHECK_TARGET_INTERNAL_ERROR);
        }
    }
    if (!mmay_owned_bytes_copy(
            &package.import_path, target->allocator,
            standard->import_path, strlen(standard->import_path)
        ) || !mmay_owned_bytes_copy(
            &package.directory, target->allocator,
            standard->source_path, strlen(standard->source_path)
        ) || !mmay_vector_init(
            &package.file_ids, target->allocator, sizeof(MarimayFileId)
        ) || !mmay_vector_push(&package.file_ids, &file_id)
        || (standard->tail != NULL
            && !mmay_vector_push(&package.file_ids, &tail_id))
        || (standard->third != NULL
            && !mmay_vector_push(&package.file_ids, &third_id))
        || !mmay_vector_push(&target->packages, &package)) {
        mmay_vector_dispose(&package.file_ids);
        mmay_owned_bytes_dispose(&package.directory);
        mmay_owned_bytes_dispose(&package.import_path);
        return MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
    }
    memset(&package, 0, sizeof(package));
    return MMAY_CLI_CHECK_TARGET_OK;
}

MarimayCliCheckTargetStatus mmay_cli_standard_packages_add(
    MarimayCliCheckTarget *target
) {
    static const MarimayCliStandardSource sources[] = {
        {
            "std/convert", "std/convert/convert.my",
            mmay_cli_std_convert_source,
            sizeof(mmay_cli_std_convert_source) - 1,
            NULL, NULL, 0,
            NULL, NULL, 0
        },
        {
            "std/io", "std/io/io.my",
            mmay_cli_std_io_source,
            sizeof(mmay_cli_std_io_source) - 1,
            NULL, NULL, 0,
            NULL, NULL, 0
        },
        {
            "std/bytes", "std/bytes/bytes.my",
            mmay_cli_std_bytes_source,
            sizeof(mmay_cli_std_bytes_source) - 1,
            "std/bytes/builders.my",
            mmay_cli_std_bytes_tail,
            sizeof(mmay_cli_std_bytes_tail) - 1,
            NULL, NULL, 0
        },
        {
            "std/text", "std/text/text.my",
            mmay_cli_std_text_source,
            sizeof(mmay_cli_std_text_source) - 1,
            "std/text/builders.my",
            mmay_cli_std_text_tail,
            sizeof(mmay_cli_std_text_tail) - 1,
            NULL, NULL, 0
        },
        {
            "std/path", "std/path/path.my",
            mmay_cli_std_path_source,
            sizeof(mmay_cli_std_path_source) - 1,
            NULL, NULL, 0,
            NULL, NULL, 0
        },
        {
            "std/fs", "std/fs/fs.my",
            mmay_cli_std_fs_source,
            sizeof(mmay_cli_std_fs_source) - 1,
            "std/fs/text.my",
            mmay_cli_std_fs_tail,
            sizeof(mmay_cli_std_fs_tail) - 1,
            NULL, NULL, 0
        }
    };
    size_t index = 0;
    if (target == NULL || !target->service_initialized
        || !target->packages_initialized) {
        return MMAY_CLI_CHECK_TARGET_INTERNAL_ERROR;
    }
    for (index = 0; index < sizeof(sources) / sizeof(sources[0]); index += 1) {
        MarimayCliCheckTargetStatus status = mmay_cli_standard_package_add(
            target, &sources[index]
        );
        if (status != MMAY_CLI_CHECK_TARGET_OK) {
            return status;
        }
    }
    {
        MarimayCliCheckTargetStatus status =
            mmay_cli_standard_context_packages_add(target);
        if (status != MMAY_CLI_CHECK_TARGET_OK) return status;
    }
    {
        MarimayCliCheckTargetStatus status =
            mmay_cli_standard_json_package_add(target);
        if (status != MMAY_CLI_CHECK_TARGET_OK) return status;
    }
    return mmay_cli_standard_cli_package_add(target);
}
