#!/usr/bin/env python3
"""Draws the board illustrations of the web portal (web/img/board-<id>.svg): vector pictures of each
supported board whose screen shows a mock-up of the real plate (the firmware's own 3D aircraft render,
the real layout and text). Original artwork, no photos.

    python3 scripts/portal_boards.py

Needs g++ and Pillow.
"""
import base64, io, os, subprocess
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "web", "img")
CREAM, INK, RED, FAINT = (244, 234, 208), (44, 32, 22), (150, 40, 30), (176, 160, 130)

# board id -> (screen pixels W, H, callsign, ICAO type, airline, route, altitude, speed, distance)
SCREENS = {
    "cyd": (320, 240, "TAP443", "A21N", "TAP Portugal", "Paris > Lisbon", "FL068", "259 kt  trk 271", "22.2 km S"),
    "cyd-st7789": (320, 240, "EZY8142", "A20N", "easyJet", "Geneva > Faro", "FL245", "441 kt  trk 238", "17.4 km NE"),
    "cyd35": (480, 320, "UAE177", "A388", "Emirates", "Dubai > Lisbon", "FL312", "478 kt  trk 296", "31.0 km SE"),
    "amoled18": (368, 448, "ETD72", "B789", "Etihad", "Abu Dhabi > Madrid", "FL371", "490 kt  trk 301", "44.8 km E"),
}
CALLSIGN_AIRLINE = {"TAP443": "TAP1", "EZY8142": "EZY1", "UAE177": "UAE1", "ETD72": "ETD1"}


def font(kind, size):
    names = {"bold": "Arial Bold.ttf", "reg": "Arial.ttf", "ital": "Arial Italic.ttf"}
    for d in ("/System/Library/Fonts/Supplemental", "/usr/share/fonts/truetype/msttcorefonts", "/usr/share/fonts/truetype/dejavu"):
        p = os.path.join(d, names[kind])
        if os.path.exists(p):
            return ImageFont.truetype(p, int(size))
    return ImageFont.load_default()


def render_plane(callsign, icao):
    """The firmware's own 3D model, right-facing, cropped, on transparent."""
    aj = subprocess.check_output(["bash", "-c", ". scripts/_arduinojson.sh && echo $AJ_DIR"], cwd=ROOT, text=True).strip()
    fw = os.path.join(ROOT, "firmware/jf_flight_display")
    exe = os.path.join(ROOT, "build/render_grid")
    os.makedirs(os.path.join(ROOT, "build"), exist_ok=True)
    if not os.path.exists(exe):
        subprocess.run(["g++", "-std=c++17", "-O2", f"-I{fw}", f"-I{aj}", os.path.join(ROOT, "tools/render_grid.cpp"),
                        *[os.path.join(fw, f) for f in ("model3d.cpp", "silhouette.cpp", "airline.cpp")], "-o", exe], check=True)
    ppm = os.path.join(ROOT, "build/board_plane.ppm")
    subprocess.run([exe, ppm, "640x420", "1", CALLSIGN_AIRLINE[callsign], icao], check=True)
    im = Image.open(ppm).convert("RGBA")
    px = im.load()
    for y in range(im.height):
        for x in range(im.width):
            if px[x, y][:3] == CREAM:
                px[x, y] = (0, 0, 0, 0)
    return im.crop(im.getbbox())


