# Omasnap

Drag-to-snap for **Omarchy 4.5 floating workspaces**. Drag a native titlebar, an
application titlebar, or use Super + left-drag. Release near an edge or corner:

| Drop target | Result |
| --- | --- |
| Left / right | Left / right half |
| Any corner | Corresponding quarter |
| Top center | Fill the usable screen area |
| Bottom center | Bottom half |

A translucent preview shows the destination. Move away before releasing to
cancel, or press Escape to cancel snapping for that drag. Drag a snapped window
again to restore its previous size. On free drops, a part of the titlebar stays
reachable. Monitor scale, rotation, negative coordinates, and reserved bar/dock
space use Hyprland's logical work area.

This plugin extends Omarchy's native floating mode and titlebars. It does not
change workspace layouts, other windows, default shortcuts, or magnetic snapping.
Only windows tagged `omarchy-floating-workspace` are eligible.

## Requirements

- Omarchy 4.5 with native floating workspaces and the shell plugin system
- Hyprland **0.56.2** (the initial native API target), with matching installed headers
- `python3`, `g++` with C++26 support, and `pkg-config`
- Hyprland's development dependencies exposed by `pkg-config hyprland`

The native module builds locally and checks the exact running Hyprland ABI before
loading. After a Hyprland update, log in to the new compositor before enabling the
plugin; new headers require a new build. No sudo, system package changes, or
replacement titlebar module is used. Other Hyprland versions need verification.

## Install locally

```sh
./validate
git add .
git commit -m "Add Omasnap floating window snapping"
./install-local
```

The repo is a standard Omarchy service plugin: `manifest.json` declares its
`Service.qml` entry point. The service builds and loads a small native Hyprland
module, owns it while enabled, and unloads it on disable. It listens to native
input events; no function hooks, input remapping, or polling is needed.

Once published, install using `omarchy plugin add <git-url> --enable`.

## Settings

Optional fields on the `omasnap.window-snap` entry in `~/.config/omarchy/shell.json`:

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

Distances are logical pixels. `edge` is the activation strip at the monitor edge;
`corner` is the length along that strip which targets a quarter. `gap` is the
inset around each snapped window. Settings apply live. The preview follows the
Omarchy accent color. Top-center placement fills the work area with a floating
window, preserving its titlebar and the panel areas.

For optional bindings or diagnostics, the native IPC accepts:

```sh
hyprctl omasnap status
hyprctl omasnap snap left       # right, top, bottom, top-left, etc.
hyprctl omasnap restore
```

## Disable / remove

```sh
omarchy plugin disable omasnap.window-snap
omarchy plugin remove omasnap.window-snap --yes
```

Builds are cached under `~/.cache/omasnap-window-snap/`; removing that cache is
optional. No Hyprland config file is edited. The display name and repository are
Omasnap; the plugin ID is `omasnap.window-snap` to distinguish it from Omarchy's
existing screenshot command, `/usr/bin/omasnap`.

## Validation

`./validate` checks geometry (including scaled, reserved and off-screen areas),
the Omarchy manifest, QML, Python syntax, and builds the native module. Test a
native drag in a live session before treating a new Hyprland ABI as supported.

`tests/integration.py` exercises a **separate test compositor** using the native
Omarchy titlebar and a virtual mouse. It checks all eight placements, exact
restoration, titlebar dragging, the preview, drag-away restoration, cancellation,
and rejection of ordinary floating windows outside native floating workspaces.
Run with the test window opened before loading Omasnap as well as after loading
it: existing titlebars can finish their drag in an earlier button listener. The
test verifies exact snapped client geometry and restoration after a client size
adjustment. `--app-id org.omarchy.agent` can target a tagged Agent test window.
It refuses to target the current compositor. Generate the client protocol header
and source from the official `wlr-virtual-pointer-unstable-v1.xml` with
`wayland-scanner`, then compile `tests/virtual-pointer.c` against `wayland-client`.
The test compositor needs an `OMASNAP-TEST` output at `(0, 0)`, scale 1, border 2,
a 30-pixel native titlebar, animations disabled, and a window with app ID
`omasnap-test` tagged `omarchy-floating-workspace`.

```sh
python3 tests/integration.py --instance <test-instance> --display <test-wayland-socket> \
  --pointer <compiled-virtual-pointer>
```

## License

MIT. See [LICENSE](LICENSE).
