# m1core — CET-derived RED4ext DLL build

This branch is a **source port**, based on yamashi's specifically supplied MIT-only CET 1.38.1 archive (original source snapshot in Git history). It is **not** the independent `m1core/native` renderer. The original source tree, including donated changes, was verified byte-for-byte before port modifications.

The build uses CET's original xmake dependency graph, D3D12/Win32 backend, overlay, VKBindings, GameMainThread hooks and Lua scripting subsystem, now initialized from `RED4ext::v1::Main`. Its standalone target is `m1core.dll`, not `.asi`. RED4ext.SDK headers must be provided at `vendor/RED4ext.SDK/include` (the donated ZIP included the empty directory). Windows CI provides the pinned SDK, then runs xmake.

Restricted CET icon/font assets and the ASI loader are explicitly excluded. The supplied standard ImGui style at `m1core/native/ImGuiCustomTheme.h` is applied, and the font atlas uses Dear ImGui's default font only. Paths are isolated under `red4ext/plugins/m1core`, not any existing CET installation.

The donated source archive omitted the original CET Lua bootstrap scripts (`json/json`, `IconGlyphs/icons`), so references to those missing scripts are disabled. The embedded Lua VM remains part of the foundation; mod-specific Lua scripts are not bundled.

A clean compile **does not** prove gameplay input, focus, all display modes, or runtime compatibility with Cyberpunk 2.50. Do not merge or ship a release until those are confirmed.
