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
    !IsDefined(this as AccessPointControllerPS) &&
    this.IsPotentiallyQuickHackable() {
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
      ArrayPush(Deref(outActions), this.NetDeckActionInfiltration());
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
  let i: Int32 = 0;
  let hasDevice: Bool = false;
  let hasPuppet: Bool = false;
  while i < ArraySize(hacks) {
    if IsDefined(hacks[i].actionRecord) {
      let actionID: TweakDBID = hacks[i].actionRecord.GetID();
      if Equals(actionID, t"DeviceAction.NetDeckInfiltration") {
        hasDevice = true;
      }
      if Equals(actionID, t"QuickHack.NetDeckPuppetInfiltration") {
        hasPuppet = true;
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
  return hacks;
}

// Use the game's existing network visual for a short propagation pulse.
public func NetDeckPulseNetwork(source: ref<ScriptableDeviceComponentPS>) -> Void {
  if !IsDefined(source) {
    return;
  }
  let device: ref<Device> = source.GetOwnerEntityWeak() as Device;
  if !IsDefined(device) {
    return;
  }
  device.PingNetworkGrid_Event(
    device.GetNetworkBeamEndpoint(),
    device.GetFxResourceByKey(n"pingNetworkLink"),
    2.5, EPingType.DIRECT, true, true, false
  );
}

// Report upload outcome once, using Codeware's interface language.
public func NetDeckNotify(game: GameInstance, affected: Int32) -> Void {
  let loc: ref<LocalizationSystem> = LocalizationSystem.GetInstance(game);
  if !IsDefined(loc) {
    return;
  }
  let notification: ref<HackingRewardNotificationEvent> = new HackingRewardNotificationEvent();
  if affected > 0 {
    notification.m_text = loc.GetText("NetDeckSpreadComplete");
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
  query.searchFilter = TSF_EnemyNPC();
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
          GameObject.GetAttitudeTowards(target, player) == EAIAttitude.AIA_Hostile &&
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

// NPC-origin NETWORM is non-lethal: interfere with hostile weapons and brake
// nearby non-quest vehicles. No damage, incapacitation or radius-based
// network shutdown is applied to any civilian or hostile puppet.
public func NetDeckDisruptNpcVicinity(puppet: ref<ScriptedPuppet>) -> Int32 {
  if !IsDefined(puppet) || puppet.IsQuest() { return 0; }
  let player: ref<PlayerPuppet> = GetPlayer(puppet.GetGame());
  if !IsDefined(player) { return 0; }
  let affected: Int32 = 0;
  let npc: ref<NPCPuppet> = puppet as NPCPuppet;
  if IsDefined(npc) &&
     (GameObject.GetAttitudeTowards(npc, player) == EAIAttitude.AIA_Hostile ||
      npc.IsCharacterPolice() || NetDeckIsNetWatch(puppet)) &&
     !StatusEffectSystem.ObjectHasStatusEffect(npc, t"BaseStatusEffect.WeaponMalfunctionLvl4") {
    StatusEffectHelper.ApplyStatusEffect(
      npc, t"BaseStatusEffect.WeaponMalfunctionLvl4", player.GetEntityID()
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
        if vehicle.IsHackable() && !vehicle.IsQuest() &&
           !vehicle.IsPlayerVehicle() && !vehicle.IsInQuestOrScenePhysicsState() {
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
  let roll: Int32 = RandRange(0, 5);
  if roll == 1 { id = t"Items.QuickHackUncommonMaterial1"; }
  if roll == 2 { id = t"Items.QuickHackRareMaterial1"; }
  if roll == 3 { id = t"Items.QuickHackEpicMaterial1"; }
  if roll == 4 { id = t"Items.QuickHackLegendaryMaterial1"; }
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
  NetDeckPulseNetwork(this);
  let affected: Int32 = this.NetDeckInfiltrateNetwork();
  affected += NetDeckBurnoutNearbyNetrunners(this);
  if affected > 0 { NetDeckGiveDataComponents(this.GetGameInstance()); }
  NetDeckNotify(this.GetGameInstance(), affected);
  return EntityNotificationType.DoNotNotifyEntity;
}

// NPCs are a different quickhack pipeline from devices. The native scanner
// uses ScriptedPuppetPS.GetAllChoices and accepts PuppetQuickHack records.
// Only add an independent action to targets that offer native Ping; this
// deliberately does not modify PingSquad or the default puppet hack list.
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
  if !IsDefined(owner) || !owner.IsQuickHackAble() || owner.IsQuest() {
    return;
  }

  let pingable: Bool = false;
  let i: Int32 = 0;
  while i < ArraySize(Deref(puppetActions)) {
    let existing: ref<PuppetAction> = Deref(puppetActions)[i];
    if IsDefined(existing) {
      let existingRecord: wref<ObjectAction_Record> = existing.GetObjectActionRecord();
      if IsDefined(existingRecord) {
        if Equals(existingRecord.ActionName(), n"NetDeckInfiltration") {
          return;
        }
        if IsDefined(existing as PingSquad) ||
          Equals(existingRecord.ActionName(), n"Ping") ||
          Equals(existingRecord.ActionName(), n"PingSquad") {
          pingable = true;
        }
      }
    }
    i += 1;
  }
  if !pingable {
    return;
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
      let link: ref<PuppetDeviceLinkPS> = this.GetDeviceLink();
      if IsDefined(link) {
        let aps: array<ref<AccessPointControllerPS>> = link.GetAccessPoints();
        if ArraySize(aps) > 0 {
          NetDeckPulseNetwork(aps[0]);
        }
        let affected: Int32 = link.NetDeckInfiltrateLinkedDevices();
        NetDeckTriggerNetWatchPursuit(puppet);
        affected += NetDeckDisruptNpcVicinity(puppet);
        if affected > 0 { NetDeckGiveDataComponents(this.GetGameInstance()); }
        NetDeckNotify(this.GetGameInstance(), affected);
      } else {
        NetDeckTriggerNetWatchPursuit(puppet);
        let affected: Int32 = NetDeckDisruptNpcVicinity(puppet);
        if affected > 0 { NetDeckGiveDataComponents(this.GetGameInstance()); }
        NetDeckNotify(this.GetGameInstance(), affected);
      }
    }
  }
  return EntityNotificationType.DoNotNotifyEntity;
}