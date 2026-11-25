// Optional M1 Arc adapter; no gameplay logic.
public class KineticleM1ArcSystem extends ScriptableSystem {
  private let running: Bool;
  private let epoch: Int32;
  private let configured: Bool;
  private let revision: Int32;

  private func OnAttach() -> Void {
    this.epoch += 1;
    this.running = true;
    this.configured = false;
    this.Schedule();
  }
  private func OnDetach() -> Void {
    this.running = false;
    this.epoch += 1;
  }
  private func Schedule() -> Void {
    if !this.running { return; }
    let cb = new KineticleM1ArcTick();
    cb.owner = this;
    cb.epoch = this.epoch;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 0.50, false);
  }
  public func Tick(epoch: Int32) -> Void {
    if !this.running || epoch != this.epoch { return; }
    let id = "m1nsook.kineticle";
    let system = KineticleSystem.Get(this.GetGameInstance());
    if IsDefined(system) {
      let revision = M1Arc_Revision();
      if !this.configured || revision != this.revision {
        let enabled = M1Arc_GetJson(id, "enabled");
        let multiplier = M1Arc_GetFloat(id, "multiplier");
        if (Equals(enabled, "true") || Equals(enabled, "false")) && multiplier >= 1.0 && multiplier <= 20.0 {
          system.Configure(M1Arc_GetBool(id, "enabled"), multiplier, M1Arc_GetBool(id, "protect"),
            M1Arc_GetBool(id, "visuals"), M1Arc_GetBool(id, "bikes"));
          this.configured = true;
          this.revision = revision;
          M1Arc_Log(id, "INFO", "Kineticle dev.2 configuration applied; force API experimental.");
        }
      }
      if M1Arc_ConsumeAction(id, "report") {
        M1Arc_Log(id, "INFO", "contacts=" + ToString(system.contacts) + "; force calls=" + ToString(system.applications)
          + "; empty force=" + ToString(system.emptyForce) + "; last delta-V=" + ToString(system.lastDeltaV)
          + "; last extra force=" + ToString(system.lastForce) + "; owns protection=" + ToString(system.HasOwnedProtection()));
      }
      if M1Arc_ConsumeAction(id, "reset") {
        system.ResetDiagnostics();
        M1Arc_Log(id, "INFO", "Collision diagnostics reset.");
      }
    }
    this.Schedule();
  }
}
public class KineticleM1ArcTick extends DelayCallback {
  public let owner: wref<KineticleM1ArcSystem>;
  public let epoch: Int32;
  public func Call() -> Void {
    if IsDefined(this.owner) { this.owner.Tick(this.epoch); }
  }
}
