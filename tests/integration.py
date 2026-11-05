#!/usr/bin/env python3
"""Exercise an already isolated Hyprland test instance; never use the real desktop."""
import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--instance", required=True)
parser.add_argument("--display", required=True)
parser.add_argument("--pointer", required=True, help="compiled tests/virtual-pointer.c")
parser.add_argument("--app-id", default="omasnap-test")
parser.add_argument("--artifacts", default="build/integration")
args = parser.parse_args()
if args.instance == os.environ.get("HYPRLAND_INSTANCE_SIGNATURE"):
    raise SystemExit("Refusing to exercise the current desktop; pass a dedicated test instance")
env = {**os.environ, "HYPRLAND_INSTANCE_SIGNATURE": args.instance, "WAYLAND_DISPLAY": args.display}
artifacts = Path(args.artifacts).resolve()
artifacts.mkdir(parents=True, exist_ok=True)


def ctl(*arguments, check=True):
    return subprocess.run(["hyprctl", "-i", args.instance, *arguments], text=True,
                          capture_output=True, check=check).stdout.strip()


def windows():
    return json.loads(ctl("-j", "clients"))


def current():
    return next(w for w in windows() if w["address"] == address)


def geometry():
    w = current()
    return w["at"], w["size"]


def position(x, y, width=700, height=500):
    reply = ctl("eval", f'hl.dispatch(hl.dsp.window.resize({{window="address:{address}",x={width},y={height}}})); '
                f'hl.dispatch(hl.dsp.window.move({{window="address:{address}",x={x},y={y}}}))')
    assert reply == "ok", reply
    time.sleep(0.15)


def shortcut(key):
    subprocess.run(["wtype", "-M", "ctrl", "-M", "alt", "-M", "shift", "-s", "100", "-k", key, "-s", "100",
                    "-m", "shift", "-m", "alt", "-m", "ctrl"], env=env, check=True)
    time.sleep(0.1)


def snap_bindings():
    return [b for b in json.loads(ctl("-j", "binds")) if b["dispatcher"] == "omasnap:snap"]


def pointer(command):
    mouse.stdin.write(command + "\n")
    mouse.stdin.flush()
    assert mouse.stdout.readline().strip() == "ok", command
    time.sleep(0.06)


def move(x, y):
    cursor = [float(v.strip()) for v in ctl("cursorpos").split(",")]
    pointer(f"relative {x - cursor[0]} {y - cursor[1]}")
    actual = [float(v.strip()) for v in ctl("cursorpos").split(",")]
    assert abs(actual[0] - x) <= 1 and abs(actual[1] - y) <= 1, (actual, (x, y))


@contextmanager
def modifier(name):
    held = subprocess.Popen(["wtype", "-M", name, "-s", "1500", "-m", name], env=env)
    try:
        time.sleep(0.1)
        yield
    finally:
        held.wait(timeout=5)


monitors = json.loads(ctl("-j", "monitors"))
mon = next(m for m in monitors if m["name"] == "OMASNAP-TEST")
assert mon["x"] == mon["y"] == 0 and mon["scale"] == 1, mon
width, height = mon["width"], mon["height"]


def expected(zone):
    left, top, right, bottom = mon["reserved"]
    x1, y1, x2, y2 = left, top, width - right, height - bottom
    mid_x, mid_y = (x1 + x2) // 2, (y1 + y2) // 2
    if zone in ["left", "top-left", "bottom-left"]: x2 = mid_x
    if zone in ["right", "top-right", "bottom-right"]: x1 = mid_x
    if zone in ["top", "top-left", "top-right"]: y2 = mid_y
    if zone in ["bottom", "bottom-left", "bottom-right"]: y1 = mid_y
    # This compositor uses a 2px border, 30px native titlebar and 8px gap.
    return [x1 + 10, y1 + 40], [x2 - x1 - 20, y2 - y1 - 50]


window = next(w for w in windows() if w["class"] == args.app_id)
address = window["address"]
assert ctl("eval", f'hl.dispatch(hl.dsp.focus({{window="address:{address}"}}))') == "ok"
assert json.loads(ctl("omasnap", "status"))["ready"]
assert ctl("omasnap", "configure 24 160 8 1 1 59a6ff") == "ok"

