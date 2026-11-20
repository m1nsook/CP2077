# m1core sample — Persistent Carry Weight

This opt-in, game-ready REDscript example tests m1core v0.5 settings,
setting metadata, persistence, and native keybind polling.

Features:
- Enabled, Carry Weight Limit (100–10,000; default 500), Toggle Mod (Unbound by default).
- Target is the *total* carry capacity including ordinary perks and gear.
- The value is saved in m1core settings.json, not in the character save.
- Reloading, game restart and death followed by reload reapply the value.
- The player tick periodically checks for changes without stacking modifiers.
- No key is assigned automatically. Choose one in m1core > Keybinds.
- The selected key toggles the sample. Live settings changes also take effect.
- The script stores its own modifier handle and removes just that modifier;
  it never deletes all modifiers or permanently edits the player's save.

Installation: the one-ZIP Actions artifact installs the sample automatically
at r6/scripts/m1core_sample_weight/m1core_sample_weight.reds. Requires
m1core, RED4ext and redscript, but not CET/Codeware/TweakXL.
To remove just this sample, delete that REDscript file and restart.

Suggested checks:
1. Open m1core using your chosen menu key; set Carry Weight Limit to 750.
2. Inspect Inventory, then change the limit to 1000; verify live updates.
3. Assign a key under Keybinds, then press it to disable and restore the mod.
4. Rebind the user-selected key under Keybinds and test the change.
5. Save, exit the game, relaunch and load; verify the value is still applied.
6. Die and reload the save; verify the limit reapplies and does not stack.
7. Alt+Tab or click your second monitor; keybind should not activate.
8. If the game doesn't compile the script, report the exact lines in
   r6/logs/redscript.log. Native CI does not compile scripts against game data.

If another mod continually sets a different exact CarryCapacity target,
both mods may compete; don't test with two such mods at the same time.

Existing testers with an old F8 value saved in `settings.json` retain that
previous binding until they choose `X` / Reset in Keybinds. The updated
sample no longer assigns F8 (or any other key) to new installations.
