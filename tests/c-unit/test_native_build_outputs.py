"""Exercise native compiler intermediates in a shared working directory."""

import ctypes
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def source(project, message):
    project.mkdir()
    path = project / "main.my"
    path.write_bytes(
        ('import { Error, println } from "std/io";\n'
         'fn main(): Result<void, Error> {\n'
         '  println("' + message + '")?;\n'
         '  return Ok();\n}\n').encode("utf-8")
    )
    return path


def command(tool, path, output, release=False):
    return [str(tool), "build", str(path), "--output", str(output)] + (
        ["--release"] if release else []
    )


def expect_build(process, output):
    stdout, stderr = process.communicate(timeout=180)
    if (process.returncode != 0 or stderr
            or stdout != (str(output) + "\n").encode("utf-8")):
        raise AssertionError(
            "native build failed: %d\n%r\n%r"
            % (process.returncode, stdout, stderr)
        )


def expect_no_objects(root):
    if any(path.is_file() for path in root.rglob("*.obj")):
        raise AssertionError("native build retained a transient object")


def expect_link_failure_cleanup(tool, root, cwd, suffix):
    path = source(root / "failed project", "unpublished")
    output = root / ("blocked artifact" + suffix)
    temporary = Path(str(output) + ".mmay-tmp")
    temporary.write_bytes(b"prior temporary bytes")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [
        ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p,
        ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p,
    ]
    kernel.CreateFileW.restype = ctypes.c_void_p
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    kernel.CloseHandle.restype = ctypes.c_int
    handle = kernel.CreateFileW(str(temporary), 0xC0000000, 0, None, 3, 0, None)
    if handle == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        result = subprocess.run(
            command(tool, path, output), cwd=cwd, capture_output=True,
            timeout=180, check=False,
        )
        if (result.returncode != 70 or result.stdout
                or b"error[B0004]" not in result.stderr
                or b"LNK" not in result.stderr or output.exists()):
            raise AssertionError("locked linker output did not fail as expected: %r" % result)
        expect_no_objects(root)
    finally:
        if not kernel.CloseHandle(handle):
            raise ctypes.WinError(ctypes.get_last_error())
    if temporary.read_bytes() != b"prior temporary bytes":
        raise AssertionError("failed native build changed the locked temporary")


def main():
    tool = Path(sys.argv[1]).resolve()
    parent = Path(sys.argv[2]).resolve()
    parent.mkdir(parents=True, exist_ok=True)
    suffix = ".exe" if os.name == "nt" else ""
    with tempfile.TemporaryDirectory(prefix="native outputs ", dir=parent) as directory:
        root = Path(directory)
        cwd = root / "shared working directory"
        cwd.mkdir()
        poison = cwd / "program.obj"
        poison.mkdir()
        processes = []
        try:
            for index, message in enumerate(("native alpha", "native beta")):
                path = source(root / ("project %d" % index), message)
                output = root / ("artifact %d%s" % (index, suffix))
                process = subprocess.Popen(
                    command(tool, path, output, release=index == 1),
                    cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                )
                processes.append((process, output, message))
            for process, output, message in processes:
                expect_build(process, output)
                result = subprocess.run([str(output)], capture_output=True, timeout=30)
                if (result.returncode != 0 or result.stderr
                        or result.stdout != (message + "\n").encode("utf-8")):
                    raise AssertionError("concurrent native artifacts were mixed: %r" % result)
            if list(poison.iterdir()):
                raise AssertionError("native compiler used the shared object destination")
            expect_no_objects(root)
            if os.name == "nt":
                expect_link_failure_cleanup(tool, root, cwd, suffix)
        finally:
            for process, _, _ in processes:
                if process.poll() is None:
                    process.kill()
                    process.communicate()


if __name__ == "__main__":
    main()
