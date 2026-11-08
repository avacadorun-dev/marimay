"""Verify the installed D114 compiler policy against ambient overrides."""

import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile


def run(arguments, cwd, environment):
    result = subprocess.run(
        [str(argument) for argument in arguments], cwd=cwd, env=environment,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=180,
    )
    if result.returncode != 0 or result.stderr:
        raise AssertionError("command failed: %r" % result)
    return result.stdout


def main():
    tool = Path(sys.argv[1]).absolute()
    parent = Path(sys.argv[2]).resolve()
    parent.mkdir(parents=True, exist_ok=True)
    clean = os.environ.copy()
    with tempfile.TemporaryDirectory(prefix="host toolchain ", dir=parent) as temporary:
        root = Path(temporary)
        fake_bin = root / "untrusted tools"
        fake_bin.mkdir()
        marker = root / "untrusted compiler executed"
        fake = fake_bin / "clang"
        fake.write_bytes(("#!/bin/sh\n/usr/bin/touch " + shlex.quote(str(marker))
                          + "\nexit 99\n").encode("utf-8"))
        fake.chmod(0o755)
        poisoned = clean.copy()
        poisoned.update({
            "PATH": str(fake_bin), "CC": str(fake), "CXX": str(fake),
            "DEVELOPER_DIR": str(fake_bin), "SDKROOT": str(fake_bin),
            "TOOLCHAINS": str(fake_bin), "CPATH": str(fake_bin),
            "C_INCLUDE_PATH": str(fake_bin), "LIBRARY_PATH": str(fake_bin),
            "CCC_OVERRIDE_OPTIONS": "+--this-must-not-reach-clang",
            "MACOSX_DEPLOYMENT_TARGET": "99.0",
        })
        baseline = json.loads(run([tool, "doctor", "--json"], root, clean))
        overridden = json.loads(run([tool, "doctor", "--json"], root, poisoned))
        compiler = baseline["compiler"]
        if (not baseline["ok"] or compiler["path"] != "/usr/bin/clang"
                or compiler["id"] != "AppleClang" or not compiler["available"]
                or compiler["policy"] != "macos-system-clang-v1"
                or not re.fullmatch(r"\d+\.\d+\.\d+", compiler["version"])
                or int(compiler["version"].split(".")[0]) < 16
                or not re.fullmatch(r"clang-\d+(?:\.\d+)*", compiler["build"])
                or not re.fullmatch(r"(?:arm64|x86_64)-apple-(?:darwin|macosx)\d+(?:\.\d+)*",
                                    compiler["target"])):
            raise AssertionError("installed doctor did not report the local Apple policy")
        if overridden != baseline:
            raise AssertionError("ambient overrides changed doctor identity")
        source = root / "main.my"
        source.write_bytes(
            b'import { Error, println } from "std/io";\n'
            b'fn main(): Result<void, Error> {\n'
            b'  println("portable local compiler")?;\n  return Ok();\n}\n'
        )
        expected = b"portable local compiler\n"
        if run([tool, "run", source], root, clean) != expected:
            raise AssertionError("VM control output changed")
        artifacts = []
        for release in (False, True):
            arguments = [tool, "build", source] + (["--release"] if release else [])
            control = run(arguments, root, clean)
            replay = run(arguments, root, poisoned)
            if replay != control or not control.endswith(b"\n"):
                raise AssertionError("ambient overrides changed native cache identity")
            artifact = Path(control[:-1].decode("utf-8"))
            if not artifact.is_absolute():
                artifact = root / artifact
            if run([artifact], root, clean) != expected:
                raise AssertionError("VM/native output differs")
            artifacts.append(artifact)
        if artifacts[0] == artifacts[1]:
            raise AssertionError("debug and release cache identities were merged")
        if marker.exists():
            raise AssertionError("a compiler from the caller environment executed")


if __name__ == "__main__":
    main()
