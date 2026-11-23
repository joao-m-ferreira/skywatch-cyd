#!/usr/bin/env python3
"""Draws the board illustrations of the web portal (web/img/board-<id>.svg): simple vector pictures of
each supported board with the plate on its screen. Original artwork, no photos.

    python3 scripts/portal_boards.py
"""
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "web", "img")
CREAM, INK, RED, FAINT = "#f4ead0", "#2c2016", "#96281e", "#b0a082"


def plane(x, y, k):
    """A small airliner seen from above-side, nose to the right, centred on (x, y), scale k."""
    return f"""<g transform="translate({x} {y}) scale({k})" stroke="{INK}" stroke-width="1.2" stroke-linejoin="round">
    <path d="M-8 -1 L-30 -24 L-18 -24 L12 -3Z" fill="#e4e2de"/>
    <path d="M-8 1 L-30 24 L-18 24 L12 3Z" fill="#cfcdc9"/>
    <path d="M-36 0 L-44 -15 L-36 -15 L-27 -2Z" fill="{RED}"/>
    <ellipse cx="0" cy="0" rx="40" ry="7" fill="#fbfaf6"/>
    <path d="M30 -5 Q40 -4 40 0 Q40 2 33 3Z" fill="#9fc2dd" stroke="none"/>
  </g>"""


def screen(x, y, w, h, landscape=True):
    """The plate: border, the plane, and grey bars standing for the text."""
    parts = [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{CREAM}"/>',
             f'<rect x="{x + 3}" y="{y + 3}" width="{w - 6}" height="{h - 6}" fill="none" stroke="{INK}" stroke-width="1"/>']
    if landscape:
        split = x + w * 0.56
        parts.append(plane(x + w * 0.28, y + h * 0.5, w / 330))
        parts.append(f'<line x1="{split}" y1="{y + 10}" x2="{split}" y2="{y + h - 10}" stroke="{FAINT}" stroke-width="1"/>')
        tx, ty, tw = split + 8, y + 14, w * 0.38
        parts.append(f'<rect x="{tx}" y="{ty}" width="{tw * 0.75}" height="9" rx="2" fill="{INK}"/>')
        parts.append(f'<rect x="{tx}" y="{ty + 15}" width="{tw * 0.4}" height="6" rx="2" fill="{RED}"/>')
        for i in range(3):
            parts.append(f'<rect x="{tx}" y="{ty + 32 + i * 17}" width="{tw * 0.35}" height="4" rx="2" fill="{FAINT}"/>')
            parts.append(f'<rect x="{tx}" y="{ty + 38 + i * 17}" width="{tw * (0.8 - i * 0.12)}" height="6" rx="2" fill="{INK}"/>')
    else:
        parts.append(plane(x + w * 0.5, y + h * 0.27, w / 300 * 1.5))
        parts.append(f'<line x1="{x + 10}" y1="{y + h * 0.5}" x2="{x + w - 10}" y2="{y + h * 0.5}" stroke="{FAINT}" stroke-width="1"/>')
        tx, ty = x + 14, y + h * 0.5 + 10
        parts.append(f'<rect x="{tx}" y="{ty}" width="{w * 0.38}" height="10" rx="2" fill="{INK}"/>')
        parts.append(f'<rect x="{tx + w * 0.45}" y="{ty + 2}" width="{w * 0.2}" height="6" rx="2" fill="{RED}"/>')
        for r in range(2):
            for c in range(2):
                px, py = tx + c * w * 0.5, ty + 22 + r * 30
                parts.append(f'<rect x="{px}" y="{py}" width="{w * 0.2}" height="4" rx="2" fill="{FAINT}"/>')
                parts.append(f'<rect x="{px}" y="{py + 7}" width="{w * 0.36}" height="7" rx="2" fill="{INK}"/>')
    return "\n  ".join(parts)


def svg(w, h, body, label):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" role="img" aria-label="{label}">\n'
            f'  <defs><filter id="s" x="-10%" y="-10%" width="120%" height="130%"><feDropShadow dx="0" dy="6" stdDeviation="6" flood-opacity=".25"/></filter></defs>\n'
            f'  {body}\n</svg>\n')


def hole(x, y):
    return f'<circle cx="{x}" cy="{y}" r="5" fill="#f6f0e0" stroke="#8a6a10" stroke-width="1.5"/>'


def usb(x, y, w, h, c="#bdbdbd"):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="2" fill="{c}" stroke="#777" stroke-width="1"/><rect x="{x + 3}" y="{y + 3}" width="{w - 6}" height="{h - 6}" rx="1" fill="#555"/>'


def cyd(two_usb=False, big=False):
    w, h = (400, 260) if big else (380, 250)
    sw, sh = (286, 196) if big else (262, 190)
    pw, ph = (360, 240) if big else (340, 230)
    px, py = 20, 10
    sx, sy = px + 38, py + (ph - sh) // 2
    body = f'<g filter="url(#s)"><rect x="{px}" y="{py}" width="{pw}" height="{ph}" rx="12" fill="#f0b81c"/>'
    body += hole(px + 12, py + 12) + hole(px + pw - 12, py + 12) + hole(px + 12, py + ph - 12) + hole(px + pw - 12, py + ph - 12)
    body += f'<rect x="{sx - 8}" y="{sy - 8}" width="{sw + 16}" height="{sh + 16}" rx="6" fill="#16120d"/>'
    body += screen(sx, sy, sw, sh) + "</g>"
    body += usb(px - 10, py + ph // 2 - 14 - (22 if two_usb else 0), 26, 28)  # micro-USB on the left edge
    if two_usb:
        body += usb(px - 10, py + ph // 2 + 8, 26, 28, "#d8d8d8")  # USB-C
    body += f'<circle cx="{px + pw - 22}" cy="{py + ph // 2 - 20}" r="6" fill="#2a2a2a"/><circle cx="{px + pw - 22}" cy="{py + ph // 2 + 4}" r="6" fill="#2a2a2a"/>'
    return svg(w, h, body, "ESP32 touchscreen board")


def amoled():
    w, h = 300, 330
    body = '<g filter="url(#s)"><rect x="55" y="10" width="190" height="300" rx="30" fill="#1a1d22"/>'
    body += '<rect x="63" y="18" width="174" height="284" rx="24" fill="#05060a"/>'
    body += screen(70, 26, 160, 262, landscape=False)
    body += '</g><rect x="128" y="302" width="44" height="18" rx="5" fill="#cfcfcf" stroke="#777" stroke-width="1"/>'
    return svg(w, h, body, "Waveshare AMOLED board")


BOARDS = {
    "cyd": cyd(),
    "cyd-st7789": cyd(two_usb=True),
    "cyd35": cyd(big=True),
    "amoled18": amoled(),
}
os.makedirs(OUT, exist_ok=True)
for name, content in BOARDS.items():
    path = os.path.join(OUT, f"board-{name}.svg")
    with open(path, "w") as f:
        f.write(content)
    print(path, len(content) // 1024 or 1, "KB")
