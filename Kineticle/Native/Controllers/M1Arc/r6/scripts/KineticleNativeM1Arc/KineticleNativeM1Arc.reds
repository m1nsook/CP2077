// Optional controller. The native Core has no REDscript dependency.
native func Kineticle_GetProbeEnabled() -> Bool;
native func Kineticle_SetProbeEnabled(enabled: Bool) -> Bool;
native func Kineticle_CaptureDiagnostics() -> Int32;
native func Kineticle_DiagnosticPart(index: Int32) -> String;
native func Kineticle_ResetDiagnostics() -> Bool;
native func Kineticle_SetControllerContext(vehicle: EntityID, detail: String, allowed: Bool) -> Bool;
native func Kineticle_SetExperimentEnabled(enabled: Bool) -> Bool;
public class KineticleNativeM1ArcSystem extends ScriptableSystem {
  private let running: Bool;
  private let initialized: Bool;
  private let epoch: Int32;
  private let lastEnabled: Bool;
  private let lastExperiment: Bool;
  private func OnAttach() -> Void {
    this.epoch += 1;
    this.running = true;
    this.initialized = false;
    this.Schedule();
  }
  private func OnDetach() -> Void {
    this.running = false;
    this.epoch += 1;
    Kineticle_SetExperimentEnabled(false);
    this.lastExperiment = false;
  }
  private func Schedule() -> Void {
    if !this.running { return; }
    let cb = new KineticleNativeM1ArcTick();
    cb.owner = this;
    cb.epoch = this.epoch;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, this.lastExperiment ? 0.10 : 0.50, false);
  }
  private func Flag(value: Bool) -> String { return value ? "1" : "0"; }
  private func SampleContext() -> Void {
    let player = GetPlayer(this.GetGameInstance()) as PlayerPuppet;
    let vehicle: wref<VehicleObject>;
    let vehicleID: EntityID;
    let state: Int32 = -1;
    let mounted: Bool = false;
    let driver: Bool = false;
    let noDriving: Bool = false;
    let forwardOnly: Bool = false;
    let scene: Bool = false;
    let detail: String;
    if !IsDefined(player) {
      Kineticle_SetControllerContext(vehicleID, "player unavailable", false);
      return;
    }
    mounted = VehicleComponent.GetVehicle(player.GetGame(), player.GetEntityID(), vehicle);
    driver = VehicleComponent.IsDriver(player.GetGame(), player);
    if IsDefined(vehicle) { vehicleID = vehicle.GetEntityID(); }
    let bb = player.GetPlayerStateMachineBlackboard();
    if IsDefined(bb) { state = bb.GetInt(GetAllBlackboardDefs().PlayerStateMachine.Vehicle); }
    noDriving = StatusEffectSystem.ObjectHasStatusEffectWithTag(player, n"NoDriving");
    forwardOnly = StatusEffectSystem.ObjectHasStatusEffectWithTag(player, n"VehicleOnlyForward");
    scene = StatusEffectSystem.ObjectHasStatusEffectWithTag(player, n"VehicleScene");
    detail = "player=1 mounted=" + this.Flag(mounted) + " vehicle_defined=" + this.Flag(IsDefined(vehicle))
      + " component_driver=" + this.Flag(driver) + " psm_vehicle=" + IntToString(state)
      + " tag_NoDriving=" + this.Flag(noDriving) + " tag_VehicleOnlyForward=" + this.Flag(forwardOnly)
      + " tag_VehicleScene=" + this.Flag(scene);
    let allowed = mounted && IsDefined(vehicle) && driver && (state == 1 || state == 6) && !noDriving && !forwardOnly && !scene;
    Kineticle_SetControllerContext(vehicleID, detail, allowed);
  }
  private func Report(id: String) -> Void {
    this.SampleContext();
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
  public func Tick(epoch: Int32) -> Void {
    if !this.running || epoch != this.epoch { return; }
    let id = "m1nsook.kineticle";
    let raw = M1Arc_GetJson(id, "probeEnabled");
    if Equals(raw, "true") || Equals(raw, "false") {
      if !this.initialized {
        this.lastEnabled = Kineticle_GetProbeEnabled();
        if M1Arc_SetBool(id, "probeEnabled", this.lastEnabled) {
          this.initialized = true;
          this.lastExperiment = false;
          Kineticle_SetExperimentEnabled(false);
          M1Arc_SetBool(id, "experimentEnabled", false);
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
        if this.lastEnabled { this.SampleContext(); }
        let wanted = M1Arc_GetBool(id, "experimentEnabled");
        if !this.lastEnabled { wanted = false; }
        if NotEquals(wanted, this.lastExperiment) {
          if Kineticle_SetExperimentEnabled(wanted) {
            this.lastExperiment = wanted;
            M1Arc_Log(id, "INFO", wanted ? "Experimental paired motion enabled for this session." : "Experimental paired motion disabled.");
          } else {
            this.lastExperiment = false;
            M1Arc_Log(id, "ERROR", "Native experimental motion could not be enabled.");
          }
        }
        if NotEquals(M1Arc_GetBool(id, "experimentEnabled"), this.lastExperiment) {
          M1Arc_SetBool(id, "experimentEnabled", this.lastExperiment);
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
