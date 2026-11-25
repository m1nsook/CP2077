module DelamainAV

// Taxi pickup prototype. No vehicle ownership, quest facts or mutation of
// the original quest record. Occupied flight is not enabled yet. PSM/door wrappers below
// delegate unchanged for every vehicle except the private mod record.
public class DelamainAVSystem extends ScriptableSystem {
  private let m_running: Bool;
  private let m_id: EntityID;
  private let m_wait: Int32;
  private let m_boardWait: Int32;
  private let m_ready: Bool;
  private let m_doorWait: Int32;
  private let m_doorOpen: Bool;
  private let m_exitWait: Int32;
  private let m_closeWait: Int32;
  private let m_returnValid: Bool;
  private let m_exitRequested: Bool;
  private let m_exitAnimationSeen: Bool;
  private let m_exitTransitionRequested: Bool;
  private let m_exitElapsedTicks: Int32;
  private let m_exitStartPosition: Vector4;
  private let m_boardPosition: Vector4;
  private let m_groundZ: Float;
  private let m_clearance: Float;
  private let m_groundCheckWait: Int32;
  private let m_groundStable: Bool;
  private let m_groundValid: Bool;
  private let m_spawnTarget: Vector4;
  private let m_arriving: Bool;
  private let m_airborne: Bool;
  private let m_arrivalStep: Int32;
  private let m_arrivalOrientation: Quaternion;
  private let m_reopenBlocked: Bool;
  private let m_airExitLogged: Bool;
  private let m_summonQueued: Bool;
  private let m_landingSettling: Bool;
  private let m_nativeHolding: Bool;
  private let m_touchdownTicks: Int32;
  private let m_acquireStable: Int32;
  private let m_acquirePosition: Vector4;
  private let m_descentStarted: Bool;
  private let m_boardGroundRejected: Bool;
  private let m_physicsSamples: Int32;
  private let m_environmentLogged: Bool;

  private func OnAttach() -> Void {
    DelamainAV_WorldState(true);
    this.m_running = true;
    this.Log("INFO", "SYSTEM_ATTACH: DelamainAV script attached; dev.24 persistent diagnostics enabled.");
    this.Schedule();
  }

  private func OnDetach() -> Void {
    // Nonpersistent Codeware entities are cleared by world teardown.
    // Never delete a mounted vehicle during teardown.
    this.m_running = false;
    this.m_arriving = false;
    this.m_nativeHolding = false;
    this.m_summonQueued = false;
    DelamainAV_WorldState(false);
  }

