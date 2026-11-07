<p align="center">
  <img src="assets/banner.svg" alt="Omarchy Gridnet — float freely, snap into place." width="100%" />
</p>

<p align="center">
  <a href="https://omarchy.org"><img src="https://img.shields.io/badge/Omarchy-4.5%20rc3-d4d4d8?style=flat-square&amp;labelColor=27272a" alt="Tested on Omarchy 4.5 rc3" /></a>
  <a href="https://hypr.land"><img src="https://img.shields.io/badge/Hyprland-0.56.2-d4d4d8?style=flat-square&amp;labelColor=27272a" alt="Tested on Hyprland 0.56.2" /></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-d4d4d8?style=flat-square&amp;labelColor=27272a" alt="MIT license" /></a>
</p>

<p align="center">
  Window snapping for Omarchy's native floating workspaces.<br />
  Horizontal halves, vertical halves, and quarters — one drag away.
</p>

<p align="center">
  <a href="#install">Install</a> ·
  <a href="#use">Use</a> ·
  <a href="#configure">Configure</a> ·
  <a href="#development">Development</a>
</p>

## See it in action

![Live demo: edge and corner previews, snapping, and drag-away restoration](assets/demo.gif)

- **See before you snap.** Live previews follow your Omarchy accent color.
- **Get your size back.** Pull a snapped window away to restore its previous size.
- **Keep windows within reach.** Free drops leave a titlebar grip on screen.
- **Fit your desktop.** Placement follows Hyprland gaps and panel reservations.

Active only in **native floating workspaces**. Uses your existing titlebars and drag bindings.

## Install

```sh
omarchy plugin add https://github.com/hudrucan/omarchy-gridnet --enable
```

**Tested on Omarchy 4.5 rc3 · Hyprland 0.56.2.** Requires Python 3, `pkg-config`,
`g++` with C++26 support, and matching Hyprland development headers
(`pkg-config hyprland` must succeed). The native module builds on first use.

After updating Hyprland, log in again before enabling Gridnet so the running
compositor matches the installed headers.

<details>
<summary><strong>Upgrading from Omasnap</strong></summary>

Omarchy Gridnet is the new name for Omasnap. The plugin ID, IPC commands, and
cache/runtime paths keep their original names so existing installations and
settings continue to work. The old GitHub URL redirects to this repository.

```sh
omarchy plugin update omasnap.window-snap
```

</details>

## Use

Drag a window by its **titlebar**, or hold **Super + left-drag** anywhere on it.
Move to an edge or corner, then release when the preview appears.

| Drop at | Snap to |
| :--- | :--- |
| Left or right edge | Half of the work area |
| Any corner | Quarter of the work area |
| Top center | Top half |
| Bottom center | Bottom half |

**Change your mind?** Move away from the edge or press **Esc** to cancel snapping.
Drag a snapped window away to restore its previous size.

Double-click the native Omarchy titlebar to fill the work area.

Using Omastation from macOS? Its **Option/Alt + drag** alias works the same way.

### Keyboard shortcuts

Hold **Ctrl + Alt + Shift** (Mac: **Control + Option + Shift**):

| Key | Snap to |
| :--- | :--- |
| `←` / `→` | Left / right half |
| `↑` / `↓` | Top / bottom half |
| `U` / `I` | Top-left / top-right quarter |
| `J` / `K` | Bottom-left / bottom-right quarter |
| `Enter` | Full work area (maximize) |

These nine shortcuts are registered while Gridnet is enabled and removed when
disabled. Outside native floating workspaces, they pass through to the focused
app. Existing bindings for the same combinations take priority.

## Configure

Defaults work out of the box. Settings apply live.

<details>
<summary><strong>Snap zones, gaps, and preview</strong></summary>

Merge these fields into the existing `omasnap.window-snap` entry in the `plugins`
array of `~/.config/omarchy/shell.json`. Keep its other fields.

```json
{
  "id": "omasnap.window-snap",
  "edge": 24,
  "corner": 160,
  "gap": null,
  "preview": true,
  "keepVisible": true
}
```

| Setting | Default | Controls |
| :--- | :--- | :--- |
| `edge` | `24` | Activation strip width |
| `corner` | `160` | Corner span along each edge |
| `gap` | `null` | Follow Hyprland; a number overrides all insets |
| `preview` | `true` | Show the destination while dragging |
| `keepVisible` | `true` | Keep a titlebar grip on screen after free drops |

Distances use logical pixels. Preview color follows your Omarchy theme.

By default, Gridnet reads gaps live from Hyprland for both preview and snapping:

- `general.gaps_in`: inset on each side of an internal split. A value of `7`
  gives a `14` px gap between adjacent windows.
- `general.float_gaps`: inset at the outside edges of the usable monitor area.
  A negative value inherits `gaps_out`. Workspace rules can override `gaps_in`,
  `float_gaps`, and the inherited `gaps_out`.
- Gap values support separate `top`, `right`, `bottom`, and `left` sides.

Omarchy Settings writes the window gap to `general.gaps_in` in
`~/.config/hypr/omasettings.lua`; change it through Settings. Existing snapped
windows pick up new gaps the next time they are snapped. An explicit Gridnet
`gap` number keeps the old uniform inset behavior; remove it or set it to `null`
to follow Hyprland.

</details>

<details>
<summary><strong>Update, disable, or remove</strong></summary>

```sh
omarchy plugin update omasnap.window-snap
omarchy plugin disable omasnap.window-snap
omarchy plugin enable omasnap.window-snap
omarchy plugin remove omasnap.window-snap --yes
```

</details>

<details>
<summary><strong>Control from the terminal</strong></summary>

```sh
hyprctl omasnap status
hyprctl omasnap snap left
hyprctl omasnap restore
```

Snap targets: `left`, `right`, `top`, `bottom`, `top-left`, `top-right`,
`bottom-left`, `bottom-right`, `full`.

</details>

## Development

```sh
git clone https://github.com/hudrucan/omarchy-gridnet.git
cd omarchy-gridnet
./validate
```

Checks geometry, the manifest, QML and Python syntax, then builds the native
module. [Integration tests](tests/README.md) cover live dragging in a separate
Hyprland instance. To install a clean, committed checkout locally, run `./install-local`.

`Service.qml` manages the native module's lifecycle; disabling the plugin unloads it.
Builds are cached in `~/.cache/omasnap-window-snap/` (or under `$XDG_CACHE_HOME`)
and checked against the running Hyprland ABI.

Found a bug? [Open an issue](https://github.com/hudrucan/omarchy-gridnet/issues) with your
Omarchy and Hyprland versions, drag method, and steps to reproduce.

---

<p align="center">Made for <a href="https://omarchy.org">Omarchy</a> · <a href="LICENSE">MIT licensed</a></p>