def plate(w, h, spec):
    """The plate at 2x: the same layout rules as firmware/jf_flight_display/layout.h."""
    _, _, call, icao, airline, route, alt, speed, dist = spec
    K = 2  # supersampling
    im = Image.new("RGB", (w * K, h * K), CREAM)
    d = ImageDraw.Draw(im)
    aspect = w / h
    stacked = aspect < 1.2
    if stacked:
        s = w / 320.0
        ch = h / s
        info_y = ch - 122 - 10
        strip_y = info_y - 24 - 2
        model = (10, 40, 300, strip_y - 44)
        compass, heading, dots = (34, strip_y + 12), (54, strip_y + 12), (240, strip_y + 12)
        call_p, type_p, air_p = (14, info_y + 2), (150, info_y + 6), (14, info_y + 30)
        rule = (14, info_y + 48, 292)
        alt_p, spd_p, dist_p = (14, info_y + 54), (166, info_y + 54), (14, info_y + 88)
        div = ("h", 10, info_y - 6, 300)
        bars = (14, info_y - 3)
        hdr_right = 306
    else:
        s = h / 240.0
        cw = w / s
        info_x = cw - 134 - 10
        model = (14, 38, info_x - 6 - 14, 128)
        mcx = 14 + model[2] / 2
        compass, heading, dots = (40, 192), (62, 192), (mcx, 216)
        x0 = info_x + 10
        call_p, type_p, air_p = (x0, 40), (x0, 68), (x0, 90)
        rule = (x0, 108, 118)
        alt_p, spd_p, dist_p = (x0, 112), (x0, 150), (x0, 188)
        div = ("v", info_x, 36, 190)
        bars = (x0, 34)
        hdr_right = cw - 14
    X = lambda v: v * s * K
    # border and header
    d.rectangle((3 * K, 3 * K, w * K - 3 * K, h * K - 3 * K), outline=INK, width=K)
    d.rectangle((6 * K, 6 * K, w * K - 6 * K, h * K - 6 * K), outline=FAINT, width=K)
    d.text((X(14), X(11)), route, font=font("reg", 16 * s * K), fill=INK)
    d.text((X(hdr_right), X(11)), "2/8  12s ago", font=font("reg", 16 * s * K), fill=FAINT, anchor="ra")
    d.line((X(10), X(30), w * K - X(10), X(30)), fill=INK, width=K)
    # the plane: the largest size that fits its area
    pl = render_plane(call, icao)
    mx, my, mw, mh = model
    k = min(mw * s * K / pl.width, mh * s * K / pl.height)
    pl = pl.resize((max(1, int(pl.width * k)), max(1, int(pl.height * k))), Image.LANCZOS)
    im.paste(pl, (int(X(mx) + (mw * s * K - pl.width) / 2), int(X(my) + (mh * s * K - pl.height) / 2)), pl)
    # compass, heading, dots
    cx, cy, r = X(compass[0]), X(compass[1]), 12 * s * K
    d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=FAINT, width=K)
    d.polygon([(cx - 3 * s * K, cy - r), (cx + 3 * s * K, cy - r), (cx, cy - r - 6 * s * K)], fill=RED)
    d.polygon([(cx - 0.9 * r, cy), (cx + 0.7 * r, cy - 0.45 * r), (cx + 0.7 * r, cy + 0.45 * r)], fill=INK)
    d.text((X(heading[0]), X(heading[1])), "W  271", font=font("bold", 17 * s * K), fill=INK, anchor="lm")
    n = 5
    for i in range(n):
        x = X(dots[0] - (n - 1) * 6 + i * 12)
        y = X(dots[1] + (0 if stacked else 0))
        rr = 3 * s * K
        d.ellipse((x - rr, y - rr, x + rr, y + rr), fill=INK if i == 2 else None, outline=INK if i == 2 else FAINT, width=K)
    # divider, particulars
    if div[0] == "v":
        d.line((X(div[1]), X(div[2]), X(div[1]), X(div[2] + div[3])), fill=FAINT, width=K)
    else:
        d.line((X(div[1]), X(div[2]), X(div[1] + div[3]), X(div[2])), fill=FAINT, width=K)
    d.text((X(call_p[0]), X(call_p[1])), call, font=font("bold", 24 * s * K), fill=INK)
    d.text((X(type_p[0]), X(type_p[1])), icao, font=font("ital", 18 * s * K), fill=RED)
    d.text((X(air_p[0]), X(air_p[1])), airline, font=font("reg", 16 * s * K), fill=INK)
    d.rectangle((X(bars[0]), X(bars[1]), X(bars[0] + 78), X(bars[1] + 3)), fill=RED)
    d.rectangle((X(bars[0] + 78), X(bars[1]), X(bars[0] + 118), X(bars[1] + 3)), fill=INK)
    d.line((X(rule[0]), X(rule[1]), X(rule[0] + rule[2]), X(rule[1])), fill=INK, width=K)
    for (px, py), label, value in ((alt_p, "ALTITUDE", alt), (spd_p, "SPEED / TRACK", speed), (dist_p, "DISTANCE", dist)):
        d.text((X(px), X(py)), label, font=font("reg", 15 * s * K), fill=FAINT)
        d.text((X(px), X(py + 16)), value, font=font("reg", 18 * s * K), fill=INK)
    return im


