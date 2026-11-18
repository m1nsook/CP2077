// NETWORM — NetWatch Σ MK.1 iconic cyberdeck (Cyberpunk 2077 2.5; test build)
// This is deliberately self-contained. It does not require Camera Killer
// or alter its hooks; NETWORM runs only while NetWatch Σ MK.1 is installed.
module NetDeck

import Codeware.Localization.*

public class NetDeckInfiltration extends ActionBool {
  public final func SetProperties() -> Void {
    this.actionName = n"NetDeckInfiltration";
    this.prop = DeviceActionPropertyFunctions.SetUpProperty_Bool(
      this.actionName, true, this.actionName, this.actionName
    );
  }
}

// Cyberware-EX can expose several System Replacement slots.
// Do not gate on a quest fact or a stale cached equipment value.
public func IsEquipped(game: GameInstance) -> Bool {
  let player: ref<PlayerPuppet> = GetPlayer(game);
  if !IsDefined(player) {
    return false;
  }
  let equipment: ref<EquipmentSystem> =
    GameInstance.GetScriptableSystemsContainer(game).Get(n"EquipmentSystem") as EquipmentSystem;
  if !IsDefined(equipment) {
    return false;
  }
  let slot: Int32 = 0;
  while slot <= 3 {
    let id: ItemID = equipment.GetItemInEquipSlot(player, gamedataEquipmentArea.SystemReplacementCW, slot);
    if ItemID.IsValid(id) && Equals(ItemID.GetTDBID(id), t"Items.NESTCyberdeck") {
      return true;
    }
    slot += 1;
  }
  return false;
}

@addMethod(ScriptableDeviceComponentPS)
public const func NetDeckNetworkMaster() -> ref<MasterControllerPS> {
  let accessPoints: array<ref<AccessPointControllerPS>> = this.GetAccessPoints();
  let i: Int32 = 0;
  while i < ArraySize(accessPoints) {
    if IsDefined(accessPoints[i]) {
      return accessPoints[i];
    }
    i += 1;
  }

  // Match Camera Killer's fallback for devices with a surveillance-system
  // parent instead of an explicit access point.
  let ancestors: array<ref<DeviceComponentPS>>;
  this.GetAncestors(ancestors);
  i = 0;
  while i < ArraySize(ancestors) {
    let surveillance: ref<SurveillanceSystemControllerPS> =
      ancestors[i] as SurveillanceSystemControllerPS;
    if IsDefined(surveillance) {
      return surveillance;
    }
    i += 1;
  }
  i = 0;
  while i < ArraySize(ancestors) {
    let accessPoint: ref<AccessPointControllerPS> =
      ancestors[i] as AccessPointControllerPS;
    if IsDefined(accessPoint) {
      return accessPoint;
    }
    i += 1;
  }
  return null;
}

@addMethod(ScriptableDeviceComponentPS)
public func NetDeckActionInfiltration() -> ref<NetDeckInfiltration> {
  let action: ref<NetDeckInfiltration> = new NetDeckInfiltration();
  action.clearanceLevel = DefaultActionsParametersHolder.GetInteractiveClearance();
  action.SetUp(this);
  action.SetProperties();
  action.AddDeviceName(this.m_deviceName);
  action.SetObjectActionID(t"DeviceAction.NetDeckInfiltration");
  action.CreateInteraction();
  action.SetExecutor(GetPlayer(this.GetGameInstance()));
  return action;
}

// VehicleComponentPS uses its own GetQuickHackActions path and
// finalizes through ScriptableDeviceComponentPS. Reuse that native vehicle
// scanner pathway, but supply a VehicleQuickHack action type.
@addMethod(ScriptableDeviceComponentPS)
public func NetDeckVehicleActionInfiltration() -> ref<NetDeckInfiltration> {
  let action: ref<NetDeckInfiltration> = new NetDeckInfiltration();
  action.clearanceLevel = DefaultActionsParametersHolder.GetInteractiveClearance();
  action.SetUp(this);
  action.SetProperties();
  action.AddDeviceName(this.m_deviceName);
  action.SetObjectActionID(t"DeviceAction.NetDeckVehicleInfiltration");
  action.CreateInteraction();
  action.SetExecutor(GetPlayer(this.GetGameInstance()));
  return action;
}

// Use the known ScriptableDeviceComponentPS finalizer signature.
// GetQuickHackActions on this base class cannot be wrapped with the old signature.
// Insert before vanilla finalization so it processes the injected action.
@wrapMethod(ScriptableDeviceComponentPS)
protected final func FinalizeGetQuickHackActions(
  outActions: script_ref<array<ref<DeviceAction>>>,
  const context: script_ref<GetActionsContext>
) -> Void {
  // Any device that vanilla considers quickhackable is a valid entry point.
  // A camera, surveillance master or access point is NOT required.
  if IsEquipped(this.GetGameInstance()) &&
    !this.IsMarkedAsQuest() &&
    !IsDefined(this as AccessPointControllerPS) {
    let vehicle: ref<VehicleObject> = this.GetOwnerEntityWeak() as VehicleObject;
    let eligible: Bool = this.IsPotentiallyQuickHackable();
    if IsDefined(vehicle) {
      // Native vehicle quickhacks require the Carhacker perk. Do not
      // modify the perk requirement on other cyberdecks.
      let player: ref<PlayerPuppet> = GetPlayer(this.GetGameInstance());
      eligible = IsDefined(player) &&
        PlayerDevelopmentSystem.GetData(player).IsNewPerkBoughtAnyLevel(
          gamedataNewPerkType.Intelligence_Right_Milestone_1
        ) && vehicle.IsHackable() && !vehicle.IsQuest() &&
        !vehicle.IsPlayerVehicle() && !vehicle.IsInQuestOrScenePhysicsState();
    }
    if eligible {
    // Lazy registration after the native vehicle scanner has selected
    // a valid quickhack target; never during global vehicle world loading.
    if IsDefined(vehicle) {
      let netDeckPlayer: ref<PlayerPuppet> = GetPlayer(this.GetGameInstance());
      if IsDefined(netDeckPlayer) {
        netDeckPlayer.NetDeckRegisterVehicle(vehicle);
      }
    }
    let exists: Bool = false;
    let i: Int32 = 0;
    while i < ArraySize(Deref(outActions)) {
      let existing: ref<ScriptableDeviceAction> =
        Deref(outActions)[i] as ScriptableDeviceAction;
      if IsDefined(existing) {
        let record: wref<ObjectAction_Record> = existing.GetObjectActionRecord();
        if IsDefined(record) && Equals(record.ActionName(), n"NetDeckInfiltration") {
          exists = true;
        }
      }
      i += 1;
    }
    if !exists {
      if IsDefined(vehicle) {
        ArrayPush(Deref(outActions), this.NetDeckVehicleActionInfiltration());
      } else {
        ArrayPush(Deref(outActions), this.NetDeckActionInfiltration());
      }
    }
    }
  }
  wrappedMethod(outActions, context);
}

