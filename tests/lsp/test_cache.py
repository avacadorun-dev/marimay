#!/usr/bin/env python3
"""Exercise analysis freshness and the optional client file-watcher contract."""

import json
import sys
import tempfile
from pathlib import Path

from test_server import Client, document_position, initialize


def watch(client, path, kind):
    client.send("workspace/didChangeWatchedFiles", {
        "changes": [{"uri": path.as_uri(), "type": kind}],
    })


def check_registration(client, root):
    client.request("initialize", {
        "processId": None, "rootUri": root.as_uri(),
        "capabilities": {"workspace": {"didChangeWatchedFiles": {
            "dynamicRegistration": True,
        }}},
    })
    client.send("initialized", {})
    request = client.receive(lambda message:
        message.get("method") == "client/registerCapability")
    assert request["id"] == "mmay-watch-1", request
    registrations = request["params"]["registrations"]
    assert len(registrations) == 1, request
    registration = registrations[0]
    assert registration["method"] == "workspace/didChangeWatchedFiles", request
    watchers = registration["registerOptions"]["watchers"]
    assert {item["globPattern"] for item in watchers} == {
        "**/*.my", "**/mmay.mod", "**/mmay.work",
    }, request
    assert all(item["kind"] == 7 for item in watchers), request
    client.raw(json.dumps({"jsonrpc": "2.0", "id": request["id"], "result": None}).encode())


def check_disk_changes(client, root):
    module = root / "module"
    module.mkdir()
    (module / "mmay.mod").write_text('module "example/cache"\nlanguage "0.1"\n')
    main = module / "main.my"
    source = "fn main(): int { return answer(); }\n"
    main.write_text(source)
    helper = module / "helper.my"
    helper_source = "fn answer(): int { return 42; }\n"
    assert client.open(main, source), "missing initial unresolved-function diagnostic"
    position = document_position(main, source, "answer")
    assert client.request("textDocument/definition", position) is None

    # Newly created closed files change package membership, even after an error
    # analysis was cached. The watcher must discover them and clear diagnostics.
    helper.write_text(helper_source)
    watch(client, helper, 1)
    assert client.diagnostics(main.as_uri(), 1) == []
    definition = client.request("textDocument/definition", position)
    assert definition["uri"] == helper.as_uri(), definition

    # Equal-size disk edits also invalidate through exact snapshot validation;
    # correctness must not depend on timestamps, sizes, or a watcher notification.
    changed = helper_source.replace("answer", "otherX")
    assert len(changed) == len(helper_source)
    helper.write_text(changed)
    assert client.request("textDocument/definition", position) is None
    helper.write_text(helper_source)
    definition = client.request("textDocument/definition", position)
    assert definition["uri"] == helper.as_uri(), definition

    helper.unlink()
    watch(client, helper, 3)
    assert client.diagnostics(main.as_uri(), 1), "deletion retained stale success"
    assert client.request("textDocument/definition", position) is None

    # Saving an open file refreshes discovery even when that file's overlay did
    # not change and the edited file belongs to another application.
    helper.write_text(helper_source)
    client.send("textDocument/didSave", {"textDocument": {"uri": main.as_uri()}})
    assert client.diagnostics(main.as_uri(), 1) == []
    definition = client.request("textDocument/definition", position)
    assert definition["uri"] == helper.as_uri(), definition

    replacement = module / "replacement.my"
    helper.rename(replacement)
    client.send("workspace/didChangeConfiguration", {"settings": {}})
    assert client.diagnostics(main.as_uri(), 1) == []
    definition = client.request("textDocument/definition", position)
    assert definition["uri"] == replacement.as_uri(), definition

    # Materializing standard-source views must not trigger a watcher feedback
    # loop. Malformed events and repeated initialized notifications are ignored.
    view = module / ".mmay/lsp/stdlib/hash/std/io/io.my"
    watch(client, view, 1)
    watch(client, module / ".mmay/cache/generated.my", 2)
    watch(client, module / ".git/fixture.my", 2)
    watch(client, root / "module-other/churn.my", 1)
    watch(client, root / "build/tests/churn.my", 2)
    client.send("workspace/didChangeWatchedFiles", {"changes": []})
    client.send("workspace/didChangeWatchedFiles", {
        "changes": [{"uri": replacement.as_uri(), "type": 99}],
    })
    client.send("workspace/didChangeWatchedFiles", {
        "changes": [{"uri": "file:///bad%XX.my", "type": 2}],
    })
    client.send("initialized", {})
    assert client.request("textDocument/hover", position) is not None
    assert not any(item.get("method") in (
        "textDocument/publishDiagnostics", "client/registerCapability",
    ) for item in client.pending), client.pending


def check_no_registration(binary, root):
    client = Client(binary, root)
    try:
        initialize(client, root)  # No dynamicRegistration capability.
        source = "fn main(): int { return 1; }\n"
        path = root / "plain.my"
        path.write_text(source)
        assert client.open(path, source) == []
        assert not any(item.get("method") == "client/registerCapability"
                       for item in client.pending), client.pending
        client.finish()
    finally:
        client.dispose()


def check_manifest_watch(binary, root):
    module = root / "broken-module"
    nested = module / "nested"
    nested.mkdir(parents=True)
    manifest = module / "mmay.mod"
    manifest.write_text('module "example/broken"\nlanguage "\n')
    path = nested / "main.my"
    source = "fn main(): int { return 1; }\n"
    path.write_text(source)
    client = Client(binary, root)
    try:
        initialize(client, root)
        assert client.open(path, source), "missing malformed-manifest diagnostic"
        manifest.write_text('module "example/broken"\nlanguage "0.1"\n')
        watch(client, manifest, 2)
        assert client.diagnostics(path.as_uri(), 1) == []
        client.finish()
    finally:
        client.dispose()


def main():
    binary = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="mmay-lsp-cache-") as temporary:
        root = Path(temporary).resolve()
        check_no_registration(binary, root)
        client = Client(binary, root)
        try:
            check_registration(client, root)
            check_disk_changes(client, root)
            client.finish()
        finally:
            client.dispose()
        check_manifest_watch(binary, root)
    print("LSP cache freshness, file discovery, save and watcher registration: ok")


if __name__ == "__main__":
    main()
