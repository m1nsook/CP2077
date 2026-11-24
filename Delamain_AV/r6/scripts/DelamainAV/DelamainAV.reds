module DelamainAV

// M1: stationary prototype only. No flight, input hooks, vehicle ownership,
// quest facts, global vehicle hooks or mutation of the original quest record.
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
  private let m_returnPosition: Vector4;
  private let m_returnOrientation: Quaternion;
  private let m_boardPosition: Vector4;

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
    spec.position = player.GetWorldPosition() + forward * 9.0;
    spec.orientation = player.GetWorldOrientation();
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
        ps.SetDoorInteractionState(EVehicleDoor.seat_back_right, VehicleDoorInteractionState.Available);
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
  }

  private func CanBoard(player: ref<PlayerPuppet>, vehicle: ref<VehicleObject>) -> Bool {
    if !IsDefined(vehicle) || !this.m_ready {
      this.Log("WARNING", "No ready test AV.");
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

  // Stationary prototype only. Return to the on-foot position captured before
  // boarding, rather than guessing an exit location inside the vehicle mesh.
  private func ExitPassenger(player: ref<PlayerPuppet>, vehicle: ref<VehicleObject>) -> Void {
    if !IsDefined(vehicle) || this.m_exitWait > 0 || this.m_boardWait > 0 { return; };
    let facility = GameInstance.GetMountingFacility(player.GetGame());
    let info = facility.GetMountingInfoSingleWithObjects(player);
    if !Equals(info.parentId, vehicle.GetEntityID()) || !Equals(info.slotId.id, n"seat_back_left") {
      this.Log("WARNING", "V is not mounted in the tracked rear passenger seat.");
      return;
    };
    if !this.m_returnValid || Vector4.Distance(vehicle.GetWorldPosition(), this.m_boardPosition) > 1.5 {
      this.Log("ERROR", "No valid stationary return position. Reload the test save rather than forcing exit.");
      return;
    };
    this.m_closeWait = 0;
    this.Door(vehicle, true);
    // Allow the physical entrance to open before requesting instant unmount.
    this.m_exitWait = 20;
    this.Log("INFO", "Controlled passenger exit scheduled.");
  }

  private func ExitTick(player: ref<PlayerPuppet>, vehicle: ref<VehicleObject>) -> Void {
    if this.m_exitWait <= 0 { return; };
    this.m_exitWait -= 1;
    if !IsDefined(vehicle) || Vector4.Distance(vehicle.GetWorldPosition(), this.m_boardPosition) > 1.5 {
      this.m_exitWait = 0;
      this.Log("ERROR", "Exit cancelled: vehicle moved or disappeared.");
      return;
    };
    let facility = GameInstance.GetMountingFacility(player.GetGame());
    if !facility.IsMountedToAnything(player.GetEntityID()) {
      GameInstance.GetTeleportationFacility(player.GetGame()).Teleport(player, this.m_returnPosition, Quaternion.ToEulerAngles(this.m_returnOrientation));
      this.m_exitWait = 0;
      this.m_returnValid = false;
      this.Log("INFO", "PASS: V unmounted; return to pre-boarding position requested.");
      return;
    };
    let info = facility.GetMountingInfoSingleWithObjects(player);
    if !Equals(info.parentId, vehicle.GetEntityID()) || !Equals(info.slotId.id, n"seat_back_left") {
      this.m_exitWait = 0;
      this.Log("WARNING", "Exit cancelled: mounting relationship changed.");
      return;
    };
    if this.m_exitWait == 15 {
      let request = new UnmountingRequest();
      request.lowLevelMountingInfo = info;
      request.mountData = new MountEventData();
      request.mountData.isInstant = true;
      facility.Unmount(request);
      this.Log("INFO", "Instant passenger unmount requested.");
    };
    if this.m_exitWait == 0 {
      this.Log("ERROR", "Unmount not confirmed. Do not remove the AV; reload test save.");
    };
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
    // Match normal mounting semantics; preservePosition is not used by DAV.
    this.m_returnPosition = player.GetWorldPosition();
    this.m_returnOrientation = player.GetWorldOrientation();
    this.m_boardPosition = vehicle.GetWorldPosition();
    this.m_returnValid = true;
    request.preservePositionAfterMounting = false;
    facility.Mount(request);
    this.m_boardWait = 16;
    this.Log("INFO", "Rear-left passenger mount requested. Waiting for actual mount confirmation.");
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
      this.DoorCheck(vehicle);
      this.ExitTick(player, vehicle);
      if this.m_closeWait > 0 {
        this.m_closeWait -= 1;
        if this.m_closeWait == 0 && IsDefined(vehicle) && vehicle.IsPlayerMounted() && this.m_exitWait == 0 { this.Door(vehicle, false); };
      };
      if this.m_boardWait > 0 {
        let info = GameInstance.GetMountingFacility(player.GetGame()).GetMountingInfoSingleWithObjects(player);
        if IsDefined(vehicle) && Equals(info.parentId, vehicle.GetEntityID()) && Equals(info.slotId.id, n"seat_back_left") {
          this.m_boardWait = 0;
          this.Log("INFO", "PASS: V mounted in rear-left passenger seat.");
          this.m_closeWait = 6;
        } else {
          this.m_boardWait -= 1;
          if this.m_boardWait == 0 {
            this.Log("ERROR", "Rear-left passenger mount not confirmed within 8 seconds.");
          };
        };
      };
      if remove { this.Remove(vehicle); }
      else {
        if spawn { this.Spawn(player); };
        if exit { this.ExitPassenger(player, vehicle); }
        else {
          if open && IsDefined(vehicle) && this.m_exitWait == 0 && Vector4.Distance(player.GetWorldPosition(), vehicle.GetWorldPosition()) <= 8.0 { this.Door(vehicle, true); };
          if close && IsDefined(vehicle) && this.m_exitWait == 0 && Vector4.Distance(player.GetWorldPosition(), vehicle.GetWorldPosition()) <= 8.0 { this.Door(vehicle, false); };
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
