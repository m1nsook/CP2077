// Kineticle 0.1.0-dev.3. Experimental instance-scoped collision enhancement.
public class KineticleSystem extends ScriptableSystem {
  private let running: Bool;
  private let epoch: Int32;
  private let enabled: Bool;
  private let multiplier: Float;
  private let protect: Bool;
  private let visuals: Bool;
  private let bikes: Bool;
  private let protectedVehicle: EntityID;
  private let ownsProtection: Bool;
  private let lastImpact: Float;
  private let hasImpact: Bool;
  public let contacts: Int32;
  public let applications: Int32;
  public let emptyForce: Int32;
  public let lastDeltaV: Float;
  public let lastForce: Float;

  public static func Get(gi: GameInstance) -> ref<KineticleSystem> {
    return GameInstance.GetScriptableSystemsContainer(gi).Get(n"KineticleSystem") as KineticleSystem;
  }

  private func OnAttach() -> Void {
    this.epoch += 1;
    this.running = true;
    this.Configure(true, 5.0, true, true, true);
    this.ResetDiagnostics();
    this.Schedule();
  }

  private func OnDetach() -> Void {
    this.running = false;
    this.epoch += 1;
    this.RemoveProtection();
  }

  public func Configure(enabled: Bool, multiplier: Float, protect: Bool, visuals: Bool, bikes: Bool) -> Void {
    this.enabled = enabled;
    this.multiplier = multiplier >= 1.0 && multiplier <= 20.0 ? multiplier : 1.0;
    this.protect = protect;
    this.visuals = visuals;
    this.bikes = bikes;
    if !enabled || !protect { this.RemoveProtection(); }
  }

  public func ResetDiagnostics() -> Void {
    this.contacts = 0;
    this.applications = 0;
    this.emptyForce = 0;
    this.lastDeltaV = 0.0;
    this.lastForce = 0.0;
  }

  public func DriverVehicle() -> wref<VehicleObject> {
    if !this.running || !this.enabled { return null; }
    let player = GetPlayer(this.GetGameInstance()) as PlayerPuppet;
    if !IsDefined(player) { return null; }
    let vehicle: wref<VehicleObject>;
    if !VehicleComponent.GetVehicle(player.GetGame(), player.GetEntityID(), vehicle)
      || !IsDefined(vehicle) || !VehicleComponent.IsDriver(player.GetGame(), player)
      || !vehicle.IsPlayerDriver() || vehicle.IsDestroyed() {
      return null;
    }
    if !IsDefined(vehicle as CarObject) && !IsDefined(vehicle as BikeObject) { return null; }
    if !this.bikes && IsDefined(vehicle as BikeObject) { return null; }
    let bb = player.GetPlayerStateMachineBlackboard();
    if !IsDefined(bb) { return null; }
    let state = IntEnum<gamePSMVehicle>(bb.GetInt(GetAllBlackboardDefs().PlayerStateMachine.Vehicle));
    if !Equals(state, gamePSMVehicle.Driving) && !Equals(state, gamePSMVehicle.DriverCombat) { return null; }
    if StatusEffectSystem.ObjectHasStatusEffectWithTag(player, n"NoDriving")
      || StatusEffectSystem.ObjectHasStatusEffectWithTag(player, n"VehicleOnlyForward")
      || StatusEffectSystem.ObjectHasStatusEffectWithTag(player, n"VehicleScene")
      || vehicle.IsVehicleRemoteControlled() {
      return null;
    }
    return vehicle;
  }

  public func ProtectsVisuals(vehicle: wref<VehicleObject>) -> Bool {
    if !this.protect || !this.visuals || !IsDefined(vehicle) { return false; }
    let driven = this.DriverVehicle();
    return IsDefined(driven) && Equals(driven.GetEntityID(), vehicle.GetEntityID());
  }

  private func RemoveProtection() -> Void {
    if this.ownsProtection {
      let gm = GameInstance.GetGodModeSystem(this.GetGameInstance());
      if IsDefined(gm) {
        gm.RemoveGodMode(this.protectedVehicle, gameGodModeType.Invulnerable, n"Kineticle");
      }
      this.ownsProtection = false;
      let empty: EntityID;
      this.protectedVehicle = empty;
    }
  }

