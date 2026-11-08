#!/usr/bin/env python3
"""Publish validated Mac assets through a draft; never overwrite public assets."""
import argparse
from pathlib import Path
import subprocess
import sys

from assets import collect
from common import ReleaseError, api, digest, remote_identity, repository_value, sha_value, version_value


def publish(repository, tag, version, sha, root, notes):
    repository_value(repository)
    version_value(version)
    sha_value(sha)
    if tag != "v" + version:
        raise ReleaseError("tag/version mismatch")
    paths = collect(root, version, sha)
    remote_identity(repository, tag, sha)
    marker = "<!-- marimay-experimental-release:" + sha + " -->"
    existing = api(repository, "releases/tags/" + tag, missing=True)
    if existing and not existing.get("draft"):
        raise ReleaseError("this release is already public; its assets will not be overwritten")
    if existing and (marker not in existing.get("body", "")
                     or existing.get("tag_name") != tag or not existing.get("prerelease")):
        raise ReleaseError("an unrelated draft already owns this tag")
    body = notes.read_text(encoding="utf-8") + "\n\nSource commit: `" + sha + "`.\n" + marker + "\n"
    data = {"tag_name": tag, "target_commitish": sha,
            "name": "Marimay " + version + " — experimental macOS alpha",
            "body": body, "draft": True, "prerelease": True,
            "make_latest": "false"}
    release = existing or api(repository, "releases", "POST", data)
    if (not release.get("draft") or release.get("tag_name") != tag
            or not release.get("prerelease") or marker not in release.get("body", "")):
        raise ReleaseError("draft identity does not match the validated release")
    allowed = {path.name for path in paths}
    if any(asset["name"] not in allowed for asset in release.get("assets", [])):
        raise ReleaseError("the draft contains unexpected assets")
    by_name = {path.name: path for path in paths}
    present = set()
    for asset in release.get("assets", []):
        path = by_name[asset["name"]]
        if (asset.get("digest") != "sha256:" + digest(path)
                or asset.get("size") != path.stat().st_size
                or asset.get("state") != "uploaded" or asset["name"] in present):
            raise ReleaseError("existing draft asset differs; it will not be overwritten")
        present.add(asset["name"])
    missing = [path for path in paths if path.name not in present]
    try:
        if missing:
            subprocess.run(["gh", "release", "upload", tag, "--repo", repository,
                            *[str(path) for path in missing]], check=True)
    except subprocess.CalledProcessError as error:
        raise ReleaseError("draft asset upload failed; draft remains unpublished") from error
    current = api(repository, "releases/" + str(int(release["id"])))
    assets = current.get("assets", [])
    if (not current.get("draft") or current.get("tag_name") != tag
            or marker not in current.get("body", "")
            or {asset["name"] for asset in assets} != allowed
            or len(assets) != len(allowed)
            or any(asset.get("state") != "uploaded" for asset in assets)):
        raise ReleaseError("draft asset verification failed")
    for asset in assets:
        if (asset.get("size") != by_name[asset["name"]].stat().st_size
                or asset.get("digest") != "sha256:" + digest(by_name[asset["name"]])):
            raise ReleaseError("uploaded asset digest/size does not match the verified file")
    remote_identity(repository, tag, sha)
    result = api(repository, "releases/" + str(int(release["id"])), "PATCH",
                 {"draft": False, "prerelease": True, "make_latest": "false"})
    print(result["html_url"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repository", "tag", "version", "sha", "assets", "notes"):
        parser.add_argument("--" + name, required=True)
    options = parser.parse_args()
    try:
        publish(options.repository, options.tag, options.version, options.sha,
                Path(options.assets), Path(options.notes))
    except (ReleaseError, OSError, ValueError) as error:
        print("release publication: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