// The scanner intersects device actions with the player's quickhack list.
// A cyberdeck's bare objectActions list does not reliably populate that
// list. Inject a virtual built-in program only while NΣT is equipped.
// It consumes none of the four physical program slots.
@wrapMethod(RPGManager)
public final static func GetPlayerQuickHackListWithQuality(
  player: wref<PlayerPuppet>
) -> array<PlayerQuickhackData> {
  let hacks: array<PlayerQuickhackData> = wrappedMethod(player);
  if !IsDefined(player) || !IsEquipped(player.GetGame()) {
    return hacks;
  }

  // Both device and puppet actions are registered, each with its own native
  // objectActionType. The game's scanner filters by target type. Never use
  // Ping's actionName, and never evict the original Ping.
  let record: wref<ObjectAction_Record> =
    TweakDBInterface.GetObjectActionRecord(t"DeviceAction.NetDeckInfiltration");
  let puppetRecord: wref<ObjectAction_Record> =
    TweakDBInterface.GetObjectActionRecord(t"QuickHack.NetDeckPuppetInfiltration");
  let vehicleRecord: wref<ObjectAction_Record> =
    TweakDBInterface.GetObjectActionRecord(t"DeviceAction.NetDeckVehicleInfiltration");
  let i: Int32 = 0;
  let hasDevice: Bool = false;
  let hasPuppet: Bool = false;
  let hasVehicle: Bool = false;
  while i < ArraySize(hacks) {
    if IsDefined(hacks[i].actionRecord) {
      let actionID: TweakDBID = hacks[i].actionRecord.GetID();
      if Equals(actionID, t"DeviceAction.NetDeckInfiltration") {
        hasDevice = true;
      }
      if Equals(actionID, t"QuickHack.NetDeckPuppetInfiltration") {
        hasPuppet = true;
      }
      if Equals(actionID, t"DeviceAction.NetDeckVehicleInfiltration") {
        hasVehicle = true;
      }
    }
    i += 1;
  }
  if IsDefined(record) && !hasDevice {
    ArrayPush(hacks, PlayerQuickhackData(ItemID.None(), record, 5));
  }
  if IsDefined(puppetRecord) && !hasPuppet {
    ArrayPush(hacks, PlayerQuickhackData(ItemID.None(), puppetRecord, 5));
  }
  if IsDefined(vehicleRecord) && !hasVehicle {
    ArrayPush(hacks, PlayerQuickhackData(ItemID.None(), vehicleRecord, 5));
  }
  return hacks;
}

// A single NETWORM upload spreads in hops, not one manual hack per target.
// Bounds prevent an uploaded worm from crawling indefinitely through NC.
public class NetDeckSpreadResult extends IScriptable {
  public let devices: Int32;
  public let npcs: Int32;
  public let netrunners: Int32;
  public let vehicles: Int32;
  public let eddies: Int32;
  public let robbedTargets: Int32;
  public let seen: array<ref<GameObject>>;
}

// Traverse genuine linked devices as well as nearby radio-range targets.
// Same visited queue is shared by NPCs, vehicles, and device controllers.
public func NetDeckEnqueueMaster(
  master: ref<MasterControllerPS>,
  out queue: array<ref<GameObject>>,
  out levels: array<Int32>,
  depth: Int32
) -> Void {
  if !IsDefined(master) { return; }
  let children: array<ref<DeviceComponentPS>>;
  master.GetAllDescendants(children);
  let i: Int32 = 0;
  while i < ArraySize(children) && ArraySize(queue) < 64 {
    let ps: ref<ScriptableDeviceComponentPS> = children[i] as ScriptableDeviceComponentPS;
    if IsDefined(ps) && !ps.IsMarkedAsQuest() {
      let obj: ref<GameObject> = ps.GetOwnerEntityWeak() as GameObject;
      if IsDefined(obj) && !ArrayContains(queue, obj) {
        ArrayPush(queue, obj);
        ArrayPush(levels, depth);
      }
    }
    i += 1;
  }
}

// Siphon small amounts of networked credits as a gameplay reward. This is
// virtual network theft, NOT a transfer from a real NPC purse/inventory.
public func NetDeckSiphon(
  game: GameInstance, result: ref<NetDeckSpreadResult>, computer: Bool
) -> Void {
  if result.eddies >= 1500 { return; }
  if RandRange(0, 99) >= 65 { return; }
  let player: ref<PlayerPuppet> = GetPlayer(game);
  if !IsDefined(player) { return; }
  let amount: Int32 = RandRange(12, 65);
  if computer { amount = RandRange(35, 125); }
  if amount > 1500 - result.eddies { amount = 1500 - result.eddies; }
  GameInstance.GetTransactionSystem(game).GiveItemByTDBID(player, t"Items.money", amount);
  result.eddies += amount;
  result.robbedTargets += 1;
}

// A real timed chain: each upload starts exactly one wave on the player.
// Searching from the player (TargetingSet.Complete) follows Echo EMP's
// full-3D search pattern; searching from arbitrary hacked NPCs did not work.
public class NetDeckWaveTickEvent extends Event {
  public let generation: Int32;
}

// TSFMV does not expose Obj_Vehicle. Track loaded VehicleObjects through
// their native attach/detach events, and inspect them separately from the
// generic targeting query. Weak refs avoid keeping streamed-out cars alive.
@addField(PlayerPuppet)
private let netDeckLoadedVehicles: array<wref<VehicleObject>>;

@addMethod(PlayerPuppet)
public func NetDeckRegisterVehicle(vehicle: wref<VehicleObject>) -> Void {
  if !IsDefined(vehicle) { return; }
  let i: Int32 = 0;
  while i < ArraySize(this.netDeckLoadedVehicles) {
    if !IsDefined(this.netDeckLoadedVehicles[i]) {
      ArrayErase(this.netDeckLoadedVehicles, i);
    } else {
      if this.netDeckLoadedVehicles[i].GetEntityID() == vehicle.GetEntityID() {
        return;
      }
      i += 1;
    }
  }
  if ArraySize(this.netDeckLoadedVehicles) >= 256 {
    ArrayErase(this.netDeckLoadedVehicles, 0);
  }
  ArrayPush(this.netDeckLoadedVehicles, vehicle);
}

@addMethod(PlayerPuppet)
public func NetDeckUnregisterVehicle(id: EntityID) -> Void {
  let i: Int32 = 0;
  while i < ArraySize(this.netDeckLoadedVehicles) {
    if !IsDefined(this.netDeckLoadedVehicles[i]) ||
       Equals(this.netDeckLoadedVehicles[i].GetEntityID(), id) {
      ArrayErase(this.netDeckLoadedVehicles, i);
    } else {
      i += 1;
    }
  }
}

// Map-load safety: do not wrap every VehicleObject's attach/detach here.
// The crash reporter repeatedly encountered the engine allocator while
// vehiclePersistentDataPS and NPCPuppet were being loaded. NETWORM now
// registers only explicitly targeted / scanned vehicles. Unscanned nearby
// vehicle spread may regress until a world-stream-safe query is verified.

// The targeting query can miss nearby NPCs even when the initial upload
// succeeded. Keep a bounded weak-ref fallback for loaded puppets, as for cars.
// No NPCs are acted on at registration time.
@addField(PlayerPuppet)
private let netDeckLoadedPuppets: array<wref<NPCPuppet>>;

