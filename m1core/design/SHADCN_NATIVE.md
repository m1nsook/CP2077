# m1core native design system — shadcn-inspired M5

## Design contract

The user's \`m1core_mod_settings.html\` establishes the information architecture.
The user's later dark settings screenshot and preference for **shadcn/ui** establish
the visual direction. Neither reference is a website to embed in Cyberpunk.

- **Native RED4ext + Dear ImGui only**; no React, CSS, Tailwind, WebView, JS
  runtime, network requests, or new gameplay dependencies.
- Dear ImGui owns widget IDs, focus and input dispatch. \`ImDrawList\` owns the
  final component geometry, fills, borders, text and selected states.
- Design tokens follow shadcn-style semantic roles
  (background, card, border, foreground, muted, accent) rather than layering
  nearly transparent blue overlays on top of the stock ImGui theme.
- Visuals: solid zinc/near-black surfaces, crisp neutral strokes,
  readable text, 8–13px radii, restrained selection blue (#3b82f6).
  No neon bloom, colored glass panels or generic purple theme.
- Typography: bundled **Geist Regular/SemiBold** for primary/headings and
  **JetBrains Mono Regular** for keycaps, numbers and logs. ImGui default font
  is the fail-safe only if packaged font assets are missing.
- The actual user-selectable menu key is always read from the registry.
  No F-key, including the one used in the HTML demo, is hardcoded.

## Native UI primitives (src/ui_components.*)

| Component | Input owner | Render owner |
| --- | --- | --- |
| Button variants (primary, secondary, ghost, danger) | ImGui InvisibleButton | ImDrawList |
| Underlined tab navigation | ImGui InvisibleButton | ImDrawList |
| Mod sidebar row and count | ImGui InvisibleButton | ImDrawList |
| 42 × 24 pill toggle | ImGui InvisibleButton | ImDrawList |
| Sliders with filled track/thumb/value pill | ImGui InvisibleButton, mouse/keyboard | ImDrawList |
| Dropdown shell/arrow | ImGui InvisibleButton + ImGui popup | ImDrawList + popup |
| Solid card sections, labels, separators | ImGui child/table layout | Theme + ImDrawList |
| Text fields and scrollable log view | ImGui InputText/clipper | Solid theme |

The registry remains authoritative; every control calls the existing
Register/Get/Set/Reset APIs rather than maintaining a separate UI JSON state.
The native "Appearance" example is a **styling reference**, not a reason
to invent dummy settings such as language or brand colors that the framework
does not actually support.

## Licensing and packaging

CI downloads fonts from **pinned upstream commits**, stages the TTFs and their
respective SIL Open Font License texts together under
\`red4ext/plugins/m1core/fonts/\`, and verifies each font is present in the
**one** game-ready ZIP. No nested ZIPs or overwrites of settings/profiles.
Sources:

- Geist — https://github.com/vercel/geist-font
- JetBrains Mono — https://github.com/JetBrains/JetBrainsMono
- shadcn/ui theme convention — https://ui.shadcn.com/docs/theming

No font binaries are stored in this source branch or fetched at game runtime.

## One combined game test

The player should test **the batch**, not every minor edit: startup input,
menu bind/unbind/close, normal mouse speed and multi-monitor focus, toggles,
live sliders, dropdowns, grouped cards/resets/search, profile operations,
logs, and carrying-weight sample persistence across restart/death. Automated
tests are not a substitute for actual gameplay verification.

Next: UI accessibility beyond mouse/keyboard, native icon assets and
more advanced visual option cards once needed by real registered settings.
