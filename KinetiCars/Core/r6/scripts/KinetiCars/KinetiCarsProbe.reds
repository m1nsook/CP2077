// KinetiCars development probe. No physics or damage writes.
@addField(PlayerPuppet)
public let kineticarsProbeDisabled: Bool;

@addField(PlayerPuppet)
public let kineticarsOutgoingBumps: Int32;

@addField(PlayerPuppet)
public let kineticarsIncomingBumps: Int32;

@addField(PlayerPuppet)
public let kineticarsLastDeltaV: Float;

@addField(PlayerPuppet)
public let kineticarsLastNormal: Vector3;

@addMethod(PlayerPuppet)
public func KinetiCarsResetProbe() -> Void {
  this.kineticarsOutgoingBumps = 0;
  this.kineticarsIncomingBumps = 0;
  this.kineticarsLastDeltaV = 0.0;
  let empty: Vector3;
  this.kineticarsLastNormal = empty;
}

@wrapMethod(VehicleObject)
protected cb func OnVehicleBumpEvent(evt: ref<VehicleBumpEvent>) -> Bool {
  let result = wrappedMethod(evt);
  if !IsDefined(evt) { return result; }
  let player = GetPlayer(this.GetGame()) as PlayerPuppet;
  if !IsDefined(player) || player.kineticarsProbeDisabled { return result; }
  let vehicle: wref<VehicleObject>;
  if !VehicleComponent.GetVehicle(player.GetGame(), player.GetEntityID(), vehicle)
    || !IsDefined(vehicle) || !VehicleComponent.IsDriver(player.GetGame(), player) {
    return result;
  }
  let bb = player.GetPlayerStateMachineBlackboard();
  if !IsDefined(bb) { return result; }
  let state = IntEnum<gamePSMVehicle>(bb.GetInt(GetAllBlackboardDefs().PlayerStateMachine.Vehicle));
  if !Equals(state, gamePSMVehicle.Driving) && !Equals(state, gamePSMVehicle.DriverCombat) {
    return result;
  }
  if StatusEffectSystem.ObjectHasStatusEffectWithTag(player, n"NoDriving")
    || StatusEffectSystem.ObjectHasStatusEffectWithTag(player, n"VehicleOnlyForward") {
    return result;
  }
  let other = evt.hitVehicle as VehicleObject;
  if !IsDefined(other) || Equals(other, this) { return result; }
  // These are event-side counts, not unique collision counts.
  if Equals(other, vehicle) {
    player.kineticarsOutgoingBumps += 1;
  } else {
    if !Equals(this, vehicle) { return result; }
    player.kineticarsIncomingBumps += 1;
  }
  player.kineticarsLastDeltaV = evt.impactVelocityChange;
  player.kineticarsLastNormal = evt.hitNormal;
  return result;
}