@addMethod(PlayerPuppet)
public func NetDeckRegisterPuppet(puppet: wref<NPCPuppet>) -> Void {
  if !IsDefined(puppet) { return; }
  let i: Int32 = 0;
  while i < ArraySize(this.netDeckLoadedPuppets) {
    if !IsDefined(this.netDeckLoadedPuppets[i]) {
      ArrayErase(this.netDeckLoadedPuppets, i);
    } else {
      if Equals(this.netDeckLoadedPuppets[i].GetEntityID(), puppet.GetEntityID()) {
        return;
      }
      i += 1;
    }
  }
  if ArraySize(this.netDeckLoadedPuppets) >= 512 {
    ArrayErase(this.netDeckLoadedPuppets, 0);
  }
  ArrayPush(this.netDeckLoadedPuppets, puppet);
}

@addMethod(PlayerPuppet)
public func NetDeckUnregisterPuppet(id: EntityID) -> Void {
  let i: Int32 = 0;
  while i < ArraySize(this.netDeckLoadedPuppets) {
    if !IsDefined(this.netDeckLoadedPuppets[i]) ||
       Equals(this.netDeckLoadedPuppets[i].GetEntityID(), id) {
      ArrayErase(this.netDeckLoadedPuppets, i);
    } else {
      i += 1;
    }
  }
}

// Do not wrap NPCPuppet.OnGameAttached or NPCPuppet.OnDetach here.
// They run during world streaming and SenseInitialize/CreateSenseMappin,
// including without an active NETWORM upload. Track only scanned or
// explicitly hacked NPCs, leaving native NPC initialization untouched.

@addField(PlayerPuppet)
private let netDeckWaveGeneration: Int32;
@addField(PlayerPuppet)
private let netDeckWaveFromDevice: Bool;
@addField(PlayerPuppet)
private let netDeckWaveOrigin: Vector4;
@addField(PlayerPuppet)
private let netDeckWaveSeen: array<EntityID>;
@addField(PlayerPuppet)
private let netDeckWaveHosts: array<Vector4>;
@addField(PlayerPuppet)
private let netDeckWaveDepths: array<Int32>;
@addField(PlayerPuppet)
private let netDeckWaveLinks: array<Int32>;
@addField(PlayerPuppet)
private let netDeckWavePending: array<ref<GameObject>>;
@addField(PlayerPuppet)
private let netDeckWavePendingDepths: array<Int32>;
@addField(PlayerPuppet)
private let netDeckWavePendingCursor: Int32;
@addField(PlayerPuppet)
private let netDeckWaveTicks: Int32;
@addField(PlayerPuppet)
private let netDeckWaveResult: ref<NetDeckSpreadResult>;
@addField(PlayerPuppet)
private let netDeckHudOwned: Bool;
@addField(PlayerPuppet)
private let netDeckHudHeader: String;
@addField(PlayerPuppet)
private let netDeckHudProgress: Float;

@addMethod(PlayerPuppet)
private func NetDeckWaveHasSeen(id: EntityID) -> Bool {
  let i: Int32 = 0;
  while i < ArraySize(this.netDeckWaveSeen) {
    if Equals(this.netDeckWaveSeen[i], id) { return true; }
    i += 1;
  }
  return false;
}

@addMethod(PlayerPuppet)
private func NetDeckWaveValid(obj: ref<GameObject>) -> Bool {
  if !IsDefined(obj) || this.NetDeckWaveHasSeen(obj.GetEntityID()) {
    return false;
  }
  let npc: ref<NPCPuppet> = obj as NPCPuppet;
  if IsDefined(npc) {
    if !npc.IsActive() || npc.IsDead() || npc.IsQuest() ||
       npc.IsCrowd() || ScriptedPuppet.IsPlayerCompanion(npc) {
      return false;
    }
    if !IsDefined(npc.GetRecord()) || npc.GetRecord().IsChild() {
      return false;
    }
    // Pre-combat gangs may be neutral until V attacks. Treat combat-capable
    // non-crowd NPCs as candidates unless they are explicitly friendly.
    // Quest NPCs, children and companions are already filtered above.
    return EnumInt(GameObject.GetAttitudeTowards(npc, this)) !=
      EnumInt(EAIAttitude.AIA_Friendly);
  }
  let vehicle: ref<VehicleObject> = obj as VehicleObject;
  if IsDefined(vehicle) {
    return !vehicle.IsQuest() && !vehicle.IsPlayerVehicle() &&
      !vehicle.IsInQuestOrScenePhysicsState();
  }
  let device: ref<Device> = obj as Device;
  if IsDefined(device) {
    let ps: ref<ScriptableDeviceComponentPS> =
      device.GetDevicePS() as ScriptableDeviceComponentPS;
    return IsDefined(ps) && !ps.IsMarkedAsQuest() && !ps.IsDisabled();
  }
  return false;
}

@addMethod(PlayerPuppet)
private func NetDeckWaveAddMaster(
  master: ref<MasterControllerPS>, depth: Int32
) -> Void {
  if !IsDefined(master) || depth > 4 { return; }
  let children: array<ref<DeviceComponentPS>>;
  master.GetAllDescendants(children);
  let i: Int32 = 0;
  while i < ArraySize(children) && ArraySize(this.netDeckWavePending) < 64 {
    let ps: ref<ScriptableDeviceComponentPS> =
      children[i] as ScriptableDeviceComponentPS;
    if IsDefined(ps) && !ps.IsMarkedAsQuest() {
      let obj: ref<GameObject> = ps.GetOwnerEntityWeak() as GameObject;
      if IsDefined(obj) && !this.NetDeckWaveHasSeen(obj.GetEntityID()) &&
         !ArrayContains(this.netDeckWavePending, obj) {
        ArrayPush(this.netDeckWavePending, obj);
        ArrayPush(this.netDeckWavePendingDepths, depth);
      }
    }
    i += 1;
  }
}

