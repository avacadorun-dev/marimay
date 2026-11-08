"""CI-only bounded current-install log deltas; never emit unrelated log data."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform
import re
import stat

from macos_payload import physical


LOG = Path("/private/var/log/install.log")
MAX_DELTA = 1024 * 1024
IDENTIFIERS = ("org.marimay.mmay", "org.marimay.ci.receipt-probe")
SERVICE = re.compile(r"package_script_service\[(\d+)\]")


def require_ci():
    if (os.environ.get("GITHUB_ACTIONS") != "true" or os.environ.get("RUNNER_OS") != "macOS"
            or platform.system() != "Darwin"):
        raise ValueError("install log diagnostics require hosted macOS CI")


def marker(path=LOG):
    physical(path)
    info = path.lstat()
    if not stat.S_ISREG(info.st_mode):
        raise ValueError("install log is not a regular physical file")
    return {"device": info.st_dev, "inode": info.st_ino, "size": info.st_size}


def validate_marker(value):
    if (type(value) is not dict or set(value) != {"device", "inode", "size"}
            or any(type(number) is not int or number < 0 for number in value.values())):
        raise ValueError("invalid install log marker")
    return value


def own_lines(data):
    lines = data.decode("utf-8", "replace").splitlines()
    owned_services = set()
    for line in lines:
        service = SERVICE.search(line)
        if service and any(identifier in line for identifier in IDENTIFIERS):
            owned_services.add(service.group(1))
    guards, service_errors, package_errors = [], [], []
    for line in lines:
        if "Marimay installer:" in line:
            guards.append("Marimay installer:" + line.split("Marimay installer:", 1)[1])
            continue
        lower = line.lower()
        owned = any(identifier in line for identifier in IDENTIFIERS)
        service = SERVICE.search(line)
        bound_service = service and service.group(1) in owned_services
        error_line = any(word in lower for word in ("error", "failed", "not found", "cannot",
            "unbound", "permission denied", "not permitted", "no such file"))
        if bound_service and (error_line or "[stderr]" in lower):
            service_errors.append(line)
        elif owned and (error_line or "script" in lower):
            package_errors.append(line)
    selected = []
    for category in (guards, service_errors, package_errors):
        remaining = 8 - len(selected)
        if remaining:
            selected.extend(category[-remaining:])
    return [line[:512] for line in selected]


def delta(before, path=LOG):
    validate_marker(before)
    physical(path)
    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    with os.fdopen(descriptor, "rb") as stream:
        info = os.fstat(stream.fileno())
        if (not stat.S_ISREG(info.st_mode) or info.st_dev != before["device"]
                or info.st_ino != before["inode"] or info.st_size < before["size"]):
            raise ValueError("install log rotated or was truncated; no delta read")
        start = max(before["size"], info.st_size - MAX_DELTA)
        stream.seek(start)
        return own_lines(stream.read(info.st_size - start))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("marker", "delta"))
    parser.add_argument("numbers", nargs="*", type=int)
    args = parser.parse_args()
    require_ci()
    if args.operation == "marker":
        if args.numbers:
            parser.error("marker takes no offsets")
        value = marker()
    else:
        if len(args.numbers) != 3:
            parser.error("delta requires device, inode and size")
        value = delta(dict(zip(("device", "inode", "size"), args.numbers)))
    print(json.dumps(value))


if __name__ == "__main__":
    main()
