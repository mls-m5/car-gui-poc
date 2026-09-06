# Replace textual vehicle warnings with graphical symbols

## Goal

Replace textual labels for standard automotive indicators with stable,
portable NanoVG vector symbols. Avoid icon fonts and emoji because glyph
coverage and appearance differ between native and WebAssembly builds.

## Implementation

- Replace UTF-8 text arrows with filled NanoVG turn-signal arrows.
- Replace the `LIGHTS`/`HIGH BEAM` text with a headlamp outline and three beam
  lines. Use cyan for high beam, white for normal headlights, and a dim symbol
  while inactive.
- Replace the bottom `BELT`, `BRAKE`, `TIRE`, `BATTERY`, and `!` warning texts
  with conventional vector indicators:
  - seated person and diagonal belt;
  - circled `P` with brake arcs;
  - tire outline with an exclamation mark;
  - battery outline and terminal with an exclamation mark;
  - warning triangle with an exclamation mark.
- Preserve fixed warning slots so indicators never shift as states change.
- Keep inactive symbols faint and active symbols amber or red as appropriate.
- Draw only with NanoVG GLES2-compatible primitives.

## Validation

- Build and run native tests.
- Launch the graphical application and inspect active and inactive symbols.
- Build the containerized Emscripten target to ensure all symbols are portable.
- Run `clang-format` and `git diff --check`.
- Move this task to `tasks/done/` after validation.
