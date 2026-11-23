# Livery references

Local reference images for improving the 3D liveries. **The images themselves are git-ignored**
(they belong to their photographers / publishers and must not be redistributed from this repo);
only the index and the tools are committed.

- `photos/` one planespotters.net thumbnail per airline (`<ICAO>.jpg`), plus `index.json`
  (airline code, the registration photographed, its ICAO type, photographer). Re-create them with
  `python3 scripts/fetch_reference_photos.py` (uses your `PLANESPOTTERS_CONTACT` from `secrets.h`).
- `pasted/` livery charts and photos shared while developing (infographics, tail charts, per-airline
  photos). Local only.

Compare a photo with the firmware's render: `python3 scripts/compare_liveries.py` writes
`build/sheets/sheetN.png` (photo on the left, 3D render of the same airline and type on the right).
Photo credit on screen goes to each photographer (see the README).