// Every affected entity is dispatched on a DIFFERENT timed tick.
// Native Tier 5 Sonic Shock (communications disruption and sensory cloak).
@addMethod(PlayerPuppet)
private func NetDeckWaveInfect(obj: ref<GameObject>, depth: Int32) -> Void {
  if !this.NetDeckWaveValid(obj) { return; }
  ArrayPush(this.netDeckWaveSeen, obj.GetEntityID());
  ArrayPush(this.netDeckWaveHosts, obj.GetWorldPosition());
  ArrayPush(this.netDeckWaveDepths, depth);
  ArrayPush(this.netDeckWaveLinks, 0);

  let npc: ref<NPCPuppet> = obj as NPCPuppet;
  let vehicle: ref<VehicleObject> = obj as VehicleObject;
  let device: ref<Device> = obj as Device;

  if IsDefined(npc) {
    // The validator rejects friends/civilians; neutral street gangs are
    // attackable even before aggro, just like native combat quickhacks.
    if EnumInt(GameObject.GetAttitudeTowards(npc, this)) !=
       EnumInt(EAIAttitude.AIA_Friendly) {
      if !StatusEffectSystem.ObjectHasStatusEffect(
          npc, t"BaseStatusEffect.CommsNoiseLevel4") {
        StatusEffectHelper.ApplyStatusEffect(
          npc, t"BaseStatusEffect.CommsNoiseLevel4", this.GetEntityID()
        );
        this.netDeckWaveResult.npcs += 1;
      }
      NetDeckTriggerNetWatchPursuit(npc);
    }
    let link: ref<PuppetDeviceLinkPS> = npc.GetDeviceLink();
    if IsDefined(link) {
      let aps: array<ref<AccessPointControllerPS>> = link.GetAccessPoints();
      let j: Int32 = 0;
      while j < ArraySize(aps) {
        this.NetDeckWaveAddMaster(aps[j], depth + 1);
        j += 1;
      }
    }
    NetDeckSiphon(this.GetGame(), this.netDeckWaveResult, false);
  } else if IsDefined(vehicle) {
    if !vehicle.IsVehicleParked() {
      // Same native command as the game's VehicleComponentPS
      // OnVehicleOverrideForceBrakes callback. No TSFMV vehicle mask.
      vehicle.ActivateNetrunnerQuickhack(VehicleNetrunnerQuickhackType.ForceBrakes);
      this.netDeckWaveResult.vehicles += 1;
    }
  } else if IsDefined(device) {
    let ps: ref<ScriptableDeviceComponentPS> =
      device.GetDevicePS() as ScriptableDeviceComponentPS;
    if IsDefined(ps) {
      // Do not run PingNetworkGrid_Event here. That effect can outlive
      // its upload and was reported to leave continuous audio in-game.
      // Reintroduce a VISUAL only after isolated, explicit cleanup tests.
      if this.netDeckWaveFromDevice {
        this.netDeckWaveResult.netrunners += NetDeckBurnoutNearbyNetrunners(ps);
      }
      if ps.IsON() && ps.NetDeckApplyToMember(ps) {
        this.netDeckWaveResult.devices += 1;
      }
      let aps: array<ref<AccessPointControllerPS>> = ps.GetAccessPoints();
      let j: Int32 = 0;
      while j < ArraySize(aps) {
        this.NetDeckWaveAddMaster(aps[j], depth + 1);
        j += 1;
      }
      this.NetDeckWaveAddMaster(ps.NetDeckNetworkMaster(), depth + 1);
      if IsDefined(ps as ComputerControllerPS) ||
         IsDefined(ps as TerminalControllerPS) {
        NetDeckSiphon(this.GetGame(), this.netDeckWaveResult, true);
      }
    }
  }

  // No per-hop notifications: the native reward event may play UI audio.
  // Emit a single outcome at the end, not a repeating reward chime.
}

// Native mission-style transfer bar. Shared globally with quests: never
// overwrite a bar that is already active. Track the exact header as an
// ownership token so a quest taking it over cannot be closed by NETWORM.
@addMethod(PlayerPuppet)
private func NetDeckHudStart() -> Void {
  this.netDeckHudOwned = false;
  let bb: ref<IBlackboard> =
    GameInstance.GetBlackboardSystem(this.GetGame()).Get(
      GetAllBlackboardDefs().UI_HUDProgressBar
    );
  if !IsDefined(bb) { return; }
  let d: ref<UI_HUDProgressBarDef> = GetAllBlackboardDefs().UI_HUDProgressBar;
  if bb.GetBool(d.Active) { return; }
  this.netDeckHudOwned = true;
  this.netDeckHudProgress = 0.0;
  this.netDeckHudHeader = "NETWORM";
  bb.SetString(d.Header, this.netDeckHudHeader, true);
  bb.SetString(d.BottomText, "", true);
  let loc: ref<LocalizationSystem> =
    LocalizationSystem.GetInstance(this.GetGame());
  if IsDefined(loc) {
    bb.SetString(d.CompletedText, loc.GetText("NetDeckSpreadComplete"), true);
  } else {
    bb.SetString(d.CompletedText, "NETWORM COMPLETE", true);
  }
  bb.SetFloat(d.Progress, 0.0, true);
  bb.SetBool(d.Active, true, true);
  this.NetDeckHudUpdate();
}

@addMethod(PlayerPuppet)
private func NetDeckHudUpdate() -> Void {
  if !this.netDeckHudOwned || !IsDefined(this.netDeckWaveResult) { return; }
  let bb: ref<IBlackboard> =
    GameInstance.GetBlackboardSystem(this.GetGame()).Get(
      GetAllBlackboardDefs().UI_HUDProgressBar
    );
  if !IsDefined(bb) { this.netDeckHudOwned = false; return; }
  let d: ref<UI_HUDProgressBarDef> = GetAllBlackboardDefs().UI_HUDProgressBar;
  if !bb.GetBool(d.Active) ||
     !Equals(bb.GetString(d.Header), this.netDeckHudHeader) {
    // The quest system or another mod owns the bar now. Don't touch it.
    this.netDeckHudOwned = false;
    return;
  }
  let loc: ref<LocalizationSystem> =
    LocalizationSystem.GetInstance(this.GetGame());
  if !IsDefined(loc) { return; }
  let r: ref<NetDeckSpreadResult> = this.netDeckWaveResult;
  let count: Int32 = r.npcs + r.vehicles + r.netrunners + r.devices;
  this.netDeckHudHeader = loc.GetText("NetDeckUploadProgress") + " " +
    IntToString(count) + " " + loc.GetText("NetDeckTargetsAffected");
  bb.SetString(d.Header, this.netDeckHudHeader, true);
  bb.SetString(d.BottomText, loc.GetText("NetDeckDevicesAffected") +
    ": " + IntToString(r.devices), true);
  bb.SetFloat(d.Progress, this.netDeckHudProgress, true);
}

@addMethod(PlayerPuppet)
private func NetDeckHudFinish() -> Void {
  if !this.netDeckHudOwned { return; }
  this.netDeckHudOwned = false;
  let bb: ref<IBlackboard> =
    GameInstance.GetBlackboardSystem(this.GetGame()).Get(
      GetAllBlackboardDefs().UI_HUDProgressBar
    );
  if !IsDefined(bb) { return; }
  let d: ref<UI_HUDProgressBarDef> = GetAllBlackboardDefs().UI_HUDProgressBar;
  if bb.GetBool(d.Active) &&
     Equals(bb.GetString(d.Header), this.netDeckHudHeader) {
    bb.SetFloat(d.Progress, 1.0, true);
    bb.SetBool(d.Active, false, true);
  }
}

@addMethod(PlayerPuppet)
private func NetDeckWaveSchedule() -> Void {
  let evt: ref<NetDeckWaveTickEvent> = new NetDeckWaveTickEvent();
  evt.generation = this.netDeckWaveGeneration;
  GameInstance.GetDelaySystem(this.GetGame()).DelayEvent(this, evt, 2.0);
}

@addMethod(PlayerPuppet)
private func NetDeckWaveFinish() -> Void {
  this.NetDeckHudFinish();
  let r: ref<NetDeckSpreadResult> = this.netDeckWaveResult;
  if !IsDefined(r) { return; }
  let affected: Int32 = r.devices + r.npcs + r.vehicles + r.netrunners;
  if affected > 0 { NetDeckGiveDataComponents(this.GetGame()); }
  NetDeckNotify(
    this.GetGame(), affected + r.robbedTargets, r.devices, r.eddies,
    // TARGETS counts NPCs and cars actually dispatched an attack effect.
    // Device actions have their own DEVICE field and must not inflate it.
    r.robbedTargets, r.npcs + r.vehicles
  );
}

