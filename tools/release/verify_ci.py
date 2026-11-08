#!/usr/bin/env python3
"""Require a complete successful C17 workflow for the exact release commit."""
import argparse
import sys
import time
import urllib.parse

from common import ReleaseError, api, repository_value, remote_identity, sha_value


def successful_runs(runs, repository, sha):
    return [run for run in runs if
            run.get("head_sha") == sha and run.get("status") == "completed"
            and run.get("conclusion") == "success"
            and run.get("event") in ("push", "workflow_dispatch")
            and run.get("head_repository", {}).get("full_name") == repository
            and run.get("path") == ".github/workflows/ci.yml"]


def complete_jobs(data):
    jobs = data.get("jobs", [])
    return (26 <= len(jobs) <= 100 and len(jobs) == data.get("total_count")
            and all(job.get("status") == "completed"
                    and job.get("conclusion") == "success" for job in jobs))


def verify(repository, sha, wait_seconds):
    deadline = time.monotonic() + wait_seconds
    while True:
        query = urllib.parse.urlencode({"head_sha": sha, "per_page": 100})
        runs = api(repository, "actions/workflows/ci.yml/runs?" + query)
        for run in successful_runs(runs.get("workflow_runs", []), repository, sha):
            data = api(repository, "actions/runs/" + str(int(run["id"]))
                       + "/jobs?filter=latest&per_page=100")
            if complete_jobs(data):
                print("Verified complete C17 matrix: " + run["html_url"])
                return run["id"]
        if time.monotonic() >= deadline:
            raise ReleaseError("no complete successful C17 matrix for this commit")
        print("Waiting for the complete C17 matrix for " + sha, flush=True)
        time.sleep(min(30, max(0, deadline - time.monotonic())))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--wait-seconds", type=int, default=3600)
    options = parser.parse_args()
    try:
        repository_value(options.repository)
        sha_value(options.sha)
        if not 0 <= options.wait_seconds <= 5400:
            raise ReleaseError("invalid CI wait bound")
        remote_identity(options.repository, options.tag, options.sha)
        verify(options.repository, options.sha, options.wait_seconds)
    except ReleaseError as error:
        print("release CI gate: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
