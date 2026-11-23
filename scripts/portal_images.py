#!/usr/bin/env python3
"""Renders the aircraft pictures of the web portal (web/img/plane-N.png): the firmware's own 3D
models on a transparent background, cropped to the plane.

    python3 scripts/portal_images.py

Needs g++ and Pillow. Edit PLANES to change the pictures.
"""
import os, subprocess
from PIL import Image

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
PLANES = [("TAP1", "A21N"), ("EZY1", "A20N"), ("UAE1", "A388"), ("AEA1", "B788"), ("RYR1", "B738"), ("ETD1", "B789")]
CW, CH, COLS = 520, 300, 3
BG = (244, 234, 208)  # the renderer's background: made transparent

aj = subprocess.check_output(["bash", "-c", ". scripts/_arduinojson.sh && echo $AJ_DIR"], cwd=root, text=True).strip()
fw = os.path.join(root, "firmware/jf_flight_display")
os.makedirs(os.path.join(root, "build"), exist_ok=True)
exe = os.path.join(root, "build/render_grid")
subprocess.run(["g++", "-std=c++17", "-O2", f"-I{fw}", f"-I{aj}", os.path.join(root, "tools/render_grid.cpp"),
                *[os.path.join(fw, f) for f in ("model3d.cpp", "silhouette.cpp", "airline.cpp")], "-o", exe], check=True)
ppm = os.path.join(root, "build/portal_grid.ppm")
subprocess.run([exe, ppm, f"{CW}x{CH}", str(COLS)] + [x for p in PLANES for x in p], check=True)
grid = Image.open(ppm).convert("RGBA")
dest = os.path.join(root, "web/img")
os.makedirs(dest, exist_ok=True)
for k in range(len(PLANES)):
    c, r = k % COLS, k // COLS
    cell = grid.crop((c * CW, r * CH, (c + 1) * CW, (r + 1) * CH))
    px = cell.load()
    for y in range(cell.height):
        for x in range(cell.width):
            if px[x, y][:3] == BG:
                px[x, y] = (0, 0, 0, 0)
    box = cell.getbbox()
    if box:
        cell = cell.crop((max(box[0] - 4, 0), max(box[1] - 4, 0), min(box[2] + 4, CW), min(box[3] + 4, CH)))
    out = os.path.join(dest, f"plane-{k + 1}.png")
    cell.save(out, optimize=True)
    print(out, cell.size, os.path.getsize(out) // 1024, "KB")