@addMethod(PlayerPuppet)
public func NetDeckWaveStart(root: ref<GameObject>, fromDevice: Bool) -> Void {
  if !IsDefined(root) || !IsEquipped(this.GetGame()) { return; }

  // Each upload supersedes old ticks and stops its previous HUD bar.
  this.NetDeckHudFinish();
  this.netDeckWaveGeneration += 1;
  let initialVehicle: wref<VehicleObject> = root as VehicleObject;
  if IsDefined(initialVehicle) { this.NetDeckRegisterVehicle(initialVehicle); }
  let initialPuppet: wref<NPCPuppet> = root as NPCPuppet;
  if IsDefined(initialPuppet) { this.NetDeckRegisterPuppet(initialPuppet); }
  this.netDeckWaveFromDevice = fromDevice;
  this.netDeckWaveOrigin = root.GetWorldPosition();
  this.netDeckWavePendingCursor = 0;
  this.netDeckWaveTicks = 0;
  this.netDeckWaveResult = new NetDeckSpreadResult();
  ArrayClear(this.netDeckWaveSeen);
  ArrayClear(this.netDeckWaveHosts);
  ArrayClear(this.netDeckWaveDepths);
  ArrayClear(this.netDeckWaveLinks);
  ArrayClear(this.netDeckWavePending);
  ArrayClear(this.netDeckWavePendingDepths);
  this.NetDeckWaveInfect(root, 0);
  this.NetDeckHudStart();
  this.NetDeckWaveSchedule();
}

@addMethod(PlayerPuppet)
protected cb func OnNetDeckWaveTickEvent(evt: ref<NetDeckWaveTickEvent>) -> Bool {
  if evt.generation != this.netDeckWaveGeneration { return true; }
  if !IsEquipped(this.GetGame()) {
    this.NetDeckHudFinish();
    return true;
  }
  // Hard safety stop: max 16 scheduled jumps (32 seconds) in this test
  // build, even if native targeting starts returning phantom candidates.
  this.netDeckWaveTicks += 1;
  if this.netDeckHudProgress < 1.0 {
    this.netDeckHudProgress += 0.0625;
  }
  if this.netDeckWaveTicks > 16 || ArraySize(this.netDeckWaveSeen) >= 40 {
    this.NetDeckWaveFinish();
    return true;
  }

  // Attack-hack priority: consume nearby eligible NPCs before devices
  // or cars. The old connected-device-first loop starved the attack chain,
  // showing many visited devices while only one gang member was glitched.
  let selected: ref<GameObject>;
  let depth: Int32 = 0;
  let nearestNpc: Float = 25.0;
  let selectedNpcHost: Int32 = -1;
  let p: Int32 = 0;
  while p < ArraySize(this.netDeckLoadedPuppets) {
    let npcCandidate: ref<NPCPuppet> = this.netDeckLoadedPuppets[p];
    if IsDefined(npcCandidate) && this.NetDeckWaveValid(npcCandidate) &&
       Vector4.Distance(this.netDeckWaveOrigin, npcCandidate.GetWorldPosition()) <= 55.0 {
      let h: Int32 = 0;
      while h < ArraySize(this.netDeckWaveHosts) {
        if this.netDeckWaveLinks[h] < 4 && this.netDeckWaveDepths[h] < 4 {
          let d: Float =
            Vector4.Distance(this.netDeckWaveHosts[h], npcCandidate.GetWorldPosition());
          if d <= nearestNpc {
            nearestNpc = d;
            selected = npcCandidate;
            selectedNpcHost = h;
          }
        }
        h += 1;
      }
    }
    p += 1;
  }
  if IsDefined(selected) && selectedNpcHost >= 0 {
    this.netDeckWaveLinks[selectedNpcHost] += 1;
    depth = this.netDeckWaveDepths[selectedNpcHost] + 1;
  }

  if !IsDefined(selected) {
    // Echo EMP's robust approach: query one full 3D area around V,
    // then select the nearest candidate within hop distance of any
    // already infected host. Querying arbitrary device-origin targets
    // previously returned no candidates in the player's game.
    let query: TargetSearchQuery;
    query.testedSet = TargetingSet.Complete;
    query.searchFilter =
      TSF_Any(TSFMV.Obj_Puppet | TSFMV.Obj_Device | TSFMV.Obj_Other);
    query.includeSecondaryTargets = false;
    query.ignoreInstigator = true;
    query.maxDistance = 55.0;
    query.filterObjectByDistance = true;
    let parts: array<TS_TargetPartInfo>;
    GameInstance.GetTargetingSystem(this.GetGame()).GetTargetParts(
      this, query, parts
    );

    let nearest: Float = 25.0;
    let selectedHost: Int32 = -1;
    let i: Int32 = 0;
    while i < ArraySize(parts) && i < 250 {
      let component: wref<TargetingComponent> =
        TS_TargetPartInfo.GetComponent(parts[i]);
      if IsDefined(component) {
        let candidate: ref<GameObject> =
          component.GetEntity() as GameObject;
        if this.NetDeckWaveValid(candidate) &&
           Vector4.Distance(this.netDeckWaveOrigin, candidate.GetWorldPosition()) <= 55.0 {
          let j: Int32 = 0;
          while j < ArraySize(this.netDeckWaveHosts) {
            if this.netDeckWaveLinks[j] < 4 &&
               this.netDeckWaveDepths[j] < 4 {
              let distance: Float =
                Vector4.Distance(this.netDeckWaveHosts[j], candidate.GetWorldPosition());
              if distance <= nearest {
                nearest = distance;
                selected = candidate;
                selectedHost = j;
              }
            }
            j += 1;
          }
        }
      }
      i += 1;
    }
    // Fallback for hostile loaded NPCs omitted by TargetingSystem. Every
    // candidate still passes the same eligibility, range, hop, visited and
    // fanout checks as the targeting query. Civilians cannot relay the worm.
    let p: Int32 = 0;
    while p < ArraySize(this.netDeckLoadedPuppets) {
      let loadedNpc: ref<NPCPuppet> = this.netDeckLoadedPuppets[p];
      if IsDefined(loadedNpc) && this.NetDeckWaveValid(loadedNpc) &&
         Vector4.Distance(this.netDeckWaveOrigin, loadedNpc.GetWorldPosition()) <= 55.0 {
        let hostNpc: Int32 = 0;
        while hostNpc < ArraySize(this.netDeckWaveHosts) {
          if this.netDeckWaveLinks[hostNpc] < 4 &&
             this.netDeckWaveDepths[hostNpc] < 4 {
            let npcDistance: Float =
              Vector4.Distance(this.netDeckWaveHosts[hostNpc], loadedNpc.GetWorldPosition());
            if npcDistance <= nearest {
              nearest = npcDistance;
              selected = loadedNpc;
              selectedHost = hostNpc;
            }
          }
          hostNpc += 1;
        }
      }
      p += 1;
    }
    // Cars are not reliably returned by TSFMV's generic object mask.
    // Match loaded VehicleObjects explicitly using their world positions.
    let k: Int32 = 0;
    while k < ArraySize(this.netDeckLoadedVehicles) {
      let car: ref<VehicleObject> = this.netDeckLoadedVehicles[k];
      if IsDefined(car) && this.NetDeckWaveValid(car) &&
         Vector4.Distance(this.netDeckWaveOrigin, car.GetWorldPosition()) <= 55.0 {
        let host: Int32 = 0;
        while host < ArraySize(this.netDeckWaveHosts) {
          if this.netDeckWaveLinks[host] < 4 &&
             this.netDeckWaveDepths[host] < 4 {
            let distance: Float =
              Vector4.Distance(this.netDeckWaveHosts[host], car.GetWorldPosition());
            if distance <= nearest {
              nearest = distance;
              selected = car;
              selectedHost = host;
            }
          }
          host += 1;
        }
      }
      k += 1;
    }
    if IsDefined(selected) && selectedHost >= 0 {
      this.netDeckWaveLinks[selectedHost] += 1;
      depth = this.netDeckWaveDepths[selectedHost] + 1;
    }
  }

  // Genuine connected devices are still supported, but never consume all
  // available ticks ahead of loaded hostile/neutral gang members.
  while this.netDeckWavePendingCursor < ArraySize(this.netDeckWavePending) &&
        !IsDefined(selected) {
    let index: Int32 = this.netDeckWavePendingCursor;
    this.netDeckWavePendingCursor += 1;
    let pendingCandidate: ref<GameObject> = this.netDeckWavePending[index];
    if this.NetDeckWaveValid(pendingCandidate) {
      selected = pendingCandidate;
      depth = this.netDeckWavePendingDepths[index];
    }
  }

  if IsDefined(selected) {
    this.NetDeckWaveInfect(selected, depth);
    this.NetDeckHudUpdate();
    this.NetDeckWaveSchedule();
  } else {
    this.NetDeckWaveFinish();
  }
  return true;
}

