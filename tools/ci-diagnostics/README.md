# Failed CTest evidence

`collect.py` runs after a failed C17, ASan/UBSan/LSan, or TSan test step. It
copies CTest logs and exact generated `program.c` files named in their native
compiler diagnostics, then records source/log/compiler/runtime hashes, the Git
revision, selected CMake settings, and configured compiler metadata. It does
not rerun the compiler or modify the cache. Only paths inside the checkout are
eligible; it does not scan application data or collect the environment.

```sh
python3 tools/ci-diagnostics/collect.py --root . --build-dir build/asan-ubsan \
  --output build/asan-ubsan/ci-failure
python3 tools/ci-diagnostics/test_collect.py
```

The packet's `manifest.json` is also printed before artifact upload. It includes
bounded excerpts around diagnostic locations and names lacking the emitter's
string-declaration spelling. This lexical observation is not C validation:
comments, preprocessing, later file changes, or another error can change its
meaning. Captured files describe the state after CTest stops, not necessarily
the bytes read earlier by the host compiler. Missing or inaccessible files are
explicit capture errors; already-deleted temporary artifacts cannot be recovered.

Run 205's CP38 failure motivated this owner: the compiler error named a missing
string constant, but the upload contained only test logs and then failed with
HTTP 500. The new packet can retain the source on a recurrence; it neither
establishes the old cause nor fixes artifact transport. Test failures remain
failures independently of capture and upload. A fresh output directory prevents
mixing different runs. This version handles the absolute POSIX diagnostics used
by the Linux CI workflow (and local macOS replay).
