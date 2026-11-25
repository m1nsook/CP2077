// Optional M1 Arc adapter; no gameplay logic.
public class KineticleM1ArcSystem extends ScriptableSystem {
  private let running: Bool;
  private let epoch: Int32;
  private let configured: Bool;
  private let revision: Int32;
  private let appliedEnabled: Bool;
  private let appliedMultiplier: Float;
  private let appliedProtect: Bool;
  private let appliedVisuals: Bool;
  private let appliedBikes: Bool;

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
          let enabledValue = M1Arc_GetBool(id, "enabled");
          let protect = M1Arc_GetBool(id, "protect");
          let visuals = M1Arc_GetBool(id, "visuals");
          let bikes = M1Arc_GetBool(id, "bikes");
          if !this.configured || NotEquals(enabledValue, this.appliedEnabled) || multiplier != this.appliedMultiplier
            || NotEquals(protect, this.appliedProtect) || NotEquals(visuals, this.appliedVisuals) || NotEquals(bikes, this.appliedBikes) {
            system.Configure(enabledValue, multiplier, protect, visuals, bikes);
            this.appliedEnabled = enabledValue;
            this.appliedMultiplier = multiplier;
            this.appliedProtect = protect;
            this.appliedVisuals = visuals;
            this.appliedBikes = bikes;
            M1Arc_Log(id, "INFO", "Kineticle Core " + system.BuildVersion() + "; controller dev.4; configuration applied.");
          }
          this.configured = true;
          this.revision = revision;
        }
      }
      if M1Arc_ConsumeAction(id, "report") {
        M1Arc_Log(id, "INFO", "Core " + system.BuildVersion() + "; controller dev.4; contacts=" + ToString(system.contacts)
          + "; force calls=" + ToString(system.applications) + "; invalid mass=" + ToString(system.invalidMass)
          + "; direction rejected=" + ToString(system.directionRejected) + "; cooldown rejected=" + ToString(system.cooldownRejected)
          + "; last delta-V=" + ToString(system.lastDeltaV) + "; last target mass=" + ToString(system.lastMass)
          + "; last extra force=" + ToString(system.lastForce) + "; velocity samples=" + ToString(system.sampledForces)
          + "; target outward velocity before=" + ToString(system.lastSpeedBefore) + "; after=" + ToString(system.lastSpeedAfter)
          + "; protected hits=" + ToString(system.protectedHits) + "; vanilla vehicle hits=" + ToString(system.otherVehicleHits)
          + "; visual resets=" + ToString(system.visualResets));
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
