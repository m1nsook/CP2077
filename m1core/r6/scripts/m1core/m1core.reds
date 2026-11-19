// m1core v0.3 native API. These names must match plugin.cpp exactly.
// Do not copy this file into the game without its matching m1core.dll.
public static native func M1Core_IsAvailable() -> Bool;
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

// Enum is zero-based; choices are UTF-8 labels separated by |, e.g. "Off|Low|High".
public static native func M1Core_RegisterEnumSetting(modName: CName, settingId: CName, defaultIndex: Int32, choices: String) -> Bool;
public static native func M1Core_GetEnumSetting(modName: CName, settingId: CName) -> Int32;
public static native func M1Core_SetEnumSetting(modName: CName, settingId: CName, index: Int32) -> Bool;
// Keybinds store Win32 virtual-key codes (0 = unbound). m1core does not
// execute mod actions; the mod consumes this setting through its own input code.
public static native func M1Core_RegisterKeybindSetting(modName: CName, settingId: CName, defaultVirtualKey: Int32) -> Bool;
public static native func M1Core_GetKeybindSetting(modName: CName, settingId: CName) -> Int32;
public static native func M1Core_SetKeybindSetting(modName: CName, settingId: CName, virtualKey: Int32) -> Bool;

// Optional display grouping (register the setting first). Group labels are
// presentation metadata, not persisted values; mods should set them on startup.
public static native func M1Core_SetSettingGroup(modName: CName, settingId: CName, groupName: CName) -> Bool;
