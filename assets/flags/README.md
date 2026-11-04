# Vendored language flags

Sudokura vendors three flag assets for its offline language selector:

- `source/us.svg` — United States flag for English.
- `source/ar.svg` — Argentina flag for Español.
- `source/es-ct.svg` — Catalan Senyera for Català.

Source: `lipis/flag-icons` (`flags/4x3/`), retrieved for Sudokura v1.2.0 on 2026-08-27.
Upstream repository: https://github.com/lipis/flag-icons
License: MIT; see `LICENSE-MIT.txt`.

The files in `raster/` are 96x72 PNG technical rasterizations of those exact
SVG sources. They are vendored so asset generation and runtime remain fully
offline and do not require an SVG rendering dependency.
