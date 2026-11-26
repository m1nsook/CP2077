// Mod Settings 0.2.35: annotations and accepted-change callback from supplied API.
public class KineticleModSettingsValues extends IScriptable {
  @runtimeProperty("ModSettings.mod", "Kineticle")
  @runtimeProperty("ModSettings.category", "Physics")
  @runtimeProperty("ModSettings.category.order", "0")
  @runtimeProperty("ModSettings.order", "0")
  @runtimeProperty("ModSettings.displayName", "Enable Kineticle")
  @runtimeProperty("ModSettings.description", "Master switch for all Kineticle effects. Configuration is saved by Core.")
  public let probeEnabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "Kineticle")
  @runtimeProperty("ModSettings.category", "Physics")
  @runtimeProperty("ModSettings.category.order", "0")
  @runtimeProperty("ModSettings.order", "1")
  @runtimeProperty("ModSettings.displayName", "Chassis mass multiplier")
  @runtimeProperty("ModSettings.description", "Persistent 1–100x alternative chassis collision mass, capped at 100,000 kg. 1 = vanilla.")
  @runtimeProperty("ModSettings.min", "1")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "1")
  public let massMultiplier: Int32 = 1;

  @runtimeProperty("ModSettings.mod", "Kineticle")
  @runtimeProperty("ModSettings.category", "Physics")
  @runtimeProperty("ModSettings.category.order", "0")
  @runtimeProperty("ModSettings.order", "2")
  @runtimeProperty("ModSettings.displayName", "Include motorcycles")
  @runtimeProperty("ModSettings.description", "Allow Kineticle effects while V drives motorcycles.")
  public let includeMotorcycles: Bool = true;

  @runtimeProperty("ModSettings.mod", "Kineticle")
  @runtimeProperty("ModSettings.category", "Protection")
  @runtimeProperty("ModSettings.category.order", "1")
  @runtimeProperty("ModSettings.order", "3")
  @runtimeProperty("ModSettings.displayName", "Mechanical damage protection")
  @runtimeProperty("ModSettings.description", "Zero the computed vehicle hit values for V's actively driven road vehicle.")
  public let mechanicalProtection: Bool = false;

  @runtimeProperty("ModSettings.mod", "Kineticle")
  @runtimeProperty("ModSettings.category", "Protection")
  @runtimeProperty("ModSettings.category.order", "1")
  @runtimeProperty("ModSettings.order", "4")
  @runtimeProperty("ModSettings.displayName", "Body deformation restoration")
  @runtimeProperty("ModSettings.description", "Reset the damage grid after the vanilla destruction callback.")
  public let gridProtection: Bool = false;

  @runtimeProperty("ModSettings.mod", "Kineticle")
  @runtimeProperty("ModSettings.category", "Protection")
  @runtimeProperty("ModSettings.category.order", "1")
  @runtimeProperty("ModSettings.order", "5")
  @runtimeProperty("ModSettings.displayName", "Glass restoration")
  @runtimeProperty("ModSettings.description", "Reset glass after the vanilla destruction callback.")
  public let glassProtection: Bool = false;
}

// Use the documented registered-class listener API. No menu wrappers or
// ConfigVar casts. Mod Settings updates accepted fields after Apply/Reset.
public class KineticleModSettingsSystem extends ScriptableSystem {
  private let values: ref<KineticleModSettingsValues>;
  private let running: Bool;
  private let epoch: Int32;
  private let initialized: Bool;
  private let lastEnabled: Bool;
  private let lastBikes: Bool;
  private let lastMass: Int32;
  private let lastFlags: Int32;

  private func OnAttach() -> Void {
    this.epoch += 1;
    this.running = true;
    this.initialized = false;
    this.Schedule();
  }

  private func OnDetach() -> Void {
    this.running = false;
    this.epoch += 1;
    if IsDefined(this.values) {
      ModSettings.UnregisterListenerToClass(this.values);
      this.values = null;
    }
    this.initialized = false;
  }

  private func Schedule() -> Void {
    if !this.running { return; }
    let cb = new KineticleModSettingsTick();
    cb.owner = this;
    cb.epoch = this.epoch;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 0.25, false);
  }

  private func TryAttach() -> Void {
    // Do not import class defaults before the framework has loaded the mod.
    let physics = ModSettings.GetVars(n"Kineticle", n"Physics");
    let protection = ModSettings.GetVars(n"Kineticle", n"Protection");
    if ArraySize(physics) != 3 || ArraySize(protection) != 3 { return; }
    this.values = new KineticleModSettingsValues();
    ModSettings.RegisterListenerToClass(this.values);
    this.initialized = true;
    this.ApplyAccepted(true);
  }

  private func ApplyAccepted(first: Bool) -> Void {
    if !IsDefined(this.values) { return; }
    let on = this.values.probeEnabled;
    let bikes = this.values.includeMotorcycles;
    let mass = this.values.massMultiplier;
    let flags: Int32 = 0;
    if this.values.mechanicalProtection { flags += 1; }
    if this.values.gridProtection { flags += 2; }
    if this.values.glassProtection { flags += 4; }

    if (first || NotEquals(on, this.lastEnabled)) && !on {
      if Kineticle_GetProbeEnabled() { Kineticle_SetProbeEnabled(false); }
    }
    if first || mass != this.lastMass {
      if mass >= 1 && mass <= 100 && Kineticle_GetMassAvailable() && mass != Kineticle_GetMassMultiplier() {
        Kineticle_SetMassMultiplier(mass);
      }
    }
    if first || NotEquals(bikes, this.lastBikes) {
      if NotEquals(bikes, Kineticle_GetIncludeMotorcycles()) { Kineticle_SetIncludeMotorcycles(bikes); }
    }
    if first || flags != this.lastFlags {
      if flags != Kineticle_GetProtectionFlags() { Kineticle_SetProtectionFlags(flags); }
    }
    if (first || NotEquals(on, this.lastEnabled)) && on {
      if !Kineticle_GetProbeEnabled() { Kineticle_SetProbeEnabled(true); }
    }
    // Avoid repeatedly submitting rejected native requests.
    this.lastEnabled = on;
    this.lastBikes = bikes;
    this.lastMass = mass;
    this.lastFlags = flags;
  }

  public func Tick(epoch: Int32) -> Void {
    if !this.running || epoch != this.epoch { return; }
    if !this.initialized {
      this.TryAttach();
    } else {
      this.ApplyAccepted(false);
    }
    this.Schedule();
  }
}

public class KineticleModSettingsTick extends DelayCallback {
  public let owner: wref<KineticleModSettingsSystem>;
  public let epoch: Int32;
  public func Call() -> Void {
    if IsDefined(this.owner) { this.owner.Tick(this.epoch); }
  }
}
