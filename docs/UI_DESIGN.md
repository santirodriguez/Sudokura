# Shared UI design

Sudokura uses one dark/light visual vocabulary across Home, About, gameplay,
Settings, Help, Saved Games, Your journey, Pause, Results and SDL-owned dialogs.
The refresh changes presentation, not game rules, saved data or command ownership.

## Owners

- `src/ui_design.h`: semantic palettes, button roles/states and label spacing.
- `src/sudokura_sdl/ui_design_draw.h`: original 24-unit vector icons and smooth
  rounded surfaces with an output-pixel coverage fringe.
- `src/sudokura_sdl/ui_action_icons.h`: matching gameplay/library action icons and
  narrow-control label geometry. Icons are omitted before compromising text;
  short-wide Undo/Redo retain their established icon-only presentation.
- `src/sudokura_sdl/ui_shared_style.inc`: adapts existing screen surfaces/focus
  to the shared primitives while retaining the board's semantic ink colors.
- `src/sudokura_sdl/ui_design.inc`: the accepted Home/About painters and their
  measured mobile text cards. These layouts are reused without duplication.
- `src/sudokura_sdl/ui_rollout.inc`: gameplay controls, Settings, hints, More,
  navigation and modal-button presentation, plus library icon adapters.
- `src/geometry.[ch]`: canonical paint, pointer, focus and semantic rectangles.
- Existing screen and modal owners: actual contents, summaries, record previews,
  scroll positions, enabled actions and safe command dispatch.

The composition uses include-local aliases to retain the established board and
screen helpers. One final `app_render_ui` routes the known screens and preserves
modal/overlay order. There is no new event loop, runtime rollout preference or
storage writer. Do not redirect input or persistence through presentation code.
Keep aliases confined to the specific include and immediately undefine them.

## Visual behavior

Home/About remain the reference for navy/indigo dark surfaces and an independently
contrasted light palette. Panels have restrained rims/depth; controls distinguish
primary, secondary, quiet, destructive, disabled, hovered, pressed and focused
states. All icon paths are original; no icon font, image library, remote fetch or
new runtime dependency is needed. Shapes own no retained SDL resources. Text uses
the existing bounded font/texture cache, dirty/wait rendering and reduced-motion
policy; the refresh introduces no continuous animation.

The Sudoku grid, values, notes, visible conflicts, selected peers and hidden-
solution neutrality keep their original owners. Painting reads the same enabled
state as keyboard/touch actions. More and Pause conceal the board; destructive
modal choices retain their existing default cancellation, stable bindings and
one-shot dispatch. Repainting a decision face does not select it. Library rows,
record IDs, paging, saved-board previews, attempt grouping, receipts and recovery
remain unchanged. Help and modal body text keep their existing bounded scrolling.

SDL-owned error/recovery banners, generation panels and dialogs share the new
surfaces. Native bootstrap and renderer-unavailable recovery remain native: an
unavailable renderer cannot safely paint a replacement. This is an intentional
platform fallback, not an additional SDL screen or a claim of visual parity.

Mobile labels keep their validated minimum size and use wrapping only after
single-line fitting fails. The established short-wide toolbar still represents
Undo/Redo with direction icons while semantic labels remain translated. Colors
and hit rectangles never infer correctness or change availability. The original
branding masters, Caja Personal credit, GPLv3 and music/source credits remain.

## Validation and acceptance

`make test` retains core, save, modal, input, accessibility, geometry and token
contrast checks. `make test-ui` also tests all system/action glyphs with the actual
SDL software renderer in dark/light at 1x/2x/3x, label bounds, pressed-state text
fit in EN/ES/CA and canonical compact/regular/mobile gameplay rectangles. The
accepted Home/About text regressions remain enabled. All added headers/fragments
are declared Makefile inputs for incremental rebuilds.

Use `[ui-review]` on a candidate commit to capture actual application pixels.
Inspect gameplay, paused/concealed board, Settings, Help, library/history,
confirmations and error states in both themes and supported compact sizes; verify
Home/About remain consistent. A green structural validator is not aesthetic
acceptance. Diagnostic SDL renders are not Android-device/store screenshots.
The maintainer tested the redesigned Linux app and the source-matched Android
debug prototype and reported successful general visual/functional operation.
This is a smoke acceptance for the unified presentation, not itemized proof of
every interaction or runtime. Full TalkBack, 16 KiB page-size execution,
production update/signing and final release approval remain separate gates.
Do not relabel an older APK or diagnostic screenshot as the final release.