mouse = subprocess.Popen([args.pointer], env=env, text=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
assert mouse.stdout.readline().strip() == "ready"
try:
    ctl("omasnap", "restore", check=False)
    position(300, 220)
    original = geometry()
    for zone in ["left", "right", "top", "bottom", "top-left", "top-right", "bottom-left", "bottom-right", "full"]:
        assert ctl("omasnap", "snap " + zone) == "ok"
        time.sleep(0.1)
        at, size = geometry()
        assert (at, size) == expected(zone), (zone, (at, size), expected(zone))
        assert ctl("omasnap", "restore") == "ok"
        time.sleep(0.1)
        assert geometry() == original, (zone, geometry(), original)
    print("All nine IPC placements and exact restoration passed", flush=True)

    assert len(snap_bindings()) == 9
    for key, zone in [("Left", "left"), ("Right", "right"), ("Up", "top"), ("Down", "bottom"),
                      ("u", "top-left"), ("i", "top-right"), ("j", "bottom-left"), ("k", "bottom-right"), ("Return", "full")]:
        shortcut(key)
        assert geometry() == expected(zone), (key, geometry(), expected(zone))
        assert ctl("omasnap", "restore") == "ok"
        time.sleep(0.1)
        assert geometry() == original
    print("All nine Ctrl+Alt+Shift keyboard shortcuts passed", flush=True)

    shortcut("Return")
    at, _ = geometry()
    move(at[0] + 150, at[1] - 15)
    pointer("button 1")
    move(at[0] + 170, at[1] - 15)
    move(600, 400)
    pointer("button 0")
    assert geometry()[1] == original[1], (geometry(), original)
    position(*original[0], *original[1])
    print("Enter fills the work area and dragging away restores the original size", flush=True)

    ctl("reload")
    time.sleep(0.3)
    assert len(snap_bindings()) == 9
    shortcut("Up")
    assert geometry() == expected("top")
    assert ctl("omasnap", "restore") == "ok"
    time.sleep(0.1)
    print("Keyboard shortcuts survive config reload without duplicates", flush=True)

    # Drag the native titlebar, with no modifier and no plugin-specific action.
    move(450, 205)
    print("Cursor at titlebar:", ctl("cursorpos"), flush=True)
    pointer("button 1")
    move(460, 205)
    move(500, 240)
    move(width - 3, height // 2)
    state = json.loads(ctl("omasnap", "status"))
    assert state["dragging"] and state["zone"] == "right", state
    subprocess.run(["grim", "-o", "OMASNAP-TEST", str(artifacts / "right-preview.png")], env=env, check=True)
    pointer("button 0")
    at, size = geometry()
    assert (at, size) == expected("right"), ((at, size), expected("right"))
    assert json.loads(ctl("omasnap", "status"))["zone"] == "none"
    print("Native titlebar drag, preview and right-half drop passed", flush=True)

    # Client configure acknowledgements may round sizes; they are not a user
    # starting a native border-resize drag and must preserve restoration.
    assert ctl("eval", f'hl.dispatch(hl.dsp.window.resize({{window="address:{address}",x={size[0]+3},y={size[1]+3}}}))') == "ok"
    time.sleep(0.1)
    at, size = geometry()

    # Pull the snapped window away: recover its old size and leave it floating.
    move(at[0] + 150, at[1] - 15)
    pointer("button 1")
    move(at[0] + 170, at[1] - 15)
    move(at[0] + 190, at[1] + 10)
    move(600, 400)
    pointer("button 0")
    assert geometry()[1] == original[1], (geometry(), original)
    assert current()["floating"] is True
    print("Drag-away restoration after client size adjustment passed", flush=True)

    # The top edge mirrors the bottom edge: full width, half the usable height.
    position(300, 220)
    move(450, 205)
    pointer("button 1")
    move(460, 205)
    move(500, 240)
    move(width // 2, 3)
    assert json.loads(ctl("omasnap", "status"))["zone"] == "top"
    pointer("button 0")
    assert geometry() == expected("top"), (geometry(), expected("top"))
    at, _ = geometry()
    move(at[0] + 150, at[1] - 15)
    pointer("button 1")
    move(at[0] + 170, at[1] - 15)
    move(600, 400)
    pointer("button 0")
    assert geometry()[1] == original[1], (geometry(), original)
    print("Top-half titlebar preview, drop and drag-away restoration passed", flush=True)

    # Move out of a hotspot before release: no snap should be applied.
    position(300, 220)
    move(450, 205)
    pointer("button 1")
    move(460, 205)
    move(500, 240)
    move(3, 3)
    assert json.loads(ctl("omasnap", "status"))["zone"] == "top-left"
    move(800, 450)
    pointer("button 0")
    assert geometry()[1] == original[1]
    print("Leaving a hotspot cancels snapping", flush=True)

    position(300, 220)
    move(450, 205)
    pointer("button 1")
    move(460, 205)
    move(500, 240)
    move(3, 3)
    assert json.loads(ctl("omasnap", "status"))["zone"] == "top-left"
    subprocess.run(["wtype", "-k", "Escape"], env=env, check=True)
    move(5, 5)
    pointer("button 0")
    assert geometry()[1] == original[1]
    assert json.loads(ctl("omasnap", "status"))["zone"] == "none"
    print("Escape cancels snapping for the rest of the drag", flush=True)

    # Native keybind drags start from the client area, rather than a titlebar.
    # Both modifier aliases must enter the same preview/snap/restore lifecycle.
    for name in ["logo", "alt"]:
        position(300, 220)
        move(450, 350)
        with modifier(name):
            pointer("button 1")
            move(460, 350)
            move(500, 400)
            move(width - 3, height // 2)
            state = json.loads(ctl("omasnap", "status"))
            assert state["zone"] == "right", (name, "preview", state, geometry())
            pointer("button 0")
        assert geometry() == expected("right"), (name, geometry(), expected("right"))
        at, _ = geometry()
        move(at[0] + 150, at[1] + 200)
        with modifier(name):
            pointer("button 1")
            move(at[0] + 170, at[1] + 210)
            move(at[0] + 190, at[1] + 220)
            move(600, 400)
            pointer("button 0")
        assert geometry()[1] == original[1], (name, "restore", geometry(), original)
        print(name + "+left-drag preview, snap and restore passed", flush=True)

    # A separate, ordinary floating window has no native workspace tag.
    input_log = artifacts / "outside-keys.bin"
    input_log.unlink(missing_ok=True)
    probe = ("import os,sys,tty\ntty.setraw(0)\n"
             "with open(sys.argv[1], 'wb', buffering=0) as output:\n"
             " while True: output.write(os.read(0, 4096))\n")
    outside = subprocess.Popen(["foot", "--app-id", "omasnap-outside", "python3", "-c", probe, str(input_log)], env=env,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for _ in range(30):
            matches = [w for w in windows() if w["class"] == "omasnap-outside"]
            if matches: break
            time.sleep(0.1)
        other = matches[0]["address"]
        assert ctl("eval", f'hl.dispatch(hl.dsp.window.float({{window="address:{other}",action="on"}})); '
                   f'hl.dispatch(hl.dsp.focus({{window="address:{other}"}}))') == "ok"
        reply = ctl("omasnap", "snap left", check=False)
        assert reply.startswith("error:"), ("non-native floating workspace was modified", reply)
        for _ in range(30):
            if input_log.exists(): break
            time.sleep(0.05)
        assert input_log.exists()
        before = next(w for w in windows() if w["address"] == other)
        shortcut("Left")
        after = next(w for w in windows() if w["address"] == other)
        assert (after["at"], after["size"]) == (before["at"], before["size"])
        assert b"\x1b[1;8D" in input_log.read_bytes(), "Shortcut did not reach the ordinary floating client"
    finally:
        outside.terminate()
        outside.wait(timeout=5)
    print("Workspace scope check passed", flush=True)
finally:
    mouse.stdin.write("button 0\nquit\n")
    mouse.stdin.flush()
    mouse.wait(timeout=5)
