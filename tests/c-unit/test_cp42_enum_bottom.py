#!/usr/bin/env python3
"""D102 public compile/execute contract; process observations include exact bytes."""
import json
import pathlib
import subprocess
import sys

TOOL, SOURCE, WORK = map(pathlib.Path, sys.argv[1:])
WORK.mkdir(parents=True, exist_ok=True)
records = []

def observe(name, command, status=0, out=b"", err=b""):
    result = subprocess.run([str(arg) for arg in command], capture_output=True, timeout=180)
    records.append({"name": name, "status": result.returncode,
                    "stdout": result.stdout.decode(), "stderr": result.stderr.decode()})
    assert (result.returncode, result.stdout, result.stderr) == (status, out, err), records[-1]

observe("format", [TOOL, "fmt", SOURCE, "--check"])
observe("check", [TOOL, "check", SOURCE, "--all-targets"])
result = subprocess.run([str(TOOL), "test", str(SOURCE), "--engine", "both"],
                        capture_output=True, timeout=180)
assert result.returncode == 0 and not result.stderr and b"5 passed; 0 failed" in result.stdout, result
records.append({"name": "test-both", "status": 0, "stdout": result.stdout.decode(), "stderr": ""})
for engine in ("vm", "debug", "release"):
    if engine == "vm":
        command = [TOOL, "run", SOURCE, "--"]
    else:
        artifact = WORK / (engine + (".exe" if sys.platform == "win32" else ""))
        build = [TOOL, "build", SOURCE, "--output", artifact]
        if engine == "release":
            build.append("--release")
        result = subprocess.run([str(arg) for arg in build], capture_output=True, timeout=180)
        assert result.returncode == 0 and not result.stderr and artifact.is_file(), result
        command = [artifact]
    observe(engine + "/empty", command)
    observe(engine + "/ok", command + ["ok"], out=b"7\n", err=b"factory\ncaller cleanup\n")
    observe(engine + "/err", command + ["err"], status=1,
            err=b"factory\ncallback cleanup\ncaller cleanup\npanic[G4000]: mapper panic\n")
    observe(engine + "/factory-panic", command + ["factory-panic"], status=1,
            err=b"caller cleanup\npanic[G4000]: factory panic\n")

negative = {
    "value": ('fn main():void{const r:Result<int,never> = Err(1);}', "T0002"),
    "unit": ('fn main():void{const r:Result<int,never> = Err();}', "T0009"),
    "unreachable": ('fn f(r:Result<int,never>):int{return match r{Ok(x)=>x,Err(x)=>0,};}'
                    'fn main():void{}', "T0014"),
    "missing": ('fn f(r:Result<int,never>):int{return match r{};}fn main():void{}', "T0013"),
    "error-type": ('fn f(r:Result<int,never>):Result<int,string>{const n=r?;return Ok(n);}'
                   'fn main():void{}', "T0002"),
}
for name, (source, code) in negative.items():
    path = WORK / (name + ".my")
    path.write_text(source)
    expected = None
    artifact = WORK / (name + (".exe" if sys.platform == "win32" else ".out"))
    artifact.unlink(missing_ok=True)
    for command in ("check", "run", "build"):
        args = [TOOL, command, path, "--diagnostic", "json"]
        if command == "build":
            args += ["--output", artifact]
        result = subprocess.run([str(arg) for arg in args], capture_output=True, timeout=180)
        diagnostics = [json.loads(line) for line in result.stderr.splitlines()]
        assert result.returncode == 1 and not result.stdout and not artifact.exists(), result
        assert len(diagnostics) == 1 and diagnostics[0]["code"] == code, diagnostics
        if expected is None:
            expected = result.stderr
        assert expected == result.stderr, (name, command, expected, result.stderr)
        records.append({"name": name + "/" + command, "status": 1, "diagnostics": diagnostics})
(WORK / "report.json").write_text(json.dumps(records, ensure_ascii=False, indent=2) + "\n")
print(f"enum bottom: {len(records)} exact process observations passed")
