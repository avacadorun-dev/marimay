"""Capture CTest logs and the generated native sources named by diagnostics.

Run after a failed CTest step, before artifact upload. This is evidence capture,
not a C validator or a replacement for the failing test's exit status.
"""

import argparse
import hashlib
import json
from pathlib import Path
import platform
import re
import subprocess


CONSTANT = rb"mmay_g_c_[0-9a-f]{32}_[0-9]+"
DECLARATION = rb"static const unsigned char (" + CONSTANT + rb")\["
CACHE_KEYS = {
    "CMAKE_BUILD_TYPE", "CMAKE_C_COMPILER", "CMAKE_C_FLAGS",
    "CMAKE_C_FLAGS_DEBUG", "CMAKE_C_FLAGS_RELEASE", "MMAY_WARNINGS_AS_ERRORS",
    "MMAY_ENABLE_ADDRESS_SANITIZER", "MMAY_ENABLE_UNDEFINED_SANITIZER",
    "MMAY_ENABLE_THREAD_SANITIZER",
}


def fingerprint(data):
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def confined(path, root):
    try:
        return path.resolve().relative_to(root)
    except ValueError:
        raise ValueError(f"diagnostic path escapes checkout: {path}") from None


def native_references(log, root):
    # CI compiler diagnostics use absolute POSIX paths; allow spaces in a
    # fixture name. Do not search caches or follow paths supplied outside root.
    expression = (re.escape(str(root)) +
                  r"/[^\r\n\"<>]*?/\.mmay/build/native/(?:debug|release)/"
                  r"[0-9a-f]{64}/program\.c(?::([0-9]+))?")
    found = {}
    for match in re.finditer(expression, log):
        path = Path(match.group(0).rsplit(":", 1)[0]
                    if match.group(1) else match.group(0))
        lines = found.setdefault(path, set())
        if match.group(1):
            lines.add(int(match.group(1)))
    return found


def inspect_source(data, locations):
    """Lexical emitter-shape observations; preprocessing is NOT modeled."""
    names = set(re.findall(CONSTANT, data))
    declarations = re.findall(DECLARATION, data)
    lines = data.decode("utf-8", errors="replace").splitlines()
    excerpts = []
    for number in sorted(locations)[:12]:
        excerpts.append({
            "reported_line": number,
            "lines": [{"line": index + 1, "text": lines[index][:1200]}
                      for index in range(max(0, number - 3),
                                         min(len(lines), number + 2))],
        })
    return {
        "line_count": len(lines),
        "nul_bytes": data.count(b"\0"),
        "constant_declarations": len(declarations),
        "names_without_emitter_declaration":
            sorted(name.decode("ascii") for name in names - set(declarations)),
        "excerpts": excerpts,
        "analysis_scope": "lexical emitter shape only; not C validity or root cause",
    }


def collect(root, build, output):
    root, build, output = root.resolve(), build.resolve(), output.resolve()
    confined(build, root)
    confined(output, build)
    # Never mix artifacts from different runs or erase an existing packet.
    output.mkdir(parents=True, exist_ok=False)
    packet = {"schema": 1, "platform": platform.platform(), "files": [],
              "native_sources": [], "capture_errors": []}

    def save(path, *, copy=True):
        relative = confined(path, root)
        data = path.read_bytes()
        if copy:
            destination = output / "files" / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
        entry = {"path": str(relative), "copied": copy, **fingerprint(data)}
        packet["files"].append(entry)
        return data

    try:
        revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root,
                                  capture_output=True, check=True, timeout=10)
        packet["revision"] = revision.stdout.decode("ascii").strip()
    except (OSError, subprocess.SubprocessError) as error:
        packet["capture_errors"].append(f"revision: {error}")

    references = {}
    logs = sorted((build / "Testing" / "Temporary").glob("*.log"))
    if not logs:
        packet["capture_errors"].append("no CTest logs available")
    for path in logs:
        try:
            data = save(path)
            for source, lines in native_references(data.decode("utf-8", "replace"), root).items():
                references.setdefault(source, set()).update(lines)
        except (OSError, ValueError) as error:
            packet["capture_errors"].append(str(error))

    for source, lines in sorted(references.items()):
        entry = {"path": str(source.relative_to(root)), "reported_lines": sorted(lines)}
        try:
            data = save(source)
            entry.update(fingerprint(data))
            entry.update(inspect_source(data, lines))
        except (OSError, ValueError) as error:
            entry["capture_error"] = str(error)
            packet["capture_errors"].append(str(error))
        packet["native_sources"].append(entry)

    # Hash the actual compiler and runtime libraries, not copied executables
    # from installation tests. Record the configured toolchain without running
    # commands or collecting ambient environment values from test output.
    for path in [build / "mmay", *sorted(build.glob("native-runtime/*/*.a")),
                 *sorted(build.glob("native-runtime/*/*.manifest"))]:
        try:
            save(path, copy=path.suffix == ".manifest")
        except (OSError, ValueError) as error:
            packet["capture_errors"].append(str(error))
    try:
        cache_path = build / "CMakeCache.txt"
        confined(cache_path, root)
        cache = cache_path.read_text()
        packet["cmake_cache"] = {}
        for line in cache.splitlines():
            match = re.match(r"([^:#=]+):[^=]+=(.*)$", line)
            if match and match.group(1) in CACHE_KEYS:
                packet["cmake_cache"][match.group(1)] = match.group(2)
        for path in sorted(build.glob("CMakeFiles/*/CMakeCCompiler.cmake")):
            save(path)
    except (OSError, ValueError) as error:
        packet["capture_errors"].append(str(error))

    # The same compact packet reaches the job log before network upload. Raw
    # source/log bytes are in files/, even if damaged or not valid UTF-8.
    report = json.dumps(packet, indent=2, ensure_ascii=True) + "\n"
    (output / "manifest.json").write_text(report)
    print(report, end="")
    return packet


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    packet = collect(args.root, args.build_dir, args.output)
    return 1 if packet["capture_errors"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
