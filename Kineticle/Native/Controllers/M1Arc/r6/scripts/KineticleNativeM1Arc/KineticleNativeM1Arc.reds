// Optional controller. The native Core has no REDscript dependency.
native func Kineticle_GetProbeEnabled() -> Bool;
native func Kineticle_SetProbeEnabled(enabled: Bool) -> Bool;
native func Kineticle_Diagnostics() -> String;
native func Kineticle_ResetDiagnostics() -> Bool;
public class KineticleNativeM1ArcSystem extends ScriptableSystem {
  private let running: Bool;
  private let initialized: Bool;
  private let epoch: Int32;
  private let lastEnabled: Bool;
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
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 0.50, false);
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
          M1Arc_Log(id, "INFO", Kineticle_Diagnostics());
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
        if M1Arc_ConsumeAction(id, "report") { M1Arc_Log(id, "INFO", Kineticle_Diagnostics()); }
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
