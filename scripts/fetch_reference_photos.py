#!/usr/bin/env python3
"""Download the reference photos listed in references/photos/index.json (one thumbnail per airline).

    python3 scripts/fetch_reference_photos.py

Uses the public planespotters.net photo API, one request per second, with the contact from the
git-ignored firmware/jf_flight_display/secrets.h in the User-Agent (their API requires one). The
photos belong to their photographers and are never committed (see references/README.md).
"""
import json, os, re, time, urllib.request

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
secrets = open(os.path.join(root, "firmware/jf_flight_display/secrets.h")).read()
m = re.search(r'PLANESPOTTERS_CONTACT\s+"([^"]+)"', secrets)
if not m:
    raise SystemExit("set PLANESPOTTERS_CONTACT in firmware/jf_flight_display/secrets.h first")
ua = f"jf-flight-display/1.0 (+mailto:{m.group(1)})"
idx_path = os.path.join(root, "references/photos/index.json")
idx = json.load(open(idx_path))
for code, info in idx.items():
    dest = os.path.join(root, f"references/photos/{code}.jpg")
    if os.path.exists(dest):
        continue
    req = urllib.request.Request(f"https://api.planespotters.net/pub/photos/reg/{info['reg']}", headers={"User-Agent": ua})
    photos = json.load(urllib.request.urlopen(req, timeout=20)).get("photos") or []
    if not photos:
        print(code, info["reg"], "no photo")
        continue
    p = photos[0]
    src = (p.get("thumbnail_large") or p["thumbnail"])["src"]
    open(dest, "wb").write(urllib.request.urlopen(urllib.request.Request(src, headers={"User-Agent": ua}), timeout=20).read())
    info["photographer"] = p.get("photographer", info.get("photographer", ""))
    print(code, info["reg"], "ok")
    time.sleep(1.0)
json.dump(idx, open(idx_path, "w"), indent=1, sort_keys=True)
