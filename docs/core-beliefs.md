# Core beliefs
1. Never weaken a check to reach green.
2. No secrets in the repo; credentials only via git-ignored files.
3. The hardware pinout is defined once, in `board.h`.
4. Display and parsing logic stays host-testable, free of Arduino headers.
5. Validate service JSON at the boundary; no network in unit tests.
