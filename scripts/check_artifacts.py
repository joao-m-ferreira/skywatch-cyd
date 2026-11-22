#!/usr/bin/env python3
"""Guard for release artifacts — the generic half of a tag-driven release pipeline.

A release built from an agent's (or a developer's) working tree can embed values from git-ignored local
files — Wi-Fi credentials, `.env` tokens. The only safe producer is a clean CI checkout, and even there a
packager can read a secret it was handed. This checker refuses a `dist/` that:

  * has an artifact with no version stamp (neither in its name nor in its bytes), or
  * contains a value taken from a git-ignored local config file (`.env*`, `*credentials*`, `*secret*`,
    `*.local.*`) — also inside zip/tar archives one level deep.

Usage (from the repo root of the CLEAN checkout; `release.yml` calls exactly this):
  check_artifacts.py check     --version 1.2.3 [--dir dist]   refuse a bad dist/
  check_artifacts.py checksums [--dir dist]                    write dist/SHA256SUMS
  check_artifacts.py verify    [--dir dist]                    re-hash every file against SHA256SUMS
  check_artifacts.py --self-test

Stdlib only. Exit 0 = ok, 1 = refused (reasons on stderr).
"""
from __future__ import annotations

import argparse
import hashlib
import re
import subprocess
import sys
import tarfile
import tempfile
import zipfile
from pathlib import Path

SUMS = "SHA256SUMS"
LOCAL_CONFIG_GLOBS = [".env", ".env.*", "*.env", "*credentials*", "*secret*", "*.local.*", "*.local"]
MIN_SECRET_LEN = 8
# Values that are config shape, not secrets: never worth refusing a build over.
BENIGN = {"true", "false", "null", "none", "localhost", "127.0.0.1", "development", "production", "staging"}


def _git(*args: str, cwd: Path) -> str:
    r = subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True)
    return r.stdout if r.returncode == 0 else ""


def local_config_files(root: Path) -> list[Path]:
    """Files that exist in the checkout, match a local-config name AND are git-ignored (a tracked file is
    public by definition; only ignored ones are 'local')."""
    out = []
    for path in root.rglob("*"):
        if not path.is_file() or ".git" in path.relative_to(root).parts or "node_modules" in path.parts:
            continue
        if not any(path.match(g) for g in LOCAL_CONFIG_GLOBS):
            continue
        rel = str(path.relative_to(root))
        if subprocess.run(["git", "check-ignore", "-q", rel], cwd=root).returncode == 0:
            out.append(path)
    return out


_VALUE = re.compile(r"""^\s*(?:export\s+)?["']?[\w.-]+["']?\s*[:=]\s*["']?([^"'\n#]+?)["']?\s*,?\s*$""")
_PAIR = re.compile(r"""["']([\w.-]+)["']\s*:\s*["']([^"']+)["']""")


def secret_values(files: list[Path]) -> set[str]:
    """Right-hand sides of KEY=value / key: value / "key": "value" lines, long enough to be a secret."""
    vals: set[str] = set()
    for f in files:
        try:
            text = f.read_text(errors="replace")
        except OSError:
            continue
        for line in text.splitlines():
            found = [m.group(1) for m in [_VALUE.match(line)] if m] + [v for _, v in _PAIR.findall(line)]
            for v in found:
                v = v.strip()
                if len(v) >= MIN_SECRET_LEN and v.lower() not in BENIGN:
                    vals.add(v)
    return vals


def _blobs(path: Path) -> list[tuple[str, bytes]]:
    """The artifact's bytes plus, for zip/tar, each member's bytes (compression hides plain text)."""
    data = path.read_bytes()
    blobs = [(path.name, data)]
    try:
        if zipfile.is_zipfile(path):
            with zipfile.ZipFile(path) as z:
                blobs += [(f"{path.name}:{n}", z.read(n)) for n in z.namelist() if not n.endswith("/")]
        elif tarfile.is_tarfile(path):
            with tarfile.open(path) as t:
                for m in t.getmembers():
                    if m.isfile():
                        blobs.append((f"{path.name}:{m.name}", t.extractfile(m).read()))  # type: ignore[union-attr]
    except (zipfile.BadZipFile, tarfile.TarError, OSError) as e:
        # a damaged archive is still scanned as raw bytes (blobs[0]); say so instead of hiding it
        print(f"note: {path.name}: archive members not readable ({type(e).__name__}); scanned as raw bytes",
              file=sys.stderr)
    return blobs


def artifacts(dist: Path) -> list[Path]:
    return sorted(p for p in dist.rglob("*") if p.is_file() and p.name != SUMS)


