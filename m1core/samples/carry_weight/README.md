# m1core sample — Persistent Carry Weight

This opt-in, game-ready REDscript example tests m1core v0.5 settings,
setting metadata, persistence, and native keybind polling.

Features:
- Enabled, Carry Weight Limit (100–10,000; default 500), Toggle Mod (F8).
- Target is the *total* carry capacity including ordinary perks and gear.
- The value is saved in m1core settings.json, not in the character save.
- Reloading, game restart and death followed by reload reapply the value.
- The player tick periodically checks for changes without stacking modifiers.
- F8 toggles the sample. Changing the setting in F11 also takes effect.
- The script stores its own modifier handle and removes just that modifier;
  it never deletes all modifiers or permanently edits the player's save.

Installation: the one-ZIP Actions artifact installs the sample automatically
at r6/scripts/m1core_sample_weight/m1core_sample_weight.reds. Requires
m1core, RED4ext and redscript, but not CET/Codeware/TweakXL.
To remove just this sample, delete that REDscript file and restart.

Suggested checks:
1. Set Carry Weight Limit to 750 in F11 and inspect the Inventory limit.
2. Change it to 1000 while playing and confirm it changes without reboot.
3. Press F8 to disable (vanilla weight), then again to restore.
4. Change the F8 binding under Keybinds and test the new key.
5. Save, exit the game, relaunch and load; verify the value is still applied.
6. Die and reload the save; verify the limit reapplies and does not stack.
7. Alt+Tab or click your second monitor; keybind should not activate.
8. If the game doesn't compile the script, report the exact lines in
   r6/logs/redscript.log. Native CI does not compile scripts against game data.

If another mod continually sets a different exact CarryCapacity target,
both mods may compete; don't test with two such mods at the same time.
