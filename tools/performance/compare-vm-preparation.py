#!/usr/bin/env python3
"""Compare immutable compilers on a saved, unchanged 32-workload catalogue."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--benchmarks", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--workload", action="append")
    parser.add_argument("--warmups", type=int, default=3)
    parser.add_argument("--samples", type=int, default=11)
    args = parser.parse_args()
    repository = args.benchmarks.resolve()
    sys.path.insert(0, str(repository / "benchmarks"))
    from catalog_snapshot import create_workspace, pin_toolchain
    from perf_environment import capture_metadata, finish_metadata
    from perf_protocol import load_protocol, sha256_file, summarize, validate_run_shape
    from run import measured_run

    _, protocol = load_protocol(repository)
    validate_run_shape(protocol, args.warmups, args.samples)
    reference = json.loads(args.reference.read_text(encoding="utf-8"))
    if reference["status"] != "passed" or len(reference["workloads"]) != 32:
        raise ValueError("reference must be a passing complete 32-workload catalogue")
    hashes = reference["metadata"]["source_sha256"]

    def verify_sources():
        for path, digest in hashes.items():
            if sha256_file(repository / path) != digest:
                raise ValueError(f"reference input changed: {path}")

    verify_sources()
    workloads = args.workload or list(reference["workloads"])
    if len(set(workloads)) != len(workloads) or any(
        name not in reference["workloads"] for name in workloads
    ):
        raise ValueError("workloads must be unique names from the reference")
    heap = reference["metadata"]["resource"]["max_heap_bytes"]
    timeout = protocol["measurement"]["timeout_seconds"]
    metadata, commands, artifacts = {}, {}, {}
    snapshot_root = Path(__file__).resolve().parents[2] / "build/vm-preparation-paired"
    for label, compiler in (("before", args.before), ("after", args.after)):
        meta = capture_metadata(repository, compiler.resolve(), warmups=args.warmups,
                                samples=args.samples, timeout_seconds=timeout,
                                max_heap_bytes=heap, command=sys.argv)
        # The upstream collector labels every host dedicated without checking
        # other work. This developer comparison cannot attest that contract.
        isolation = meta["isolation"]
        isolation["collector_reported_mode"] = isolation["mode"]
        isolation["mode"] = "local-unverified"
        pinned, files = pin_toolchain(compiler, create_workspace(snapshot_root), meta)
        artifacts.update(files)
        metadata[label] = meta
        commands[label] = {}
        for name in workloads:
            row = reference["workloads"][name]
            command = list(row["commands"]["marimay-vm"])
            # Relocate only the executable and source directory. All engine,
            # resource and workload arguments remain exactly as recorded.
            if command[1] != "run" or "--engine" not in command:
                raise ValueError(f"reference is not source-run VM: {name}")
            source = Path("marimay/benchmarks") / name.replace("-", "_") \
                if row.get("anchor") else Path("marimay/catalog")
            if not Path(command[2]).as_posix().endswith("/" + source.as_posix()):
                raise ValueError(f"unexpected reference source: {name}")
            command[0], command[2] = str(pinned), str(repository / source)
            commands[label][name] = command

    raw = {name: {label: [] for label in metadata} for name in workloads}
    events = []
    result = {"schema": "marimay.vm-preparation-paired/1", "status": "running",
              "reference_sha256": sha256_file(args.reference),
              "source_sha256": hashes, "metadata": metadata,
              "commands": commands, "raw_ms": raw, "observations": events}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.with_suffix(".journal.jsonl").open("w", encoding="utf-8") as journal:
        journal.write(json.dumps(result) + "\n")
        journal.flush()
        for round_id in range(args.warmups + args.samples):
            offset = round_id % len(workloads)
            order = workloads[offset:] + workloads[:offset]
            if round_id % 2:
                order = list(reversed(order))
            for index, name in enumerate(order):
                participants = ["before", "after"]
                if (round_id + index) % 2:
                    participants.reverse()
                for label in participants:
                    elapsed = measured_run(commands[label][name],
                                           reference["workloads"][name]["expected_stdout"],
                                           repository, timeout)
                    event = {"round": round_id, "workload": name,
                             "participant": label, "elapsed_ms": elapsed,
                             "warmup": round_id < args.warmups, "status": "passed"}
                    events.append(event)
                    journal.write(json.dumps(event) + "\n")
                    journal.flush()
                    if not event["warmup"]:
                        raw[name][label].append(elapsed)
            print(f"Completed paired round {round_id + 1}/{args.warmups + args.samples}",
                  flush=True)
        verify_sources()
        for path, digest in artifacts.items():
            if sha256_file(Path(path)) != digest:
                raise ValueError(f"pinned artifact changed: {path}")
        for meta in metadata.values():
            finish_metadata(meta)
        result["summary"] = {
            name: {label: summarize(values) for label, values in cells.items()}
            for name, cells in raw.items()
        }
        result["status"] = "passed"
        journal.write(json.dumps({"status": "passed", "integrity": "verified"}) + "\n")
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    for name, cells in result["summary"].items():
        before, after = cells["before"]["median_ms"], cells["after"]["median_ms"]
        print(f"{name:24} {before:10.2f} -> {after:10.2f} ms ({after / before:.3f}x)")


if __name__ == "__main__":
    main()