// Scope trace suppression strictly to NETWORM. A proven game-compatible
// wrapper signature is also used by other REDscript quickhack mods.
@wrapMethod(QuickHackableHelper)
public final static func WillHackRevealPlayer(
  playerRef: ref<PlayerPuppet>,
  targetRecordId: TweakDBID,
  action: ref<ScriptableDeviceAction>,
  opt programItemID: ItemID
) -> Bool {
  if IsDefined(action) {
    let record: wref<ObjectAction_Record> = action.GetObjectActionRecord();
    if IsDefined(record) && Equals(record.ActionName(), n"NetDeckInfiltration") {
      return false;
    }
  }
  return wrappedMethod(playerRef, targetRecordId, action, programItemID);
}

// Deliberately no PingNetworkGrid_Event call: stale network audio must not
// survive a NETWORM upload or make the user uninstall the mod to silence it.

// Report upload outcome once, using Codeware's interface language.
public func NetDeckNotify(
  game: GameInstance, affected: Int32, deviceCount: Int32,
  stolen: Int32, robbedTargets: Int32, totalTargets: Int32
) -> Void {
  let loc: ref<LocalizationSystem> = LocalizationSystem.GetInstance(game);
  if !IsDefined(loc) {
    return;
  }
  let notification: ref<HackingRewardNotificationEvent> = new HackingRewardNotificationEvent();
  if affected > 0 {
    // Count only device actions dispatched via the actual linked network.
    // NPC Weapon Glitch, Synapse Burnout and vehicle braking are excluded.
    notification.m_text =
      loc.GetText("NetDeckSpreadComplete") + " | " +
      loc.GetText("NetDeckDevicesAffected") + ": " + IntToString(deviceCount);
    notification.m_text += " | " + loc.GetText("NetDeckTargetsAffected") +
      ": " + IntToString(totalTargets);
    if stolen > 0 {
      notification.m_text += " | " + loc.GetText("NetDeckEddiesStolen") +
        " €$" + IntToString(stolen) + " / " + IntToString(robbedTargets) +
        " " + loc.GetText("NetDeckStolenTargets");
    }
  } else {
    notification.m_text = loc.GetText("NetDeckSpreadEmpty");
  }
  GameInstance.GetUISystem(game).QueueEvent(notification);
}

// No access-point scanner hacks: access points use their own breach flow.
// NETWORM may still cross their existing network as a device's controller.

// Synapse Burnout is deliberately the *only* damaging NPC effect and only
// fires for enemy netrunners near a DEVICE upload. No civilians or quest NPCs.
public func NetDeckBurnoutNearbyNetrunners(source: ref<ScriptableDeviceComponentPS>) -> Int32 {
  let origin: ref<GameObject> = source.GetOwnerEntityWeak() as GameObject;
  let player: ref<PlayerPuppet> = GetPlayer(source.GetGameInstance());
  if !IsDefined(origin) || !IsDefined(player) { return 0; }
  let query: TargetSearchQuery;
  query.testedSet = TargetingSet.Complete;
  query.searchFilter = TSF_NPC();
  query.includeSecondaryTargets = false;
  query.maxDistance = 25.0;
  query.filterObjectByDistance = true;
  let parts: array<TS_TargetPartInfo>;
  GameInstance.GetTargetingSystem(source.GetGameInstance()).GetTargetParts(origin, query, parts);
  let seen: array<ref<ScriptedPuppet>>;
  let hits: Int32 = 0;
  let i: Int32 = 0;
  while i < ArraySize(parts) {
    let component: wref<TargetingComponent> = TS_TargetPartInfo.GetComponent(parts[i]);
    if IsDefined(component) {
      let target: ref<ScriptedPuppet> = component.GetEntity() as ScriptedPuppet;
      if IsDefined(target) && !ArrayContains(seen, target) {
        ArrayPush(seen, target);
        if target.IsNetrunnerPuppet() && !target.IsQuest() &&
          EnumInt(GameObject.GetAttitudeTowards(target, player)) == EnumInt(EAIAttitude.AIA_Hostile) &&
          !StatusEffectSystem.ObjectHasStatusEffect(target, t"BaseStatusEffect.BrainMeltLevel4") {
          StatusEffectHelper.ApplyStatusEffect(
            target, t"BaseStatusEffect.BrainMeltLevel4", player.GetEntityID()
          );
          hits += 1;
        }
      }
    }
    i += 1;
  }
  return hits;
}

public func NetDeckIsNetWatch(puppet: ref<ScriptedPuppet>) -> Bool {
  let npc: ref<NPCPuppet> = puppet as NPCPuppet;
  if !IsDefined(npc) { return false; }
  let record: wref<Character_Record> = TweakDBInterface.GetCharacterRecord(npc.GetRecordID());
  if !IsDefined(record) { return false; }
  let affiliation: wref<Affiliation_Record> = record.Affiliation();
  return IsDefined(affiliation) && Equals(affiliation.Type(), gamedataAffiliation.NetWatch);
}

// Detected by NetWatch: immediate five-star response and the game's own
// reveal-position attempt. This doesn't modify trace timers for other NPCs.
public func NetDeckTriggerNetWatchPursuit(puppet: ref<ScriptedPuppet>) -> Void {
  if !NetDeckIsNetWatch(puppet) { return; }
  let player: ref<PlayerPuppet> = GetPlayer(puppet.GetGame());
  if !IsDefined(player) { return; }
  let wanted: ref<SetWantedLevel> = new SetWantedLevel();
  wanted.m_wantedLevel = EPreventionHeatStage.Heat_5;
  wanted.m_forcePlayerPositionAsLastCrimePoint = true;
  PreventionSystem.QueueRequest(puppet.GetGame(), wanted);
  NPCPuppet.RevealPlayerPositionIfNeeded(puppet, player.GetEntityID(), false);
}

