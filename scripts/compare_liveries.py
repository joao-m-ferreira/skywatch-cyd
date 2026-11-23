#!/usr/bin/env python3
"""Side-by-side sheets: real photo (references/photos/<ICAO>.jpg) | the firmware's 3D render.

    python3 scripts/compare_liveries.py [per_sheet] [CODE ...]   # writes build/sheets/sheetN.png (all, or only the listed airlines)

Needs Pillow and g++. The photos are git-ignored (fetch them with scripts/fetch_reference_photos.py).
Each pair is rendered with the callsign prefix of the airline and the ICAO type from index.json.
"""
import json, os, subprocess, sys
from PIL import Image, ImageDraw, ImageFont

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
os.makedirs(os.path.join(root, "build/sheets"), exist_ok=True)
idx = json.load(open(os.path.join(root, "references/photos/index.json")))
aj = subprocess.check_output(["bash", "-c", ". scripts/_arduinojson.sh && echo $AJ_DIR"], cwd=root, text=True).strip()
fw = os.path.join(root, "firmware/jf_flight_display")
exe = os.path.join(root, "build/render_pairs")
subprocess.run(["g++", "-std=c++17", "-O2", f"-I{fw}", f"-I{aj}", os.path.join(root, "tools/render_pairs.cpp"),
                *[os.path.join(fw, f) for f in ("model3d.cpp", "silhouette.cpp", "airline.cpp")], "-o", exe], check=True)
per = int(sys.argv[1]) if len(sys.argv) > 1 else 6
codes = [c for c in idx if os.path.exists(os.path.join(root, f"references/photos/{c}.jpg"))]
if len(sys.argv) > 2:
    wanted = sys.argv[2:]
    codes = [c for c in wanted if c in codes]
font = ImageFont.truetype("/System/Library/Fonts/Supplemental/Arial Bold.ttf", 16) if os.path.exists(
    "/System/Library/Fonts/Supplemental/Arial Bold.ttf") else ImageFont.load_default()
for si in range(0, len(codes), per):
    chunk = codes[si:si + per]
    ppm = os.path.join(root, "build/sheets/strip.ppm")
    subprocess.run([exe, ppm] + [x for c in chunk for x in (c + "1", idx[c]["type"])], check=True)
    strip = Image.open(ppm).convert("RGB")
    sheet = Image.new("RGB", (720, 240 * len(chunk)), (255, 255, 255))
    d = ImageDraw.Draw(sheet)
    for k, c in enumerate(chunk):
        ph = Image.open(os.path.join(root, f"references/photos/{c}.jpg")).convert("RGB")
        ph.thumbnail((360, 240))
        sheet.paste(ph, (0, k * 240))
        sheet.paste(strip.crop((0, k * 240, 360, (k + 1) * 240)), (360, k * 240))
        d.rectangle((0, k * 240, 170, k * 240 + 22), fill=(0, 0, 0))
        d.text((4, k * 240 + 3), f"{c} {idx[c]['reg']} {idx[c]['type']}", font=font, fill=(255, 255, 255))
    out = os.path.join(root, f"build/sheets/sheet{si // per + 1}.png")
    sheet.save(out)
    print(out, chunk)
