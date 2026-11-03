# Omasnap

Drag-to-snap for Omarchy's native floating workspaces.

Drag a titlebar or use Super + left-drag, then release:

| Drop target | Result |
| --- | --- |
| Left / right edge | Half screen |
| Corner | Quarter screen |
| Top center | Fill the work area |
| Bottom center | Bottom half |

A preview shows the destination. Move away or press Escape to cancel. Drag a
snapped window again to restore its previous size. Free drops keep part of the
titlebar reachable.

Omastation's Option/Alt + drag alias uses the same behavior. Omasnap only acts in
native floating workspaces and leaves existing bindings and titlebars in place.

## Requirements

- Omarchy 4.5 rc3 and Hyprland 0.56.2 (tested)
- Matching Hyprland headers and dependencies available through `pkg-config hyprland`
- Python 3, `pkg-config`, and `g++` with C++26 support

The native module builds on first use and checks the running Hyprland ABI.
After updating Hyprland, log in again before enabling the plugin.

## Install

From a clean, committed checkout:

```sh
./install-local
```

This validates the repo, installs it in
`~/.config/omarchy/plugins/omasnap.window-snap`, and enables the plugin.

```sh
omarchy plugin disable omasnap.window-snap
omarchy plugin enable omasnap.window-snap
omarchy plugin remove omasnap.window-snap --yes
```

## Settings

Optional fields on the plugin's entry in `~/.config/omarchy/shell.json`:

```json
{
  "id": "omasnap.window-snap",
  "edge": 24,
  "corner": 160,
  "gap": 8,
  "preview": true,
  "keepVisible": true
}
```

Distances are logical pixels: `edge` sets the activation strip, `corner` sets
the corner span along each edge, and `gap` sets the inset. `keepVisible` keeps a
titlebar grip on screen after a free drop. Settings apply live; the preview
follows the Omarchy accent color. Placement respects reserved panel areas.

## Commands

```sh
hyprctl omasnap status
hyprctl omasnap snap left    # right, top, bottom, top-left, etc.
hyprctl omasnap restore
```

## Development

```sh
./validate
```

Checks geometry, the manifest, QML and Python syntax, then builds the native
module. See [integration tests](tests/README.md) for live drag tests.

`Service.qml` owns the native module and unloads it when disabled. Builds are
cached in `~/.cache/omasnap-window-snap/` or under `$XDG_CACHE_HOME`.

## License

[MIT](LICENSE).
