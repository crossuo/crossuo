// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_manifest_embedded: the embedded T1 manifest (VUOI plan §7.5, resolved
// decision #1). This checked-in stub ships an EMPTY table: T1 then only holds
// the static tier set and the client falls back to on-demand (T2) for
// everything else, which is correct but slower after login.
//
// Deployment flow: record a real run with `--streaming-record=<file>` and
// feed the CSV to the embedding step, which REWRITES this file (the table
// below) and rebuilds. Stale lists can never break correctness - they only
// cost a few extra T2 faults (§7.5).
//
// Schema of a generated table (in recorded first-access order):
//
//   static const std::vector<vfs_embedded_entry> s_entries = {
//       // { uop_hash, uop_file, name }
//       { 0xfc599ef40003cf84, "artLegacyMUL.uop", "" },
//       { 0, "", "gumpart.mul" },
//   };
//   const std::vector<vfs_embedded_entry> &vfs_manifest_embedded()
//   {
//       return s_entries;
//   }

#include "vfs_manifest.h"

const std::vector<vfs_embedded_entry> &vfs_manifest_embedded()
{
    static const std::vector<vfs_embedded_entry> s_empty;
    return s_empty;
}
