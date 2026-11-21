# Login screen themes

The main login screen (`CGumpScreenMain`) can be fully driven by data: a shard
can replace the background, move every element, use its own gump art, labels,
fonts and colours, add a music toggle and clickable community links — without
rebuilding the client. When no theme is present the built-in layout is used.

## Where a theme comes from

First available source wins, in this order:

1. `--login-theme <path|url>`
   - a local path, or
   - `http(s)://` / `crossuo://` url (fetched on every boot, cached for
     offline starts), or
   - `embedded` — the theme baked into the binary (see below).
2. `LoginTheme=<path|url>` in `crossuo.cfg` (or the shard profile).
3. `login.theme` in the UO data directory (`CustomPath`).

A source that cannot be read or parsed is logged and skipped. `none` disables
a lower-priority source explicitly.

### Embedding a theme

Configure with `-DXUO_LOGIN_THEME_FILE=path/to/shard.theme`; the build compiles
it in and `--login-theme embedded` selects it. Shard images referenced by the
theme still have to be reachable (url or data-dir name), so an embedded theme
normally points at urls.

## Format

Line based, `key = value`, `#` or `;` comments, `{ … }` widget blocks. Numbers
accept decimal or `0x` hex. A line may pack several keys with commas
(`x = 0, y = 0`).

Coordinates are pixels on the fixed 640x480 login screen.

### Background: prefer gump data

The login background is normally **gump art**, exactly like the stock screen
(`0x014E` is the built-in full-screen frame). A shard that wants its own art
either ships a modified gump in its data files or adds its own ids; a theme
just places them:

```ini
{ id = backdrop
  type = gump
  graphic = 0x014E }        # or the shard's own full-screen gump id
```

This needs no image pipeline, follows the shard's hue/art changes and works
everywhere the client data works. Art ids the client cannot resolve are logged
(`login theme: graphic 0xNNNN … is not in the data files`), so a missing gump
is visible immediately.

Use the image path below only when the art is not, and cannot be, part of the
data files (a website-hosted splash, per-launch art).

### Images

```ini
[image NAME]
file = bg.png                 # theme-relative path, absolute path, or url
mode = stretch                # stretch | fit | tile | none
anchor-x = 0
anchor-y = 0
```

`stretch` fills the widget box, `fit` aspect-fits and centres, `tile` repeats,
`none` keeps the natural size at the anchor. PNG (and bmp/jpg). Relative paths
resolve against the theme file's directory, then the UO data dir. Images
decode into the client's own art format (16-bit color-key, like every mul/uop
graphic), so they render exactly like game art - including its 1-bit alpha.

### Widgets

```
{ id = some-name              # id is the gump serial for mouse handling
  type = …                    # see the table below
  … }
```

| type | purpose | keys |
| --- | --- | --- |
| `image` | shard image | `image` (name from `[image]`), `x`, `y`, `width`, `height` |
| `gump` | one gump art id | `graphic`, `x`, `y` |
| `gump_tiled` | tiled gump art | `graphic`, `x`, `y`, `width`, `height` |
| `resizepic` | resizable gump frame | `graphic`, `x`, `y`, `width`, `height`, `bind` |
| `text` | label | `text`, `x`, `y`, `font`, `color`, `width` |
| `textbox` | account/password entry | `bind`, `x`, `y`, `width`, `font`, `color`, `color-selected`, `color-focused`, `max-length` |
| `button` | clickable art | `action`, `graphic`, `graphic-selected`, `graphic-pressed`, `x`, `y`, `blink`, `tooltip` |
| `checkbox` | toggle | `bind`, `graphic`, `graphic-checked`, `graphic-disabled`, `x`, `y`, `text`, `font`, `color`, `text-position` |
| `slider` | music volume | `bind = music_volume`, `x`, `y`, `width`, `min`, `max` |
| `html_link` | one-line clickable link | `text`, `url`, `x`, `y`, `width`, `font`, `link-color` (a gump hue like `0x03E0`, or a web color `#00FF00`; `color` is a no-op here - the html renderer colors anchor text only from the body link color) |

Draw order is file order: put backgrounds first, then frames and art, then
labels and entries, then buttons.

### Bindings

`bind` connects a widget to the client state. These are required for the
screen to work; when a theme omits one it gets an off-screen default so saving
the login config still works:

- `account`, `password` — the text entries
- `auto_login`, `save_password` — the option checkboxes
- `music`, `music_volume` — optional login-music toggle and volume slider
  (stored in the client settings, shared with the options gump)

### Actions

`action` on a button: `connect` (submit the login), `quit`, `web:account`,
`web:web`, `web:help`, `web:video`, `web:credits` (open the shard's link from
the profile), `launcher` (embedded launcher builds), or `page:<n>` to switch
pages without firing the button event.

### Text

Labels use the client's own fonts — there is no TTF support: `font` is an index
into `fonts.mul` (`2` is the login label face, `9` the small UI face). `color`
is a gump text hue (`0x0386`, `0x0802`). Literal text supports `\1` (client
version string), `\2` (the full version line) and `\n`; `cliloc:<id>` renders a
localised string instead.

## Examples

`resources/themes/` ships three:

- `crossuo-default.theme` — reproduces the built-in modern layout exactly; the
  best starting point for a shard theme.
- `fc.theme` + `fc-login-bg.png` — full-bleed art, narrowed entries, a music
  toggle and two community links. It uses the shard's own checkbox art
  (`0x16C3`…); any art id that the client cannot resolve is logged as
  `login theme: graphic 0xNNNN … is not in the data files`, so a shard owner
  sees missing art or a typo immediately.

## Diagnostics

The log reports:

- `login theme: loaded '<source>' (N widgets, M images)`
- `login theme: image '<file>' loaded (WxH)`
- `login theme: widget '<id>' …` and `… has no widgets, ignoring` for bad data
- `login theme: graphic 0xNNNN … is not in the data files` for unresolvable art

## Writing a theme

Start from `resources/themes/crossuo-default.theme`, then move/restyle
elements, run the client with `--login-theme <file>` and check the log. The
reference id/position tables of a shard's own login implementation translate
directly into `resizepic` (boxes), `text` (labels), `button` (art + action) and
`checkbox` (toggle art) blocks.
