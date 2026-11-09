#!/usr/bin/env python3
"""Keep compiler errors in closed files and module manifests visible in the editor."""

import sys
import tempfile
from pathlib import Path

from test_server import Client, initialize, position


def related_error(diagnostics, path, message):
    for diagnostic in diagnostics:
        for related in diagnostic.get("relatedInformation", []):
            if (related["location"]["uri"] == path.as_uri()
                    and message in related["message"]):
                return related
    raise AssertionError((path, message, diagnostics))


def check_dependency_errors(binary, directory):
    module = directory / "dependency errors"
    module.mkdir()
    (module / "mmay.mod").write_text(
        'module "example/lsp-diagnostics"\nlanguage "0.1"\n', encoding="utf-8")
    main = module / "main.my"
    other = module / "ошибка % file.my"
    main_source = "fn main(): int { return answer(); }\n"
    other_source = "fn answer(): int { return missing; }\n"
    repaired = "fn answer(): int { return 42; }\n"
    main.write_text(main_source, encoding="utf-8")
    other.write_text(other_source, encoding="utf-8")
    client = Client(binary, directory)
    try:
        initialize(client, directory)
        diagnostics = client.open(main, main_source)
        related = related_error(diagnostics, other, "unknown name")
        assert related["location"]["range"]["start"] == position(other_source, "missing"), related
        assert any(d["code"] == "lsp-dependency" for d in diagnostics), diagnostics

        # A closed file repair followed by didSave invalidates analysis and clears blockers.
        other.write_text(repaired, encoding="utf-8")
        client.send("textDocument/didSave", {"textDocument": {"uri": main.as_uri()}})
        assert client.diagnostics(main.as_uri(), 1) == []

        # Unsaved syntax errors in another buffer must likewise explain blocked checking.
        assert client.open(other, repaired) == []
        client.diagnostics(main.as_uri(), 1)
        broken = "fn answer(: int { return 42; }\n"
        client.change(other, broken, 2)
        diagnostics = client.diagnostics(main.as_uri(), 1)
        assert related_error(diagnostics, other, ""), diagnostics
        assert client.diagnostics(other.as_uri(), 2), "missing local parser diagnostics"
        client.change(other, repaired, 3)
        assert client.diagnostics(main.as_uri(), 1) == []
        assert client.diagnostics(other.as_uri(), 3) == []
        client.finish()
    finally:
        client.dispose()


def check_manifest_errors(binary, directory):
    module = directory / "manifest errors"
    module.mkdir()
    manifest = module / "mmay.mod"
    manifest.write_text('module "example/lsp-manifest"\n', encoding="utf-8")
    main = module / "main.my"
    source = "fn main(): int { return 0; }\n"
    main.write_text(source, encoding="utf-8")
    client = Client(binary, directory)
    try:
        initialize(client, directory)
        diagnostics = client.open(main, source)
        assert any("exactly one module and one language" in d["message"]
                   for d in diagnostics), diagnostics
        related_error(diagnostics, manifest, "module configuration")
        assert all(d["code"] != "lsp-analysis" for d in diagnostics), diagnostics

        manifest.write_text(
            'module "example/lsp-manifest"\nlanguage "0.1"\n', encoding="utf-8")
        client.send("textDocument/didSave", {"textDocument": {"uri": main.as_uri()}})
        assert client.diagnostics(main.as_uri(), 1) == []
        manifest.write_text('unknown "directive"\n', encoding="utf-8")
        client.send("textDocument/didSave", {"textDocument": {"uri": main.as_uri()}})
        diagnostics = client.diagnostics(main.as_uri(), 1)
        assert diagnostics and all(d["code"] != "lsp-analysis" for d in diagnostics), diagnostics
        related_error(diagnostics, manifest, "module configuration")
        client.finish()
    finally:
        client.dispose()


def main():
    binary = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="mmay-lsp-diagnostics-") as temporary:
        directory = Path(temporary).resolve()
        check_dependency_errors(binary, directory)
        check_manifest_errors(binary, directory)
    print("mmay_lsp_diagnostics: ok")


if __name__ == "__main__":
    main()