  private func UpdateProtection() -> Void {
    let vehicle = this.DriverVehicle();
    if !this.protect || !IsDefined(vehicle) {
      this.RemoveProtection();
      return;
    }
    let id = vehicle.GetEntityID();
    if this.ownsProtection && NotEquals(id, this.protectedVehicle) { this.RemoveProtection(); }
    if !this.ownsProtection {
      let gm = GameInstance.GetGodModeSystem(this.GetGameInstance());
      if IsDefined(gm) {
        if gm.AddGodMode(id, gameGodModeType.Invulnerable, n"Kineticle") {
          this.protectedVehicle = id;
          this.ownsProtection = true;
        }
      }
    }
  }

  public func HasOwnedProtection() -> Bool {
    return this.ownsProtection;
  }

  private func Schedule() -> Void {
    if !this.running { return; }
    let cb = new KineticleTick();
    cb.owner = this;
    cb.epoch = this.epoch;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 0.20, false);
  }

  public func Tick(epoch: Int32) -> Void {
    if !this.running || epoch != this.epoch { return; }
    this.UpdateProtection();
    this.Schedule();
  }

  public func OnBump(target: ref<VehicleObject>, evt: ref<VehicleBumpEvent>) -> Void {
    if !IsDefined(evt) || !IsDefined(target) || target.IsDestroyed() { return; }
    let driver = this.DriverVehicle();
    let other = evt.hitVehicle as VehicleObject;
    // Only the event on the OTHER vehicle, never on V's vehicle.
    if !IsDefined(driver) || !IsDefined(other) { return; }
    let driverID = driver.GetEntityID();
    if NotEquals(other.GetEntityID(), driverID) || Equals(target.GetEntityID(), driverID) { return; }
    let delta = AbsF(evt.impactVelocityChange);
    if !(delta >= 1.0 && delta <= 100.0) { return; }
    this.contacts += 1;
    this.lastDeltaV = delta;
    if this.multiplier <= 1.0 { return; }

    // Reject stationary V and impacts not broadly in V's movement direction.
    let a = driver.GetWorldPosition();
    let b = target.GetWorldPosition();
    let x = b.X - a.X;
    let y = b.Y - a.Y;
    let length = SqrtF(x*x + y*y);
    if !(length > 0.1 && length < 30.0) { return; }
    x = x / length;
    y = y / length;
    let velocity = driver.GetLinearVelocity();
    let forward = velocity.X*x + velocity.Y*y;
    if !(forward > 1.0 && forward < 150.0) { return; }

    let now = EngineTime.ToFloat(GameInstance.GetSimTime(this.GetGameInstance()));
    if this.hasImpact && now >= this.lastImpact && now - this.lastImpact < 0.30 { return; }

    // The native force API is present in the 2.50 RTTI dump. Its runtime
    // semantics and units are unverified. Prefer the measured force magnitude;
    // do not invent force when the engine reports zero or invalid data.
    let force = target.GetCollisionForce();
    let magnitude = SqrtF(force.X*force.X + force.Y*force.Y + force.Z*force.Z);
    if !(magnitude > 0.01 && magnitude < 10000000.0) {
      this.emptyForce += 1;
      return;
    }
    let extra = ClampF(magnitude * (this.multiplier - 1.0), 0.0, 30000.0);
    let impulse: Vector4;
    impulse.X = x * extra;
    impulse.Y = y * extra;
    impulse.Z = 0.0;
    impulse.W = 0.0;
    // Set the guard before dispatch in case the native call is reentrant.
    this.hasImpact = true;
    this.lastImpact = now;
    this.lastForce = extra;
    this.applications += 1;
    target.AddCollisionForce(impulse);
  }
}

public class KineticleTick extends DelayCallback {
  public let owner: wref<KineticleSystem>;
  public let epoch: Int32;
  public func Call() -> Void {
    if IsDefined(this.owner) { this.owner.Tick(this.epoch); }
  }
}

@wrapMethod(VehicleObject)
protected cb func OnVehicleBumpEvent(evt: ref<VehicleBumpEvent>) -> Bool {
  let result = wrappedMethod(evt);
  let system = KineticleSystem.Get(this.GetGame());
  if IsDefined(system) { system.OnBump(this, evt); }
  return result;
}

@wrapMethod(VehicleComponent)
protected cb func OnGridDestruction(evt: ref<VehicleGridDestructionEvent>) -> Bool {
  let result = wrappedMethod(evt);
  let vehicle = this.GetVehicle();
  if IsDefined(vehicle) {
    let system = KineticleSystem.Get(vehicle.GetGame());
    if IsDefined(system) && system.ProtectsVisuals(vehicle) {
      vehicle.DestructionResetGrid();
      vehicle.DestructionResetGlass();
    }
  }
  return result;
}
