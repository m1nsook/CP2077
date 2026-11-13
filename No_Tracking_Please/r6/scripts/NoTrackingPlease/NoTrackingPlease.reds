module MisoNoTrackingPlease

// Settings persist in db.json through the CET companion. 0 is deliberately the
// default: disabling this mod is an explicit player choice.
public abstract class NTPState {
  public static func Enabled(gi: GameInstance) -> Bool {
    let qs = GameInstance.GetQuestsSystem(gi);
    return !IsDefined(qs) || qs.GetFactStr("miso_ntp_disabled") != 1;
  }
}

// Stops the standard hack-back, netrunner and police position reveal request.
@wrapMethod(NPCPuppet)
public static func RevealPlayerPositionIfNeeded(ownerPuppet: wref<ScriptedPuppet>, playerID: EntityID, opt isPrevention: Bool) -> Bool {
  if IsDefined(ownerPuppet) && NTPState.Enabled(ownerPuppet.GetGame()) {
    return false;
  }
  return wrappedMethod(ownerPuppet, playerID, isPrevention);
}

// Secondary protection for hacks initiated by AI outside the normal NPC path.
// Only the location-reveal hack is discarded: damaging and disabling hacks work.
@wrapMethod(PlayerPuppet)
protected cb func OnHackTargetEvent(evt: ref<HackTargetEvent>) -> Bool {
  if IsDefined(evt) && evt.settings.isRevealPositionAction && NTPState.Enabled(this.GetGame()) {
    this.SetIsBeingRevealed(false);
    return false;
  }
  return wrappedMethod(evt);
}

// Prevents quickhack awareness/trace acceleration when tracing is disabled.
@wrapMethod(QuickHackableQueueHelper)
public static func IsAwarenessBumpingAllowed(target: ref<ScriptedPuppet>) -> Bool {
  if IsDefined(target) && NTPState.Enabled(target.GetGame()) {
    return false;
  }
  return wrappedMethod(target);
}

// Mod Settings: optional at compile time via ModuleExists. Not a requirement.
// CET/NSUI and Mod Settings communicate through the reserved quest fact.
public class NTPSettings extends ScriptableSystem {
  @runtimeProperty("ModSettings.mod", "No Tracking Please")
  @runtimeProperty("ModSettings.category", "General")
  @runtimeProperty("ModSettings.displayName", "Disable enemy position tracing")
  @runtimeProperty("ModSettings.description", "Block enemy netrunner reveal, police reveal and quickhack hack-backs. Does not prevent ordinary visual detection.")
  public let enabled: Bool = true;

  private let previous: Bool;
  private let ready: Bool;

  private func OnAttach() {
    this.enabled = NTPState.Enabled(this.GetGameInstance());
    this.previous = this.enabled;
    NTPRegister(this);
    this.QueueTick();
  }

  private func OnDetach() {
    NTPUnregister(this);
  }

  private func QueueTick() {
    let cb = new NTPSettingsTick();
    cb.owner = this;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 0.75, false);
  }

  public func Tick() {
    let gi = this.GetGameInstance();
    let qs = GameInstance.GetQuestsSystem(gi);
    if IsDefined(qs) {
      if NotEquals(this.enabled, this.previous) {
        qs.SetFactStr("miso_ntp_disabled", this.enabled ? 0 : 1);
        this.previous = this.enabled;
      } else {
        // Sync changes performed through the optional Native Settings UI.
        let fromBridge = NTPState.Enabled(gi);
        if NotEquals(this.enabled, fromBridge) {
          this.enabled = fromBridge;
          this.previous = fromBridge;
        }
      }
    }
    this.QueueTick();
  }
}

public class NTPSettingsTick extends DelayCallback {
  public let owner: wref<NTPSettings>;
  public func Call() {
    if IsDefined(this.owner) { this.owner.Tick(); }
  }
}

@if(ModuleExists("ModSettingsModule"))
public func NTPRegister(s: ref<NTPSettings>) {
  ModSettings.RegisterListenerToClass(s);
}
@if(!ModuleExists("ModSettingsModule"))
public func NTPRegister(s: ref<NTPSettings>) {}
@if(ModuleExists("ModSettingsModule"))
public func NTPUnregister(s: ref<NTPSettings>) {
  ModSettings.UnregisterListenerToClass(s);
}
@if(!ModuleExists("ModSettingsModule"))
public func NTPUnregister(s: ref<NTPSettings>) {}
