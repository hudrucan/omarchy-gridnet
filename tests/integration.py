#!/usr/bin/env python3
"""Exercise an already isolated Hyprland test instance; never use the real desktop."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--instance", required=True)
parser.add_argument("--display", required=True)
parser.add_argument("--pointer", required=True, help="compiled tests/virtual-pointer.c")
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


monitors = json.loads(ctl("-j", "monitors"))
mon = next(m for m in monitors if m["name"] == "OMASNAP-TEST")
assert mon["x"] == mon["y"] == 0 and mon["scale"] == 1, mon
width, height = mon["width"], mon["height"]
window = next(w for w in windows() if w["class"] == "omasnap-test")
address = window["address"]
assert ctl("eval", f'hl.dispatch(hl.dsp.focus({{window="address:{address}"}}))') == "ok"
assert json.loads(ctl("omasnap", "status"))["ready"]
assert ctl("omasnap", "configure 24 160 8 1 1 59a6ff") == "ok"

controller = subprocess.Popen([args.pointer], env=env, text=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
mouse = controller
assert mouse.stdout.readline().strip() == "ready"
try:
    ctl("omasnap", "restore", check=False)
    position(300, 220)
    original = geometry()
    for zone in ["left", "right", "top", "bottom", "top-left", "top-right", "bottom-left", "bottom-right"]:
        assert ctl("omasnap", "snap " + zone) == "ok"
        time.sleep(0.1)
        at, size = geometry()
        assert size[0] <= width and size[1] <= height and min(size) > 0, (zone, at, size)
        assert at[0] >= 8 and at[1] >= 38, ("titlebar must be on screen", zone, at)
        assert ctl("omasnap", "restore") == "ok"
        time.sleep(0.1)
        assert geometry() == original, (zone, geometry(), original)
    print("All eight IPC placements and exact restoration passed", flush=True)

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
    assert at[0] > width / 2 and size[0] < width / 2, (at, size)
    assert json.loads(ctl("omasnap", "status"))["zone"] == "none"
    print("Native titlebar drag, preview and right-half drop passed", flush=True)

    # Pull the snapped window away: recover its old size and leave it floating.
    move(at[0] + 150, at[1] - 15)
    pointer("button 1")
    move(at[0] + 170, at[1] - 15)
    move(at[0] + 190, at[1] + 10)
    move(600, 400)
    pointer("button 0")
    assert geometry()[1] == original[1], (geometry(), original)
    assert current()["floating"] is True
    print("Drag-away restoration passed", flush=True)

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

    # A separate, ordinary floating window has no native workspace tag.
    outside = subprocess.Popen(["foot", "--app-id", "omasnap-outside", "sleep", "30"], env=env,
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
    finally:
        outside.terminate()
        outside.wait(timeout=5)
    print("Workspace scope check passed", flush=True)
finally:
    mouse.stdin.write("button 0\nquit\n")
    mouse.stdin.flush()
    mouse.wait(timeout=5)
