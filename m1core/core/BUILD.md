# m1core — standalone RED4ext DLL build

**m1core is an independent project**: it is built as a native RED4ext
`m1core.dll`, not an ASI module, and neither the build nor players need a
separate CET installation. Its connected game window, Win32/DX12 backend,
ImGui, raw keyboard/mouse integration and LuaJIT engine incorporate
code directly donated and authorized for use in m1core. The original
technical source snapshot remains in Git history for developers, but
m1core does not adopt its branding, UI, releases or package identity.

The Windows build uses xmake, with RED4ext.SDK headers at
`vendor/RED4ext.SDK/include` supplied by a pinned CI checkout.
The independently licensed Karla, JetBrains Mono and Phosphor fonts are
loaded by Dear ImGui. Restricted original-project icons/fonts and its
ASI loader are not packaged. Installation remains under
`red4ext/plugins/m1core/`.

The donated source archive omitted the original CET Lua bootstrap scripts (`json/json`, `IconGlyphs/icons`), so references to those missing scripts are disabled. The embedded Lua VM remains part of the foundation; mod-specific Lua scripts are not bundled.

A clean compile **does not** prove gameplay input, focus, all display modes, or runtime compatibility with Cyberpunk 2.50. Do not merge or ship a release until those are confirmed.


### Public visual identity boundary (confirmed with yamashi, November 2026)

The license-compatible technical foundation does **not** imply any permission or
intent to reproduce the appearance of CET: **m1core must not look like CET or
retain CET-associated visual elements**. Keep m1core's own ImGui-first Settings /
Keybinds / Profiles / Logs / About interface, Karla, JetBrains Mono, Phosphor
icons and independent theme. In particular, **the inherited ImGuiNotify toast
styling is not a finished m1core-branded startup notice** even though its
shortcut message moved to the left and no longer says CET. An upcoming small
presentation-only change should distinguish that startup notice, while reusing
the proven native input, window hooks and D3D12 backend. Preserve third-party
toast API behavior as needed.

**The author of the donated code expressly authorized the maintainer to use
that contribution as part of her own independent project** without requiring
his name or the old project's name in m1core's public credits or project
license. His requested acknowledgment is a **Nexus Mods shout-out when m1core
is released**, which the maintainer will handle. Accordingly the release ZIP
contains the `m1core/LICENSE` project MIT license, not a separate
author-specific license/branding copy. This understanding is specific to the
donor's own contribution; it does **not** waive requirements for independently
copyrighted third-party libraries or fonts. Their notices remain packaged.

### m1core application UI

The native m1core runtime hosts the m1core settings registry and REDscript API. The main UI is a **single Dear ImGui settings window**, with tabs for Settings, Keybinds, Profiles, Logs and About; controls use stock ImGui widgets and the supplied ImGui theme. CET-only developer panels (Console/TweakDB Editor/Game Log/ImGui Debug/Reload All Mods) are no longer presented in the main interface. Keybinding still uses the original donor widget and raw input implementation; its internal persisted `cet` binding namespace remains temporarily unchanged to avoid deleting existing users' shortcuts. This is internal compatibility, not user-visible branding. The old independent D3D12 renderer is not compiled or used.

The active runtime source is now under `m1core/core` (formerly `m1core/cet-foundation`). It uses an m1core-named runtime class, M1CoreVersion configuration header, and stable RED4ext `Main` entry. The source-donation snapshot remains in the Git history for internal development reference. No restricted donor fonts/icons, ASI loader, or unrelated mods are shipped. The new separately licensed Karla/JetBrains Mono/Phosphor fonts are explicitly permitted and documented below.

### Windows CI dependencies

Windows CI caches the local xmake package/build cache between runs. It compiles the actual renamed native runtime (including the m1core registry UI), verifies the original author's licensing notices, and produces a single game-root install artifact only on successful compilation. A compiled DLL is not a substitute for an in-game check.


### Cyberpunk 2.50 menu/cloud-sync investigation

During an earlier development build the game displayed "Cloud sync in progress" indefinitely when m1core.dll was loaded. This bug was subsequently resolved and confirmed by the user. Removing the DLL immediately removed the problem (reported by the user). The earlier CET-derived Console displayed repeated `Function appears to be hooked but isn't?` messages.

Found a concrete defect in the gifted runtime's `FunctionOverride::HookRunPureScriptFunction`: it treated *every* game script function with `isNative` set as if it belonged to the m1core override table and, if absent, returned `false`. The global hook was therefore aborting ordinary game script calls that m1core never registered. We now unlock and forward unregistered calls to `RealRunPureScriptFunction`, leaving registered Lua overrides and the original CET input/DirectX systems unchanged. The Windows CI includes a fail-closed source guard for this case.

**Verification:** the user confirmed that the original script-function passthrough patch resolved the cloud-sync hang. Steam cloud settings and user saves were not modified.


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
