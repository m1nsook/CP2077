// Kineticle 0.1.0-dev.4. Experimental instance-scoped collision enhancement.
public class KineticleSystem extends ScriptableSystem {
  private let running: Bool;
  private let enabled: Bool;
  private let multiplier: Float;
  private let protect: Bool;
  private let visuals: Bool;
  private let bikes: Bool;
  private let lastImpact: Float;
  private let hasImpact: Bool;
  public let contacts: Int32;
  public let applications: Int32;
  public let invalidMass: Int32;
  public let directionRejected: Int32;
  public let cooldownRejected: Int32;
  public let protectedHits: Int32;
  public let otherVehicleHits: Int32;
  public let visualResets: Int32;
  public let sampledForces: Int32;
  private let forceSequence: Int32;
  public let lastMass: Float;
  public let lastSpeedBefore: Float;
  public let lastSpeedAfter: Float;
  public let lastDeltaV: Float;
  public let lastForce: Float;

  public static func Get(gi: GameInstance) -> ref<KineticleSystem> {
    return GameInstance.GetScriptableSystemsContainer(gi).Get(n"KineticleSystem") as KineticleSystem;
  }

  private func OnAttach() -> Void {
    this.running = true;
    this.Configure(true, 5.0, true, true, true);
    this.ResetDiagnostics();
  }

  private func OnDetach() -> Void {
    this.running = false;
    this.forceSequence += 1;
  }

  public func BuildVersion() -> String {
    return "0.1.0-dev.4";
  }

  public func Configure(enabled: Bool, multiplier: Float, protect: Bool, visuals: Bool, bikes: Bool) -> Void {
    this.enabled = enabled;
    this.multiplier = multiplier >= 1.0 && multiplier <= 20.0 ? multiplier : 1.0;
    this.protect = protect;
    this.visuals = visuals;
    this.bikes = bikes;
  }

  public func ResetDiagnostics() -> Void {
    this.contacts = 0;
    this.applications = 0;
    this.invalidMass = 0;
    this.directionRejected = 0;
    this.cooldownRejected = 0;
    this.protectedHits = 0;
    this.otherVehicleHits = 0;
    this.visualResets = 0;
    this.sampledForces = 0;
    this.forceSequence += 1;
    this.lastMass = 0.0;
    this.lastSpeedBefore = 0.0;
    this.lastSpeedAfter = 0.0;
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
      || vehicle.IsVehicleRemoteControlled() || vehicle.IsInQuestOrScenePhysicsState() {
      return null;
    }
    return vehicle;
  }

  // Event-local checks only. No GodMode, stat modifiers, or persistent entity flags.
  public func ProtectsVehicle(vehicle: wref<VehicleObject>) -> Bool {
    if !this.protect || !IsDefined(vehicle) { return false; }
    let driven = this.DriverVehicle();
    return IsDefined(driven) && Equals(driven.GetEntityID(), vehicle.GetEntityID());
  }

  public func ProtectsVisuals(vehicle: wref<VehicleObject>) -> Bool {
    return this.visuals && this.ProtectsVehicle(vehicle);
  }

  public func FilterDamage(hitEvent: ref<gameHitEvent>) -> Void {
    if !IsDefined(hitEvent) { return; }
    let vehicle = hitEvent.target as VehicleObject;
    if !IsDefined(vehicle) { return; }
    if this.ProtectsVehicle(vehicle) && IsDefined(hitEvent.attackComputed) {
      // Leave the vanilla damage pipeline running, but zero the computed damage
      // for this exact hit. No changes survive the end of this callback.
      hitEvent.attackComputed.MultAttackValue(0.0);
      this.protectedHits += 1;
    } else {
      this.otherVehicleHits += 1;
    }
  }

  public func RecordForceSample(sequence: Int32, vehicle: wref<VehicleObject>, x: Float, y: Float) -> Void {
    if !this.running || sequence != this.forceSequence || !IsDefined(vehicle) { return; }
    let velocity = vehicle.GetLinearVelocity();
    this.lastSpeedAfter = velocity.X*x + velocity.Y*y;
    this.sampledForces += 1;
  }

  public func OnBump(target: ref<VehicleObject>, evt: ref<VehicleBumpEvent>) -> Void {
    if !IsDefined(evt) || !IsDefined(target) || target.IsDestroyed() || target.IsInQuestOrScenePhysicsState() { return; }
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
    if !(forward > 0.25 && forward < 150.0) {
      this.directionRejected += 1;
      return;
    }

    let now = EngineTime.ToFloat(GameInstance.GetSimTime(this.GetGameInstance()));
    if this.hasImpact && now >= this.lastImpact && now - this.lastImpact < 0.30 {
      this.cooldownRejected += 1;
      return;
    }

    // Estimate extra momentum from the struck vehicle's actual mass and the
    // event delta-V, not GetCollisionForce() (usually zero in dev.3 tests).
    // The native API's units are not established; cap both the requested
    // additional speed and absolute magnitude for the experimental build.
    let mass = target.GetTotalMass();
    if !(mass >= 25.0 && mass <= 50000.0) {
      this.invalidMass += 1;
      return;
    }
    let extra = ClampF(mass * ClampF(delta * (this.multiplier - 1.0), 0.0, 12.0), 0.0, 30000.0);
    let impulse: Vector4;
    impulse.X = x * extra;
    impulse.Y = y * extra;
    impulse.Z = 0.0;
    impulse.W = 0.0;
    // Set the guard before dispatch in case the native call is reentrant.
    this.hasImpact = true;
    this.lastImpact = now;
    this.lastForce = extra;
    this.lastMass = mass;
    let before = target.GetLinearVelocity();
    this.lastSpeedBefore = before.X*x + before.Y*y;
    this.lastSpeedAfter = this.lastSpeedBefore;
    this.applications += 1;
    this.forceSequence += 1;
    // A bump already places the target in contact with V; wake before adding force.
    target.PhysicsWakeUp();
    target.AddCollisionForce(impulse);
    let sample = new KineticleForceSample();
    sample.owner = this;
    sample.vehicle = target;
    sample.sequence = this.forceSequence;
    sample.x = x;
    sample.y = y;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(sample, 0.10, false);
  }
}

public class KineticleForceSample extends DelayCallback {
  public let owner: wref<KineticleSystem>;
  public let vehicle: wref<VehicleObject>;
  public let sequence: Int32;
  public let x: Float;
  public let y: Float;
  public func Call() -> Void {
    if IsDefined(this.owner) {
      this.owner.RecordForceSample(this.sequence, this.vehicle, this.x, this.y);
    }
  }
}

@wrapMethod(DamageSystem)
private final func DealDamages(hitEvent: ref<gameHitEvent>) -> Void {
  if IsDefined(hitEvent) {
    let vehicle = hitEvent.target as VehicleObject;
    if IsDefined(vehicle) {
      let system = KineticleSystem.Get(vehicle.GetGame());
      if IsDefined(system) { system.FilterDamage(hitEvent); }
    }
  }
  wrappedMethod(hitEvent);
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
      system.visualResets += 1;
      vehicle.DestructionResetGrid();
      vehicle.DestructionResetGlass();
    }
  }
  return result;
}
