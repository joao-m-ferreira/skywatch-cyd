#!/usr/bin/env python3
"""Regenerates docs/images/models.png: twelve airlines on twelve aircraft types, rendered on the host
with the firmware's own 3D code (tools/render_grid.cpp) and labelled with Pillow.

    python3 scripts/readme_image.py

Needs g++ and Pillow. One airline per plane, varied types: edit PLANES to change the picture.
"""
import os, subprocess
from PIL import Image, ImageDraw, ImageFont

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
# (callsign prefix of the airline, ICAO type, airline name, aircraft name)
PLANES = [
    ("TAP1", "A21N", "TAP Air Portugal", "Airbus A321neo"),
    ("EZY1", "A20N", "easyJet", "Airbus A320neo"),
    ("AEA1", "B788", "Air Europa", "Boeing 787-8"),
    ("UAE1", "A388", "Emirates", "Airbus A380-800"),
    ("ETD1", "B789", "Etihad Airways", "Boeing 787-9"),
    ("SRR1", "B763", "Maersk Air Cargo", "Boeing 767-300F"),
    ("TSC1", "A333", "Air Transat", "Airbus A330-300"),
    ("DLH1", "A346", "Lufthansa", "Airbus A340-600"),
    ("RYR1", "B738", "Ryanair", "Boeing 737-800"),
    ("BAW1", "B744", "British Airways", "Boeing 747-400"),
    ("LOT1", "E195", "LOT Polish Airlines", "Embraer E195"),
    ("LOG1", "AT76", "Loganair", "ATR 72-600"),
]
COLS, CW, CH, LABEL = 4, 440, 300, 56

aj = subprocess.check_output(["bash", "-c", ". scripts/_arduinojson.sh && echo $AJ_DIR"], cwd=root, text=True).strip()
fw = os.path.join(root, "firmware/jf_flight_display")
os.makedirs(os.path.join(root, "build"), exist_ok=True)
exe = os.path.join(root, "build/render_grid")
subprocess.run(["g++", "-std=c++17", "-O2", f"-I{fw}", f"-I{aj}", os.path.join(root, "tools/render_grid.cpp"),
                *[os.path.join(fw, f) for f in ("model3d.cpp", "silhouette.cpp", "airline.cpp")], "-o", exe], check=True)
ppm = os.path.join(root, "build/readme_grid.ppm")
subprocess.run([exe, ppm, f"{CW}x{CH}", str(COLS)] + [x for p in PLANES for x in p[:2]], check=True)
grid = Image.open(ppm).convert("RGB")
rows = (len(PLANES) + COLS - 1) // COLS
out = Image.new("RGB", (CW * COLS, (CH + LABEL) * rows), (244, 234, 208))
d = ImageDraw.Draw(out)
bold, plain = "/System/Library/Fonts/Supplemental/Arial Bold.ttf", "/System/Library/Fonts/Supplemental/Arial.ttf"
f1 = ImageFont.truetype(bold, 26) if os.path.exists(bold) else ImageFont.load_default()
f2 = ImageFont.truetype(plain, 21) if os.path.exists(plain) else ImageFont.load_default()
for k, (_, _, airline, model) in enumerate(PLANES):
    c, r = k % COLS, k // COLS
    out.paste(grid.crop((c * CW, r * CH, (c + 1) * CW, (r + 1) * CH)), (c * CW, r * (CH + LABEL)))
    y = r * (CH + LABEL) + CH - 6
    d.text((c * CW + CW // 2, y), airline, font=f1, fill=(44, 32, 22), anchor="mt")
    d.text((c * CW + CW // 2, y + 30), model, font=f2, fill=(110, 95, 75), anchor="mt")
dest = os.path.join(root, "docs/images/models.png")
out.save(dest, optimize=True)
print(dest, out.size)
