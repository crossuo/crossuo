# Vendored: QuickJS-ng
#
# Upstream: https://github.com/quickjs-ng/quickjs
# Pinned release: v0.16.2 (tag commit 1ab8676f4b6d6d669baeb5f21790fb9734636a20)
# License: MIT (see LICENSE in this directory)
#
# Copied verbatim from the release tarball (no local modifications):
#   dtoa.c dtoa.h libregexp.c libregexp.h libregexp-opcode.h libunicode.c
#   libunicode.h libunicode-table.h list.h quickjs.c quickjs.h quickjs-atom.h
#   quickjs-c-atomics.h quickjs-opcode.h cutils.h builtin-array-fromasync.h
#   builtin-iterator-zip.h builtin-iterator-zip-keyed.h
#
# Not vendored (unused by the embedded runtime): quickjs-libc.{c,h} (the
# std/os module - the assistant provides its own Player./Client.
# surface instead), the qjs/qjsc REPL tools, tests and build files.
#
# Built directly into the xuoassist static lib (see tools/xuoassist/
# CMakeLists.txt); the wrapper lives in tools/xuoassist/script/engine.* and
# is namespace xuoassist, the vendored C stays unnamespaced.
