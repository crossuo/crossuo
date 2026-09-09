<!--
SPDX-FileCopyrightText: 2026 CrossUO Team
SPDX-License-Identifier: AGPL-3.0-or-later
-->

# NOTES — deferred decisions and known limitations (M2)

## TLS verification is deferred

`vfs_fetch` runs with `CURLOPT_SSL_VERIFYPEER 0`, mirroring the four existing
sites in `xuocore/http.cpp`. The official product/file repos are plain http
today, so nothing is verified in practice yet; https mirrors would currently be
exposed to man-in-the-middle substitution of blobs.

Closure options (pick when the first https mirror lands):

- M5 optional `.meta` sha256 sidecar verification (plan §10.4), which authenticates
  content regardless of transport; or
- a CA-bundle strategy (ship/bundle roots, enable `CURLOPT_SSL_VERIFYPEER` +
  `CURLOPT_CAINFO`) for https repos.

## Tier refinement: anim.idx vs anim.mul

`anim.idx` (1.7 MB) is T1: LoadCommon requires it, so it is fetched during boot
(§7.2 "sized at boot"). `anim.mul` (190 MB legacy fallback) stays T2: the boot
skeleton prepares it valid-but-unfetched so LoadCommon passes, and content
converges via the idle tier (§7.3 T3).

## Product manifest staleness

The plan is rebuilt only when the product serial changes; until M5 adds
conditional GET / manifest diffing, a re-signed but same-serial manifest is not
re-fetched, and the local copy can lag the repo between boots.

## Embedded T1 tables: converter TODO

The T1 tables are meant to be embedded from recorded runs (§7.5), but the
CSV → C++ converter tool does not exist yet, so recordings cannot be embedded;
this is tracked for M4/M5.
