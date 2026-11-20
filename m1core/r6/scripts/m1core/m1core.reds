// m1core v0.5 development API. These names must match plugin.cpp exactly.
// Do not copy this file into the game without its matching m1core.dll.
public static native func M1Core_IsAvailable() -> Bool;

// Core lifecycle handshake: keep native input/GPU/cursor hooks inactive while
// Cyberpunk displays logos and "Press to Continue" at startup. Only real V
// attaching to a loaded game permits installation; setting APIs work earlier.
public static native func M1Core_GameplayAttached() -> Bool;

@wrapMethod(PlayerPuppet)
protected cb func OnGameAttached() -> Bool {
    let result: Bool = wrappedMethod();
    if !this.IsReplacer() && !this.IsJohnnyReplacer() {
        M1Core_GameplayAttached();
    }
    return result;
}
public static native func M1Core_RegisterBoolSetting(modName: CName, settingId: CName, defaultValue: Bool) -> Bool;
public static native func M1Core_RegisterIntSetting(modName: CName, settingId: CName, defaultValue: Int32, minValue: Int32, maxValue: Int32) -> Bool;
public static native func M1Core_RegisterFloatSetting(modName: CName, settingId: CName, defaultValue: Float, minValue: Float, maxValue: Float) -> Bool;
public static native func M1Core_GetBoolSetting(modName: CName, settingId: CName) -> Bool;
public static native func M1Core_GetIntSetting(modName: CName, settingId: CName) -> Int32;
public static native func M1Core_GetFloatSetting(modName: CName, settingId: CName) -> Float;
public static native func M1Core_SetBoolSetting(modName: CName, settingId: CName, value: Bool) -> Bool;
public static native func M1Core_SetIntSetting(modName: CName, settingId: CName, value: Int32) -> Bool;
public static native func M1Core_SetFloatSetting(modName: CName, settingId: CName, value: Float) -> Bool;
public static native func M1Core_ResetSetting(modName: CName, settingId: CName) -> Bool;
public static native func M1Core_SaveSettings() -> Bool;
// Diagnostic severity: Debug=0, Info=1, Warning=2, Error=3.
public static native func M1Core_LogEvent(modName: CName, level: Int32, message: String) -> Bool;

// Enum is zero-based; choices are UTF-8 labels separated by |, e.g. "Off|Low|High".
public static native func M1Core_RegisterEnumSetting(modName: CName, settingId: CName, defaultIndex: Int32, choices: String) -> Bool;
public static native func M1Core_GetEnumSetting(modName: CName, settingId: CName) -> Int32;
public static native func M1Core_SetEnumSetting(modName: CName, settingId: CName, index: Int32) -> Bool;
// Keybinds store Win32 virtual-key codes (0 = unbound). m1core does not
// execute mod actions; the mod consumes this setting through its own input code.
public static native func M1Core_RegisterKeybindSetting(modName: CName, settingId: CName, defaultVirtualKey: Int32) -> Bool;
public static native func M1Core_GetKeybindSetting(modName: CName, settingId: CName) -> Int32;
// Poll once per game update to detect a fresh key press, even with RIDEV_NOLEGACY.
// First poll primes state; holding/repeating does not retrigger. The user
// chooses the m1core menu key; no particular F-key is reserved.
// Suppressed while m1core menu is open or Cyberpunk is not foreground.
public static native func M1Core_PollKeybindPressed(modName: CName, settingId: CName) -> Bool;
public static native func M1Core_SetKeybindSetting(modName: CName, settingId: CName, virtualKey: Int32) -> Bool;

// Optional display grouping (register the setting first). Group labels are
// presentation metadata, not persisted values; mods should set them on startup.
public static native func M1Core_SetSettingGroup(modName: CName, settingId: CName, groupName: CName) -> Bool;
// Optional UI-only metadata: call after setting registration on every startup.
// Empty displayName restores the original CName label; empty description clears help.
public static native func M1Core_SetSettingDisplayName(modName: CName, settingId: CName, displayName: String) -> Bool;
public static native func M1Core_SetSettingDescription(modName: CName, settingId: CName, description: String) -> Bool;
// Session-local token: 0 before changes, increments only on effective changes.
// Returns -1 for unknown settings. Compare tokens, do not subtract them.
public static native func M1Core_GetSettingRevision(modName: CName, settingId: CName) -> Int32;
// Bulk resets change only the registered settings. SaveSettings() persists them.
// The zero/unassigned group is also supported by ResetGroupSettings.
public static native func M1Core_ResetModSettings(modName: CName) -> Bool;
public static native func M1Core_ResetGroupSettings(modName: CName, groupName: CName) -> Bool;
