#!/usr/bin/env python3
"""Completion, local/field navigation and transactional rename over live pipes."""

import json
import sys
import tempfile
from pathlib import Path

from test_server import Client, apply_edits, document_position, initialize, position


def at_end(path, source, needle):
    offset = source.index(needle) + len(needle)
    prefix = source[:offset]
    return {"textDocument": {"uri": path.as_uri()}, "position": {
        "line": prefix.count("\n"),
        "character": len(prefix.rsplit("\n", 1)[-1].encode("utf-16-le")) // 2,
    }}


def complete(client, path, source, needle):
    client.open(path, source)
    result = client.request("textDocument/completion", at_end(path, source, needle))
    assert isinstance(result.get("items"), list), result
    return result["items"]


def close(client, path):
    client.send("textDocument/didClose", {"textDocument": {"uri": path.as_uri()}})


def check_completion(client, root):
    path = root / "completion % пример.my"
    cases = [
        ("fn demo(seed: int): int { const value = seed; return val; }\n",
         "return val", "value", "seed"),
        ("fn demo(seed: int): int { const output = see; const future = 1; return future; }\n",
         "output = see", "seed", "future"),
        ("fn demo(): string { const item = 1; { const item = \"x\"; return it; } }\n",
         "return it", "item", None),
        ("fn demo(): int { if true { const gone = 1; } return go; }\n",
         "return go", None, "gone"),
        ("type User { public name: string, age: int, }\n"
         "type Other { ghost: int, }\n"
         "fn string(user: User): string { return \"user\"; }\n"
         "fn demo(user: User): int { return user.; }\n",
         "return user.", "age", "ghost"),
        ("type Address { public street: string, }\n"
         "type User { public address: Address, }\n"
         "fn demo(user: User): string { return user.address.st; }\n",
         "return user.address.st", "street", "address"),
        ("type User { mutable age: int, }\n"
         "fn demo(user: User): int { user.ag = 3; return user.age; }\n",
         "{ user.ag", "age", None),
        ("type User { name: string, }\n"
         "fn string(user: User): string { return \"user\"; }\n"
         "fn demo(user: User): string { return user.str(); }\n",
         "return user.str", "string", None),
        ("/* 😀 */ fn demo(): int { const значение = 1; return зна; }\n",
         "return зна", "значение", None),
    ]
    for source, needle, wanted, forbidden in cases:
        items = complete(client, path, source, needle)
        labels = [item["label"] for item in items]
        if wanted is not None:
            assert wanted in labels, (needle, labels)
            chosen = next(item for item in items if item["label"] == wanted)
            edit = chosen["textEdit"]
            assert edit["range"]["end"] == at_end(path, source, needle)["position"], edit
            assert edit["newText"] == wanted, chosen
            if wanted == "item":
                assert labels.count("item") == 1 and chosen["detail"] == "string", chosen
        if forbidden is not None:
            assert forbidden not in labels, (needle, labels)
        close(client, path)
    for source, needle in (
        ('fn demo(): string { return "value"; }\n', '"val'),
        ('fn demo(): int { /* value */ return 1; }\n', '/* val'),
    ):
        assert complete(client, path, source, needle) == []
        close(client, path)
    shadow = ('import * as codec from "std/json";\n'
              'fn demo(): int { const codec = 1; return co; }\n')
    items = complete(client, path, shadow, 'return co')
    labels = [item['label'] for item in items]
    assert 'codec' in labels and not any(label.startswith('codec.') for label in labels), labels
    close(client, path)


def check_import_completion(client, root):
    module = root / 'module'
    (module / 'model').mkdir(parents=True)
    (module / 'app').mkdir()
    (module / 'mmay.mod').write_text('module "example/editing"\nlanguage "0.1"\n')
    (module / 'model/api.my').write_text(
        'public type User { public name: string, secret: int, }\n'
        'public fn helper(): int { return 1; }\n'
        'fn hidden(): int { return 2; }\n')
    path = module / 'app/main.my'
    cases = [
        ('import * as model from "example/editing/model";\n'
         'fn demo(user: model.User): string { return user.; }\n',
         'return user.', 'name', 'secret'),
        ('import * as model from "example/editing/model";\n'
         'fn demo(): int { return model.he; }\n',
         'return model.he', 'helper', 'hidden'),
        ('import { helper as renamed } from "example/editing/model";\n'
         'fn demo(): int { return ren; }\n',
         'return ren', 'renamed', 'helper'),
        ('import * as model from "example/editing/model";\n'
         'fn demo(): int { const model = 1; return mo; }\n',
         'return mo', 'model', 'model.User'),
    ]
    for source, needle, wanted, forbidden in cases:
        path.write_text(source)
        items = complete(client, path, source, needle)
        labels = [item['label'] for item in items]
        assert wanted in labels and forbidden not in labels, (needle, labels)
        close(client, path)


