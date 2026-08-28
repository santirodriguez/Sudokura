# Packed branding source assets

Two supplied web-icon variants are preserved here as chunked Base64 text so their original bytes remain reproducible without altering or recompressing the artwork.

| Asset | Dimensions | Bytes | SHA-256 |
|---|---:|---:|---|
| `apple-touch-icon.png` | 180x180 | 19,584 | `2ab16a34551772a520cf3e462a1bd9e8db2ad2a8f78363061aa969c5621914e6` |
| `android-chrome-192x192.png` | 192x192 | 21,142 | `0991568b17ea2a007ccccb4ad4ef8c424d817cda6c07bd5802e69d022fd53729` |

For each asset, concatenate its `part-*.b64` files in lexical order and Base64-decode the result. `scripts/validate_assets.py` performs this reconstruction in memory and verifies dimensions, byte length, and SHA-256.

The other supplied branding files remain directly under `../source/`.