  private func Schedule() -> Void {
    if !this.m_running { return; }
    let cb = new DelamainAVTick();
    cb.owner = this;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 0.5, false);
  }

  private func Log(level: String, message: String) -> Void {
    DelamainAV_DiagnosticLog(level, message);
    M1Arc_Log("m1nsook.delamain-av", level, message);
  }

  private func Vehicle() -> wref<VehicleObject> {
    if !EntityID.IsDefined(this.m_id) { return null; }
    return GameInstance.FindEntityByID(this.GetGameInstance(), this.m_id) as VehicleObject;
  }

  public func QueueSummon() -> Void {
    this.Log("INFO", "SUMMON_QUEUED: native vehicle menu request.");
    this.m_summonQueued = true;
  }

  private func Spawn(player: ref<PlayerPuppet>, arrival: Bool) -> Void {
    this.Log("INFO", "SPAWN_ENTER: arrival=" + ToString(arrival) + "; tracked=" + ToString(EntityID.IsDefined(this.m_id)));
    if !this.m_environmentLogged {
      this.m_environmentLogged = true;
      DelamainAV_DiagnosticEnvironment("first summon or stationary spawn");
    };
    if EntityID.IsDefined(this.m_id) {
      this.Log("WARNING", "A test AV is already tracked. Remove it first.");
      return;
    };
    if GameInstance.GetMountingFacility(player.GetGame()).IsMountedToAnything(player.GetEntityID()) {
      this.Log("WARNING", "Exit your current vehicle first.");
      return;
    };
    let record = TweakDBInterface.GetVehicleRecord(t"Vehicle.m1nsook_delamain_av");
    if !IsDefined(record) {
      this.Log("ERROR", "Custom vehicle record missing. Check TweakXL logs.");
      return;
    };
    let system = GameInstance.GetDynamicEntitySystem();
    if !IsDefined(system) || !system.IsReady() {
      this.Log("WARNING", "Codeware dynamic entity system is not ready.");
      return;
    };
    let spec = new DynamicEntitySpec();
    spec.recordID = t"Vehicle.m1nsook_delamain_av";
    // Use the original quest entity's default appearance, not DAV's custom archive.
    let forward = player.GetWorldForward();
    forward.Z = 0.0;
    forward = Vector4.Normalize(forward);
    let target = player.GetWorldPosition() + forward * 9.0;
    let rayStart = target + new Vector4(0.0, 0.0, 2.0, 0.0);
    let rayEnd = target - new Vector4(0.0, 0.0, 3.0, 0.0);
    this.Log("INFO", "SPAWN_RAY: target=" + ToString(target.X) + "," + ToString(target.Y) + "," + ToString(target.Z) + "; rayStartZ=" + ToString(rayStart.Z) + "; rayEndZ=" + ToString(rayEnd.Z));
    let hit: TraceResult;
    let spatial = GameInstance.GetSpatialQueriesSystem(player.GetGame());
    if !spatial.SyncRaycastByCollisionGroup(rayStart, rayEnd, n"Static", hit, true, false) {
      this.Log("WARNING", "GROUND_NO_HIT: No static surface below pickup point. Choose flat open ground.");
      return;
    };
    this.Log("INFO", "SPAWN_RAY_HIT: xyz=" + ToString(hit.position.X) + "," + ToString(hit.position.Y) + "," + ToString(hit.position.Z) + "; normal=" + ToString(hit.normal.X) + "," + ToString(hit.normal.Y) + "," + ToString(hit.normal.Z));
    if hit.normal.Z < 0.95 {
      this.Log("WARNING", "GROUND_SLOPE: Surface too steep for stationary test.");
      return;
    };
    this.m_groundZ = hit.position.Z;
    this.m_clearance = M1Arc_GetFloat("m1nsook.delamain-av", "ground_clearance");
    if this.m_clearance < 0.75 || this.m_clearance > 2.0 { this.m_clearance = 1.0; };
    target.Z = this.m_groundZ + this.m_clearance;
    // For the native taxi call, descend visibly from twenty metres above the
    // proven pickup. Reject static overhead obstructions before spawning.
    this.m_arriving = false;
    this.m_airborne = false;
    this.m_arrivalStep = 0;
    this.m_landingSettling = false;
    this.m_nativeHolding = false;
    this.m_touchdownTicks = 0;
    this.m_acquireStable = 0;
    this.m_descentStarted = false;
    this.m_boardGroundRejected = false;
    this.m_reopenBlocked = false;
    this.m_airExitLogged = false;
    this.m_arrivalOrientation = Quaternion.BuildFromDirectionVector(forward);
    if arrival {
      let overhead: TraceResult;
      let rayBottom = target + new Vector4(0.0, 0.0, 0.25, 0.0);
      let rayTop = target + new Vector4(0.0, 0.0, 23.0, 0.0);
      if spatial.SyncRaycastByCollisionGroup(rayBottom, rayTop, n"Static", overhead, true, false) ||
         spatial.SyncRaycastByCollisionGroup(rayTop, rayBottom, n"Static", overhead, true, false) {
        this.Log("WARNING", "TAXI_PICKUP_BLOCKED: overhead obstruction; choose open ground.");
        return;
      };
      this.m_arriving = true;
      this.m_airborne = true;
    };
    spec.position = target;
    if this.m_arriving { spec.position = target + new Vector4(0.0, 0.0, 20.0, 0.0); };
    this.m_acquirePosition = spec.position;
    spec.orientation = this.m_arrivalOrientation;
    this.m_spawnTarget = target;
    this.m_groundStable = false;
    this.m_groundValid = false;
    this.m_groundCheckWait = 0;
    this.Log("INFO", "GROUND_SPAWN: surface Z=" + ToString(this.m_groundZ) + "; requested origin height=" + ToString(this.m_clearance));
    spec.persistState = false;
    spec.persistSpawn = false;
    spec.alwaysSpawned = true;
    spec.spawnInView = true;
    spec.active = true;
    let origin = spec.position;
    this.Log("INFO", "CREATE_BEFORE: record=Vehicle.m1nsook_delamain_av; origin=" + ToString(origin.X) + "," + ToString(origin.Y) + "," + ToString(origin.Z) + "; persist=false; active=true; alwaysSpawned=true; spawnInView=true");
    this.m_id = system.CreateEntity(spec);
    this.Log("INFO", "CREATE_AFTER: defined=" + ToString(EntityID.IsDefined(this.m_id)));
    if !EntityID.IsDefined(this.m_id) {
      this.m_arriving = false;
      this.m_airborne = false;
      this.Log("ERROR", "CreateEntity returned an undefined ID.");
      return;
    };
    this.m_wait = 20;
    this.m_ready = false;
    this.m_returnValid = false;
    this.Log("INFO", "AV creation requested. Waiting up to 10 seconds for the entity.");
  }

  private func Configure(vehicle: ref<VehicleObject>) -> Void {
    this.Log("INFO", "CONFIGURE_ENTER: car=" + ToString(IsDefined(vehicle as CarObject)) + "; AV=" + ToString(IsDefined(vehicle as AVObject)) + "; recordMatch=" + ToString(Equals(vehicle.GetRecordID(), t"Vehicle.m1nsook_delamain_av")));
    DelamainAV_DiagnosticSnapshot(vehicle, "configure entry");
    let position = vehicle.GetWorldPosition();
    this.Log("INFO", "CONFIGURE_POSITION: xyz=" + ToString(position.X) + "," + ToString(position.Y) + "," + ToString(position.Z));
    let controller = vehicle.GetAccessoryController();
    this.Log("INFO", "CONFIGURE_ACCESSORY: controllerDefined=" + ToString(IsDefined(controller)));
    if IsDefined(controller) {
      let ps = controller.GetPS() as vehicleControllerPS;
      this.Log("INFO", "CONFIGURE_ACCESSORY: psDefined=" + ToString(IsDefined(ps)));
      if IsDefined(ps) {
        ps.SetDoorInteractionState(EVehicleDoor.seat_front_left, VehicleDoorInteractionState.Disabled);
        ps.SetDoorInteractionState(EVehicleDoor.seat_front_right, VehicleDoorInteractionState.Disabled);
        if this.m_airborne {
          ps.SetDoorInteractionState(EVehicleDoor.seat_back_left, VehicleDoorInteractionState.Disabled);
        } else {
          ps.SetDoorInteractionState(EVehicleDoor.seat_back_left, VehicleDoorInteractionState.Available);
        };
        ps.SetDoorInteractionState(EVehicleDoor.seat_back_right, VehicleDoorInteractionState.Disabled);
        ps.SetDoorInteractionState(EVehicleDoor.trunk, VehicleDoorInteractionState.Disabled);
        ps.SetDoorInteractionState(EVehicleDoor.hood, VehicleDoorInteractionState.Disabled);
      };
    };
    // The private ArchiveXL copy replaces only the entity root class,
    // not the Excalibur's visuals, rig or cinematic passenger workspot.
    if !IsDefined(vehicle as CarObject) {
      this.AbortArrival(vehicle, "private entity is not the expected car-based Excalibur; inspect ArchiveXL patch");
      return;
    };
    this.SetThrusters(vehicle, this.m_arriving);
    this.Log("INFO", "Spawned private car-physics Excalibur; arriving=" + ToString(this.m_arriving));
    this.m_ready = true;
    if this.m_arriving {
      this.Door(vehicle, false);
      this.m_physicsSamples = 0;
      this.Log("INFO", "PHYSICS_BEGIN_BEFORE");
      let result = DelamainAV_PhysicsBegin(vehicle);
      this.Log("INFO", "PHYSICS_BEGIN_AFTER: result=" + ToString(result));
      if result != 1 {
        this.AbortArrival(vehicle, "native bridge refused startup; code=" + ToString(result));
        return;
      };
      this.m_nativeHolding = true;
      this.Log("INFO", "TAXI_ARRIVAL_NATIVE: car-physics entity bound; waiting for native physics initialization.");
      this.ScheduleArrival();
    } else {
      this.m_groundCheckWait = 6;
      this.MeasureGround(vehicle, "GROUND_INITIAL");
    };
  }

  private func SetThrusters(vehicle: ref<VehicleObject>, enabled: Bool) -> Void {
    let ps = vehicle.GetVehiclePS();
    this.Log("INFO", "THRUSTER_BEFORE: psDefined=" + ToString(IsDefined(ps)) + "; enabled=" + ToString(enabled));
    if !IsDefined(ps) { return; };
    // Original vehicle quest event updates persisted thruster state and VFX.
    let evt = new VehicleQuestAVThrusterEvent();
    evt.enable = enabled;
    GameInstance.GetPersistencySystem(vehicle.GetGame()).QueuePSEvent(ps.GetID(), ps.GetClassName(), evt);
    this.Log("INFO", "AV_THRUSTER_REQUEST: enabled=" + ToString(enabled));
  }

  private func LandingSound(vehicle: ref<VehicleObject>) -> Void {
    // Exact exterior landing audio event from q203_04_delamain.scene.
    GameInstance.GetAudioSystem(vehicle.GetGame()).Play(n"v_av_rayfield_excalibur_sc_203_ext_engine_landing_to_idle", vehicle.GetEntityID());
    this.Log("INFO", "AV_AUDIO_REQUEST: original Excalibur exterior landing sound.");
  }

  // Keep the same entity and authoritative physics position. Hold zero
  // velocity at the landing point, then verify clearance before boarding.
  private func FinishArrival(vehicle: ref<VehicleObject>) -> Void {
    if !IsDefined(vehicle) || vehicle.IsPlayerMounted() { return; };
    this.SetThrusters(vehicle, false);
    this.m_arriving = false;
    this.m_groundStable = false;
    this.m_groundValid = false;
    this.m_landingSettling = true;
    this.m_groundCheckWait = 6;
    this.Log("INFO", "TAXI_NATIVE_SETTLING: same entity retained, zero velocity hold, verifying ground.");
  }

  private func ScheduleArrival() -> Void {
    if !this.m_running || !this.m_arriving { return; };
    let cb = new DelamainAVArrivalTick();
    cb.owner = this;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 0.05, false);
  }

  private func AbortArrival(vehicle: ref<VehicleObject>, reason: String) -> Void {
    this.Log("ERROR", "ABORT_ENTER: " + reason + "; vehicleDefined=" + ToString(IsDefined(vehicle)) + "; step=" + ToString(this.m_arrivalStep));
    if IsDefined(vehicle) { DelamainAV_DiagnosticSnapshot(vehicle, "before abort"); };
    DelamainAV_PhysicsStop(this.m_id);
    this.m_nativeHolding = false;
    this.m_arriving = false;
    this.m_ready = false;
    this.Log("ERROR", "TAXI_NATIVE_ABORT: " + reason + ". No transform fallback.");
    if !IsDefined(vehicle) { return; };
    let mounts = GameInstance.GetMountingFacility(vehicle.GetGame()).GetMountingInfoMultipleWithObjects(null, vehicle);
    if vehicle.IsPlayerMounted() || ArraySize(mounts) > 0 { return; };
    this.SetThrusters(vehicle, false);
    this.Log("WARNING", "DELETE_BEFORE: reason=arrival abort; occupantCount=" + ToString(ArraySize(mounts)));
    if GameInstance.GetDynamicEntitySystem().DeleteEntity(this.m_id) {
      this.m_id = EntityID();
      this.m_airborne = false;
      this.m_doorWait = 0;
      this.Log("INFO", "TAXI_NATIVE_ABORT: empty arrival entity removed; stationary diagnostic spawn remains available.");
    } else {
      this.Log("ERROR", "DELETE_AFTER: arrival abort deletion rejected.");
    };
  }

  public func ArrivalTick() -> Void {
    if !this.m_running || !this.m_arriving { return; };
    let vehicle = this.Vehicle();
    DelamainAV_DiagnosticLog("TRACE", "ARRIVAL_TICK_ENTER: step=" + ToString(this.m_arrivalStep) + "; vehicleDefined=" + ToString(IsDefined(vehicle)));
    if !IsDefined(vehicle) || vehicle.IsPlayerMounted() {
      this.AbortArrival(vehicle, "missing or occupied AV");
      return;
    };
    let mounts = GameInstance.GetMountingFacility(this.GetGameInstance()).GetMountingInfoMultipleWithObjects(null, vehicle);
    if ArraySize(mounts) > 0 {
      this.AbortArrival(vehicle, "occupant detected");
      return;
    };
    this.m_arrivalStep += 1;
    let pos = vehicle.GetWorldPosition();
    let target = this.m_spawnTarget;
    let remaining = pos.Z - target.Z;
    let lateral = SqrtF((pos.X - target.X) * (pos.X - target.X) + (pos.Y - target.Y) * (pos.Y - target.Y));
    DelamainAV_DiagnosticLog("TRACE", "ARRIVAL_POSITION: step=" + ToString(this.m_arrivalStep) + "; xyz=" + ToString(pos.X) + "," + ToString(pos.Y) + "," + ToString(pos.Z) + "; target=" + ToString(target.X) + "," + ToString(target.Y) + "," + ToString(target.Z) + "; zDelta=" + ToString(remaining) + "; lateral=" + ToString(lateral) + "; mounts=" + ToString(ArraySize(mounts)));
    // DAV waits for a live body before descent and measures actual ground
    // beneath it. Allow only a bounded initial zero-velocity acquisition;
    // do not confuse a startup pose correction with sustained flight drift.
    if !this.m_descentStarted {
      if !(remaining >= 18.0 && remaining <= 21.5 && lateral <= 2.0) {
        this.AbortArrival(vehicle, "initial physics pose outside acquisition envelope; lateral=" + ToString(lateral));
        return;
      };
      let acquired = DelamainAV_PhysicsPulse(vehicle, 0.0);
      if acquired < 0 && acquired != -20 {
        this.AbortArrival(vehicle, "initial physics hold fault=" + ToString(acquired));
        return;
      };
      if acquired > 0 && Vector4.Distance(pos, this.m_acquirePosition) <= 0.05 {
        this.m_acquireStable += 1;
      } else {
        this.m_acquireStable = 0;
      };
      this.m_acquirePosition = pos;
      if this.m_acquireStable >= 6 {
        // Verify the actual new center and overhead column before committing
        // to descent. The original ground height must still be equivalent.
        let hit: TraceResult;
        let overhead: TraceResult;
        let center = new Vector4(pos.X, pos.Y, this.m_groundZ, 1.0);
        let spatial = GameInstance.GetSpatialQueriesSystem(vehicle.GetGame());
        if !spatial.SyncRaycastByCollisionGroup(center + new Vector4(0.0, 0.0, 2.0, 0.0), center - new Vector4(0.0, 0.0, 3.0, 0.0), n"Static", hit, true, false) ||
           hit.normal.Z < 0.95 || AbsF(hit.position.Z - this.m_groundZ) > 0.25 {
          this.AbortArrival(vehicle, "acquired landing surface invalid");
          return;
        };
        let bottom = hit.position + new Vector4(0.0, 0.0, this.m_clearance + 0.25, 0.0);
        let top = pos + new Vector4(0.0, 0.0, 3.0, 0.0);
        if spatial.SyncRaycastByCollisionGroup(bottom, top, n"Static", overhead, true, false) ||
           spatial.SyncRaycastByCollisionGroup(top, bottom, n"Static", overhead, true, false) {
          this.AbortArrival(vehicle, "acquired landing column obstructed");
          return;
        };
        this.m_groundZ = hit.position.Z;
        this.m_spawnTarget = new Vector4(pos.X, pos.Y, this.m_groundZ + this.m_clearance, 1.0);
        this.m_descentStarted = true;
        this.Log("INFO", "TAXI_NATIVE_ACQUIRED: stable physics pose; lateral initialization shift=" + ToString(lateral) + "; samples=" + ToString(acquired));
      } else {
        if this.m_arrivalStep >= 40 {
          this.AbortArrival(vehicle, "physics pose did not stabilize");
          return;
        };
      };
      this.ScheduleArrival();
      return;
    };
    if !(remaining >= -0.20 && remaining <= 21.5 && lateral <= 0.8) {
      this.AbortArrival(vehicle, "position outside pickup corridor; zDelta=" + ToString(remaining) + "; lateral=" + ToString(lateral));
      return;
    };
    // Recheck the landing column; a changed surface aborts this empty pickup.
    if this.m_arrivalStep % 10 == 0 {
      let hit: TraceResult;
      let top = target + new Vector4(0.0, 0.0, 0.3, 0.0);
      let bottom = target - new Vector4(0.0, 0.0, 3.0, 0.0);
      if !GameInstance.GetSpatialQueriesSystem(vehicle.GetGame()).SyncRaycastByCollisionGroup(top, bottom, n"Static", hit, true, false) ||
         hit.normal.Z < 0.95 || AbsF(hit.position.Z - this.m_groundZ) > 0.25 {
        this.AbortArrival(vehicle, "landing surface changed");
        return;
      };
    };
    let speed = -MinF(5.0, MaxF(0.12, remaining * 0.85));
    if remaining <= 0.15 { speed = 0.0; };
    DelamainAV_DiagnosticLog("TRACE", "PULSE_BEFORE: requestedVz=" + ToString(speed));
    let samples = DelamainAV_PhysicsPulse(vehicle, speed);
    DelamainAV_DiagnosticLog("TRACE", "PULSE_AFTER: result=" + ToString(samples));
    if samples == -20 {
      if this.m_arrivalStep % 20 == 0 { this.Log("INFO", "TAXI_NATIVE_INIT: waiting for reciprocal physics body."); };
      this.ScheduleArrival();
      return;
    };
    if samples < 0 {
      this.AbortArrival(vehicle, "physics bridge fault=" + ToString(samples));
      return;
    };
    if this.m_arrivalStep % 20 == 0 {
      this.Log("INFO", "TAXI_NATIVE_TRACE: callbackSamples=" + ToString(samples) + "; remaining=" + ToString(remaining) + "; targetVz=" + ToString(speed) + "; lateral=" + ToString(lateral));
    };
    if this.m_arrivalStep >= 100 && samples == 0 {
      this.AbortArrival(vehicle, "no matching resistance physics tick");
      return;
    };
    if remaining <= 0.15 && samples > 0 {
      this.m_touchdownTicks += 1;
      if this.m_touchdownTicks >= 8 {
        this.m_arrivalStep = 100;
        this.Log("INFO", "TAXI_NATIVE_TOUCHDOWN: same entity reached pickup; samples=" + ToString(samples));
        this.FinishArrival(vehicle);
        return;
      };
    } else {
      this.m_touchdownTicks = 0;
    };
    if this.m_arrivalStep >= 360 {
      this.AbortArrival(vehicle, "native descent timed out");
      return;
    };
    this.m_physicsSamples = samples;
    this.ScheduleArrival();
  }

  public func IsTrackedLanded(vehicle: ref<VehicleObject>) -> Bool {
    return IsDefined(vehicle) && Equals(vehicle.GetEntityID(), this.m_id) && this.IsLanded(vehicle);
  }

  public func IsLanded(vehicle: ref<VehicleObject>) -> Bool {
    if !IsDefined(vehicle) || !this.m_ready || this.m_arriving || this.m_airborne || !this.m_groundStable { return false; };
    let pos = vehicle.GetWorldPosition();
    if Vector4.Distance(pos, this.m_spawnTarget) > 0.5 { return false; };
    let hit: TraceResult;
    let start = pos + new Vector4(0.0, 0.0, 0.2, 0.0);
    let end = pos - new Vector4(0.0, 0.0, 3.0, 0.0);
    if !GameInstance.GetSpatialQueriesSystem(vehicle.GetGame()).SyncRaycastByCollisionGroup(start, end, n"Static", hit, true, false) { return false; };
    return hit.normal.Z >= 0.95 && AbsF(pos.Z - hit.position.Z - this.m_clearance) <= 0.25;
  }

  private func AutoDoors(player: ref<PlayerPuppet>, vehicle: ref<VehicleObject>) -> Void {
    if !IsDefined(vehicle) || !this.m_ready || this.m_arriving || this.m_exitWait > 0 || this.m_boardWait > 0 || this.m_closeWait > 0 || this.m_doorWait > 0 { return; };
    if !this.IsLanded(vehicle) {
      if this.m_doorOpen { this.Door(vehicle, false); };
      return;
    };
    let mount = GameInstance.GetMountingFacility(player.GetGame()).GetMountingInfoSingleWithObjects(player);
    if Equals(mount.parentId, vehicle.GetEntityID()) { return; };
    let distance = Vector4.Distance(player.GetWorldPosition(), vehicle.GetWorldPosition());
    if distance > 8.0 {
      this.m_reopenBlocked = false;
      if this.m_doorOpen { this.Door(vehicle, false); };
    } else {
      if distance <= 6.0 && !this.m_reopenBlocked && !this.m_doorOpen {
        this.Door(vehicle, true);
        this.Log("INFO", "TAXI_DOOR_APPROACH: Delamain opened the entrance.");
      };
    };
  }

  // Never access a field on a temporary struct returned by a native call.
  // Validate the complete position before subtracting heights. dev.4 logged
  // impossible values while the vehicle was visibly parked above the road.
  private func MeasureGround(vehicle: ref<VehicleObject>, stage: String) -> Bool {
    this.m_groundValid = false;
    if !IsDefined(vehicle) { return false; };
    let pos: Vector4 = vehicle.GetWorldPosition();
    let x: Float = pos.X;
    let y: Float = pos.Y;
    let z: Float = pos.Z;
    let surface: Float = this.m_groundZ;
    let expected: Vector4 = this.m_spawnTarget;
    let expectedZ: Float = expected.Z;
    let requested: Float = this.m_clearance;
    // Positive comparisons reject NaN/Inf as well as implausible coordinates.
    if !(AbsF(x - expected.X) <= 5.0 && AbsF(y - expected.Y) <= 5.0 &&
         AbsF(z - expectedZ) <= 10.0 && AbsF(surface) < 100000.0) {
      this.Log("ERROR", stage + ": POSITION_INVALID; x=" + ToString(x) + "; y=" + ToString(y) + "; z=" + ToString(z) + "; expectedZ=" + ToString(expectedZ) + ". Not classified as ground drift.");
      return false;
    };
    let delta: Float = z - surface;
    if !(AbsF(delta) <= 12.0) {
      this.Log("ERROR", stage + ": HEIGHT_INVALID; position Z=" + ToString(z) + "; surface Z=" + ToString(surface));
      return false;
    };
    this.m_groundValid = true;
    this.Log("INFO", stage + ": position Z=" + ToString(z) + "; surface Z=" + ToString(surface) + "; clearance=" + ToString(delta) + "; requested=" + ToString(requested));
    return AbsF(delta - requested) <= 0.25;
  }

  private func GroundCheck(vehicle: ref<VehicleObject>) -> Void {
    if this.m_groundCheckWait <= 0 || !IsDefined(vehicle) { return; };
    this.m_groundCheckWait -= 1;
    if this.m_groundCheckWait != 0 { return; };
    this.m_groundStable = this.MeasureGround(vehicle, "GROUND_SETTLED");
    if !this.m_groundValid {
      this.Log("WARNING", "GROUND_UNVERIFIED: position sample is invalid. Boarding blocked; send diagnostics.");
    } else {
      if !this.m_groundStable {
        this.Log("WARNING", "GROUND_DRIFT: valid position differs from requested clearance. Do not board if clipped.");
      } else {
        this.Log("INFO", "GROUND_PASS: clearance within tolerance. Confirm visible landing gear.");
        if this.m_landingSettling && this.m_airborne && this.m_arrivalStep >= 100 {
          this.m_airborne = false;
          let controller = vehicle.GetAccessoryController();
          if IsDefined(controller) {
            let ps = controller.GetPS() as vehicleControllerPS;
            if IsDefined(ps) { ps.SetDoorInteractionState(EVehicleDoor.seat_back_left, VehicleDoorInteractionState.Available); };
          };
          if this.IsLanded(vehicle) {
            this.m_landingSettling = false;
            this.LandingSound(vehicle);
            this.Door(vehicle, true);
            this.Log("INFO", "TAXI_PICKUP_READY: same native physics entity verified, entrance opened.");
          } else {
            this.m_airborne = true;
            if IsDefined(controller) {
              let ps = controller.GetPS() as vehicleControllerPS;
              if IsDefined(ps) { ps.SetDoorInteractionState(EVehicleDoor.seat_back_left, VehicleDoorInteractionState.Disabled); };
            };
            this.Log("ERROR", "TAXI_PICKUP_UNVERIFIED: live ground check failed.");
          };
        };
      };
    };
  }

  private func CanBoard(player: ref<PlayerPuppet>, vehicle: ref<VehicleObject>) -> Bool {
    if !IsDefined(vehicle) || !this.m_ready {
      this.Log("WARNING", "No ready test AV.");
      return false;
    };
    if !this.IsLanded(vehicle) { return false; };
    if this.m_groundCheckWait > 0 {
      this.Log("WARNING", "Board blocked: waiting for the clearance measurement.");
      return false;
    };
    this.m_groundStable = this.MeasureGround(vehicle, "GROUND_BOARD");
    if !this.m_groundStable {
      this.Log("WARNING", "Board blocked: ground clearance is outside tolerance or cannot be verified.");
      return false;
    };
    if Vector4.Distance(player.GetWorldPosition(), vehicle.GetWorldPosition()) > 8.0 {
      this.Log("WARNING", "Move closer to the test AV.");
      return false;
    };
    if GameInstance.GetMountingFacility(player.GetGame()).IsMountedToAnything(player.GetEntityID()) {
      this.Log("WARNING", "V is already mounted.");
      return false;
    };
    return true;
  }

  // Excalibur uses front-left as its physical entrance even for rear passengers.
  // Send the actual door enum name, bypassing passenger-seat-to-bone conversion.
  private func Door(vehicle: ref<VehicleObject>, opening: Bool) -> Void {
    if !IsDefined(vehicle) || !this.m_ready { return; };
    let ps = vehicle.GetVehiclePS();
    if !IsDefined(ps) {
      this.Log("ERROR", "VehicleComponentPS unavailable.");
      return;
    };
    let persistence = GameInstance.GetPersistencySystem(vehicle.GetGame());
    if opening {
      let evt = new VehicleDoorOpen();
      evt.slotID = n"seat_front_left";
      evt.forceScene = false;
      evt.shouldAutoClose = false;
      persistence.QueuePSEvent(ps.GetID(), ps.GetClassName(), evt);
      GameInstance.GetAudioSystem(vehicle.GetGame()).Play(n"v_av_rayfield_excalibur_door_open_fast", vehicle.GetEntityID(), n"vehicle_general_emitter");
      this.Log("INFO", "Physical front-left entrance open requested.");
    } else {
      let evt = new VehicleDoorClose();
      evt.slotID = n"seat_front_left";
      evt.forceScene = false;
      persistence.QueuePSEvent(ps.GetID(), ps.GetClassName(), evt);
      GameInstance.GetAudioSystem(vehicle.GetGame()).Play(n"v_av_rayfield_excalibur_door_close", vehicle.GetEntityID(), n"vehicle_general_emitter");
      this.Log("INFO", "Physical front-left entrance close requested.");
    };
    this.m_doorOpen = opening;
    this.m_doorWait = 6;
  }

  private func DoorCheck(vehicle: ref<VehicleObject>) -> Void {
    if this.m_doorWait <= 0 || !IsDefined(vehicle) { return; };
    this.m_doorWait -= 1;
    if this.m_doorWait != 0 { return; };
    let ps = vehicle.GetVehiclePS();
    if !IsDefined(ps) { return; };
    let state = ps.GetDoorState(EVehicleDoor.seat_front_left);
    if (this.m_doorOpen && Equals(state, VehicleDoorState.Open)) ||
       (!this.m_doorOpen && Equals(state, VehicleDoorState.Closed)) {
      this.Log("INFO", "Entrance door logical state confirmed. Visual animation must be checked in game.");
    } else {
      this.Log("WARNING", "Entrance door logical state did not reach the requested value.");
    };
  }

  // Use the stock passenger PSM's Delamain exit request, not a low-level
  // instant unmount or a teleport. Require a real workspot and a valid exit.
  private func IsPassengerState(player: ref<PlayerPuppet>) -> Bool {
    let bb = GameInstance.GetBlackboardSystem(player.GetGame()).GetLocalInstanced(player.GetEntityID(), GetAllBlackboardDefs().PlayerStateMachine);
    if !IsDefined(bb) { return false; };
    return Equals(bb.GetInt(GetAllBlackboardDefs().PlayerStateMachine.Vehicle), EnumInt(gamePSMVehicle.Passenger));
  }

  private func LogWorkspot(player: ref<PlayerPuppet>, vehicle: ref<VehicleObject>) -> Void {
    let ws = GameInstance.GetWorkspotSystem(player.GetGame());
    let active = ws.IsActorInWorkspot(player);
    let seated = ws.IsInVehicleWorkspot(vehicle, player, n"seat_back_left");
    this.Log("INFO", "WORKSPOT: active=" + ToString(active) + "; rearLeft=" + ToString(seated));
    if active {
      let info = ws.GetExtendedInfo(player);
      if IsDefined(info) {
        this.Log("INFO", "WORKSPOT_PHASE: entering=" + ToString(info.entering) + "; exiting=" + ToString(info.exiting) + "; active=" + ToString(info.isActive));
      };
      this.Log("INFO", "WORKSPOT_EXIT: hasExitNodes=" + ToString(ws.HasExitNodes(player, true, true, true)) + "; structural=" + ToString(ws.HasExitNodes(player, true, true, false)));
    };
  }

  private func CanStartAVDisembark(player: ref<PlayerPuppet>, vehicle: ref<VehicleObject>) -> Bool {
    if !this.IsPassengerState(player) { return false; };
    let ws = GameInstance.GetWorkspotSystem(player.GetGame());
    if !ws.IsInVehicleWorkspot(vehicle, player, n"seat_back_left") { return false; };
    let info = ws.GetExtendedInfo(player);
    if !IsDefined(info) || !info.isActive || info.entering || info.exiting { return false; };
    // Verify the custom cinematic exit node without road-car navmesh checks.
    if !ws.HasExitNodes(player, true, true, false) { return false; };
    let exitId = ws.GetIdOfExitAnim(player, n"sit_couch__AV_excalibur__01__to__stand__2h_on_sides__01__turn270__01");
    this.Log("INFO", "AV_EXIT_RESOURCE: named exit entry=" + ToString(exitId));
    if exitId != 6 { return false; };
    return true;
  }

  // Manual native input and the M1 Arc button share this guarded door/exit path.
  public func RequestManualExit(player: ref<PlayerPuppet>, vehicle: ref<VehicleObject>) -> Void {
    if !IsDefined(vehicle) || !Equals(vehicle.GetEntityID(), this.m_id) { return; };
    this.ExitPassenger(player, vehicle);
  }

  private func ObserveManualPassenger(player: ref<PlayerPuppet>, vehicle: ref<VehicleObject>) -> Void {
    if !IsDefined(vehicle) || !this.m_ready || this.m_returnValid || this.m_boardWait > 0 || this.m_exitWait > 0 { return; };
    if !this.IsPassengerState(player) { return; };
    let info = GameInstance.GetMountingFacility(player.GetGame()).GetMountingInfoSingleWithObjects(player);
    if !Equals(info.parentId, vehicle.GetEntityID()) || !Equals(info.slotId.id, n"seat_back_left") { return; };
    if this.m_boardGroundRejected { return; };
    if !this.MeasureGround(vehicle, "GROUND_MANUAL_BOARD") {
      this.m_boardGroundRejected = true;
      this.Log("ERROR", "TAXI_BOARD_POSITION_INVALID: ground check failed after mount; exit remains protected. No automatic teleport.");
      return;
    };
    this.m_boardPosition = vehicle.GetWorldPosition();
    this.m_returnValid = true;
    this.m_closeWait = 6;
    this.Log("INFO", "MANUAL_BOARD: native passenger detected; physical door will close.");
  }

  private func ExitPassenger(player: ref<PlayerPuppet>, vehicle: ref<VehicleObject>) -> Void {
    if !IsDefined(vehicle) || this.m_exitWait > 0 || this.m_boardWait > 0 { return; };
    if !this.IsLanded(vehicle) {
      if !this.m_airExitLogged {
        this.m_airExitLogged = true;
        this.Log("WARNING", "TAXI_EXIT_BLOCKED: AV not verified landed.");
      };
      return;
    };
    this.m_airExitLogged = false;
    let facility = GameInstance.GetMountingFacility(player.GetGame());
    let info = facility.GetMountingInfoSingleWithObjects(player);
    if !Equals(info.parentId, vehicle.GetEntityID()) || !Equals(info.slotId.id, n"seat_back_left") {
      this.Log("WARNING", "V is not mounted in the tracked rear passenger seat.");
      return;
    };
    if !this.m_returnValid && this.IsPassengerState(player) {
      this.m_boardPosition = vehicle.GetWorldPosition();
      this.m_returnValid = true;
    };
    if !this.m_returnValid || Vector4.Distance(vehicle.GetWorldPosition(), this.m_boardPosition) > 0.5 || !this.MeasureGround(vehicle, "GROUND_EXIT") {
      this.Log("ERROR", "EXIT_BLOCKED: stationary landing not verified. Reload the test save.");
      return;
    };
    // A diagnostic exit can recover a normally mounted, landed passenger even
    // if the boarding observer missed the original state update.
    this.LogWorkspot(player, vehicle);
    if !this.CanStartAVDisembark(player, vehicle) {
      this.Log("WARNING", "EXIT_BLOCKED: no active seated AV workspot. Reload the test save; no forced unmount will be attempted.");
      return;
    };
    this.m_closeWait = 0;
    this.m_exitRequested = false;
    this.m_exitAnimationSeen = false;
    this.m_exitTransitionRequested = false;
    this.m_exitElapsedTicks = 0;
    this.Door(vehicle, true);
    this.m_exitWait = 40;
    this.Log("INFO", "AV_DISEMBARK_PENDING: waiting for physical entrance.");
  }

  // The native workspot unmount applies the private exit node's movement
  // and releases the workspot. No timed detach or generic car exit recovery.
  private func ExitTick(player: ref<PlayerPuppet>, vehicle: ref<VehicleObject>) -> Void {
    if this.m_exitWait <= 0 { return; };
    this.m_exitWait -= 1;
    if !IsDefined(vehicle) { this.m_exitWait = 0; return; };
    let ps = vehicle.GetVehiclePS();
    let facility = GameInstance.GetMountingFacility(player.GetGame());
    let ws = GameInstance.GetWorkspotSystem(player.GetGame());
    let active = ws.IsActorInWorkspot(player);
    let mounted = facility.IsMountedToAnything(player.GetEntityID());
    let bb = GameInstance.GetBlackboardSystem(player.GetGame()).GetLocalInstanced(player.GetEntityID(), GetAllBlackboardDefs().PlayerStateMachine);
    let psm = -1;
    if IsDefined(bb) { psm = bb.GetInt(GetAllBlackboardDefs().PlayerStateMachine.Vehicle); };
    if this.m_exitRequested {
      this.m_exitElapsedTicks += 1;
      let phase = ws.GetExtendedInfo(player);
      let anim = n"None";
      let entryId: Int32 = -1;
      let instance = ws.GetWorkspotInstance(player.GetEntityID());
      if IsDefined(instance) {
        anim = instance.animName;
        let entry = instance.entryId;
        entryId = Cast<Int32>(entry.id);
      };
      let displacement = Vector4.Distance(player.GetWorldPosition(), this.m_exitStartPosition);
      let pos = player.GetWorldPosition();
      let flags = "";
      if IsDefined(phase) {
        flags = "; exiting=" + ToString(phase.exiting) + "; inMotion=" + ToString(phase.inMotion);
        if phase.exiting || phase.inMotion { this.m_exitAnimationSeen = true; };
      };
      this.Log("INFO", "AV_EXIT_TRACE: t=" + ToString(Cast<Float>(this.m_exitElapsedTicks) * 0.5) + "; anim=" + NameToString(anim) + "; entry=" + ToString(entryId) + "; active=" + ToString(active) + "; mounted=" + ToString(mounted) + "; psm=" + ToString(psm) + flags + "; delta=" + ToString(displacement) + "; xyz=" + ToString(pos.X) + "," + ToString(pos.Y) + "," + ToString(pos.Z));
    };
    if !mounted && !active && psm == EnumInt(gamePSMVehicle.Default) {
      if IsDefined(ps) { ps.SetShouldForceExitDelamain(false); };
      this.m_exitWait = 0;
      this.m_exitRequested = false;
      this.m_exitTransitionRequested = false;
      this.m_returnValid = false;
      this.m_boardGroundRejected = false;
      let displacement = Vector4.Distance(player.GetWorldPosition(), this.m_exitStartPosition);
      this.Log("INFO", "AV_EXIT_STATE_RELEASED: mount=false; workspot=false; vehiclePSM=0; displacement=" + ToString(displacement));
      if this.m_exitAnimationSeen && displacement >= 1.0 {
        this.m_reopenBlocked = true;
        this.Door(vehicle, false);
      } else {
        this.Log("WARNING", "AV_EXIT_POSITION_UNVERIFIED: door stays open.");
      };
      return;
    };
    if !this.m_exitRequested {
      let info = facility.GetMountingInfoSingleWithObjects(player);
      if !Equals(info.parentId, vehicle.GetEntityID()) || !Equals(info.slotId.id, n"seat_back_left") ||
         Vector4.Distance(vehicle.GetWorldPosition(), this.m_boardPosition) > 0.5 {
        this.m_exitWait = 0;
        this.Log("WARNING", "AV_DISEMBARK_CANCELLED: mounting relationship or landing position changed.");
        return;
      };
      if this.m_exitWait <= 34 && this.IsLanded(vehicle) && IsDefined(ps) && Equals(ps.GetDoorState(EVehicleDoor.seat_front_left), VehicleDoorState.Open) {
        if !this.CanStartAVDisembark(player, vehicle) {
          this.m_exitWait = 0;
          this.Log("WARNING", "AV_EXIT_RESOURCE_REJECTED: no valid private exit entry.");
          return;
        };
        this.m_exitStartPosition = player.GetWorldPosition();
        this.m_exitRequested = true;
        this.m_exitTransitionRequested = true;
        ps.SetShouldForceExitDelamain(true);
        this.Log("INFO", "AV_EXIT_REQUEST: requesting native release with private Excalibur exit workspot; no car direction selection.");
      };
    };
    if this.m_exitWait == 0 {
      if IsDefined(ps) { ps.SetShouldForceExitDelamain(false); };
      this.m_exitRequested = false;
      this.m_exitTransitionRequested = false;
      this.Log("ERROR", "AV_EXIT_TIMEOUT: mount=" + ToString(mounted) + "; workspot=" + ToString(active) + "; vehiclePSM=" + ToString(psm) + ". No forced unmount or teleport.");
    };
  }

  public func IsAVExitRequested(vehicle: ref<VehicleObject>) -> Bool {
    return IsDefined(vehicle) && Equals(vehicle.GetEntityID(), this.m_id) && this.m_exitTransitionRequested;
  }

  private func Board(player: ref<PlayerPuppet>, vehicle: ref<VehicleObject>) -> Void {
    if this.m_boardWait > 0 || this.m_exitWait > 0 || !this.CanBoard(player, vehicle) { return; };
    let facility = GameInstance.GetMountingFacility(player.GetGame());
    let slot = VehicleComponent.GetBackLeftPassengerSlotID();
    let occupant = facility.GetMountingInfoSingleWithIds(EntityID(), vehicle.GetEntityID(), slot);
    if EntityID.IsDefined(occupant.childId) {
      this.Log("WARNING", "Rear-left seat is occupied.");
      return;
    };
    let data = new MountEventData();
    data.slotName = slot.id;
    data.mountParentEntityId = vehicle.GetEntityID();
    data.isInstant = false;
    data.mountEventOptions = new MountEventOptions();
    let info: MountingInfo;
    info.childId = player.GetEntityID();
    info.parentId = vehicle.GetEntityID();
    info.slotId = slot;
    let request = new MountingRequest();
    request.lowLevelMountingInfo = info;
    request.mountData = data;
    // Match vanilla gamevehicleVehicleMountableComponent.MountEntityToSlot.
    this.m_boardPosition = vehicle.GetWorldPosition();
    this.m_returnValid = true;
    request.preservePositionAfterMounting = true;
    facility.Mount(request);
    this.m_boardWait = 24;
    this.Log("INFO", "Rear-left passenger mount requested. Waiting for actual mount confirmation.");
  }

  private func LogPassengerState(player: ref<PlayerPuppet>) -> Void {
    let bb = GameInstance.GetBlackboardSystem(player.GetGame()).GetLocalInstanced(player.GetEntityID(), GetAllBlackboardDefs().PlayerStateMachine);
    if !IsDefined(bb) {
      this.Log("WARNING", "PSM_UNAVAILABLE: no local player state machine blackboard.");
      return;
    };
    let state = bb.GetInt(GetAllBlackboardDefs().PlayerStateMachine.Vehicle);
    let high = bb.GetInt(GetAllBlackboardDefs().PlayerStateMachine.HighLevel);
    if Equals(state, EnumInt(gamePSMVehicle.Passenger)) {
      this.Log("INFO", "PSM_PASSENGER: native passenger state confirmed; vehicle=" + ToString(state) + "; highLevel=" + ToString(high));
    } else {
      this.Log("INFO", "PSM_OTHER: vehicle=" + ToString(state) + "; highLevel=" + ToString(high));
    };
  }

  private func Inspect(player: ref<PlayerPuppet>, vehicle: ref<VehicleObject>) -> Void {
    if !IsDefined(vehicle) {
      this.Log("INFO", "No materialized test AV.");
      return;
    };
    if Equals(vehicle.GetRecordID(), t"Vehicle.m1nsook_delamain_av") {
      this.Log("INFO", "Record confirmed: Vehicle.m1nsook_delamain_av.");
    } else {
      this.Log("ERROR", "Unexpected vehicle record.");
    };
    this.MeasureGround(vehicle, "GROUND_STATUS");
    this.LogPassengerState(player);
    this.LogWorkspot(player, vehicle);
    let mount = GameInstance.GetMountingFacility(player.GetGame()).GetMountingInfoSingleWithObjects(player);
    if Equals(mount.parentId, vehicle.GetEntityID()) {
      this.Log("INFO", "V is mounted in slot: " + NameToString(mount.slotId.id));
    } else {
      this.Log("INFO", "V is not mounted in this AV.");
    };
  }

  private func Remove(vehicle: ref<VehicleObject>) -> Void {
    if !EntityID.IsDefined(this.m_id) { return; };
    if this.m_arriving || this.m_boardWait > 0 || this.m_exitWait > 0 {
      this.Log("WARNING", "Boarding or exit is pending. Wait before removing.");
      return;
    };
    if IsDefined(vehicle) {
      let facility = GameInstance.GetMountingFacility(this.GetGameInstance());
      let mounts = facility.GetMountingInfoMultipleWithObjects(null, vehicle);
      if ArraySize(mounts) > 0 || vehicle.IsPlayerMounted() {
        this.Log("WARNING", "AV is occupied. Exit normally before removal.");
        return;
      };
    };
    this.Log("INFO", "DELETE_BEFORE: reason=manual removal");
    if IsDefined(vehicle) { DelamainAV_DiagnosticSnapshot(vehicle, "before manual remove"); };
    DelamainAV_PhysicsStop(this.m_id);
    if GameInstance.GetDynamicEntitySystem().DeleteEntity(this.m_id) {
      this.m_id = EntityID();
      this.m_wait = 0;
      this.m_ready = false;
      this.m_doorWait = 0;
      this.m_groundCheckWait = 0;
      this.m_groundStable = false;
      this.m_groundValid = false;
      this.m_closeWait = 0;
      this.m_returnValid = false;
      this.m_airborne = false;
      this.m_arriving = false;
      this.m_reopenBlocked = false;
      this.m_landingSettling = false;
      this.m_nativeHolding = false;
      this.m_boardGroundRejected = false;
      this.Log("INFO", "Test AV removed.");
    } else {
      this.Log("ERROR", "DeleteEntity failed; retaining ID to avoid duplicate spawn.");
    };
  }

  public func Tick() -> Void {
    if !this.m_running { return; };
    let spawn = M1Arc_ConsumeAction("m1nsook.delamain-av", "spawn");
    let exit = M1Arc_ConsumeAction("m1nsook.delamain-av", "exit");
    let summon = this.m_summonQueued;
    this.m_summonQueued = false;
    let status = M1Arc_ConsumeAction("m1nsook.delamain-av", "status");
    let remove = M1Arc_ConsumeAction("m1nsook.delamain-av", "remove");
    let player = GetPlayer(this.GetGameInstance()) as PlayerPuppet;
    if IsDefined(player) && !player.IsDead() {
      let vehicle = this.Vehicle();
      if this.m_wait > 0 {
        DelamainAV_DiagnosticLog("TRACE", "ENTITY_WAIT: remaining=" + ToString(this.m_wait) + "; found=" + ToString(IsDefined(vehicle)));
        if IsDefined(vehicle) {
          this.m_wait = 0;
          this.Configure(vehicle);
        } else {
          this.m_wait -= 1;
          if this.m_wait == 0 {
            this.Log("ERROR", "AV did not materialize in 10 seconds. Use Remove before retrying.");
          };
        };
      };
      // DAV keeps zero velocity on the landed vehicle. Keep our command
      // lease alive only for the exact tracked entity; never issue flight
      // commands with a passenger aboard.
      if this.m_nativeHolding && !this.m_arriving && IsDefined(vehicle) {
        let hold = DelamainAV_PhysicsPulse(vehicle, 0.0);
        if hold < 0 {
          this.m_nativeHolding = false;
          this.m_groundStable = false;
          this.Log("ERROR", "TAXI_NATIVE_HOLD_LOST: code=" + ToString(hold));
        };
      };
      this.GroundCheck(vehicle);
      this.DoorCheck(vehicle);
      this.ObserveManualPassenger(player, vehicle);
      this.ExitTick(player, vehicle);
      this.AutoDoors(player, vehicle);
      if this.m_closeWait > 0 {
        this.m_closeWait -= 1;
        if this.m_closeWait == 0 && IsDefined(vehicle) && vehicle.IsPlayerMounted() && this.m_exitWait == 0 { this.Door(vehicle, false); };
      };
      if this.m_boardWait > 0 {
        let info = GameInstance.GetMountingFacility(player.GetGame()).GetMountingInfoSingleWithObjects(player);
        if IsDefined(vehicle) && Equals(info.parentId, vehicle.GetEntityID()) && Equals(info.slotId.id, n"seat_back_left") && this.IsPassengerState(player) {
          this.m_boardWait = 0;
          this.Log("INFO", "PASS: V mounted in rear-left passenger seat.");
          this.LogPassengerState(player);
          this.LogWorkspot(player, vehicle);
          this.m_closeWait = 6;
        } else {
          this.m_boardWait -= 1;
          if this.m_boardWait == 0 {
            this.Log("ERROR", "Rear-left passenger PSM not confirmed within 12 seconds. Reload if mounted.");
          };
        };
      };
      if remove { this.Remove(vehicle); }
      else {
        if summon { this.Spawn(player, true); } else { if spawn { this.Spawn(player, false); }; };
        if exit { this.ExitPassenger(player, vehicle); };
        if status { this.Inspect(player, vehicle); };
      };
    } else {
      if spawn || summon || status || remove || exit {
        this.Log("WARNING", "Load a save with V alive before testing.");
      };
    };
    this.Schedule();
  }
}

