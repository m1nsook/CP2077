# m1core interface reference (user-supplied HTML, Nov 20 2026)

Source: user-uploaded **m1core_mod_settings.html** (the actual user
design, not the earlier layout-only demo.html). Do not substitute a browser
HTML/JavaScript/CSS runtime for the game's native RED4ext + Dear ImGui UI.

### Visual language

- Geist primary; JetBrains Mono for keycaps, stats and logs
- Font Awesome icons in the reference; Lucide acceptable as an alternative
  in the native build after licensing/asset integration
- Deep canvas #09090b; surface #0f1014; elevated #14161c; hover #1b1e26
- Accent #38bdf8 and restrained cyan glow; amber #f59e0b for warnings
- Primary text #fafafa, secondary #a1a1aa, muted #71717a
- Rounded glass-like panels, quiet 1px borders, subtle depth, compact density
- Slider fill and circular thumb; pill toggle switches; inset keycap labels
- Light UI transitions and discrete hover states, not distracting animations

### Information architecture

1. Top header: framework name/version, global mod and settings search
2. Navigation: All Mods, Keybinds, Profiles, Logs, About
3. Registered Mods sidebar with counts and selected state
4. Main content: setting groups in elevated cards, descriptive labels,
   sliders + values, toggles, dropdowns, reset per-setting/group/mod
5. Keybinds: explicit per-user recording and clearing, conflict warning
6. Profiles: save/load/overwrite/delete/rename
7. Logs: severity + source search, copy/clear, mono formatting
8. About: framework status, selected menu key, emergency option, disk paths
9. Fixed bottom footer: dirty state, Save, Save & Close, Close

### Behavior retained

- The UI is native, not a web view. No web/CDN runtime dependencies.
- All changes apply live and survive load/death/restart through existing APIs.
- No default keyboard shortcuts, including for m1core's own menu.
- A user's menu key opens and closes. Never swallow the corresponding
  key-up or OS close shortcuts; release cursor and capture on close.
- The original HTML contains example F10 values, **not a new default key**.

The native M5 UI batch now includes grouped setting cards with visible
per-setting descriptions, cyan pill toggle switches, responsive sidebar,
search clearing, global keybind-conflict warnings, safer first-run key
selection, readable About diagnostics (including pointer mode), and profile
selection improvements. Native slider/combobox rendering remains ImGui-based.

**Still future work:** embedded icon atlas integration (Lucide/Font Awesome),
more advanced slider styling, animation refinement, and broader controller / IME
accessibility. This implementation is a testable native ImGui UI, not a webview.
Only CI has checked these new controls; player visual/runtime testing remains open.
