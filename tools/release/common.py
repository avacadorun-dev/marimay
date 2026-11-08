"""Bounded inputs and GitHub API calls for the experimental release workflow."""
import hashlib
import json
import os
import re
import subprocess
import urllib.error
import urllib.request


class ReleaseError(Exception):
    pass


def version_value(value):
    if not re.fullmatch(r"0\.[0-9]+\.[0-9]+-alpha\.[1-9][0-9]*", value):
        raise ReleaseError("expected an explicit 0.x.y-alpha.N version")
    return value


def sha_value(value):
    if not re.fullmatch(r"[0-9a-f]{40}", value):
        raise ReleaseError("expected an exact commit SHA")
    return value


def repository_value(value):
    if value != "marimay-lang/marimay":
        raise ReleaseError("publication is restricted to marimay-lang/marimay")
    return value


def git(*args):
    try:
        return subprocess.check_output(["git", *args], text=True,
                                       stderr=subprocess.PIPE).strip()
    except subprocess.CalledProcessError as error:
        raise ReleaseError("git release identity check failed") from error


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def api(repository, path, method="GET", data=None, missing=False):
    repository_value(repository)
    token = os.environ.get("GH_TOKEN", "")
    if not token:
        raise ReleaseError("GH_TOKEN is required for the release API")
    request = urllib.request.Request(
        "https://api.github.com/repos/" + repository + "/" + path,
        method=method,
        data=None if data is None else json.dumps(data).encode(),
        headers={"Accept": "application/vnd.github+json",
                 "Authorization": "Bearer " + token,
                 "Content-Type": "application/json",
                 "X-GitHub-Api-Version": "2022-11-28"})
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            payload = response.read(8 * 1024 * 1024 + 1)
    except urllib.error.HTTPError as error:
        if missing and error.code == 404:
            return None
        raise ReleaseError("GitHub release API returned HTTP " + str(error.code)) from error
    except (OSError, urllib.error.URLError) as error:
        raise ReleaseError("GitHub release API request failed") from error
    if len(payload) > 8 * 1024 * 1024:
        raise ReleaseError("GitHub release API response exceeded its bound")
    try:
        return json.loads(payload)
    except (ValueError, UnicodeError) as error:
        raise ReleaseError("invalid GitHub release API response") from error


def remote_identity(repository, tag, sha):
    sha_value(sha)
    if not tag.startswith("v"):
        raise ReleaseError("expected an alpha tag")
    version_value(tag[1:])
    if api(repository, "commits/" + tag)["sha"] != sha:
        raise ReleaseError("release tag changed after validation")
    comparison = api(repository, "compare/" + sha + "...main")
    if comparison.get("status") not in ("ahead", "identical"):
        raise ReleaseError("release commit is not an ancestor of current main")
