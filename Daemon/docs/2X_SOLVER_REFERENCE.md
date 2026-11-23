# Cyberpunk 2077 2.x solver/input references

Reviewed [Marcin Cichocki's Breach Protocol Autosolver](https://github.com/marcincichocki/breach-protocol-autosolver)
(MIT-licensed) including `docs/how-it-works.md`,
`src/core/solver/breach-protocol.ts`,
`src/core/solver/sequence-factory.ts`, `src/common/node/resolver.ts`,
`src/electron/worker/autosolver.ts` and its `patch: '2.x'` default.

**Correct architecture:** This app is a standalone Electron tool. It
screenshots the game, uses Tesseract OCR to identify the matrix, daemon
sequences and buffer, searches for paths, and simulates OS keyboard/mouse
input. It does **not** use RED4ext, REDscript or the game's native hacking
controller. Its 2.x compatibility must not be confused with compatibility
of Skip Breach's native, signature-gated selector.

**Useful design:**
- Generate daemon combinations with overlapping token sequences and
  prioritize more valuable daemon groups.
- Use BFS to find a short path respecting the first-row rule, alternating
  column/row selections, cell reuse and buffer capacity.
- For keyboard navigation, default arrow keys and Enter are configurable,
  and already-consumed grid cells affect navigation distances.
- For 2.x, handle stopping/exiting independently of the older 1.x
  automatic-exit assumptions.
- Report failure when recognition, planning or input resolution fails,
  rather than silently letting the timer expire.

**Adaptation:** Keep the current in-game REDscript board capture to avoid
OCR and language dependencies. A future version-agnostic keyboard
navigation backend may replace the brittle executable signature, but it
must be strictly gated to a verified active minigame, game focus, and known
navigation bindings. Do not add unverified blind input injection.

The independent unit test `dev/test_solver.py::test_cyberpunk_2x_real_seven_by_seven_matrix`
uses the 7x7 board transcribed from the user's screenshot and the already
selected buffer token reconstructed in its original cell. It proves the
valid route `7A 7A 7A 1C 1C` completes all three displayed daemons, but
does not verify live REDscript capture or in-game cell selection.

The user's earlier supplied AutoBreach 0.4.0 ZIP is a DIFFERENT tool: it
uses CET Lua + REDscript + a game-build-specific RED4ext input DLL.
