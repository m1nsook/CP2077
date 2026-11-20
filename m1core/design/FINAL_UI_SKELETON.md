# Authoritative m1core UI — user's final skeleton and HTML (November 20, 2026)

**Use the user's own \`m1core-skeleton(1).zip\` as the implementation
reference and its \`final.html\` as the visual target.** The previous
floating-window redesigns were rejected. Do not reinterpret the target as a
movable Dear ImGui "cheat menu" or replace its palette with blue accents.

The user supplied:
- \`m1core-skeleton/main.cpp\` — already-built native Dear ImGui C++ UI,
  containing custom controls, explicit header/nav/sidebar/content/footer,
  exact colors, typography and font/icon loading
- \`m1core-skeleton/final.html\` — the authoritative finished visual layout
- \`m1core-skeleton/assets/\` — Geist Regular/Medium/SemiBold,
  JetBrains Mono and Font Awesome solid font references
- screenshots of the Visual Studio skeleton and the intended HTML design

## Implementation boundary

**NCREC is the proven fullscreen layout precedent**, not the source for
a new design theme. m1core uses the same fixed-region geometry strategy:
- Fullscreen native game viewport; no 940px floating window and no overlay
  that can be dragged, resized or maximized
- 56px solid \`#070709\` top header: app name/version, global search, status
- 44px \`#09090b\` navigation with icons, badges and white underline
- 240px \`#0a0a0d\` sidebar for Mods/Keybinds with persistent revision notice
- Centered maximum 940px scrollable main settings pane; no fake opaque glass
- One continuous group-card \`#111114\` with \`#18181b\` group headings,
  subtle \`#27272a\` separators, per-row descriptions and right controls
- Fixed 56px black footer with actual Save/Discard/Close functionality
- No CSGO-styled windows or detached configuration panels

The real registry remains authoritative. Do not copy the user's **standalone
demo state** (\`M1C::g.mods\`, dummy profiles/logs, GLSL/GLFW main loop, etc.)
or replace the RED4ext DX12 renderer. Integrate the user's view/components
with \`Registry\`, \`Diagnostics\`, registered settings and native keybind
capture so all live changes and restarts still work.

## Component/theme parity

- \`#09090b\` primary canvas, \`#0c0c0e\` surface, \`#111114\` card,
  \`#18181b\` muted, \`#27272a\` border
- White \`#fafafa\` primary actions, **not blue**, and white on/off switches,
  sliders, selected tabs; amber \`#fbbf24\` only for action-required/dirty
- Native \`ImDrawList\` widgets retaining working input events
- Geist Regular 14px, Medium 14px, SemiBold headings; JetBrains Mono
  12.5px technical text; Font Awesome solid glyph atlas for icons
- Fonts resolved from game-root \`red4ext/plugins/m1core/fonts\`, not a
  Visual Studio / GLFW relative \`assets\` working-directory path
- One GitHub Actions ZIP with game-root \`r6/\` and \`red4ext/\` folders,
  including licensed fonts/icons, **no nested ZIPs**

## Input safety

CET uses Cyberpunk's \`InputSystemWin32Base::ForceCursor\` engine path.
m1core still has the Win32 system-cursor path + RAWINPUT fallback, which is
NOT equivalent (raw deltas lack pointer acceleration). The earlier slow
pointer problem must NOT be masked by further invented gain multipliers.
Treat proper engine cursor ownership as a separate verified integration;
do not break startup passthrough, focus, Alt+F4, key up, capture or the
second monitor to reproduce CET's input without validation.

The user's key is always selected and persisted; the C++ reference's
standalone demo states and F-key examples do not establish default binds.
Use one substantial release candidate and request one combined in-game
regression rather than individual micro-fix installations.
