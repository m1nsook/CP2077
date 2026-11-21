# m1core — Living project status and handoff

> **Canonical development context for new ChatGPT sessions.** Read this document **and the current GitHub branch and PR** before making changes. Update this file in the **same commit or PR update** whenever the verified status, active branch, build, design decision, or next steps change. Do not rely on prior conversations as the only source of truth.
>
> **Last updated:** 2026-11-21 (Europe/Warsaw). This file represents the status at the commit that contains it. Check newer commits, PR comments and CI before assuming it is current.

## Quick links

- **Source repo:** https://github.com/m1nsook/CP2077
- **Active development branch:** [`dev/m1core-cet-foundation`](https://github.com/m1nsook/CP2077/tree/dev/m1core-cet-foundation)
- **Active draft PR:** [#41 — CET-based native m1core core](https://github.com/m1nsook/CP2077/pull/41)
- **This document (stable handoff URL):** https://github.com/m1nsook/CP2077/blob/dev/m1core-cet-foundation/m1core/PROJECT_STATUS.md
- **Developer API:** [`m1core/DEVELOPERS.md`](DEVELOPERS.md)
- **Build and licenses:** [`m1core/core/BUILD.md`](core/BUILD.md)
- **Latest USER-confirmed in-game Windows CI:** [#44907145458](https://github.com/m1nsook/CP2077/actions/runs/44907145458); source commit `153451235f4deccf11b5631deacf50f0287e0e4d`.
- **Latest CI-verified build (native profile tests):** [#44914222641](https://github.com/m1nsook/CP2077/actions/runs/44914222641); source commit `6cbd24fac3721b68241de0c60bfcb5a3145950d4`.
- **Current ZIP**: `m1core-native-x64-dev` (GitHub artifact ID `14711440432`, CI #44914222641). Actions artifacts can expire; do not invent a link or confuse it with older DLLs.

## Goal and nonnegotiable choices

Build **m1core**, a native, standalone **RED4ext x64 DLL** for Cyberpunk 2077, as a mod-development/settings framework on top of a **special MIT-only CET 1.38.1 source archive supplied directly by yamashi**. Use the **working CET input/window/DX12/ImGui implementation as foundation**, not a separately invented imitation. No requirement for a separate CET installation. Preserve the game's input and avoid interference with saves and startup. Use the user's standard Dear ImGui theme, ordinary built-in ImGui widgets rather than custom drawing, and one game-root install ZIP. Do not include CET's icons, fonts, or original ASI loader. Retain relevant original-author MIT and third-party notices. The user will personally handle the Nexus Mods listing and shout-out at release.

Only work on the m1core development branch and draft PR unless explicitly authorized; **do not merge into `main`**, rewrite other mods, or add CET requirements. Avoid asking users to repeat already confirmed input/UI tests. Do not promise background work.

## Current verified behavior (user-tested)

1. **Native CET-derived foundation loads in Cyberpunk 2077 v2.5.** Original donated-runtime UI/console/bindings worked in-game with keyboard and mouse. The native m1core Dear ImGui interface subsequently appeared and worked.
2. **Cloud-sync infinite loop / blocked main menu is resolved (2026-11-21, user-confirmed).** The source-level defect was `core/src/scripting/FunctionOverride.cpp` returning `false` for vanilla native game functions not registered by m1core. The patch unlocks the registry mutex and dispatches those calls to `RealRunPureScriptFunction`. This fixed the game's persistent `Cloud sync in progress...` loop. **Never restore that aborting code path.**
3. Previous development builds established that the ImGui theme and mouse/keyboard inputs are functional. **Do not claim** all graphics modes, third-party APIs or profile behavior are already release-tested.

## Current implementation

- `m1core/core/` contains the native m1core runtime derived from the **hash-verified 131-file** donor source, including CET's connected Win32, raw input, D3D12, hotkey, overlay, Lua and game hooks. `M1CoreRuntime`, `M1CoreVersion`, `red4ext_plugin.cpp`, the m1core settings UI and the REDscript/native registry API are implemented here.
- `m1core/native/registry.cpp`, `registry.hpp`, `state.cpp` and `state.hpp` are data-only settings/state modules compiled into the CET-derived core, not a second input backend.
- The UI has standard tabs: **Settings**, **Keybinds**, **Profiles**, **Logs**, **About**. Stored settings use `red4ext/plugins/m1core/settings.json`. The native API provides `M1Core_Version`, `M1Core_RegisterJson`, `M1Core_GetJson`, `M1Core_SetJson`, `M1Core_ResetSetting`, `M1Core_ResetMod`, `M1Core_SaveProfile`, `M1Core_LoadProfile`, `M1Core_DeleteProfile`.
- `m1core/core/xmake.lua` compiles the active **`m1core.dll`**. Windows workflow: [`.github/workflows/m1core-core.yml`](../.github/workflows/m1core-core.yml). Package structure: `red4ext/plugins/m1core/m1core.dll`, project and yamashi MIT licenses, third-party license notices.
- Prior independent native renderer under `m1core/native` and prior draft PR [#40](https://github.com/m1nsook/CP2077/pull/40) are preserved for history, **not the active input/graphics implementation**. PR #41 is the active integration branch.

## Open work (not yet validated)

- **Registry + profiles (native CI VERIFIED; game verification pending):** loading profiles now restores values for offline/unregistered mods, preserves newer fields, and rejects malformed/invalid registered values without partial changes. New tests cover save/load, late registration, restart, invalid profiles, and persistence. Source commits [`8329e52`](https://github.com/m1nsook/CP2077/commit/8329e525cf70174f4689e197bae433dd57fe3130) and [`6cbd24f`](https://github.com/m1nsook/CP2077/commit/6cbd24fac3721b68241de0c60bfcb5a3145950d4).
- **Windows CI PASS:** [#44914222641](https://github.com/m1nsook/CP2077/actions/runs/44914222641) passed the full native DLL compile, **both registry and state test targets**, license/asset checks, and single game-root ZIP packaging. GitHub Actions artifact ID `14711440432`, artifact name `m1core-native-x64-dev`, ZIP SHA256 `7f438bbbeb3e5ed9f959ce668fd81ff08361f3bb88599add626a406cbf95c5c6`. The verified ZIP includes only `red4ext/plugins/m1core/m1core.dll` (SHA256 `55f6b76f9b1f7e15e1fec92c1ef56089482e3981f2a8c1320b8da864e79c75e0`) and the three required licenses/notices. **No new in-game profile test yet.** The previous CI #44913706268 failed only because the standalone test targets lacked `NOMINMAX`; that build-target issue is corrected and superseded by this successful run.
- **Developer integration:** provide a small, real consuming mod/fixture that registers settings through native RTTI/REDscript and verifies registration, UI changes and reloads. At present the UI can show zero mods because no consumers are installed; this alone is not a bug.
- **Lifecycle and compatibility:** test persistence and profile restore in-game after restart. Avoid touching the game-tested input/draw/hotkey and cloud script pass-through except to fix a reproduced regression.
- **Release:** do not publish or merge until user approves; retain MIT/third-party licenses and no restricted assets. This project is still in development.

## Instructions for an assistant continuing this project

Read this file, [the active PR](https://github.com/m1nsook/CP2077/pull/41), `m1core/DEVELOPERS.md`, current branch head, and latest Windows GitHub Actions results first. **Use GitHub connector to actually edit/push code** and get an actual Windows build; don't respond with a speculative plan or unrelated changes. Preserve known good CET-derived input/DX12 implementation and the pass-through fix. Add tests when modifying settings/persistence. Use a draft PR; don't merge or touch unrelated mods. Update this document in the same work session with honest verification status, commit SHA, CI run and remaining issues. Never call CI green proof of in-game success.

## Copy/paste prompt for a new chat

> Continue development of **m1core** in `m1nsook/CP2077`. **Read the canonical handoff first:** https://github.com/m1nsook/CP2077/blob/dev/m1core-cet-foundation/m1core/PROJECT_STATUS.md and active draft PR #41 https://github.com/m1nsook/CP2077/pull/41. This is a native RED4ext DLL built **on yamashi's MIT-only CET 1.38.1 source package**; do not replace/reinvent the working CET-based input, rendering, window or cursor lifecycle; keep original licenses and exclude donor icons/fonts/ASI. User confirmed keyboard/mouse/interface work and cloud-sync infinite loop is gone after native-script pass-through patch `1534512`. Continue substantive developer integration and profile persistence in-game validation, push to `dev/m1core-cet-foundation`, run Windows CI, keep PR draft, don't merge or touch other mods. Update `m1core/PROJECT_STATUS.md` with actual new results.
