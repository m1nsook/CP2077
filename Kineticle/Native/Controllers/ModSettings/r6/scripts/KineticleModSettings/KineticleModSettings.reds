// Mod Settings 0.2.35: annotations and accepted-change callback from supplied API.
public class KineticleModSettingsValues {
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

// UI mirrors Core. Only this mod's ConfigVar objects are touched. SetValue
// stages a pending change when switching from another controller; the player
// chooses Apply/Reset. Never call the global AcceptChanges ourselves.
public class KineticleModSettingsListener extends IScriptable {
  private let vars: array<ref<ConfigVar>>;
  private func HasFlag(flags: Int32, bit: Int32) -> Bool {
    if bit == 1 { return flags == 1 || flags == 3 || flags == 5 || flags == 7; }
    if bit == 2 { return flags == 2 || flags == 3 || flags == 6 || flags == 7; }
    return flags >= 4 && flags <= 7;
  }
  private func Bit(name: CName) -> Int32 {
    if Equals(name,n"mechanicalProtection") { return 1; }
    if Equals(name,n"gridProtection") { return 2; }
    if Equals(name,n"glassProtection") { return 4; }
    return 0;
  }
  private func BoolValue(name: CName) -> Bool {
    if Equals(name,n"probeEnabled") { return Kineticle_GetProbeEnabled(); }
    if Equals(name,n"includeMotorcycles") { return Kineticle_GetIncludeMotorcycles(); }
    return this.HasFlag(Kineticle_GetProtectionFlags(),this.Bit(name));
  }
  public func Attach() -> Void {
    let group = ModSettings.GetVars(n"Kineticle", n"Physics");
    for item in group { ArrayPush(this.vars,item); }
    group = ModSettings.GetVars(n"Kineticle", n"Protection");
    for item in group { ArrayPush(this.vars,item); }
    for item in this.vars {
      if IsDefined(item) {
        let name = item.GetName();
        let slider = item as ModConfigVarInt32;
        let toggle = item as ModConfigVarBool;
        if Equals(name,n"massMultiplier") && IsDefined(slider) {
          let value = Kineticle_GetMassMultiplier();
          if slider.GetValue() != value { slider.SetValue(value); }
        } else {
          if IsDefined(toggle) && (Equals(name,n"probeEnabled") || Equals(name,n"includeMotorcycles") || this.Bit(name) > 0) {
            let value = this.BoolValue(name);
            if NotEquals(toggle.GetValue(),value) { toggle.SetValue(value); }
          }
        }
      }
    }
    ModSettings.RegisterListenerToModifications(this);
  }
  public func Detach() -> Void {
    ModSettings.UnregisterListenerToModifications(this);
    ArrayClear(this.vars);
  }
  public cb func OnModVariableChangeAccepted(groupPath: CName, varName: CName) -> Void {
    // Match the actual registered group/name pair, not another mod's same-named variable.
    for item in this.vars {
      if IsDefined(item) && Equals(item.GetGroupPath(),groupPath) && Equals(item.GetName(),varName) {
        if Equals(varName,n"massMultiplier") {
          let slider = item as ModConfigVarInt32;
          if IsDefined(slider) && slider.GetValue() != Kineticle_GetMassMultiplier() {
            Kineticle_SetMassMultiplier(slider.GetValue());
          }
          return;
        }
        let toggle = item as ModConfigVarBool;
        if !IsDefined(toggle) { return; }
        let value = toggle.GetValue();
        if Equals(varName,n"probeEnabled") {
          if NotEquals(value,Kineticle_GetProbeEnabled()) { Kineticle_SetProbeEnabled(value); }
          return;
        }
        if Equals(varName,n"includeMotorcycles") {
          if NotEquals(value,Kineticle_GetIncludeMotorcycles()) { Kineticle_SetIncludeMotorcycles(value); }
          return;
        }
        let bit = this.Bit(varName);
        if bit > 0 {
          let flags = Kineticle_GetProtectionFlags();
          if NotEquals(this.HasFlag(flags,bit),value) {
            Kineticle_SetProtectionFlags(value ? flags + bit : flags - bit);
          }
        }
        return;
      }
    }
  }
}

// The supplied 0.2.35 screen exists in both pregame and pause menus.
// Listener lifetime is bounded by the actual screen.
@addField(ModStngsMainGameController)
private let kineticleSettingsListener: ref<KineticleModSettingsListener>;

@wrapMethod(ModStngsMainGameController)
protected cb func OnInitialize() -> Void {
  this.kineticleSettingsListener = new KineticleModSettingsListener();
  this.kineticleSettingsListener.Attach();
  wrappedMethod();
}

@wrapMethod(ModStngsMainGameController)
protected cb func OnUninitialize() -> Bool {
  if IsDefined(this.kineticleSettingsListener) {
    this.kineticleSettingsListener.Detach();
    this.kineticleSettingsListener = null;
  }
  return wrappedMethod();
}
