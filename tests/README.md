# Integration tests

`integration.py` drives a virtual mouse in a separate Hyprland instance. It
checks exact snap geometry, preview, restoration after client size adjustments,
Escape cancellation, Super/Alt dragging, all nine keyboard shortcuts, config
reload, and shortcut passthrough outside native floating workspaces. It
refuses to target the current compositor.

## Build the virtual mouse

Requires `wayland-scanner`, a C compiler, `wayland-client`, and the
[wlr virtual-pointer protocol XML](https://github.com/swaywm/wlr-protocols/blob/master/unstable/wlr-virtual-pointer-unstable-v1.xml).
Run from the repo root:

```sh
protocol_xml=/path/to/wlr-virtual-pointer-unstable-v1.xml
mkdir -p build
wayland-scanner client-header "$protocol_xml" build/virtual-pointer.h
wayland-scanner private-code "$protocol_xml" build/virtual-pointer.c
cc -Ibuild tests/virtual-pointer.c build/virtual-pointer.c \
  -o build/virtual-pointer $(pkg-config --cflags --libs wayland-client)
```

## Test session

Use a dedicated compositor with:

- Output `OMASNAP-TEST` at `(0, 0)`, scale 1.
- Border 2px, native Omarchy titlebar 30px, animations disabled.
- Super/Alt + left-button bindings using `hl.dsp.window.drag()`.
- `input.resolve_binds_by_sym = true` so bindings honor `wtype`'s custom keymap.
- A floating window with app ID `omasnap-test`, tagged `omarchy-floating-workspace`.
- Gridnet loaded through `native-controller run <unique-token>` with stdin open.
- `foot`, `wtype`, and `grim` available.

```sh
python3 tests/integration.py --instance <test-instance> \
  --display <test-wayland-socket> --pointer build/virtual-pointer
```

Run with the window opened both before and after loading Gridnet; titlebar
listener order differs. Use `--app-id org.omarchy.agent` for an Agent test window.
Send `quit` to the controller and close the test compositor when finished.
