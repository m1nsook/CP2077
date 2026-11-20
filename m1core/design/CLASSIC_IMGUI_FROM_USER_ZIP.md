# m1core UI preference — uploaded files.zip (2026-11-20)

The user chose the older interface from `files.zip` **over** the latest
full-screen standard ImGui interface. This is a **UI-only** preference, not a
request to downgrade m1core's functionality or input backend.

The uploaded ZIP has `files/ui.cpp` and older runtime sources.
The preferred UI uses:

- A classic movable, closable Dear ImGui **940×600** settings window
  rather than the game's full-screen viewport
- A simple top search field and horizontal **All Mods, Keybinds,
  Profiles, Logs, About** button navigation
- A **220px** registered-mod sidebar and adjacent settings area
- Standard two-column ImGui settings tables with column headers,
  group labels, dropdowns, sliders, checkboxes, keybind controls,
  reset actions and hover tooltips for descriptions
- Simple inline Save, Save & Close, Close actions

For compatibility the window is capped by the viewport dimensions on
first use. The plugin's currently shipped Geist/JetBrains fonts and
neutral ImGui theme are retained, with window rounding and border
restored to fit the floating-window format.

**Preserved from the current branch** instead of reverting ZIP sources:
- RED4ext/DX12 renderer, game-owned cursor, input/Alt+Tab/Alt+F4 safeguards
- No default hotkey, compact first-run key assignment, foreground-only polling
- Live Registry, settings persistence, group resets, all current mods
- Profile creation/load/rename/delete and in-game diagnostics
- Keybind conflict warnings, capture cancellation and recovery behavior

This batch changes `ui.cpp` layout and `ui_theme.cpp` window styling
only. No backend, cursor, overlay, registration, profiles serialization
or native input files are overwritten by the uploaded ZIP.

**Visual appearance must be verified in-game.** CI confirms compilation
and packaging, not that the UI matches every pixel of the ZIP era.
