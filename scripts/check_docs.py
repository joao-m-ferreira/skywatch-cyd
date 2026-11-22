#!/usr/bin/env python3
"""Validate the knowledge base: required docs exist and relative links resolve."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REQUIRED = ["CONTRIBUTING.md", "README.md", "docs/ARCHITECTURE.md", "docs/core-beliefs.md", "docs/PRD.md"]
LINK = re.compile(r"\]\((?!https?:|#|mailto:)([^)#\s]+)")


def main() -> int:
    errors = [f"missing {p}" for p in REQUIRED if not (ROOT / p).is_file()]
    for md in [*ROOT.glob("*.md"), *ROOT.glob("docs/**/*.md")]:
        for target in LINK.findall(md.read_text(encoding="utf-8")):
            if not (md.parent / target).exists():
                errors.append(f"{md.relative_to(ROOT)}: broken link {target}")
    for e in errors:
        print(e, file=sys.stderr)
    print("docs ok" if not errors else f"{len(errors)} docs problem(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
