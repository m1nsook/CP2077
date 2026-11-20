# M5 UI direction: native ImGui controls

**Decision:** The player discontinued the custom ImDrawList control experiment after
repeated visual and input regressions. Do not resume pixel-perfect shadcn
or HTML cloning unless explicitly requested.

- Preserve the functional fullscreen workspace, real registry, settings,
  named profiles, logs, mod list and player-selected keybinds.
- Use official Dear ImGui controls for checkboxes, sliders, combo boxes,
  navigation selection and buttons. No InvisibleButton + custom control
  drawing or custom mouse movement for ordinary controls.
- A styled dark theme remains supported through ImGuiStyle, custom
  ImGuiCol values, spacing, rounding and padding.
- Geist Regular/Medium/SemiBold, JetBrains Mono and the OFL Font Awesome
  glyph atlas remain bundled in the existing game ZIP. These fonts and
  icons do **not** require custom widgets.
- The first-run setup is a small centered ImGui window to assign a menu key.
  On successful key assignment, save immediately and dismiss the overlay.
  The full-screen settings workspace is opened later using that key.
- During active input capture, draw a native Dear ImGui software pointer at
  the physical Windows mouse coordinates. The game may hide its OS cursor
  even when m1core has successfully requested engine cursor ownership.
  NEVER modify mouse gain or Windows pointer speed as a workaround.
- On focus loss, do not draw/capture pointer and never query global
  keyboard state to assign bindings from another application.
- Continue to preserve Ctrl+Alt+Delete, Alt+F4, normal game shutdown,
  and second-monitor focus recovery. In-game signoff required.

No new default function keys. The user-selected binding persists in
settings.json. This simplification intentionally favors reliability and
maintainability over pixel-perfect custom UI.
