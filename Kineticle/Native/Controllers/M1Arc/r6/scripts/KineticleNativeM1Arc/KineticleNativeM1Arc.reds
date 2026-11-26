// Optional controller. The native Core has no REDscript dependency.
native func Kineticle_GetProbeEnabled() -> Bool;
native func Kineticle_SetProbeEnabled(enabled: Bool) -> Bool;
native func Kineticle_CaptureDiagnostics() -> Int32;
native func Kineticle_DiagnosticPart(index: Int32) -> String;
native func Kineticle_ResetDiagnostics() -> Bool;
native func Kineticle_GetMassMultiplier() -> Int32;
native func Kineticle_GetIncludeMotorcycles() -> Bool;
native func Kineticle_SetIncludeMotorcycles(enabled: Bool) -> Bool;
native func Kineticle_GetMassAvailable() -> Bool;
native func Kineticle_SetMassMultiplier(multiplier: Int32) -> Bool;
native func Kineticle_GetProtectionFlags() -> Int32;
native func Kineticle_SetProtectionFlags(flags: Int32) -> Bool;
public class KineticleNativeM1ArcSystem extends ScriptableSystem {
  private let running: Bool;
  private let initialized: Bool;
  private let epoch: Int32;
  private let lastEnabled: Bool;
  private let lastFlags: Int32;
  private let lastMass: Int32;
  private let lastMassAttempt: Int32;
  private let pendingMass: Int32;
  private let stableTicks: Int32;
  private let lastBikes: Bool;
  private func OnAttach() -> Void {
    this.epoch += 1;
    this.running = true;
    this.initialized = false;
    this.Schedule();
  }
  private func OnDetach() -> Void {
    this.running = false;
    this.epoch += 1;
  }
  private func Schedule() -> Void {
    if !this.running { return; }
    let cb = new KineticleNativeM1ArcTick();
    cb.owner = this;
    cb.epoch = this.epoch;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 0.25, false);
  }
  private func Report(id: String) -> Void {
    let count = Kineticle_CaptureDiagnostics();
    let i: Int32 = 0;
    let failures: Int32 = 0;
    let message: String;
    if count <= 0 || count > 32 {
      M1Arc_Log(id, "ERROR", "Native diagnostic snapshot returned an invalid page count.");
      return;
    }
    while i < count {
      message = Kineticle_DiagnosticPart(i);
      if Equals(message, "") {
        failures += 1;
      } else {
        if !M1Arc_Log(id, "INFO", message) { failures += 1; }
      }
      i += 1;
    }
    if failures > 0 {
      M1Arc_Log(id, "ERROR", "Native diagnostic report incomplete: " + IntToString(failures) + " page(s) rejected.");
    }
  }
  private func SyncFlags(id: String, flags: Int32) -> Void {
    M1Arc_SetBool(id, "mechanicalProtection", flags == 1 || flags == 3 || flags == 5 || flags == 7);
    M1Arc_SetBool(id, "gridProtection", flags == 2 || flags == 3 || flags == 6 || flags == 7);
    M1Arc_SetBool(id, "glassProtection", flags >= 4 && flags <= 7);
  }
  public func Tick(epoch: Int32) -> Void {
    if !this.running || epoch != this.epoch { return; }
    let id = "m1nsook.kineticle";
    let raw = M1Arc_GetJson(id, "probeEnabled");
    if Equals(raw, "true") || Equals(raw, "false") {
      if !this.initialized {
        this.lastEnabled = Kineticle_GetProbeEnabled();
        if M1Arc_SetBool(id, "probeEnabled", this.lastEnabled) {
          this.initialized = true;
          this.lastFlags = Kineticle_GetProtectionFlags();
          this.SyncFlags(id, this.lastFlags);
          this.lastMass = Kineticle_GetMassMultiplier();
          this.lastMassAttempt = this.lastMass;
          this.pendingMass = this.lastMass;
          this.stableTicks = 0;
          this.lastBikes = Kineticle_GetIncludeMotorcycles();
          M1Arc_SetInt(id, "massMultiplier", this.lastMass);
          M1Arc_SetBool(id, "includeMotorcycles", this.lastBikes);
          if !Kineticle_GetMassAvailable() { M1Arc_Log(id, "ERROR", "Native chassis mass unavailable; see Core diagnostics."); }
          this.Report(id);
        }
      } else {
        let value = M1Arc_GetBool(id, "probeEnabled");
        if NotEquals(value, this.lastEnabled) {
          if Kineticle_SetProbeEnabled(value) {
            this.lastEnabled = value;
          } else {
            M1Arc_SetBool(id, "probeEnabled", this.lastEnabled);
            M1Arc_Log(id, "ERROR", "Could not persist native probe setting.");
          }
        }
      }
      if this.initialized {
        let massWanted: Int32 = M1Arc_GetInt(id, "massMultiplier");
        if massWanted != this.pendingMass {
          this.pendingMass = massWanted;
          this.stableTicks = 0;
        } else {
          if this.stableTicks < 3 { this.stableTicks += 1; }
        }
        if this.stableTicks >= 2 && massWanted != this.lastMassAttempt {
          this.lastMassAttempt = massWanted;
          if Kineticle_SetMassMultiplier(massWanted) {
            this.lastMass = massWanted;
            M1Arc_Log(id, "INFO", "Saved chassis mass multiplier: " + IntToString(massWanted));
          } else {
            M1Arc_SetInt(id, "massMultiplier", this.lastMass);
            M1Arc_Log(id, "ERROR", "Native mass request rejected; see Core diagnostics.");
          }
        }
        let bikes = M1Arc_GetBool(id, "includeMotorcycles");
        if NotEquals(bikes, this.lastBikes) {
          if Kineticle_SetIncludeMotorcycles(bikes) { this.lastBikes = bikes; }
          else { M1Arc_SetBool(id, "includeMotorcycles", this.lastBikes); }
        }
        let wanted: Int32 = 0;
        if M1Arc_GetBool(id, "mechanicalProtection") { wanted += 1; }
        if M1Arc_GetBool(id, "gridProtection") { wanted += 2; }
        if M1Arc_GetBool(id, "glassProtection") { wanted += 4; }
        if wanted != this.lastFlags {
          if Kineticle_SetProtectionFlags(wanted) {
            this.lastFlags = wanted;
            M1Arc_Log(id, "INFO", "Protection flags saved: " + IntToString(wanted));
          } else {
            this.SyncFlags(id, this.lastFlags);
            M1Arc_Log(id, "ERROR", "Native protection configuration failed.");
          }
        }
        if M1Arc_ConsumeAction(id, "report") { this.Report(id); }
        if M1Arc_ConsumeAction(id, "reset") {
          Kineticle_ResetDiagnostics();
          M1Arc_Log(id, "INFO", "Native diagnostics reset.");
        }
      }
    }
    this.Schedule();
  }
}
public class KineticleNativeM1ArcTick extends DelayCallback {
  public let owner: wref<KineticleNativeM1ArcSystem>;
  public let epoch: Int32;
  public func Call() -> Void {
    if IsDefined(this.owner) { this.owner.Tick(this.epoch); }
  }
}
