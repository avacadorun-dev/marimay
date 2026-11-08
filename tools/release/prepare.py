#!/usr/bin/env python3
"""Validate release metadata/ref and emit bounded workflow outputs."""
import os
from pathlib import Path
import re
import sys

from common import ReleaseError, git, sha_value, version_value


def prepare(source, event, requested_tag, sha, main_sha=None):
    match = re.search(r'^set\(MMAY_COMPILER_VERSION "([^"\n]+)"\)$', source, re.M)
    if not match:
        raise ReleaseError("compiler version is missing")
    version = version_value(match[1])
    if 'set(MMAY_RELEASE_CHANNEL "experimental-alpha")' not in source:
        raise ReleaseError("only the experimental-alpha channel can be published")
    if 'set(MMAY_RELEASE_PUBLICATION_READY_JSON "false")' not in source:
        raise ReleaseError("experimental publication must preserve bounded readiness")
    sha_value(sha)
    if event not in ("push", "workflow_dispatch", "pull_request"):
        raise ReleaseError("unsupported release event")
    publish = event != "pull_request"
    tag = "v" + version
    if publish and requested_tag != tag:
        raise ReleaseError("tag must exactly match the compiled alpha version")
    if main_sha is not None:
        sha_value(main_sha)
    return {"version": version, "tag": tag, "sha": sha,
            "publish": "true" if publish else "false"}


def main():
    try:
        sha = sha_value(git("rev-parse", "HEAD"))
        result = prepare(Path("CMakeLists.txt").read_text(),
                         os.environ["GITHUB_EVENT_NAME"],
                         os.environ.get("REQUESTED_TAG", ""), sha)
        if result["publish"] == "true":
            if git("rev-parse", "refs/tags/" + result["tag"] + "^{commit}") != sha:
                raise ReleaseError("checkout does not match the release tag")
            git("merge-base", "--is-ancestor", sha, "refs/remotes/origin/main")
        if not Path("docs/releases/" + result["version"] + ".md").is_file():
            raise ReleaseError("release notes are missing")
        with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as stream:
            for key, value in result.items():
                stream.write(key + "=" + value + "\n")
        print("Validated " + result["tag"] + " at " + sha)
    except (ReleaseError, KeyError, OSError) as error:
        print("release preparation: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
