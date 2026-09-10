<!-- SPDX-FileCopyrightText: 2026 CrossUO Team -->
<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->

# VUOI golden fixtures

Captured live from the official EA/Broadsword Ultima Online patch servers on
2026-09-08 during authoring of the VUOI streaming-install implementation. These are
factual protocol/metadata artifacts used as byte-stream integrity fixtures for
the offline test suite; they contain no game art, map, audio or other
copyrighted content.

At test runtime nothing is fetched: the suite reads only these files.

* `prod.xml` (1654 bytes) — product manifest, verbatim (plain XML) from
  `http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo/manifest/uo-legacyrelease.prod`.
  Records serial `1787679382`, launchfile `client.exe`, the manifest repo
  (`http://patch.uo.broadsword.com/uopatch-sa/legacyrelease/uo/manifest/`)
  and the file repo
  (`http://patch.ak-cdn.eamythic.com/uopatch-sa/250/legacyrelease/uo/files/`).
* `pkg.mft` (3739 bytes) — base package manifest, zlib-decompressed form of
  `http://patch.uo.broadsword.com/uopatch-sa/legacyrelease/uo/manifest/base/pkg.mft`
  (1792 bytes compressed; XML is stored decompressed so tests need no
  network). Lists 11 sub-manifests (`unpacked.mft`, the per-UOP `*.mft`s),
  all `ct="1"` (zlib).
* `part_0003cf84fc599ef4.zlib` (6423 bytes) — one real pack part from
  `http://patch.ak-cdn.eamythic.com/uopatch-sa/250/legacyrelease/uo/files/base/artLegacyMUL.uop/0003cf84fc599ef4`
  (chunk manifest entry
  `<f ph="3cf84" sh="fc599ef4" ul="1dc2" ct="1" cl="1917" ml="88"/>`).
  Committed in its compressed (shipped) form on purpose: it is only ever used
  as a zlib byte stream, decompressing to 7618 bytes of opaque asset payload
  which the tests never decode.

Source server responses are factual protocol data; test code lives under the
repository license (see `tests/vfs/*.cpp` headers).
