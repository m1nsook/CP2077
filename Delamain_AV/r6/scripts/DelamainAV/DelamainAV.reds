module DelamainAV

// M1: stationary prototype only. No flight, input hooks, vehicle ownership,
// quest facts or mutation of the original quest record. PSM/door wrappers below
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
  private let m_exitCleanupRequested: Bool;
  private let m_exitElapsedTicks: Int32;
  private let m_exitStartPosition: Vector4;
  private let m_boardPosition: Vector4;
  private let m_groundZ: Float;
  private let m_clearance: Float;
  private let m_groundCheckWait: Int32;
  private let m_groundStable: Bool;
  private let m_groundValid: Bool;
  private let m_spawnTarget: Vector4;

  private func OnAttach() -> Void {
    this.m_running = true;
    this.Schedule();
  }

  private func OnDetach() -> Void {
    // Nonpersistent Codeware entities are cleared by world teardown.
    // Never delete a mounted vehicle during teardown.
    this.m_running = false;
  }

  private func Schedule() -> Void {
    if !this.m_running { return; }
    let cb = new DelamainAVTick();
    cb.owner = this;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 0.5, false);
  }

  private func Log(level: String, message: String) -> Void {
    M1Arc_Log("m1nsook.delamain-av", level, message);
  }

  private func Vehicle() -> wref<VehicleObject> {
    if !EntityID.IsDefined(this.m_id) { return null; }
    return GameInstance.FindEntityByID(this.GetGameInstance(), this.m_id) as VehicleObject;
  }

  private func Spawn(player: ref<PlayerPuppet>) -> Void {
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
    let hit: TraceResult;
    let spatial = GameInstance.GetSpatialQueriesSystem(player.GetGame());
    if !spatial.SyncRaycastByCollisionGroup(rayStart, rayEnd, n"Static", hit, true, false) {
      this.Log("WARNING", "GROUND_NO_HIT: No static surface below pickup point. Choose flat open ground.");
      return;
    };
    if hit.normal.Z < 0.95 {
      this.Log("WARNING", "GROUND_SLOPE: Surface too steep for stationary test.");
      return;
    };
    this.m_groundZ = hit.position.Z;
    this.m_clearance = M1Arc_GetFloat("m1nsook.delamain-av", "ground_clearance");
    if this.m_clearance < 0.75 || this.m_clearance > 2.0 { this.m_clearance = 1.0; };
    target.Z = this.m_groundZ + this.m_clearance;
    spec.position = target;
    spec.orientation = Quaternion.BuildFromDirectionVector(forward);
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
    this.m_id = system.CreateEntity(spec);
    if !EntityID.IsDefined(this.m_id) {
      this.Log("ERROR", "CreateEntity returned an undefined ID.");
      return;
    };
    this.m_wait = 20;
    this.m_ready = false;
    this.m_returnValid = false;
    this.Log("INFO", "AV creation requested. Waiting up to 10 seconds for the entity.");
  }

  private func Configure(vehicle: ref<VehicleObject>) -> Void {
    let controller = vehicle.GetAccessoryController();
    if IsDefined(controller) {
      let ps = controller.GetPS() as vehicleControllerPS;
      if IsDefined(ps) {
        ps.SetDoorInteractionState(EVehicleDoor.seat_front_left, VehicleDoorInteractionState.Disabled);
        ps.SetDoorInteractionState(EVehicleDoor.seat_front_right, VehicleDoorInteractionState.Disabled);
        ps.SetDoorInteractionState(EVehicleDoor.seat_back_left, VehicleDoorInteractionState.Available);
        ps.SetDoorInteractionState(EVehicleDoor.seat_back_right, VehicleDoorInteractionState.Disabled);
        ps.SetDoorInteractionState(EVehicleDoor.trunk, VehicleDoorInteractionState.Disabled);
        ps.SetDoorInteractionState(EVehicleDoor.hood, VehicleDoorInteractionState.Disabled);
      };
    };
    let av = vehicle as AVObject;
    if IsDefined(av) {
      av.TurnOffThrusters();
      this.Log("INFO", "Spawned custom AVObject. Thrusters off.");
    } else {
      this.Log("WARNING", "Entity is a VehicleObject but not an AVObject. Flight is not available.");
    };
    this.m_ready = true;
    this.m_groundCheckWait = 6;
    this.MeasureGround(vehicle, "GROUND_INITIAL");
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
      };
    };
  }

  private func CanBoard(player: ref<PlayerPuppet>, vehicle: ref<VehicleObject>) -> Bool {
    if !IsDefined(vehicle) || !this.m_ready {
      this.Log("WARNING", "No ready test AV.");
      return false;
    };
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
      this.Log("INFO", "Physical front-left entrance open requested.");
    } else {
      let evt = new VehicleDoorClose();
      evt.slotID = n"seat_front_left";
      evt.forceScene = false;
      persistence.QueuePSEvent(ps.GetID(), ps.GetClassName(), evt);
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
    // The private AV has a finite disembark clip, not a road-car exit node.
    // Stationary landing and seating are validated before playing the clip.
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
    if !this.MeasureGround(vehicle, "GROUND_MANUAL_BOARD") { return; };
    this.m_boardPosition = vehicle.GetWorldPosition();
    this.m_returnValid = true;
    this.m_closeWait = 6;
    this.Log("INFO", "MANUAL_BOARD: native passenger detected; physical door will close.");
  }

  private func ExitPassenger(player: ref<PlayerPuppet>, vehicle: ref<VehicleObject>) -> Void {
    if !IsDefined(vehicle) || this.m_exitWait > 0 || this.m_boardWait > 0 { return; };
    let facility = GameInstance.GetMountingFacility(player.GetGame());
    let info = facility.GetMountingInfoSingleWithObjects(player);
    if !Equals(info.parentId, vehicle.GetEntityID()) || !Equals(info.slotId.id, n"seat_back_left") {
      this.Log("WARNING", "V is not mounted in the tracked rear passenger seat.");
      return;
    };
    if !this.m_returnValid || Vector4.Distance(vehicle.GetWorldPosition(), this.m_boardPosition) > 0.5 || !this.MeasureGround(vehicle, "GROUND_EXIT") {
      this.Log("ERROR", "EXIT_BLOCKED: stationary landing not verified. Reload the test save.");
      return;
    };
    this.LogWorkspot(player, vehicle);
    if !this.CanStartAVDisembark(player, vehicle) {
      this.Log("WARNING", "EXIT_BLOCKED: no active seated AV workspot. Reload the test save; no forced unmount will be attempted.");
      return;
    };
    this.m_closeWait = 0;
    this.m_exitRequested = false;
    this.m_exitAnimationSeen = false;
    this.m_exitCleanupRequested = false;
    this.m_exitElapsedTicks = 0;
    this.Door(vehicle, true);
    this.m_exitWait = 30;
    this.Log("INFO", "AV_DISEMBARK_PENDING: waiting for physical entrance.");
  }

  // AV lifecycle: door -> cinematic workspot clip -> native mount cleanup.
  // Do not enter the car exit PSM while the passenger animation is playing.
  private func ExitTick(player: ref<PlayerPuppet>, vehicle: ref<VehicleObject>) -> Void {
    if this.m_exitWait <= 0 { return; };
    this.m_exitWait -= 1;
    if !IsDefined(vehicle) { this.m_exitWait = 0; return; };
    let ps = vehicle.GetVehiclePS();
    let facility = GameInstance.GetMountingFacility(player.GetGame());
    let ws = GameInstance.GetWorkspotSystem(player.GetGame());
    let active = ws.IsActorInWorkspot(player);
    if this.m_exitRequested {
      this.m_exitElapsedTicks += 1;
      let phase = ws.GetExtendedInfo(player);
      if IsDefined(phase) && phase.inMotion && !this.m_exitAnimationSeen {
        this.m_exitAnimationSeen = true;
        this.Log("INFO", "AV_DISEMBARK_MOTION: workspot reports root motion.");
      };
    };
    if !facility.IsMountedToAnything(player.GetEntityID()) {
      if IsDefined(ps) { ps.SetShouldForceExitDelamain(false); };
      this.m_exitWait = 0;
      this.m_exitRequested = false;
      this.m_returnValid = false;
      let elapsed = Cast<Float>(this.m_exitElapsedTicks) * 0.5;
      let displacement = Vector4.Distance(player.GetWorldPosition(), this.m_exitStartPosition);
      this.Log("INFO", "AV_DETACHED: elapsed=" + ToString(elapsed) + "; motionObserved=" + ToString(this.m_exitAnimationSeen) + "; displacement=" + ToString(displacement));
      if this.m_exitCleanupRequested && !active {
        this.Door(vehicle, false);
        this.Log("INFO", "AV_CLEANUP_COMPLETE: clip ended before detach. Exterior clearance still needs runtime confirmation.");
      } else {
        this.Log("WARNING", "AV_EARLY_DETACH: native mount cleared before validated clip completion; door remains open.");
      };
      this.m_exitCleanupRequested = false;
      this.LogPassengerState(player);
      return;
    };
    let info = facility.GetMountingInfoSingleWithObjects(player);
    if !Equals(info.parentId, vehicle.GetEntityID()) || !Equals(info.slotId.id, n"seat_back_left") ||
       Vector4.Distance(vehicle.GetWorldPosition(), this.m_boardPosition) > 0.5 {
      if IsDefined(ps) { ps.SetShouldForceExitDelamain(false); };
      this.m_exitWait = 0;
      this.m_exitRequested = false;
      this.m_exitCleanupRequested = false;
      this.Log("WARNING", "AV_DISEMBARK_CANCELLED: mounting relationship or landing position changed.");
      return;
    };
    if !this.m_exitRequested && this.m_exitWait <= 24 {
      if IsDefined(ps) && Equals(ps.GetDoorState(EVehicleDoor.seat_front_left), VehicleDoorState.Open) {
        if !this.CanStartAVDisembark(player, vehicle) {
          this.m_exitWait = 0;
          this.Log("WARNING", "AV_DISEMBARK_BLOCKED: active seated workspot no longer verified.");
          return;
        };
        this.m_exitStartPosition = player.GetWorldPosition();
        this.m_exitRequested = true;
        // The private workspot's finite workAnimClip has entry ID 6.
        // The quest uses this same 5-second skeletal clip before unmounting.
        ws.SendJumpCommandEnt(player, 6, true);
        this.Log("INFO", "AV_DISEMBARK_PLAY: cinematic clip 6; passenger mount retained.");
      };
    };
    if this.m_exitRequested && !this.m_exitCleanupRequested && !active {
      let elapsed = Cast<Float>(this.m_exitElapsedTicks) * 0.5;
      let displacement = Vector4.Distance(player.GetWorldPosition(), this.m_exitStartPosition);
      if elapsed < 4.5 || displacement < 1.0 {
        this.m_exitWait = 0;
        this.m_exitRequested = false;
        this.Log("ERROR", "AV_DISEMBARK_INTERRUPTED: elapsed=" + ToString(elapsed) + "; displacement=" + ToString(displacement) + ". No car-exit fallback.");
        return;
      };
      if IsDefined(ps) {
        this.m_exitCleanupRequested = true;
        ps.SetShouldForceExitDelamain(true);
        this.Log("INFO", "AV_DISEMBARK_CLIP_ENDED: requesting native mount cleanup only.");
      };
    };
    if this.m_exitWait == 0 {
      if IsDefined(ps) { ps.SetShouldForceExitDelamain(false); };
      this.m_exitRequested = false;
      this.m_exitCleanupRequested = false;
      this.LogWorkspot(player, vehicle);
      this.Log("ERROR", "AV_DISEMBARK_TIMEOUT: no validated clip completion. No forced exit or teleport fallback.");
    };
  }

  public func IsAVCleanupReady(vehicle: ref<VehicleObject>) -> Bool {
    return IsDefined(vehicle) && Equals(vehicle.GetEntityID(), this.m_id) && this.m_exitCleanupRequested;
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
    if this.m_boardWait > 0 || this.m_exitWait > 0 {
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
      this.Log("INFO", "Test AV removed.");
    } else {
      this.Log("ERROR", "DeleteEntity failed; retaining ID to avoid duplicate spawn.");
    };
  }

  public func Tick() -> Void {
    if !this.m_running { return; };
    let spawn = M1Arc_ConsumeAction("m1nsook.delamain-av", "spawn");
    let open = M1Arc_ConsumeAction("m1nsook.delamain-av", "open");
    let board = M1Arc_ConsumeAction("m1nsook.delamain-av", "board");
    let close = M1Arc_ConsumeAction("m1nsook.delamain-av", "close");
    let exit = M1Arc_ConsumeAction("m1nsook.delamain-av", "exit");
    let status = M1Arc_ConsumeAction("m1nsook.delamain-av", "status");
    let remove = M1Arc_ConsumeAction("m1nsook.delamain-av", "remove");
    let player = GetPlayer(this.GetGameInstance()) as PlayerPuppet;
    if IsDefined(player) && !player.IsDead() {
      let vehicle = this.Vehicle();
      if this.m_wait > 0 {
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
      this.GroundCheck(vehicle);
      this.DoorCheck(vehicle);
      this.ObserveManualPassenger(player, vehicle);
      this.ExitTick(player, vehicle);
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
        if spawn { this.Spawn(player); };
        if exit { this.ExitPassenger(player, vehicle); }
        else {
          if open && IsDefined(vehicle) && this.m_exitWait == 0 && this.m_boardWait == 0 && Vector4.Distance(player.GetWorldPosition(), vehicle.GetWorldPosition()) <= 8.0 { this.Door(vehicle, true); };
          if close && IsDefined(vehicle) && this.m_exitWait == 0 && this.m_boardWait == 0 && Vector4.Distance(player.GetWorldPosition(), vehicle.GetWorldPosition()) <= 8.0 { this.Door(vehicle, false); };
          if board { this.Board(player, vehicle); };
        };
        if status { this.Inspect(player, vehicle); };
      };
    } else {
      if spawn || open || board || close || exit || status || remove {
        this.Log("WARNING", "Load a save with V alive before testing.");
      };
    };
    this.Schedule();
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
   // AV controller owns disembark motion. Native PSM is requested only after the clip ends.
   if ps.ShouldForceExitDelamain() {
     ps.SetShouldForceExitDelamain(false);
     let system = GameInstance.GetScriptableSystemsContainer(player.GetGame()).Get(n"DelamainAV.DelamainAVSystem") as DelamainAVSystem;
     if !IsDefined(system) || !system.IsAVCleanupReady(vehicle) { return false; };
     this.DeactivateTimeDilationCW(stateContext, scriptInterface);
     stateContext.RemovePermanentIntParameter(n"vehUnmountDir");
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
   // No vehicle exit animation here. AV controller has already played it.
   // Native ExitingDecisions observes the completed workspot and ExitEvents
   // clears the mount, camera, input and player state.
   let player = scriptInterface.executionOwner as PlayerPuppet;
   if IsDefined(player) {
     let system = GameInstance.GetScriptableSystemsContainer(player.GetGame()).Get(n"DelamainAV.DelamainAVSystem") as DelamainAVSystem;
     if IsDefined(system) && system.IsAVCleanupReady(scriptInterface.owner as VehicleObject) { return; };
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
