# m1core — CET-derived RED4ext DLL build

This branch is a **source port**, based on yamashi's specifically supplied MIT-only CET 1.38.1 archive (original source snapshot in Git history). It is **not** the independent `m1core/native` renderer. The original source tree, including donated changes, was verified byte-for-byte before port modifications.

The build uses CET's original xmake dependency graph, D3D12/Win32 backend, overlay, VKBindings, GameMainThread hooks and Lua scripting subsystem, now initialized from `RED4ext::v1::Main`. Its standalone target is `m1core.dll`, not `.asi`. RED4ext.SDK headers must be provided at `vendor/RED4ext.SDK/include` (the donated ZIP included the empty directory). Windows CI provides the pinned SDK, then runs xmake.

Restricted donor CET font/icon assets and the ASI loader are explicitly excluded. The supplied ImGui style at `m1core/native/ImGuiCustomTheme.h` is preserved, while the **independently licensed** Karla, JetBrains Mono and Phosphor fonts are loaded through ImGui's normal font atlas. Paths are isolated under `red4ext/plugins/m1core`, not any existing CET installation.

The donated source archive omitted the original CET Lua bootstrap scripts (`json/json`, `IconGlyphs/icons`), so references to those missing scripts are disabled. The embedded Lua VM remains part of the foundation; mod-specific Lua scripts are not bundled.

A clean compile **does not** prove gameplay input, focus, all display modes, or runtime compatibility with Cyberpunk 2.50. Do not merge or ship a release until those are confirmed.


### m1core application UI

The CET-derived runtime now hosts the original m1core native registry and REDscript settings API. The main UI is a **single Dear ImGui settings window**, with tabs for Settings, Keybinds, Profiles, Logs and About; controls use stock ImGui widgets and the supplied ImGui theme. CET-only developer panels (Console/TweakDB Editor/Game Log/ImGui Debug/Reload All Mods) are no longer presented in the main interface. Keybinding still uses the original donor widget and raw input implementation; its internal persisted `cet` binding namespace remains temporarily unchanged to avoid deleting existing users' shortcuts. This is internal compatibility, not user-visible branding. The old independent D3D12 renderer is not compiled or used.

The active runtime source is now under `m1core/core` (formerly `m1core/cet-foundation`). It uses an m1core-named runtime class, M1CoreVersion configuration header, and stable RED4ext `Main` entry. The original gifted source and its hash-verified 131-file snapshot remain in the earlier commits on this branch. No restricted CET fonts/icons, ASI loader, or unrelated mods are shipped. The new separately licensed Karla/JetBrains Mono/Phosphor fonts are explicitly permitted and documented below.

### Windows CI dependencies

Windows CI caches the local xmake package/build cache between runs. It compiles the actual renamed native runtime (including the m1core registry UI), verifies the original author's licensing notices, and produces a single game-root install artifact only on successful compilation. A compiled DLL is not a substitute for an in-game check.


### Cyberpunk 2.50 menu/cloud-sync investigation

The last in-game build displayed the m1core window but, with the DLL loaded, the game's "Cloud sync in progress" notice never completed and gameplay input remained blocked. Removing the DLL immediately removed the problem (reported by the user). The earlier CET-derived Console displayed repeated `Function appears to be hooked but isn't?` messages.

Found a concrete defect in the gifted runtime's `FunctionOverride::HookRunPureScriptFunction`: it treated *every* game script function with `isNative` set as if it belonged to the m1core override table and, if absent, returned `false`. The global hook was therefore aborting ordinary game script calls that m1core never registered. We now unlock and forward unregistered calls to `RealRunPureScriptFunction`, leaving registered Lua overrides and the original CET input/DirectX systems unchanged. The Windows CI includes a fail-closed source guard for this case.

**Root cause attribution:** the early script-abort behavior is directly confirmed by source. Whether it alone explains the cloud-sync loop still requires validation in Cyberpunk. Steam cloud settings and the user's saves are not modified.


## Bundled UI typography and icon font

The **single** native game-root ZIP now includes official independently licensed
fonts in `red4ext/plugins/m1core/fonts/`: Karla Regular and JetBrains Mono
Regular (SIL Open Font License 1.1), and Phosphor Icons regular (MIT).
Individual licenses are shipped next to each font file. All three files are
fetched by the Windows packaging job from **pinned upstream commits** of
`googlefonts/karla`, `JetBrains/JetBrainsMono`, and `phosphor-icons/web`.
Nothing is copied from CET's restricted visual assets; no CET .asi is bundled.

Only the standard Dear ImGui font atlas is used. The `m1core.dll` handles
loading these fonts on ImGui initialization, including ordinary fallback fonts
and plain-text labels if files are missing. The existing HUD/input implementation
remains unchanged. This is a source/build verification detail; actual game
visuals need a user check.
