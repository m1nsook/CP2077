// m1core M5 sample: persistent player carry-weight limit.
// RED4ext, redscript, m1core required. No CET, Codeware or TweakXL.
// The one transient modifier is reapplied after game load/death; m1core
// stores the user's target in settings.json independent of a save file.

public class M1CoreSampleWeightTick extends Event {
    public let generation: Int32;
}

@addField(PlayerPuppet)
private let m_m1SampleWeightModifier: ref<gameStatModifierData>;

@addField(PlayerPuppet)
private let m_m1SampleWeightGeneration: Int32;

@addField(PlayerPuppet)
private let m_m1SampleWeightLastLimit: Int32;

@addField(PlayerPuppet)
private let m_m1SampleWeightLastEnabled: Bool;

@addField(PlayerPuppet)
private let m_m1SampleWeightRefreshTicks: Int32;

@wrapMethod(PlayerPuppet)
protected cb func OnGameAttached() -> Bool {
    let result: Bool = wrappedMethod();
    if this.IsReplacer() || this.IsJohnnyReplacer() {
        return result;
    }

    // If this actor is reattached, remove only our tracked modifier.
    let stats: ref<StatsSystem> = GameInstance.GetStatsSystem(this.GetGame());
    if IsDefined(stats) && IsDefined(this.m_m1SampleWeightModifier) {
        stats.RemoveModifier(Cast<StatsObjectID>(this.GetEntityID()), this.m_m1SampleWeightModifier);
    }
    this.m_m1SampleWeightModifier = null;
    this.m_m1SampleWeightLastLimit = -1;
    this.m_m1SampleWeightLastEnabled = false;
    this.m_m1SampleWeightRefreshTicks = 0;
    this.m_m1SampleWeightGeneration += 1;

    if M1Core_IsAvailable() {
        M1Core_RegisterBoolSetting(n"m1core.sample.weight", n"Enabled", true);
        M1Core_RegisterIntSetting(n"m1core.sample.weight", n"WeightLimit", 500, 100, 10000);
        // F8 = VK 119. Can be changed/unbound through the m1core UI.
        M1Core_RegisterKeybindSetting(n"m1core.sample.weight", n"Toggle", 119);
        M1Core_SetSettingGroup(n"m1core.sample.weight", n"Enabled", n"Carry Weight");
        M1Core_SetSettingGroup(n"m1core.sample.weight", n"WeightLimit", n"Carry Weight");
        M1Core_SetSettingGroup(n"m1core.sample.weight", n"Toggle", n"Hotkeys");
        M1Core_SetSettingDisplayName(n"m1core.sample.weight", n"Enabled", "Enabled");
        M1Core_SetSettingDisplayName(n"m1core.sample.weight", n"WeightLimit", "Carry Weight Limit");
        M1Core_SetSettingDisplayName(n"m1core.sample.weight", n"Toggle", "Toggle Mod");
        M1Core_SetSettingDescription(n"m1core.sample.weight", n"Enabled",
            "Restore vanilla capacity when disabled. Changes persist in settings.json.");
        M1Core_SetSettingDescription(n"m1core.sample.weight", n"WeightLimit",
            "Target total carry capacity, including perks. Reapplied on game reload.");
        M1Core_SetSettingDescription(n"m1core.sample.weight", n"Toggle",
            "Press during gameplay to toggle the weight limit. Default F8.");
    }

    this.M1SampleWeightSchedule(0.5);
    return result;
}

@addMethod(PlayerPuppet)
public final func M1SampleWeightSchedule(delay: Float) -> Void {
    let evt: ref<M1CoreSampleWeightTick> = new M1CoreSampleWeightTick();
    evt.generation = this.m_m1SampleWeightGeneration;
    GameInstance.GetDelaySystem(this.GetGame()).DelayEvent(this, evt, delay, false);
}

// Own only one removable modifier. Recalculate relative to the player's
// current base after removing ours; don't delete perks/other stat modifiers.
@addMethod(PlayerPuppet)
public final func M1SampleWeightReconcile(enabled: Bool, limit: Int32) -> Void {
    let stats: ref<StatsSystem> = GameInstance.GetStatsSystem(this.GetGame());
    if !IsDefined(stats) {
        return;
    }
    let owner: StatsObjectID = Cast<StatsObjectID>(this.GetEntityID());

    // Don't touch stats again if the effective limit is already correct.
    if enabled && this.m_m1SampleWeightLastEnabled && this.m_m1SampleWeightLastLimit == limit {
        let current: Float = stats.GetStatValue(owner, gamedataStatType.CarryCapacity);
        if current > Cast<Float>(limit) - 0.5 && current < Cast<Float>(limit) + 0.5 {
            return;
        }
    }

    if IsDefined(this.m_m1SampleWeightModifier) {
        stats.RemoveModifier(owner, this.m_m1SampleWeightModifier);
        this.m_m1SampleWeightModifier = null;
    }

    this.m_m1SampleWeightLastEnabled = enabled;
    this.m_m1SampleWeightLastLimit = limit;
    if !enabled {
        return;
    }

    let actualBase: Float = stats.GetStatValue(owner, gamedataStatType.CarryCapacity);
    let difference: Float = Cast<Float>(limit) - actualBase;
    if difference > 0.5 || difference < -0.5 {
        let modifier: ref<gameStatModifierData> = RPGManager.CreateStatModifier(
            gamedataStatType.CarryCapacity, gameStatModifierType.Additive, difference);
        if IsDefined(modifier) {
            if stats.AddModifier(owner, modifier) {
                this.m_m1SampleWeightModifier = modifier;
            }
        }
    }
}

@addMethod(PlayerPuppet)
protected cb func OnM1CoreSampleWeightTick(evt: ref<M1CoreSampleWeightTick>) -> Bool {
    if evt.generation != this.m_m1SampleWeightGeneration {
        return true;
    }

    if M1Core_IsAvailable() {
        // The native API handles press edges, focus and F11 suppression.
        if M1Core_PollKeybindPressed(n"m1core.sample.weight", n"Toggle") {
            let toggled: Bool = !M1Core_GetBoolSetting(n"m1core.sample.weight", n"Enabled");
            if M1Core_SetBoolSetting(n"m1core.sample.weight", n"Enabled", toggled) {
                M1Core_SaveSettings();
                M1Core_LogEvent(n"m1core.sample.weight", 1, "Carry weight sample hotkey toggled");
            }
        }

        let enabled: Bool = M1Core_GetBoolSetting(n"m1core.sample.weight", n"Enabled");
        let limit: Int32 = M1Core_GetIntSetting(n"m1core.sample.weight", n"WeightLimit");
        // Reconcile instantly after setting change and periodically for stat
        // resets triggered by death, reload or other game systems.
        this.m_m1SampleWeightRefreshTicks += 1;
        if enabled != this.m_m1SampleWeightLastEnabled ||
           limit != this.m_m1SampleWeightLastLimit ||
           this.m_m1SampleWeightRefreshTicks >= 15 {
            this.m_m1SampleWeightRefreshTicks = 0;
            this.M1SampleWeightReconcile(enabled, limit);
        }
    }

    this.M1SampleWeightSchedule(0.2);
    return true;
}