def check(dist: Path, version: str, root: Path) -> list[str]:
    problems: list[str] = []
    files = artifacts(dist)
    if not files:
        return [f"{dist}: no artifacts — a release with nothing to publish"]
    vbytes = version.encode()
    secrets = secret_values(local_config_files(root))
    for f in files:
        blobs = _blobs(f)
        if version not in f.name and not any(vbytes in b for _, b in blobs):
            problems.append(f"{f.name}: no version stamp {version!r} in its name or its bytes")
        for label, blob in blobs:
            for s in secrets:
                if s.encode() in blob:
                    # never print the value: this output lands in a public Actions log
                    problems.append(f"{label}: contains a value from a git-ignored local config file "
                                    f"(len {len(s)}, sha256 {hashlib.sha256(s.encode()).hexdigest()[:8]})")
                    break
    return problems


def _sha(p: Path) -> str:
    h = hashlib.sha256()
    with p.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def write_checksums(dist: Path) -> int:
    lines = [f"{_sha(p)}  {p.relative_to(dist)}" for p in artifacts(dist)]
    (dist / SUMS).write_text("\n".join(lines) + "\n")
    return len(lines)


def verify_checksums(dist: Path) -> list[str]:
    sums = dist / SUMS
    if not sums.is_file():
        return [f"{dist}/{SUMS}: missing"]
    listed, problems = {}, []
    for line in sums.read_text().splitlines():
        digest, _, name = line.partition("  ")
        listed[name] = digest
    for p in artifacts(dist):
        name = str(p.relative_to(dist))
        if name not in listed:
            problems.append(f"{name}: not listed in {SUMS}")
        elif listed.pop(name) != _sha(p):
            problems.append(f"{name}: checksum mismatch")
    problems += [f"{n}: listed in {SUMS} but missing" for n in listed]
    return problems


def self_test() -> int:
    with tempfile.TemporaryDirectory() as d:
        root = Path(d)
        subprocess.run(["git", "init", "-q"], cwd=root, check=True)
        (root / ".gitignore").write_text(".env.local\ncreds.local.json\n")
        (root / ".env.local").write_text("WIFI_PASSWORD=planted-wifi-pass-123\nDEBUG=true\nSHORT=abc\n")
        (root / "creds.local.json").write_text('{"token": "planted-json-token-456"}\n')
        (root / ".env.example").write_text("WIFI_PASSWORD=public-placeholder-1\n")  # tracked-style, not ignored
        dist = root / "dist"
        dist.mkdir()

        def run(version="1.2.3"):
            return check(dist, version, root)

        assert run() and "no artifacts" in run()[0]
        (dist / "app-1.2.3.bin").write_bytes(b"firmware v1.2.3 clean")
        assert run() == [], run()                                   # clean, stamped
        assert any("no version stamp" in p for p in run("9.9.9"))   # wrong version
        (dist / "unstamped.bin").write_bytes(b"clean but unversioned")
        assert any("unstamped.bin" in p and "version" in p for p in run())
        (dist / "unstamped.bin").unlink()
        (dist / "app-1.2.3.bin").write_bytes(b"v1.2.3 ssid=x pass=planted-wifi-pass-123")
        bad = run()
        assert bad and "planted-wifi-pass-123" not in " ".join(bad), bad  # refused, value never echoed
        (dist / "app-1.2.3.bin").write_bytes(b"v1.2.3 clean")
        with zipfile.ZipFile(dist / "bundle-1.2.3.zip", "w", zipfile.ZIP_DEFLATED) as z:
            z.writestr("cfg.json", '{"x": "planted-json-token-456"}')
        assert any("bundle-1.2.3.zip:cfg.json" in p for p in run()), run()  # secret inside an archive
        (dist / "bundle-1.2.3.zip").unlink()
        (dist / "tracked-1.2.3.txt").write_text("public-placeholder-1 and abc and true")
        assert run() == [], run()                                   # tracked/example values and short or benign ones pass
        n = write_checksums(dist)
        assert n == 2 and verify_checksums(dist) == []
        (dist / "app-1.2.3.bin").write_bytes(b"tampered v1.2.3")
        assert any("checksum mismatch" in p for p in verify_checksums(dist))
        (dist / "extra-1.2.3").write_text("x")
        assert any("not listed" in p for p in verify_checksums(dist))
    print("check_artifacts self-test OK")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--self-test", action="store_true")
    sub = ap.add_subparsers(dest="cmd")
    for name in ("check", "checksums", "verify"):
        p = sub.add_parser(name)
        p.add_argument("--dir", default="dist")
        if name == "check":
            p.add_argument("--version", required=True)
            p.add_argument("--root", default=".")
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    if not a.cmd:
        ap.print_help()
        return 2
    dist = Path(a.dir)
    if a.cmd == "check":
        problems = check(dist, a.version.lstrip("v"), Path(a.root))
    elif a.cmd == "checksums":
        print(f"wrote {dist / SUMS} ({write_checksums(dist)} files)")
        return 0
    else:
        problems = verify_checksums(dist)
    for p in problems:
        print(f"REFUSED: {p}", file=sys.stderr)
    if not problems:
        print(f"{a.cmd}: ok")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
