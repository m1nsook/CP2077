// Optional controller. The native Core has no REDscript dependency.
native func Kineticle_GetProbeEnabled() -> Bool;
native func Kineticle_SetProbeEnabled(enabled: Bool) -> Bool;
native func Kineticle_GetMassMultiplier() -> Int32;
native func Kineticle_GetIncludeMotorcycles() -> Bool;
native func Kineticle_SetIncludeMotorcycles(enabled: Bool) -> Bool;
native func Kineticle_GetMassAvailable() -> Bool;
native func Kineticle_SetControllerContext(vehicle: EntityID, detail: String, allowed: Bool) -> Bool;
native func Kineticle_SetMassMultiplier(multiplier: Int32) -> Bool;
native func Kineticle_GetProtectionFlags() -> Int32;
native func Kineticle_SetProtectionFlags(flags: Int32) -> Bool;

// Identical scope bridge in each alternate controller package. One controller at a time.
// Derived from the user-confirmed dev.15 M1 Arc bridge; no UI framework calls.
public class KineticleAlternateScopeSystem extends ScriptableSystem {
  private let running: Bool;
  private let epoch: Int32;
  private func OnAttach() -> Void {
    this.epoch += 1;
    this.running = true;
    let emptyID: EntityID;
    Kineticle_SetControllerContext(emptyID, "alternate controller attached", false);
    this.Schedule();
  }
  private func OnDetach() -> Void {
    this.running = false;
    this.epoch += 1;
    let emptyID: EntityID;
    Kineticle_SetControllerContext(emptyID, "alternate controller detached", false);
  }
  private func Schedule() -> Void {
    if !this.running { return; }
    let cb = new KineticleAlternateScopeTick();
    cb.owner = this;
    cb.epoch = this.epoch;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 0.10, false);
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
    let allowed = false;
    if mounted && IsDefined(vehicle) && driver && (state == 1 || state == 6) && !noDriving && !forwardOnly && !scene {
      allowed = vehicle.IsPlayerDriver() && !vehicle.IsDestroyed() && !vehicle.IsVehicleRemoteControlled()
        && !vehicle.IsInQuestOrScenePhysicsState() && (IsDefined(vehicle as CarObject) || IsDefined(vehicle as BikeObject));
    }
    if allowed && !Kineticle_GetIncludeMotorcycles() && IsDefined(vehicle as BikeObject) { allowed = false; }
    Kineticle_SetControllerContext(vehicleID, detail, allowed);
  }
  public func Tick(epoch: Int32) -> Void {
    if !this.running || epoch != this.epoch { return; }
    this.SampleContext();
    this.Schedule();
  }
}
public class KineticleAlternateScopeTick extends DelayCallback {
  public let owner: wref<KineticleAlternateScopeSystem>;
  public let epoch: Int32;
  public func Call() -> Void {
    if IsDefined(this.owner) { this.owner.Tick(this.epoch); }
  }
}