def png_b64(im):
    buf = io.BytesIO()
    im.save(buf, "PNG", optimize=True)
    return base64.b64encode(buf.getvalue()).decode()


def usb(x, y, w, h, c="#bdbdbd"):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="2" fill="{c}" stroke="#777" stroke-width="1"/><rect x="{x + 3}" y="{y + 3}" width="{w - 6}" height="{h - 6}" rx="1" fill="#555"/>'


def hole(x, y):
    return f'<circle cx="{x}" cy="{y}" r="5" fill="#f6f0e0" stroke="#8a6a10" stroke-width="1.5"/>'


def svg(w, h, body, label):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="0 0 {w} {h}" role="img" aria-label="{label}">\n'
            '  <defs><filter id="s" x="-10%" y="-10%" width="120%" height="130%"><feDropShadow dx="0" dy="6" stdDeviation="6" flood-opacity=".25"/></filter></defs>\n'
            f'  {body}\n</svg>\n')


def cyd_svg(bid, two_usb=False, big=False):
    w, h = (420, 270) if big else (380, 250)
    pw, ph = (380, 250) if big else (340, 230)
    sw = 296 if big else 262
    sh = round(sw * (2 / 3 if big else 3 / 4))
    px, py = 20, 10
    sx, sy = px + 38, py + (ph - sh) // 2
    im = plate(*SCREENS[bid][:2], SCREENS[bid])
    body = f'<g filter="url(#s)"><rect x="{px}" y="{py}" width="{pw}" height="{ph}" rx="12" fill="#f0b81c"/>'
    body += hole(px + 12, py + 12) + hole(px + pw - 12, py + 12) + hole(px + 12, py + ph - 12) + hole(px + pw - 12, py + ph - 12)
    body += f'<rect x="{sx - 8}" y="{sy - 8}" width="{sw + 16}" height="{sh + 16}" rx="6" fill="#16120d"/>'
    body += f'<image x="{sx}" y="{sy}" width="{sw}" height="{sh}" preserveAspectRatio="none" xlink:href="data:image/png;base64,{png_b64(im)}"/></g>'
    body += usb(px - 10, py + ph // 2 - 14 - (22 if two_usb else 0), 26, 28)
    if two_usb:
        body += usb(px - 10, py + ph // 2 + 8, 26, 28, "#d8d8d8")
    body += f'<circle cx="{px + pw - 22}" cy="{py + ph // 2 - 20}" r="6" fill="#2a2a2a"/><circle cx="{px + pw - 22}" cy="{py + ph // 2 + 4}" r="6" fill="#2a2a2a"/>'
    return svg(w, h, body, "ESP32 touchscreen board showing a flight")


def amoled_svg():
    w, h = 300, 340
    sw, sh = 212, 258  # the glass: 368:448
    im = plate(*SCREENS["amoled18"][:2], SCREENS["amoled18"])
    body = '<g filter="url(#s)"><rect x="35" y="10" width="230" height="305" rx="30" fill="#1a1d22"/>'
    body += f'<rect x="43" y="18" width="{sw + 14}" height="{sh + 14}" rx="22" fill="#05060a"/>'
    body += f'<image x="50" y="25" width="{sw}" height="{sh}" preserveAspectRatio="none" xlink:href="data:image/png;base64,{png_b64(im)}"/></g>'
    body += '<rect x="128" y="300" width="44" height="18" rx="5" fill="#cfcfcf" stroke="#777" stroke-width="1"/>'
    return svg(w, h, body, "Waveshare AMOLED board showing a flight")


BOARDS = {"cyd": cyd_svg("cyd"), "cyd-st7789": cyd_svg("cyd-st7789", two_usb=True),
          "cyd35": cyd_svg("cyd35", big=True), "amoled18": amoled_svg()}
os.makedirs(OUT, exist_ok=True)
for name, content in BOARDS.items():
    path = os.path.join(OUT, f"board-{name}.svg")
    with open(path, "w") as f:
        f.write(content)
    print(path, len(content) // 1024, "KB")
