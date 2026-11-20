# M1CORE UI target: sharp-edged colored toolbar (2026-11-20)

**User's reference:** image of the M1CORE 1.4.1 mockup with a white title
strip, black CLOSE button, black workspace, left registered-mods sidebar,
search and strongly color-coded SETTINGS / KEYBINDS / PROFILES / LOGS /
ABOUT buttons.

The user clarified that the image's **background blur was applied in
Photoshop**. It is not part of the requested game UI, and must not be
implemented with DirectX shaders, RenderTargets, game screenshots, or any
other rendering effects.

## Native implementation

- A centered native ImGui window, up to 1100×625px, with square corners
  and a thin gray border, clamped to fit the current game viewport.
- A 46px white title strip with real version text (v0.5.0-dev) and a
  black native CLOSE button.
- A 53px black navigation strip, with native InputText search and
  native rectangular buttons: blue SETTINGS, white KEYBINDS, green
  PROFILES, amber LOGS, and orange ABOUT.
- A fixed 224px black mod sidebar using normal ImGui Selectable rows.
  Inactive names are muted; selected names are white and decorated by a
  yellow arrow. No custom mouse handling or invisible hitboxes.
- The rest of the window displays the **real mod settings registry**
  using the existing built-in ImGui tables, sliders, toggles, combos
  and keybind controls; the mockup's empty black region is not shipped.
- A small dark footer preserves Discard, Save, Save & Close and Close.
- Geist and JetBrains Mono fonts and merged Font Awesome icons are kept
  from existing packaging; color, padding and square geometry are
  configured through ImGuiStyle / PushStyleColor.

The compact first-run key selection remains unchanged. All of m1core's
engine cursor, input capture, focus, startup, shutdown, persistence,
profiles and diagnostics backend remains unchanged by this visual pass.

**No new background blur, no input rewrites, no new menu shortcuts.
The color-coded styling is visual only.**

## Signoff

CI validates C++ compilation, unit tests and a single game-ready ZIP.
Only the user can verify actual in-game typography, spacing and visual
fidelity. Do not claim the mockup is pixel-perfect before that check.
