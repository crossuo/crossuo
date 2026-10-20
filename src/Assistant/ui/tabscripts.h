// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The Scripts tab: the loaded-scripts list (Load imports a file through the
// native picker, double-click opens an editor, Play/Pause/Stop run the file)
// plus the shared toolbar actions and the sort option. The list persists with
// the profile ([scripts] loadedscript0..N) and is restored on the next start.

namespace xuoassist {

void TabScripts_Draw();

} // namespace xuoassist
