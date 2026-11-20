# m1core standard ImGui styling — visual quality pass

The user chose **standard Dear ImGui controls** after multiple unstable
custom-drawn iterations. This update keeps that decision.

What has been improved, without changing input ownership or live settings:
- Geist UI font 16px, medium 16px, headings 23px; JetBrains Mono 14px;
  Font Awesome glyphs merged into the main Geist atlas at 15px
- Selected navigation tabs retain Dear ImGui Selectable for interactions
  but use an underline instead of a large solid selected rectangle
- Sidebar rows use actual ImGui Selectable interactions with independent
  left-side names and right-aligned counts; no custom invisible hit-testing
- Standard button variants now have distinct white primary, neutral secondary,
  transparent-ish ghost and danger appearances, all drawn by ImGui::Button
- Consistent right-aligned Checkbox/SliderScalar/BeginCombo/keybind controls,
  generous setting row height, larger section headings
- Fixed header/navigation separators, expanded sidebar status card and
  slightly wider centered settings workspace
- Retained the compact first-time keybind dialog and engine-owned mouse
  cursor path from the previous build

The actual settings widgets stay native; there is no HTML renderer, custom
cursor movement, external UI runtime, default menu shortcut, or changed
profile/persistence behavior.

**CI cannot verify actual font appearance, UI contrast, game focus, shutdown
or mouse feel. These require one combined in-game test.**