public class DelamainAVArrivalTick extends DelayCallback {
  public let owner: wref<DelamainAVSystem>;
  public func Call() -> Void {
    if IsDefined(this.owner) { this.owner.ArrivalTick(); };
  }
}

public class DelamainAVTick extends DelayCallback {
  public let owner: wref<DelamainAVSystem>;
  public func Call() -> Void {
    if IsDefined(this.owner) { this.owner.Tick(); };
  }
}

 // Only this mod's private AV record uses the special route. Every other vehicle
 // retains all vanilla behavior, including the original q203 Delamain AV.
 public static func IsCustomDelamainAV(vehicle: wref<VehicleObject>) -> Bool {
   return IsDefined(vehicle) && Equals(vehicle.GetRecordID(), t"Vehicle.m1nsook_delamain_av");
 }

 @wrapMethod(VehicleComponentPS)
 public func OnVehicleDoorOpen(evt: ref<VehicleDoorOpen>) -> EntityNotificationType {
   if IsCustomDelamainAV(this.GetOwnerEntity()) && Equals(evt.slotID, n"seat_back_left") {
     evt.slotID = n"seat_front_left";
     // The mod observes the passenger state and closes the door after entry.
     evt.shouldAutoClose = false;
   };
   return wrappedMethod(evt);
 }

 @wrapMethod(VehicleComponentPS)
 public func OnVehicleDoorClose(evt: ref<VehicleDoorClose>) -> EntityNotificationType {
   if IsCustomDelamainAV(this.GetOwnerEntity()) && Equals(evt.slotID, n"seat_back_left") {
     evt.slotID = n"seat_front_left";
   };
   return wrappedMethod(evt);
 }

 @wrapMethod(VehicleEventsTransition)
 protected func HandleExitRequest(timeDelta: Float, stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>) -> Bool {
   let vehicle = scriptInterface.owner as VehicleObject;
   if !IsCustomDelamainAV(vehicle) { return wrappedMethod(timeDelta, stateContext, scriptInterface); };
   let player = scriptInterface.executionOwner as PlayerPuppet;
   if !IsDefined(player) { return false; };
   let ps = vehicle.GetVehiclePS();
   if !IsDefined(ps) { return false; };
   let info = scriptInterface.GetMountingInfo(player);
   if NotEquals(info.slotId.id, n"seat_back_left") { return false; };
   // The AV-specific controller opens the entrance and verifies the private exit node first.
   if ps.ShouldForceExitDelamain() {
     ps.SetShouldForceExitDelamain(false);
     let system = GameInstance.GetScriptableSystemsContainer(player.GetGame()).Get(n"DelamainAV.DelamainAVSystem") as DelamainAVSystem;
     if !IsDefined(system) || !system.IsAVExitRequested(vehicle) || !system.IsLanded(vehicle) { return false; };
     this.DeactivateTimeDilationCW(stateContext, scriptInterface);
     stateContext.RemovePermanentIntParameter(n"vehUnmountDir");
     this.SetIsCar(stateContext, false);
     this.SendIsCar(stateContext, scriptInterface);
     M1Arc_Log("m1nsook.delamain-av", "INFO", "AV_EXIT_PSM: VehiclePassenger.isCar=false; private exit requested.");
     stateContext.SetPermanentBoolParameter(n"validExitRequest", true, true);
     return true;
   };
   if this.GetInStateTime() >= 0.3 && scriptInterface.IsActionJustPressed(n"Exit") && !this.IsExitVehicleBlocked(scriptInterface) {
     let system = GameInstance.GetScriptableSystemsContainer(player.GetGame()).Get(n"DelamainAV.DelamainAVSystem") as DelamainAVSystem;
     if IsDefined(system) { system.RequestManualExit(player, vehicle); };
   };
   return false;
 }

 @wrapMethod(VehicleTransition)
 protected func ExitWorkspot(stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>, isInstant: Bool, opt upsideDown: Bool) -> Void {
   if !IsCustomDelamainAV(scriptInterface.owner as VehicleObject) || isInstant || upsideDown {
     wrappedMethod(stateContext, scriptInterface, isInstant, upsideDown);
     return;
   };
   let player = scriptInterface.executionOwner as PlayerPuppet;
   if IsDefined(player) {
     let system = GameInstance.GetScriptableSystemsContainer(player.GetGame()).Get(n"DelamainAV.DelamainAVSystem") as DelamainAVSystem;
     if IsDefined(system) && system.IsAVExitRequested(scriptInterface.owner as VehicleObject) {
       this.SetSide(stateContext, scriptInterface);
       // This is the actual workspot release, not a pose-only jump. Pass the
       // private exit node's slot explicitly, with identity offsets.
       scriptInterface.GetWorkspotSystem().UnmountFromVehicle(scriptInterface.owner, scriptInterface.executionOwner, false, new Vector4(0.0, 0.0, 0.0, 0.0), new Quaternion(0.0, 0.0, 0.0, 1.0), n"default");
       M1Arc_Log("m1nsook.delamain-av", "INFO", "AV_EXIT_WORKSPOT: native release using private q000 cinematic exit node.");
       return;
     };
   };
   wrappedMethod(stateContext, scriptInterface, isInstant, upsideDown);
 }

 @wrapMethod(ExitingEvents)
 public func OnUpdate(timeDelta: Float, stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>) -> Void {
   if !IsCustomDelamainAV(scriptInterface.owner as VehicleObject) {
     wrappedMethod(timeDelta, stateContext, scriptInterface);
     return;
   };
   // Preserve the stock delayed animation start; omit its road-car geometry
   // recovery branch which can teleport V above the Excalibur.
   let exitDelay = this.GetVehicleDataPackage(stateContext).ExitDelay();
   let started = stateContext.GetPermanentBoolParameter(n"startedExiting");
   if exitDelay > 0.0 && this.GetInStateTime() >= exitDelay && !started.value {
     this.StartExiting(stateContext, scriptInterface);
   };
   // ExitingDecisions/ExitEvents remain native and complete the unmount.
 }

 // Hard interlock at the actual native mount request, not merely a disabled UI
 // choice. The temporary arriving AV cannot accept a passenger.
 @wrapMethod(gamevehicleVehicleMountableComponent)
 protected func MountEntityToSlot(parentID: EntityID, childId: EntityID, slot: CName, mountType: MountType) -> Void {
   let vehicle = this.GetEntity() as VehicleObject;
   if IsCustomDelamainAV(vehicle) {
     let system = GameInstance.GetScriptableSystemsContainer(vehicle.GetGame()).Get(n"DelamainAV.DelamainAVSystem") as DelamainAVSystem;
     if !IsDefined(system) || !system.IsTrackedLanded(vehicle) || NotEquals(slot, n"seat_back_left") {
       M1Arc_Log("m1nsook.delamain-av", "WARNING", "TAXI_MOUNT_BLOCKED: pickup not ready or invalid seat.");
       return;
     };
   };
   wrappedMethod(parentID, childId, slot, mountType);
 }