def renamed_sources(edit, sources, versions):
    assert "documentChanges" in edit and "changes" not in edit, edit
    updated = dict(sources)
    for document in edit["documentChanges"]:
        uri = document["textDocument"]["uri"]
        assert uri in sources, (uri, sources)
        assert document["textDocument"]["version"] == versions[uri], document
        updated[uri] = apply_edits(sources[uri], document["edits"])
    return updated


def check_rename(client, root):
    path = root / "rename.my"
    source = (
        "type User { mutable score: int, }\n"
        "type Other { score: int, }\n"
        "fn demo(input: int): int {\n"
        " let value = input;\n"
        " { const value = 2; const copy = value; }\n"
        " const user = User { score: value };\n"
        " user.score = value;\n"
        " // value and score stay in this comment\n"
        " const text = \"value score\";\n"
        " return user.score + value;\n"
        "}\n"
    )
    assert client.open(path, source) == []
    request = document_position(path, source, "value = input")
    prepared = client.request("textDocument/prepareRename", request)
    assert prepared["placeholder"] == "value", prepared
    for needle, name in (("let value", "value"), ("demo(input", "input"),
                         ("mutable score", "score")):
        prepared = client.request("textDocument/prepareRename", at_end(path, source, needle))
        assert prepared["placeholder"] == name, prepared
    client.request("textDocument/prepareRename", at_end(path, source, "let value "), error=-32803)
    result = client.request("textDocument/rename", {**request, "newName": "total"})
    changed = renamed_sources(result, {path.as_uri(): source}, {path.as_uri(): 1})[path.as_uri()]
    assert "let total = input" in changed and "score: total" in changed, changed
    assert "{ const value = 2; const copy = value; }" in changed, changed
    assert '// value and score stay in this comment' in changed, changed
    assert '"value score"' in changed and "user.score = total" in changed, changed
    for name in ("input", "return", "bad-name", ""):
        client.request("textDocument/rename", {**request, "newName": name}, error=-32803)
    field = document_position(path, source, "score: int")
    result = client.request("textDocument/rename", {**field, "newName": "points"})
    changed = renamed_sources(result, {path.as_uri(): source}, {path.as_uri(): 1})[path.as_uri()]
    assert "type User { mutable points: int" in changed, changed
    assert "type Other { score: int" in changed, changed
    assert "User { points: value }" in changed and "user.points = value" in changed, changed
    definition = client.request("textDocument/definition",
                                document_position(path, source, "input;"))
    assert definition["range"]["start"] == position(source, "input: int"), definition
    definition = client.request("textDocument/definition",
                                document_position(path, source, "score = value"))
    assert definition["range"]["start"] == position(source, "score: int"), definition
    definition = client.request("textDocument/definition", at_end(path, source, "user.score"))
    assert definition["range"]["start"] == position(source, "score: int"), definition
    close(client, path)


def main():
    binary = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="mmay-lsp-editing-") as directory:
        root = Path(directory).resolve()
        client = Client(binary, root)
        try:
            capabilities = client.request("initialize", {
                "processId": None, "rootUri": root.as_uri(), "capabilities": {},
            })["capabilities"]
            assert capabilities.get("completionProvider") is not None, capabilities
            assert capabilities.get("renameProvider"), capabilities
            client.send("initialized", {})
            check_completion(client, root)
            check_import_completion(client, root)
            check_rename(client, root)
            client.finish()
        finally:
            client.dispose()
    print("LSP completion, scoped navigation and transactional rename: ok")


if __name__ == "__main__":
    main()
