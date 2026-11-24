module DelamainAV

// M1: stationary prototype only. No flight, input hooks, vehicle ownership,
// quest facts, global vehicle hooks or mutation of the original quest record.
public class DelamainAVSystem extends ScriptableSystem {
  private let m_running: Bool;
  private let m_id: EntityID;
  private let m_wait: Int32;
  private let m_boardWait: Int32;
  private let m_ready: Bool;

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

  private func Board(player: ref<PlayerPuppet>, vehicle: ref<VehicleObject>) -> Void {
    if this.m_boardWait > 0 || !this.CanBoard(player, vehicle) { return; };
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
    request.preservePositionAfterMounting = true;
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
    if this.m_boardWait > 0 {
      this.Log("WARNING", "Boarding is pending. Wait before removing.");
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
      if this.m_boardWait > 0 {
        let info = GameInstance.GetMountingFacility(player.GetGame()).GetMountingInfoSingleWithObjects(player);
        if IsDefined(vehicle) && Equals(info.parentId, vehicle.GetEntityID()) && Equals(info.slotId.id, n"seat_back_left") {
          this.m_boardWait = 0;
          this.Log("INFO", "PASS: V mounted in rear-left passenger seat.");
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
        if open && this.CanBoard(player, vehicle) {
          if VehicleComponent.OpenDoor(vehicle, VehicleComponent.GetBackLeftPassengerSlotID()) {
            this.Log("INFO", "Rear-left door open requested.");
          } else {
            this.Log("WARNING", "Door request rejected by vehicle.");
          };
        };
        if board { this.Board(player, vehicle); };
        if status { this.Inspect(player, vehicle); };
      };
    } else {
      if spawn || open || board || status || remove {
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
