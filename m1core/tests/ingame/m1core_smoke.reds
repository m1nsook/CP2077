// Optional m1core development test. Not part of a production build.
// CET: print(Game.GetPlayer():M1Core_SmokeTest(1))
// Fully restart game, then: print(Game.GetPlayer():M1Core_SmokeTest(2))
// Return value 0 = PASS, nonzero = failure bitmask.
@addMethod(PlayerPuppet)
public func M1Core_SmokeTest(stage: Int32) -> Int32 {
    let failed: Int32 = 0;
    if !M1Core_IsAvailable() {
        return 1;
    }
    if !M1Core_RegisterBoolSetting(n"m1core.smoke", n"Enabled", true) {
        failed = failed + 2;
    }
    if !M1Core_RegisterIntSetting(n"m1core.smoke", n"Count", 2, 0, 10) {
        failed = failed + 4;
    }
    if !M1Core_RegisterFloatSetting(n"m1core.smoke", n"Multiplier", 1.0, 0.5, 4.0) {
        failed = failed + 8;
    }
    if !M1Core_RegisterEnumSetting(n"m1core.smoke", n"Mode", 1, "Off|Normal|Strong") {
        failed = failed + 1048576;
    }
    if !M1Core_RegisterKeybindSetting(n"m1core.smoke", n"ActionKey", 117) {
        failed = failed + 2097152;
    }
    if !M1Core_SetSettingGroup(n"m1core.smoke", n"Enabled", n"General") {
        failed = failed + 67108864;
    }
    if !M1Core_SetSettingGroup(n"m1core.smoke", n"Mode", n"Gameplay") {
        failed = failed + 134217728;
    }
    if failed != 0 {
        return failed;
    }

    if stage == 1 {
        if !M1Core_LogEvent(n"m1core.smoke", 1, "Native diagnostic smoke test") {
            failed = failed + 268435456;
        }
        if !M1Core_ResetSetting(n"m1core.smoke", n"Enabled") {
            failed = failed + 16;
        }
        if !M1Core_ResetSetting(n"m1core.smoke", n"Count") {
            failed = failed + 32;
        }
        if !M1Core_ResetSetting(n"m1core.smoke", n"Multiplier") {
            failed = failed + 64;
        }
        if !M1Core_GetBoolSetting(n"m1core.smoke", n"Enabled") {
            failed = failed + 128;
        }
        if M1Core_GetIntSetting(n"m1core.smoke", n"Count") != 2 {
            failed = failed + 256;
        }
        if M1Core_GetFloatSetting(n"m1core.smoke", n"Multiplier") != 1.0 {
            failed = failed + 512;
        }
        if !M1Core_SetBoolSetting(n"m1core.smoke", n"Enabled", false) {
            failed = failed + 1024;
        }
        if !M1Core_SetIntSetting(n"m1core.smoke", n"Count", 7) {
            failed = failed + 2048;
        }
        if !M1Core_SetFloatSetting(n"m1core.smoke", n"Multiplier", 2.25) {
            failed = failed + 4096;
        }
        if M1Core_GetBoolSetting(n"m1core.smoke", n"Enabled") {
            failed = failed + 8192;
        }
        if M1Core_GetIntSetting(n"m1core.smoke", n"Count") != 7 {
            failed = failed + 16384;
        }
        if M1Core_GetFloatSetting(n"m1core.smoke", n"Multiplier") != 2.25 {
            failed = failed + 32768;
        }
        if !M1Core_SetEnumSetting(n"m1core.smoke", n"Mode", 2) {
            failed = failed + 4194304;
        }
        if !M1Core_SetKeybindSetting(n"m1core.smoke", n"ActionKey", 118) {
            failed = failed + 8388608;
        }
        if M1Core_GetEnumSetting(n"m1core.smoke", n"Mode") != 2 {
            failed = failed + 16777216;
        }
        if M1Core_GetKeybindSetting(n"m1core.smoke", n"ActionKey") != 118 {
            failed = failed + 33554432;
        }
        if !M1Core_SaveSettings() {
            failed = failed + 65536;
        }
    } else {
        if stage == 2 {
            if M1Core_GetEnumSetting(n"m1core.smoke", n"Mode") != 2 {
                failed = failed + 4194304;
            }
            if M1Core_GetKeybindSetting(n"m1core.smoke", n"ActionKey") != 118 {
                failed = failed + 8388608;
            }
            if M1Core_GetBoolSetting(n"m1core.smoke", n"Enabled") {
                failed = failed + 131072;
            }
            if M1Core_GetIntSetting(n"m1core.smoke", n"Count") != 7 {
                failed = failed + 262144;
            }
            if M1Core_GetFloatSetting(n"m1core.smoke", n"Multiplier") != 2.25 {
                failed = failed + 524288;
            }
        } else {
            return -1;
        }
    }
    return failed;
}

// M5 optional runtime test. Returns 0 on success, otherwise a failure bitmask.
// Console: print(Game.GetPlayer():M1Core_M5SmokeTest())
@addMethod(PlayerPuppet)
public func M1Core_M5SmokeTest() -> Int32 {
    if !M1Core_IsAvailable() {
        return 1;
    }
    if !M1Core_RegisterBoolSetting(n"m1core.m5.smoke", n"Feature", false) {
        return 2;
    }
    if !M1Core_SetSettingDisplayName(n"m1core.m5.smoke", n"Feature", "M5 Feature") {
        return 4;
    }
    if !M1Core_SetSettingDescription(n"m1core.m5.smoke", n"Feature", "Revision test for m1core") {
        return 8;
    }
    if !M1Core_ResetSetting(n"m1core.m5.smoke", n"Feature") {
        return 16;
    }

    let before: Int32 = M1Core_GetSettingRevision(n"m1core.m5.smoke", n"Feature");
    if before < 0 {
        return 32;
    }
    if !M1Core_SetBoolSetting(n"m1core.m5.smoke", n"Feature", true) {
        return 64;
    }
    let changed: Int32 = M1Core_GetSettingRevision(n"m1core.m5.smoke", n"Feature");
    if changed == before || !M1Core_GetBoolSetting(n"m1core.m5.smoke", n"Feature") {
        return 128;
    }
    if !M1Core_SetBoolSetting(n"m1core.m5.smoke", n"Feature", true) {
        return 256;
    }
    if M1Core_GetSettingRevision(n"m1core.m5.smoke", n"Feature") != changed {
        return 512;
    }
    if !M1Core_ResetSetting(n"m1core.m5.smoke", n"Feature") {
        return 1024;
    }
    if M1Core_GetSettingRevision(n"m1core.m5.smoke", n"Feature") == changed {
        return 2048;
    }
    return 0;
}
