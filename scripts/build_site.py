#!/usr/bin/env python3
"""Builds the web-flash portal (a static site for GitHub Pages) from a release's files.

    python3 scripts/build_site.py --dist dist --out build/site
    python3 scripts/build_site.py --self-test

For every hardware model in firmware/boards.json that has release files in --dist it copies the
firmware, writes two ESP Web Tools manifests (a full install and an app-only update) and a
boards.json for the page with sizes, SHA-256 and the honest test status. The page itself is web/.

Why the firmware is copied into the site instead of linking to the release: browsers can only read
files served with CORS headers, which GitHub release downloads do not have.

Stdlib only.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
STATUSES = {"verified", "untested", "experimental", "simulator"}
CHIPS = {"ESP32", "ESP32-S2", "ESP32-S3", "ESP32-C3"}
APP_OFFSET = 0x10000


def load_boards(path: Path = ROOT / "firmware" / "boards.json") -> list[dict]:
    boards = json.loads(path.read_text(encoding="utf-8"))["boards"]
    seen = set()
    for b in boards:
        for key in ("id", "name", "short", "chipFamily", "fqbn", "flags", "bootloaderOffset", "flashBytes", "status", "notes"):
            if key not in b:
                raise ValueError(f"board {b.get('id', '?')}: missing '{key}'")
        if not re.fullmatch(r"[a-z0-9][a-z0-9-]*", b["id"]):
            raise ValueError(f"board id {b['id']!r}: use lowercase letters, digits and dashes")
        if b["id"] in seen:
            raise ValueError(f"duplicate board id {b['id']}")
        seen.add(b["id"])
        if b["status"] not in STATUSES:
            raise ValueError(f"board {b['id']}: status must be one of {sorted(STATUSES)}")
        if b["chipFamily"] not in CHIPS:
            raise ValueError(f"board {b['id']}: unknown chipFamily {b['chipFamily']}")
        if not isinstance(b["bootloaderOffset"], int) or not isinstance(b["flashBytes"], int):
            raise ValueError(f"board {b['id']}: offsets and sizes are integers")
    return boards


def sha256(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def build(dist: Path, out: Path, boards: list[dict], web: Path = ROOT / "web") -> dict:
    """Writes the site into `out`; returns a summary. Raises ValueError on an unusable dist."""
    if out.exists():
        shutil.rmtree(out)
    shutil.copytree(web, out)
    (out / "manifests").mkdir()
    published, skipped, version = [], [], None
    for b in boards:
        found = {}
        for kind in ("merged", "firmware"):
            hits = sorted(dist.glob(f"skywatch-cyd-*-{b['id']}-{kind}.bin"))
            if len(hits) > 1:
                raise ValueError(f"{b['id']}: several {kind} files in {dist}")
            found[kind] = hits[0] if hits else None
        if not found["merged"] and not found["firmware"]:
            skipped.append(b["id"])
            continue
        if not (found["merged"] and found["firmware"]):
            raise ValueError(f"{b['id']}: need both the merged image and the firmware in {dist}")
        m = re.fullmatch(rf"skywatch-cyd-(.+)-{re.escape(b['id'])}-merged\.bin", found["merged"].name)
        ver = m.group(1) if m else None
        if not ver or found["firmware"].name != f"skywatch-cyd-{ver}-{b['id']}-firmware.bin":
            raise ValueError(f"{b['id']}: the two files do not share one version")
        if version and ver != version:
            raise ValueError(f"mixed versions in {dist}: {version} and {ver}")
        version = ver
        fdir = out / "firmware" / ver
        fdir.mkdir(parents=True, exist_ok=True)
        for kind, src in found.items():
            shutil.copy2(src, fdir / src.name)
        title = f"skywatch-cyd {ver} for {b['short']}"
        full = {"name": title, "version": ver, "new_install_prompt_erase": True,
                "builds": [{"chipFamily": b["chipFamily"],
                            "parts": [{"path": f"../firmware/{ver}/{found['merged'].name}", "offset": 0}]}]}
        upd = {"name": title + " (update, keeps settings)", "version": ver, "new_install_prompt_erase": False,
               "builds": [{"chipFamily": b["chipFamily"],
                           "parts": [{"path": f"../firmware/{ver}/{found['firmware'].name}", "offset": APP_OFFSET}]}]}
        (out / "manifests" / f"{b['id']}.json").write_text(json.dumps(full, indent=2) + "\n")
        (out / "manifests" / f"{b['id']}-update.json").write_text(json.dumps(upd, indent=2) + "\n")
        published.append({**{k: b[k] for k in ("id", "name", "short", "chipFamily", "status", "notes")},
                          "title": b.get("title", b["short"]), "tagline": b.get("tagline", ""), "tab": b.get("tab", b["short"]),
                          "version": ver,
                          "mergedBytes": found["merged"].stat().st_size,
                          "firmwareBytes": found["firmware"].stat().st_size,
                          "mergedSha256": sha256(found["merged"]),
                          "firmwareSha256": sha256(found["firmware"])})
    if not published:
        raise ValueError(f"no board files found in {dist}")
    (out / "boards.json").write_text(json.dumps({"version": version, "boards": published}, indent=2) + "\n")
    (out / ".nojekyll").write_text("")
    return {"version": version, "published": [p["id"] for p in published], "skipped": skipped}


def check_site(out: Path) -> list[str]:
    """What a browser needs: every manifest part resolves, offsets are sane, the page lists every board."""
    errors = []
    data = json.loads((out / "boards.json").read_text())
    for b in data["boards"]:
        for suffix, want_erase in (("", True), ("-update", False)):
            mp = out / "manifests" / f"{b['id']}{suffix}.json"
            if not mp.is_file():
                errors.append(f"missing manifest {mp.name}")
                continue
            m = json.loads(mp.read_text())
            if m.get("new_install_prompt_erase") is not want_erase:
                errors.append(f"{mp.name}: unexpected erase prompt")
            for build in m["builds"]:
                if build["chipFamily"] != b["chipFamily"]:
                    errors.append(f"{mp.name}: chip family differs from boards.json")
                for part in build["parts"]:
                    target = (mp.parent / part["path"]).resolve()
                    if not target.is_file():
                        errors.append(f"{mp.name}: {part['path']} does not exist")
                    if part["offset"] != (0 if suffix == "" else APP_OFFSET):
                        errors.append(f"{mp.name}: wrong offset {part['offset']}")
    for need in ("index.html", "app.js", "style.css", "config.json"):
        if not (out / need).is_file():
            errors.append(f"missing {need}")
    return errors


def self_test() -> int:
    boards = load_boards()
    assert any(b["status"] == "verified" for b in boards), "at least one verified board"
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        dist = td / "dist"
        dist.mkdir()
        for b in boards[:2]:  # only two boards have files: the others are skipped, not failed
            (dist / f"skywatch-cyd-9.9.9-{b['id']}-merged.bin").write_bytes(b"\xe9" * 2048)
            (dist / f"skywatch-cyd-9.9.9-{b['id']}-firmware.bin").write_bytes(b"\xe9" * 1024)
        summary = build(dist, td / "site", boards)
        assert summary["version"] == "9.9.9" and summary["published"] == [b["id"] for b in boards[:2]], summary
        assert summary["skipped"] == [b["id"] for b in boards[2:]], summary
        errs = check_site(td / "site")
        assert not errs, errs
        data = json.loads((td / "site" / "boards.json").read_text())
        assert data["boards"][0]["mergedBytes"] == 2048 and len(data["boards"][0]["mergedSha256"]) == 64
        # a mixed-version dist is refused
        (dist / f"skywatch-cyd-9.9.8-{boards[1]['id']}-merged.bin").write_bytes(b"x")
        try:
            build(dist, td / "site2", boards)
            raise AssertionError("expected a refusal for several files")
        except ValueError:
            pass
        # a dist with nothing known is refused
        empty = td / "empty"
        empty.mkdir()
        try:
            build(empty, td / "site3", boards)
            raise AssertionError("expected a refusal for an empty dist")
        except ValueError:
            pass
        # bad registries are refused
        bad = td / "bad.json"
        bad.write_text(json.dumps({"boards": [{**boards[0], "status": "great"}]}))
        try:
            load_boards(bad)
            raise AssertionError("expected a refusal for a bad status")
        except ValueError:
            pass
    print("build_site self-test OK")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dist", type=Path, default=ROOT / "dist")
    ap.add_argument("--out", type=Path, default=ROOT / "build" / "site")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    try:
        summary = build(a.dist, a.out, load_boards())
    except ValueError as e:
        print(f"refused: {e}", file=sys.stderr)
        return 1
    errors = check_site(a.out)
    for e in errors:
        print(f"site problem: {e}", file=sys.stderr)
    print(f"site for {summary['version']} in {a.out}: boards {', '.join(summary['published'])}"
          + (f" (no files for: {', '.join(summary['skipped'])})" if summary["skipped"] else ""))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
