# Drawables — provenance

`ph_*.xml` are vector drawables derived from **Phosphor Icons**, not drawn here.

| file | upstream asset | weight | note |
|---|---|---|---|
| `ph_microphone.xml` | `assets/duotone/microphone-duotone.svg` | duotone | the active-state fill |
| `ph_gear.xml` | `assets/regular/gear.svg` | regular | |
| `ph_caret_down.xml` | `assets/regular/caret-down.svg` | regular | the layout chip's ▾ |

## Source

- Project: <https://github.com/phosphor-icons/core>
- Version: **2.1.1**, fetched from `https://unpkg.com/@phosphor-icons/core@2.1.1/`
- Licence: **MIT** — the full text is in `LICENSE.md` beside this file, and the
  same terms are recorded in `debian/copyright`.

## Method

One step, mechanically: the `viewBox="0 0 256 256"` became
`android:viewportWidth/Height`, `fill="currentColor"` became an explicit
`#FF000000` (the toolbar tints in code, so the drawable must not carry a colour
of its own), and the duotone file's `opacity="0.2"` became `android:fillAlpha`.
The `d` attributes are the upstream ones, unedited — Phosphor's geometry is the
thing worth having, and re-typing it by hand is where a licensed icon becomes a
lookalike of one.

To regenerate after an upstream bump:

```sh
curl -sS -o /tmp/m.svg https://unpkg.com/@phosphor-icons/core@<version>/assets/<weight>/<name>.svg
```

then apply the three substitutions above. Nothing in the toolbar's layout logic
depends on any particular icon; only `bind()` maps the ids to states.

## What these are not

Not a vendored copy of the icon set — only the three glyphs the strip uses. No
font is shipped; a font would mean a licence to carry and a tool to subset.