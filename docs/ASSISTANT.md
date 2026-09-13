# X:UO Assist — the in-client assistant

The CrossUO assistant is the former external X:UO Assist plugin (a Qt
application plus a Win32 DLL injected into the client) rebuilt as an
in-process ImGui overlay inside the client. It ships with the client binary
(build flag `XUO_ASSISTANT`, on by default) and needs no DLL, no plugin
directory entry and no separate app.

## Starting and stopping

- The assistant exists only in-game (it initializes on entering the game
  stage and shuts down automatically on logout, character select or
  disconnect).
- **Alt+U** toggles the assistant window. When hidden, its hotkeys, macro
  playback and script runtime keep running; a second Alt+U brings the window
  back.
- The window can be dragged outside the game window (pop-out; it becomes a
  real OS window) and docked back, like any ImGui multi-viewport window.

## Tabs

- **Main** — the option pages (filters, speech, sound, helpers), profiles,
  autostart, the display-caption editor input.
- **Lists** — Types / Objects / Find / Ignore / Friends / Enemies lists, each
  with add, remove, from-target capture and drag ordering.
- **Filters** — speech filters, text replaces (mode + type), sound filters.
- **Hotkeys** — bind keys to actions/macros/scripts, duplicate rejection,
  pass-to-client toggle, external code.
- **Macros** — create, record (the Record button captures your actions from
  the outgoing packets and target cursors), play, loop, XML-backed storage.
- **Scripts** — QuickJS-based scripting with the editor, command menu,
  running-script list (pause/stop), examples.
- **Agents** — Dress (sets with .coa-compatible storage), Party, Shop.
- **Skills** — skill table with deltas, locks and copy buttons.
- **Display** — the player caption (title bar) modes and its items/groups
  editors, with static-art previews.
- **Map** — radar window with zoom/rotate/follow, facets, opacity and the
  map settings form.
- **About** — build info and links.

## Profiles and files

Everything is stored under the game directory, in the same layout the Qt
application used:

- `Config/<ProfileName>/` — per-profile `Options.xml`, `Hotkeys.xml`,
  `Macros.xml`, `Objects.xml`, `Party.xml`, `Friends.xml`, `Enemies.xml`,
  `Dress.coa`.
- `GlobalConfig/` — shared `Types.xml`, `FindList.xml`, `IgnoreList.xml`,
  `SpeechFilter.xml`, `TextReplaces.xml`, `SoundFilter.xml`, `ShopList.xml`,
  `DisplayItems.xml`, `DisplayGroups.xml`, `Profiles.xml`, `Map.xml`.

Existing Qt-assistant config files load as-is (the writers reproduce the
original XML byte for byte; `Hotkeys.xml` also accepts the legacy version 0
records). All saves are crash-safe: content is written to a temporary file in
the same directory and atomically renamed over the target, so a crash can
never truncate a profile; leftover `*.tmp` files are ignored and replaced on
the next save.

Multi-instance note: two clients sharing one game directory share the same
`Config/` tree. As with the Qt assistant, simultaneous writes to the same
profile can lose updates — use separate profiles (or directories) per client.
Files are not locked by design.

## Scripting

Scripts are JavaScript (QuickJS) with the Orion/XUO Assist API surface
(`Player.*`, `Client.*`, `Orion.*` and the engine classes Gump, Menu,
GumpHook, ClientMacro, File). The Scripts tab opens/reloads/runs/pauses/stops
scripts; the API registration table and the coverage notes live in
`tools/xuoassist/script/apiscript.h` (the coverage table and the per-function
deviation notes are in `tools/xuoassist/script/apiscript.cpp`). Existing Qt
assistant scripts run unmodified except for the documented deviations
(no audio playback, no speech-mode variants, `LoadProfile` restrictions).

## Differences from the external Qt assistant

- No system tray icon and no minimize-to-tray: **Alt+U** hides/shows the
  window instead.
- No native file dialogs: script and hotkey files are picked in-app (the
  script editor has its own picker; `Config/` sandbox rules apply to the
  script file API).
- The native color picker is replaced by the ImGui color picker (same hex
  in/out); Ctrl+E completion is an ImGui popup.
- The fixed 650x400 window is replaced by a dockable/pop-out layout of the
  same content.
- The external UO Auto Map / UOA command pipes (the `-` command notify and
  the WM_USER position broadcasts) have no in-process equivalent; the commands
  remain normal speech lines and the map window replaces UOAM.
- The display caption modes produce a plain OS window title (colors and
  inline icons are not possible in an OS caption); the items editor previews
  the static-art icons instead.
- `savehotkeys`/`loadhotkeys` target the profile's `Hotkeys.xml` (the chosen
  path argument is not honored); target-hook tile names match by graphic,
  `land` and `any` — the Qt `tree`/`water`/`mine` category lists were part of
  the old plugin's own tile caches and are not available.

## Building

`XUO_ASSISTANT=ON` (default) links the assistant into the client and disables
the automatic load of the external `xuoassist.dll` plugin at startup — the
built-in assistant supersedes it, and loading both would double-handle
packets. `XUO_ASSISTANT=OFF` removes the assistant from the build completely
and restores the original plugin loading path.
