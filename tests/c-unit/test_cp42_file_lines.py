"""D101 real-file/stdin byte parity; data and assertions are synthetic."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

compiler, source, work = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve(), Path(sys.argv[3]).resolve()
work.mkdir(parents=True, exist_ok=True)
root = work / "inputs"
root.mkdir(exist_ok=True)
started = time.monotonic()


def invoke(command, data=None):
    return subprocess.run([str(x) for x in command], input=data, capture_output=True, timeout=180)


def build(name, flags):
    output = work / (name + (".exe" if os.name == "nt" else ""))
    result = invoke([compiler, "build", source, *flags, "--output", output])
    assert result.returncode == 0 and not result.stderr, result
    return [output]


grants = ["--allow-read", root, "--max-heap-bytes", "16777216"]
engines = {"vm": [compiler, "run", source, *grants, "--"],
           "debug": build("lines-debug", grants),
           "release": build("lines-release", [*grants, "--release"])}
count = 0
cases = []


def compare(arguments, expected, data=None, selected=None):
    global count
    for engine, prefix in (engines if selected is None else selected).items():
        result = invoke([*prefix, *arguments], data)
        actual = (result.returncode, result.stdout, result.stderr)
        assert actual == expected, (engine, arguments, actual, expected)
        count += 1
    cases.append({"arguments": [str(x) for x in arguments], "input_sha256": hashlib.sha256(data or b"").hexdigest(),
                  "status": expected[0], "stdout_sha256": hashlib.sha256(expected[1]).hexdigest(),
                  "stderr": expected[2].decode("utf8")})


samples = [
    (b"", 0, b""), (b"\n", 0, b"\n"), (b"\r\n", 0, b"\n"),
    (b"a\x00b\r\n\nend\r", 4, b"a\x00b\n\nend\r\n"),
    ("Привет\r\n🌿\n終".encode(), 12, "Привет\n🌿\n終\n".encode()),
    (b"x" * 4095 + "🌿".encode() + b"\r\nlast", 4099, b"x" * 4095 + "🌿".encode() + b"\nlast\n"),
    (b"x" * 4095 + b"\r\nlast\n", 4095, b"x" * 4095 + b"\nlast\n"),
    (b"x" * 1048576 + b"\r\n", 1048576, b"x" * 1048576 + b"\n"),
]
for index, (data, limit, output) in enumerate(samples):
    file = root / f"sample {index} ü.txt"
    file.write_bytes(data)
    for name in (file, "-"):
        compare(["echo", name, limit], (0, output, b""), data)
    assert file.read_bytes() == data

# Errors publish no prefix of the failing line, including a split UTF-8 sequence.
errors = [(b"ab\n", 1, b"file exceeds 1-byte limit\n", b"standard input line exceeds 1-byte limit\n"),
          (b"a\r", 1, b"file exceeds 1-byte limit\n", b"standard input line exceeds 1-byte limit\n"),
          (b"a\xff\n", 8, b"file is not valid UTF-8 at byte 1\n", b"standard input is not valid UTF-8\n"),
          (b"x" * 4095 + b"\xf0\x9f\n", 4099, b"file is not valid UTF-8 at byte 4095\n", b"standard input is not valid UTF-8\n"),
          (b"", -1, b"invalid filesystem limit -1\n", b"standard input line exceeds -1-byte limit\n"),
          (b"", 1048577, b"invalid filesystem limit 1048577\n", b"standard input line exceeds 1048577-byte limit\n")]
for index, (data, limit, fs_error, io_error) in enumerate(errors):
    file = root / f"error-{index}"
    file.write_bytes(data)
    compare(["echo", file, limit], (1, b"", fs_error), data)
    compare(["echo", "-", limit], (1, b"", io_error), data)

file = root / "cursor"
file.write_bytes(b"a\r\nb\nc\r")
compare(["cursor", file, 1], (0, b"cursor ok\n", b""))
large = (b"x" * 2047 + b"\n") * 1025
file = root / "large"
file.write_bytes(large)
for name in (file, "-"):
    compare(["count", name, 2047], (0, b"1025:2098175\n", b""), large)
compare(["echo", root / "missing", 1], (1, b"", b"filesystem path was not found\n"))
if os.name != "nt":
    link = root / "link"
    if link.is_symlink(): link.unlink()
    link.symlink_to(file)
    compare(["echo", link, 1], (1, b"", b"symbolic links are not followed\n"))

denied = {"vm": [compiler, "run", source, "--"], "native": build("lines-denied", ["--release"])}
compare(["echo", file, 1], (1, b"", f"fs.read capability denied for {file}\n".encode()), selected=denied)

pipes = 0
if os.name != "nt":
    for prefix in engines.values():
        read_fd, write_fd = os.pipe()
        os.close(read_fd)
        try:
            child = subprocess.Popen([str(x) for x in [*prefix, "echo", "-", "8"]], stdin=subprocess.PIPE,
                                     stdout=write_fd, stderr=subprocess.PIPE)
        finally:
            os.close(write_fd)
        _, error = child.communicate(b"hello\n", timeout=180)
        assert child.returncode == 1 and error in (b"standard stream write failed\n", b"standard stream flush failed\n"), (child.returncode, error)
        pipes += 1

report = {"schema": "marimay.file-lines/1", "compiler_sha256": hashlib.sha256(compiler.read_bytes()).hexdigest(),
          "cases": cases, "executions": count, "closed_pipes": pipes,
          "large_bytes": len(large), "managed_heap_limit": 16777216,
          "elapsed_seconds": time.monotonic() - started}
(work / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
print(f"file lines: {count} exact process checks; {pipes} closed pipes; {len(large)}-byte stream")
