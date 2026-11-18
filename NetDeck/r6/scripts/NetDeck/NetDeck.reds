// Infiltration — NΣT iconic cyberdeck (Cyberpunk 2077 2.5; test build)
// This is deliberately self-contained. It does not require Camera Killer
// or alter its hooks; Infiltration runs only while the NΣT deck is installed.
module NetDeck

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
  if IsEquipped(this.GetGameInstance()) &&
    this.IsON() && !this.IsMarkedAsQuest() &&
    IsDefined(this.NetDeckNetworkMaster()) {
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

  let record: wref<ObjectAction_Record> =
    TweakDBInterface.GetObjectActionRecord(t"DeviceAction.NetDeckInfiltration");
  if !IsDefined(record) {
    return hacks;
  }

  let i: Int32 = 0;
  while i < ArraySize(hacks) {
    if IsDefined(hacks[i].actionRecord) &&
      Equals(hacks[i].actionRecord.ActionName(), n"NetDeckInfiltration") {
      return hacks;
    }
    i += 1;
  }
  ArrayPush(hacks, PlayerQuickhackData(ItemID.None(), record, 5));
  return hacks;
}

@wrapMethod(ScriptableDeviceComponentPS)
public const func IsPotentiallyQuickHackable() -> Bool {
  if wrappedMethod() {
    return true;
  }
  return IsEquipped(this.GetGameInstance()) &&
    this.IsON() && !this.IsMarkedAsQuest() &&
    IsDefined(this.NetDeckNetworkMaster());
}

// Only act on actual links. Cameras on a neighboring network are untouched.
// Multiple APs sharing one target are deduplicated, not processed twice.
@addMethod(ScriptableDeviceComponentPS)
public func NetDeckInfiltrateNetwork() -> Void {
  if !IsEquipped(this.GetGameInstance()) || this.IsMarkedAsQuest() {
    return;
  }

  let seen: array<ref<ScriptableDeviceComponentPS>>;
  let accessPoints: array<ref<AccessPointControllerPS>> = this.GetAccessPoints();
  let i: Int32 = 0;
  let foundAP: Bool = false;
  while i < ArraySize(accessPoints) {
    if IsDefined(accessPoints[i]) {
      foundAP = true;
      this.NetDeckInfiltrateMaster(accessPoints[i], seen);
    }
    i += 1;
  }

  // Devices on a virtual surveillance system may not have an explicit AP.
  if !foundAP {
    this.NetDeckInfiltrateMaster(this.NetDeckNetworkMaster(), seen);
  }
}

@addMethod(ScriptableDeviceComponentPS)
private func NetDeckInfiltrateMaster(
  master: ref<MasterControllerPS>,
  out seen: array<ref<ScriptableDeviceComponentPS>>
) -> Void {
  if !IsDefined(master) {
    return;
  }
  let members: array<ref<DeviceComponentPS>>;
  master.GetAllDescendants(members);
  let i: Int32 = 0;
  while i < ArraySize(members) {
    let member: ref<ScriptableDeviceComponentPS> =
      members[i] as ScriptableDeviceComponentPS;
    if IsDefined(member) && !ArrayContains(seen, member) {
      ArrayPush(seen, member);
      if member.IsON() && !member.IsMarkedAsQuest() {
        this.NetDeckApplyToMember(member);
      }
    }
    i += 1;
  }
}

@addMethod(ScriptableDeviceComponentPS)
private func NetDeckApplyToMember(member: ref<ScriptableDeviceComponentPS>) -> Void {
  if IsDefined(member as SurveillanceCameraControllerPS) ||
    IsDefined(member as SecurityTurretControllerPS) ||
    IsDefined(member as ExplosiveTriggerDeviceControllerPS) {
    // Uses the same game's OFF event as Camera Killer, but only when the
    // player intentionally uploads Infiltration. No automatic global hooks.
    this.QueuePSEvent(member, member.ActionSetDeviceOFF());
    return;
  }

  // Don't send a synthetic distraction to every device: the base controller
  // constructs a QuickHackDistraction even for devices with no such feature.
  // Only dispatch an actual offered, active game-created quickhack action.
  // Master controllers are not eligible distraction targets.
  // PuppetDeviceLinkPS is not a ScriptableDeviceComponentPS subtype.
  if IsDefined(member as MasterControllerPS) {
    return;
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
      return;
    }
    i += 1;
  }
}

// The game's action dispatcher is responsible for invoking this after the
// quickhack action is accepted. Do not set scanner or breach blackboards.
@addMethod(ScriptableDeviceComponentPS)
protected cb func OnNetDeckInfiltration(evt: ref<NetDeckInfiltration>) -> EntityNotificationType {
  this.NetDeckInfiltrateNetwork();
  return EntityNotificationType.DoNotNotifyEntity;
}