// NPC-origin NETWORM is non-lethal: disrupt hostile communications and brake
// nearby non-quest vehicles. No damage, incapacitation or radius-based
// network shutdown is applied to any civilian or hostile puppet.
public func NetDeckDisruptNpcVicinity(puppet: ref<ScriptedPuppet>) -> Int32 {
  if !IsDefined(puppet) || puppet.IsQuest() { return 0; }
  let player: ref<PlayerPuppet> = GetPlayer(puppet.GetGame());
  if !IsDefined(player) { return 0; }
  let affected: Int32 = 0;
  let npc: ref<NPCPuppet> = puppet as NPCPuppet;
  if IsDefined(npc) &&
     (EnumInt(GameObject.GetAttitudeTowards(npc, player)) == EnumInt(EAIAttitude.AIA_Hostile) ||
      npc.IsCharacterPolice() || NetDeckIsNetWatch(puppet)) &&
     !StatusEffectSystem.ObjectHasStatusEffect(npc, t"BaseStatusEffect.CommsNoiseLevel4") {
    StatusEffectHelper.ApplyStatusEffect(
      npc, t"BaseStatusEffect.CommsNoiseLevel4", player.GetEntityID()
    );
    affected += 1;
  }

  // TargetingSystem only returns loaded, targetable vehicles in this radius.
  // No driving quest/scene vehicles, no player's own vehicle.
  let query: TargetSearchQuery;
  query.testedSet = TargetingSet.Complete;
  query.searchFilter = TSF_Any(TSFMV.Obj_Other | TSFMV.Obj_Device);
  query.includeSecondaryTargets = false;
  query.maxDistance = 25.0;
  query.filterObjectByDistance = true;
  let parts: array<TS_TargetPartInfo>;
  GameInstance.GetTargetingSystem(puppet.GetGame()).GetTargetParts(puppet, query, parts);
  let seen: array<ref<VehicleObject>>;
  let i: Int32 = 0;
  while i < ArraySize(parts) {
    let component: wref<TargetingComponent> = TS_TargetPartInfo.GetComponent(parts[i]);
    if IsDefined(component) {
      let vehicle: ref<VehicleObject> = component.GetEntity() as VehicleObject;
      if IsDefined(vehicle) && !ArrayContains(seen, vehicle) {
        ArrayPush(seen, vehicle);
        if !vehicle.IsQuest() && !vehicle.IsPlayerVehicle() &&
           !vehicle.IsVehicleParked() && !vehicle.IsInQuestOrScenePhysicsState() {
          vehicle.ForceBrakesFor(4.0);
          affected += 1;
        }
      }
    }
    i += 1;
  }
  return affected;
}

// "DATA EXTRACTED" is the game's native hacking reward notification.
// Give a small random pool of real crafting/quickhack components on success.
// No explicit XP or skill-progression awards are issued by NETWORM.
public func NetDeckGiveDataComponents(game: GameInstance) -> Void {
  let player: ref<PlayerPuppet> = GetPlayer(game);
  if !IsDefined(player) { return; }
  let ts: ref<TransactionSystem> = GameInstance.GetTransactionSystem(game);
  if !IsDefined(ts) { return; }
  let id: TweakDBID = t"Items.QuickHackCommonMaterial1";
  let roll: Int32 = RandRange(0, 9);
  if roll == 1 { id = t"Items.QuickHackUncommonMaterial1"; }
  if roll == 2 { id = t"Items.QuickHackRareMaterial1"; }
  if roll == 3 { id = t"Items.QuickHackEpicMaterial1"; }
  if roll == 4 { id = t"Items.QuickHackLegendaryMaterial1"; }
  if roll == 5 { id = t"Items.CommonMaterial1"; }
  if roll == 6 { id = t"Items.UncommonMaterial1"; }
  if roll == 7 { id = t"Items.RareMaterial1"; }
  if roll == 8 { id = t"Items.EpicMaterial1"; }
  if roll == 9 { id = t"Items.LegendaryMaterial1"; }
  ts.GiveItemByTDBID(player, id, RandRange(1, 4));
}

// Preserve vanilla IsPotentiallyQuickHackable unchanged. Ping remains native.

// Only act on actual links. Cameras on a neighboring network are untouched.
// Multiple APs sharing one target are deduplicated, not processed twice.
@addMethod(ScriptableDeviceComponentPS)
public func NetDeckInfiltrateNetwork() -> Int32 {
  if !IsEquipped(this.GetGameInstance()) || this.IsMarkedAsQuest() {
    return 0;
  }

  let affected: Int32 = 0;
  let seen: array<ref<ScriptableDeviceComponentPS>>;
  // A pingable access point or network controller IS a valid cast target.
  // Traverse its own descendants instead of expecting a camera-like parent.
  let directMaster: ref<MasterControllerPS> = this as MasterControllerPS;
  if IsDefined(directMaster) {
    return this.NetDeckInfiltrateMaster(directMaster, seen);
  }
  let accessPoints: array<ref<AccessPointControllerPS>> = this.GetAccessPoints();
  let i: Int32 = 0;
  let foundAP: Bool = false;
  while i < ArraySize(accessPoints) {
    if IsDefined(accessPoints[i]) {
      foundAP = true;
      affected += this.NetDeckInfiltrateMaster(accessPoints[i], seen);
    }
    i += 1;
  }

  // Devices on a virtual surveillance system may not have an explicit AP.
  if !foundAP {
    affected += this.NetDeckInfiltrateMaster(this.NetDeckNetworkMaster(), seen);
  }
  // A Ping-capable standalone terminal, turret, mine, camera, or other
  // electronic device must still respond when it has no shared controller.
  if !foundAP && !IsDefined(this.NetDeckNetworkMaster()) && this.IsON() {
    if this.NetDeckApplyToMember(this) {
      affected += 1;
    }
  }
  return affected;
}

@addMethod(ScriptableDeviceComponentPS)
private func NetDeckInfiltrateMaster(
  master: ref<MasterControllerPS>,
  out seen: array<ref<ScriptableDeviceComponentPS>>
) -> Int32 {
  if !IsDefined(master) {
    return 0;
  }
  let affected: Int32 = 0;
  let members: array<ref<DeviceComponentPS>>;
  master.GetAllDescendants(members);
  let i: Int32 = 0;
  while i < ArraySize(members) {
    let member: ref<ScriptableDeviceComponentPS> =
      members[i] as ScriptableDeviceComponentPS;
    if IsDefined(member) && !ArrayContains(seen, member) {
      ArrayPush(seen, member);
      if member.IsON() && !member.IsMarkedAsQuest() {
        if this.NetDeckApplyToMember(member) {
          affected += 1;
        }
      }
    }
    i += 1;
  }
  return affected;
}

