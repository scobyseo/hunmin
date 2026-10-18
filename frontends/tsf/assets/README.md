# Hunmin icon assets

## Provenance

- **Source image** (`king-src.jpg`, 1271×1268): the King Sejong illustration at
  https://www.urbanbrush.net/web/wp-content/uploads/edd/2022/09/urbanbrush-20220919233634326231.jpg
  (어반브러시 / Urbanbrush), retrieved 2026-10-18 with curl and a browser
  user agent after the bare fetch hit the site's bot wall.
- **Licence term taken from the site's own terms page**
  (https://www.urbanbrush.net/ready/, 이용약관[무료일반]): watermark/logo
  removal REQUIRES the credit "출처: 어반브러시" (online use). The source
  attribution is recorded here and in `debian/copyright` (shortcut/resource
  icons count as online distribution artefacts — that record is the stated
  credit). If the maintainer prefers a no-terms-tracked icon, replace the
  asset; the swap is tiny (`hunmin.ico` + `hunmin-tsf.rc` reference).
- **What was excluded and by how much**: the five pink mugunghwa flowers and
  the watermark badge at the illustration's bottom (crop 200..1070 x,
  20..890 y of the source) — an icon must carry one silhouette.
- **Method**: Pillow 13.0.0 (`hunmin-256.png` preview; `hunmin.ico` stores
  256/64/48/32/24/16 from the same 512-px circle render).
- **What this artefact does NOT represent**: it is not a redrawn engine
  identity — the king bust is the sourced illustration inside a 10-px golden
  ring, circular mask at 4x downsampled to 512.

## Files

- `hunmin.ico` — the multi-size icon (resource id 1 of the TIP DLL; the
  registration profile icon and the language-bar button both point at the
  serving DLL's own resources, so no separate icon file ships).
- `hunmin-256.png` — the 256-px preview/source of the same render.
- `king-src.jpg` — the retrieved source image, kept for the provenance chain.
