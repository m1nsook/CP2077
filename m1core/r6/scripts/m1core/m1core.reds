// m1core v0.1 native API. These names must match plugin.cpp exactly.
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
