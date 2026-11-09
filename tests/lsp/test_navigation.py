#!/usr/bin/env python3
"""Cross-package and exact embedded-source navigation through LSP stdio."""

import hashlib
import sys
import tempfile
from pathlib import Path
from urllib.parse import urlparse
from urllib.request import url2pathname

from test_server import Client, document_position, initialize, offset_at, position


def close(client, path):
    client.send("textDocument/didClose", {"textDocument": {"uri": path.as_uri()}})


def references(client, request, declaration=True):
    return client.request("textDocument/references", {
        **request, "context": {"includeDeclaration": declaration},
    })


def check_packages(client, root):
    module = root / "packages"
    (module / "model").mkdir(parents=True)
    (module / "app").mkdir()
    (module / "mmay.mod").write_text('module "example/navigation"\nlanguage "0.1"\n')
    provider = module / "model/api.my"
    provider_source = "public fn answer(): int { return 42; }\n"
    provider.write_text(provider_source)
    path = module / "app/main.my"
    for imported, call in (
        ('import { answer } from "example/navigation/model";', "answer"),
        ('import { answer as renamed } from "example/navigation/model";', "renamed"),
        ('import * as model from "example/navigation/model";', "model.answer"),
    ):
        source = imported + f"\nfn main(): int {{ return {call}(); }}\n"
        path.write_text(source)
        assert client.open(path, source) == []
        request = document_position(path, source, call.rsplit(".", 1)[-1] + "()")
        definition = client.request("textDocument/definition", request)
        assert definition["uri"] == provider.as_uri(), definition
        assert definition["range"]["start"] == position(provider_source, "answer("), definition
        assert {item["uri"] for item in references(client, request)} == {
            path.as_uri(), provider.as_uri()}, definition
        close(client, path)


def source_view(definition, root):
    uri = urlparse(definition["uri"])
    assert uri.scheme == "file" and not uri.netloc, definition
    path = Path(url2pathname(uri.path))
    relative = path.relative_to(root / ".mmay/lsp/stdlib")
    content = path.read_bytes()
    assert len(relative.parts[0]) == 64 and relative.parts[0] == hashlib.sha256(content).hexdigest()
    assert relative.parts[1] == "std" and path.suffix == ".my", path
    source = content.decode("utf-8")
    start = offset_at(source, definition["range"]["start"])
    end = offset_at(source, definition["range"]["end"])
    return path, source, source[start:end]


def check_standard_sources(client, root):
    module = root / "standard space %"
    module.mkdir()
    (module / "mmay.mod").write_text('module "example/standard-navigation"\nlanguage "0.1"\n')
    path = module / "main.my"
    source = ('import * as cli from "std/cli";\n'
              'import * as json from "std/json";\n'
              'fn values(parsed: cli.Parsed): []string { return cli.positionalValues(parsed, "mode"); }\n'
              'fn message(error: json.Error): string { return json.message(error); }\n')
    path.write_text(source)
    assert client.open(path, source) == []
    for name in ("positionalValues", "message"):
        request = document_position(path, source, name + "(" if name != "message" else "message(error);")
        definition = client.request("textDocument/definition", request)
        assert definition, (name, definition)
        view, view_source, spelling = source_view(definition, module)
        assert spelling == name, (spelling, definition)
        assert client.request("textDocument/hover", request), name
        found = references(client, request)
        assert any(item == definition for item in found), found
        assert any(item["uri"] == path.as_uri() for item in found), found
        assert all(item != definition for item in references(client, request, False))

        # Opening the generated view must preserve its standard package identity.
        diagnostics = client.open(view, view_source)
        assert diagnostics == [], diagnostics
        at_definition = {"textDocument": {"uri": view.as_uri()},
                         "position": definition["range"]["start"]}
        assert client.request("textDocument/definition", at_definition) == definition
        assert client.request("textDocument/hover", at_definition)
        assert any(item["uri"] == path.as_uri() for item in references(client, at_definition))
        if name == "positionalValues":
            point = position(view_source, "fn positionalValues(")
            point["character"] += len("fn positionalValues(parsed: ")
            imported_type = client.request("textDocument/definition", {
                "textDocument": {"uri": view.as_uri()}, "position": point,
            })
            type_view, _, spelling = source_view(imported_type, module)
            assert spelling == "Parsed" and type_view != view, imported_type

        client.change(view, view_source + "\nfn injected(): int { return 7; }\n", 2)
        diagnostics = client.diagnostics(view.as_uri(), 2)
        assert any(item["code"] == "lsp-source-view" for item in diagnostics), diagnostics
        assert client.request("workspace/symbol", {"query": "injected"}) == []
        assert client.request("textDocument/definition", request) == definition
        client.change(view, view_source, 3)
        assert client.diagnostics(view.as_uri(), 3) == []
        assert client.request("textDocument/definition", at_definition) == definition
        close(client, view)

    # A source view is a derived cache, not another package loaded on the next check.
    client.change(path, source, 2)
    assert client.diagnostics(path.as_uri(), 2) == []
    assert client.request("textDocument/definition", document_position(path, source, "positionalValues("))
    close(client, path)


def main():
    binary = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="mmay-lsp-navigation-") as directory:
        root = Path(directory).resolve()
        client = Client(binary, root)
        try:
            initialize(client, root)
            check_packages(client, root)
            check_standard_sources(client, root)
            client.finish()
        finally:
            client.dispose()
    print("LSP cross-package and embedded-source navigation: ok")


if __name__ == "__main__":
    main()