@addMethod(ScriptableDeviceComponentPS)
public func NetDeckApplyToMember(member: ref<ScriptableDeviceComponentPS>) -> Bool {
  // RoadBlockControllerPS is used for street barriers and sliding blockades.
  let barrier: ref<RoadBlockControllerPS> = member as RoadBlockControllerPS;
  if IsDefined(barrier) {
    if barrier.IsMarkedAsQuest() || !barrier.IsON() ||
       barrier.IsDisabled() || !barrier.IsBlocking() {
      return false;
    }
    let toggle: ref<ToggleBlockade> = new ToggleBlockade();
    toggle.SetUp(barrier);
    toggle.SetProperties(true, t"Interactions.Raise", t"Interactions.Lower");
    this.QueuePSEvent(barrier, toggle);
    return true;
  }

  // Remotely take over ordinary doors and windows using their real controller
  // actions, never quest / sealed / disabled / unpowered doors or elevators.
  let door: ref<DoorControllerPS> = member as DoorControllerPS;
  if IsDefined(door) {
    if door.IsMarkedAsQuest() || door.IsSealed() || door.IsDisabled() ||
       door.IsUnpowered() || door.IsLiftDoor() || door.IsOpen() {
      return false;
    }
    if door.IsLocked() {
      this.QueuePSEvent(door, door.ActionToggleLock());
    }
    this.QueuePSEvent(door, door.ActionSetOpened());
    return true;
  }

  if IsDefined(member as SurveillanceCameraControllerPS) ||
    IsDefined(member as SecurityTurretControllerPS) ||
    IsDefined(member as ExplosiveTriggerDeviceControllerPS) {
    // Uses the same game's OFF event as Camera Killer, but only when the
    // player intentionally uploads Infiltration. No automatic global hooks.
    this.QueuePSEvent(member, member.ActionSetDeviceOFF());
    return true;
  }

  // Don't send a synthetic distraction to every device: the base controller
  // constructs a QuickHackDistraction even for devices with no such feature.
  // Only dispatch an actual offered, active game-created quickhack action.
  // Master controllers are not eligible distraction targets.
  // PuppetDeviceLinkPS is not a ScriptableDeviceComponentPS subtype.
  if IsDefined(member as MasterControllerPS) {
    return false;
  }
  let actions: array<ref<DeviceAction>>;
  let context: GetActionsContext = member.GenerateContext(
    gamedeviceRequestType.Remote,
    member.GetClearance(),
    member.GetPlayerMainObject(),
    PersistentID.ExtractEntityID(member.GetID())
  );
  context.ignoresRPG = true;
  member.GetQuickHackActions(actions, context);

  let i: Int32 = 0;
  while i < ArraySize(actions) {
    let distraction: ref<QuickHackDistraction> = actions[i] as QuickHackDistraction;
    if IsDefined(distraction) && !distraction.IsInactive() {
      distraction.SetExecutor(GetPlayer(this.GetGameInstance()));
      this.QueuePSEvent(member, distraction);
      return true;
    }
    i += 1;
  }
  return false;
}

// The game's action dispatcher is responsible for invoking this after the
// quickhack action is accepted. Do not set scanner or breach blackboards.
@addMethod(ScriptableDeviceComponentPS)
protected cb func OnNetDeckInfiltration(evt: ref<NetDeckInfiltration>) -> EntityNotificationType {
  let source: ref<GameObject> = this.GetOwnerEntityWeak() as GameObject;
  let player: ref<PlayerPuppet> = GetPlayer(this.GetGameInstance());
  if IsDefined(player) { player.NetDeckWaveStart(source, true); }
  return EntityNotificationType.DoNotNotifyEntity;
}

// Canto-style hostile quickhack targeting. The old code required a native
// Ping action to be present; many ordinary enemy puppets lack that action,
// so NETWORM was never offered to them. Unlike a network discovery hack,
// an ATTACK hack must use the NPC scanner path and target hostiles directly.
public class NetDeckPuppetInfiltration extends PuppetAction {}

@wrapMethod(ScriptedPuppetPS)
public final const func GetAllChoices(
  const actions: script_ref<array<wref<ObjectAction_Record>>>,
  const context: script_ref<GetActionsContext>,
  puppetActions: script_ref<array<ref<PuppetAction>>>
) -> Void {
  wrappedMethod(actions, context, puppetActions);
  if !IsEquipped(this.GetGameInstance()) {
    return;
  }
  let owner: ref<ScriptedPuppet> = this.GetOwnerEntityWeak() as ScriptedPuppet;
  let npc: ref<NPCPuppet> = owner as NPCPuppet;
  if !IsDefined(npc) || !npc.IsQuickHackAble() || !npc.IsActive() ||
     npc.IsQuest() || npc.IsDead() || npc.IsCrowd() ||
     ScriptedPuppet.IsPlayerCompanion(npc) {
    return;
  }
  if !IsDefined(npc.GetRecord()) || npc.GetRecord().IsChild() {
    return;
  }

  // Lazy, scan-driven candidate registration. Never hook the entity attach
  // path: loading a save must not mutate a registry for every spawned NPC.
  let netDeckPlayer: ref<PlayerPuppet> = GetPlayer(this.GetGameInstance());
  if IsDefined(netDeckPlayer) {
    netDeckPlayer.NetDeckRegisterPuppet(npc);
  }

  // The available base action is Canto's Blackwall Gateway, not FreePing.
  // Keep vanilla Ping untouched and avoid inserting duplicate custom actions.
  let i: Int32 = 0;
  while i < ArraySize(Deref(puppetActions)) {
    let existing: ref<PuppetAction> = Deref(puppetActions)[i];
    if IsDefined(existing) {
      let existingRecord: wref<ObjectAction_Record> = existing.GetObjectActionRecord();
      if IsDefined(existingRecord) &&
         Equals(existingRecord.ActionName(), n"NetDeckInfiltration") {
        return;
      }
    }
    i += 1;
  }

  let record: wref<ObjectAction_Record> =
    TweakDBInterface.GetObjectActionRecord(t"QuickHack.NetDeckPuppetInfiltration");
  if !IsDefined(record) {
    return;
  }
  let action: ref<NetDeckPuppetInfiltration> = new NetDeckPuppetInfiltration();
  action.SetUp(this);
  action.RegisterAsRequester(PersistentID.ExtractEntityID(this.GetID()));
  action.SetExecutor(GetPlayer(this.GetGameInstance()));
  action.SetObjectActionID(record.GetID());
  action.CreateInteraction();
  ArrayPush(Deref(puppetActions), action);
}

// Puppet network links are not ScriptableDeviceComponentPS. Use the link's
// own real access points and their descendants. No radius-based search.
@addMethod(PuppetDeviceLinkPS)
public func NetDeckInfiltrateLinkedDevices() -> Int32 {
  if !this.IsConnected() {
    return 0;
  }
  let affected: Int32 = 0;
  let aps: array<ref<AccessPointControllerPS>> = this.GetAccessPoints();
  let seen: array<ref<ScriptableDeviceComponentPS>>;
  let i: Int32 = 0;
  while i < ArraySize(aps) {
    if IsDefined(aps[i]) {
      let children: array<ref<DeviceComponentPS>>;
      aps[i].GetAllDescendants(children);
      let j: Int32 = 0;
      while j < ArraySize(children) {
        let member: ref<ScriptableDeviceComponentPS> =
          children[j] as ScriptableDeviceComponentPS;
        if IsDefined(member) && !ArrayContains(seen, member) {
          ArrayPush(seen, member);
          if member.IsON() && !member.IsMarkedAsQuest() {
            if member.NetDeckApplyToMember(member) {
              affected += 1;
            }
          }
        }
        j += 1;
      }
    }
    i += 1;
  }
  return affected;
}

@addMethod(ScriptedPuppetPS)
protected cb func OnNetDeckPuppetInfiltration(
  evt: ref<NetDeckPuppetInfiltration>
) -> EntityNotificationType {
  if IsEquipped(this.GetGameInstance()) {
    let puppet: ref<ScriptedPuppet> = this.GetOwnerEntityWeak() as ScriptedPuppet;
    if IsDefined(puppet) && !puppet.IsQuest() {
      let player: ref<PlayerPuppet> = GetPlayer(this.GetGameInstance());
      if IsDefined(player) { player.NetDeckWaveStart(puppet, false); }
    }
  }
  return EntityNotificationType.DoNotNotifyEntity;
}