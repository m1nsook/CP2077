module MisoFreeChrome

// Free Chrome only changes TweakDB cost variants from its CET companion.
// Mod Settings and Native Settings UI both drive the same persistent db.json
// flag via this quest fact. Nothing here edits inventory, perks, eddies, or
// maximum Cyberware Capacity.
public abstract class FreeChromeState {
  public static func Enabled(gi: GameInstance) -> Bool {
    let qs = GameInstance.GetQuestsSystem(gi);
    return !IsDefined(qs) || qs.GetFactStr("miso_free_chrome_disabled") != 1;
  }
}

public class FreeChromeSettings extends ScriptableSystem {
  @runtimeProperty("ModSettings.mod", "Free Chrome")
  @runtimeProperty("ModSettings.category", "General")
  @runtimeProperty("ModSettings.displayName", "Zero Humanity cost")
  @runtimeProperty("ModSettings.description", "Set implant Humanity/Capacity cost to zero. Does not change prices, maximum Capacity, or implant stats.")
  public let enabled: Bool = true;

  private let previous: Bool;

  private func OnAttach() {
    this.enabled = FreeChromeState.Enabled(this.GetGameInstance());
    this.previous = this.enabled;
    FCRegister(this);
    this.Schedule();
  }

  private func OnDetach() {
    FCUnregister(this);
  }

  private func Schedule() {
    let cb = new FreeChromeSettingsTick();
    cb.owner = this;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 0.75, false);
  }

  public func Tick() {
    let gi = this.GetGameInstance();
    let qs = GameInstance.GetQuestsSystem(gi);
    if IsDefined(qs) {
      if NotEquals(this.enabled, this.previous) {
        qs.SetFactStr("miso_free_chrome_disabled", this.enabled ? 0 : 1);
        this.previous = this.enabled;
      } else {
        let fromBridge = FreeChromeState.Enabled(gi);
        if NotEquals(this.enabled, fromBridge) {
          this.enabled = fromBridge;
          this.previous = fromBridge;
        }
      }
    }
    this.Schedule();
  }
}

public class FreeChromeSettingsTick extends DelayCallback {
  public let owner: wref<FreeChromeSettings>;
  public func Call() {
    if IsDefined(this.owner) { this.owner.Tick(); }
  }
}

// Mod Settings is genuinely optional; no compile dependency if uninstalled.
@if(ModuleExists("ModSettingsModule"))
public func FCRegister(s: ref<FreeChromeSettings>) {
  ModSettings.RegisterListenerToClass(s);
}
@if(!ModuleExists("ModSettingsModule"))
public func FCRegister(s: ref<FreeChromeSettings>) {}
@if(ModuleExists("ModSettingsModule"))
public func FCUnregister(s: ref<FreeChromeSettings>) {
  ModSettings.UnregisterListenerToClass(s);
}
@if(!ModuleExists("ModSettingsModule"))
public func FCUnregister(s: ref<FreeChromeSettings>) {}
