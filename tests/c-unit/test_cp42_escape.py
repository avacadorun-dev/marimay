#!/usr/bin/env python3
"""D103: exact nonfatal diagnostics and unchanged execution on both engines."""
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys

TOOL, ORIGINAL, WORK = map(pathlib.Path, sys.argv[1:])
WORK.mkdir(parents=True, exist_ok=True)
SOURCE = WORK / "source"
shutil.rmtree(SOURCE, ignore_errors=True)
shutil.copytree(ORIGINAL, SOURCE, ignore=shutil.ignore_patterns(".mmay"))
records = []

def run(name, command, status=0, out=None, err=None):
    result = subprocess.run([str(arg) for arg in command], capture_output=True, timeout=180)
    records.append({"name": name, "command": [str(arg) for arg in command], "status": result.returncode,
                    "stdout": result.stdout.decode(), "stderr": result.stderr.decode()})
    assert result.returncode == status, records[-1]
    if out is not None:
        assert result.stdout == out, records[-1]
    if err is not None:
        assert result.stderr == err, records[-1]
    return result

run("format", [TOOL, "fmt", SOURCE, "--check"], out=b"", err=b"")
expected = []
for file, start, end in (("counter/counter.my", 52, 94), ("main_test.my", 109, 121)):
    path = (SOURCE / file).resolve()
    expected.append({"file": path.as_posix(), "startByte": start, "endByte": end,
                     "snapshot": "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()})
baseline = {}
for style in ("human", "json"):
    for mode in ("main", "all"):
        flags = ["--all-targets"] if mode == "all" else []
        result = run(f"check/{style}/{mode}", [TOOL, "check", SOURCE, *flags, "--diagnostic", style], out=b"")
        baseline[style, mode] = result.stderr
        if style == "json":
            diagnostics = [json.loads(line) for line in result.stderr.splitlines()]
            assert len(diagnostics) == (2 if mode == "all" else 1), diagnostics
            for d, want in zip(diagnostics, expected):
                assert d["severity"] == "warning" and d["code"] == "T0020", d
                assert d["schema"] == "marimay.diagnostic/1", d
                assert all(d["primary"][key] == value for key, value in want.items()), d
        else:
            assert result.stderr.count(b"warning[T0020]") == (2 if mode == "all" else 1), result
    for engine in ("vm", "native", "both"):
        result = run(f"test/{style}/{engine}", [TOOL, "test", SOURCE, "--engine", engine,
                     "--diagnostic", style], err=baseline[style, "all"])
        assert b"3 passed; 0 failed" in result.stdout, result
    vm = [TOOL, "run", SOURCE, "--diagnostic", style, "--"]
    run(f"vm/{style}/ok", vm, out=b"5\n", err=baseline[style, "main"] + b"cleanup\n")
    panic = b"panic[G4000]: escape fixture\n"
    if style == "json":
        panic = (json.dumps({"schema": "marimay.diagnostic/1", "code": "G4000", "severity": "error",
                            "message": "escape fixture", "primary": None, "labels": [], "notes": [], "fixes": []},
                           separators=(",", ":")) + "\n").encode()
    run(f"vm/{style}/panic", vm + ["panic"], 1, out=b"",
        err=baseline[style, "main"] + b"cleanup\n" + panic)
    for profile in ("debug", "release"):
        binary = WORK / (profile + "-" + style + (".exe" if sys.platform == "win32" else ""))
        args = [TOOL, "build", SOURCE, "--output", binary, "--diagnostic", style]
        if profile == "release":
            args.append("--release")
        run(f"build/{style}/{profile}", args, err=baseline[style, "main"])
        assert binary.is_file()
        run(f"native/{style}/{profile}/ok", [binary], out=b"5\n", err=b"cleanup\n")
        run(f"native/{style}/{profile}/panic", [binary, "panic"], 1, out=b"",
            err=b"cleanup\npanic[G4000]: escape fixture\n")

# Warning in an imported package cannot mask a later real type error or permit
# an artifact. Error and warning observations must agree across all front doors.
source = SOURCE / "main.my"
source.write_bytes(source.read_bytes().replace(b"return 0;", b'return "wrong";'))
artifact = WORK / "must-not-exist"
artifact.unlink(missing_ok=True)
failed = None
for command in ("check", "run", "build", "test"):
    args = [TOOL, command, SOURCE, "--diagnostic", "json"]
    if command == "build":
        args += ["--output", artifact]
    result = run("error/" + command, args, 1, out=b"")
    diagnostics = [json.loads(line) for line in result.stderr.splitlines()]
    assert any(d["code"] == "T0020" and d["severity"] == "warning" for d in diagnostics), diagnostics
    assert sum(d["severity"] == "error" for d in diagnostics) == 1, diagnostics
    comparable = [d for d in diagnostics if not d["primary"]["file"].endswith("_test.my")]
    if failed is None:
        failed = comparable
    assert comparable == failed and not artifact.exists(), diagnostics
(WORK / "report.json").write_bytes(
    (json.dumps(records, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
print(f"escape warning: {len(records)} exact process observations passed")
