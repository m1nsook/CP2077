// NΣT — Infiltration (experimental v0.1.0)
// This is deliberately self-contained. It does not require Camera Killer
// or alter its hooks; Infiltration runs only while the NΣT deck is installed.
module NEST

public class NESTInfiltration extends ActionBool {
  public final func SetProperties() -> Void {
    this.actionName = n"NESTInfiltration";
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
public const func NESTNetworkMaster() -> ref<MasterControllerPS> {
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
public func NESTActionInfiltration() -> ref<NESTInfiltration> {
  let action: ref<NESTInfiltration> = new NESTInfiltration();
  action.clearanceLevel = DefaultActionsParametersHolder.GetInteractiveClearance();
  action.SetUp(this);
  action.SetProperties();
  action.AddDeviceName(this.m_deviceName);
  action.CreateInteraction();
  action.SetObjectActionID(t"DeviceAction.NESTInfiltration");
  action.SetExecutor(GetPlayer(this.GetGameInstance()));
  return action;
}

// The custom entry appears in the vanilla scanner only on a real linked
// device, with NΣT equipped. Leave other decks' quickhack lists alone.
@wrapMethod(ScriptableDeviceComponentPS)
public func GetRemoteActions(
  out outActions: array<ref<DeviceAction>>,
  const context: script_ref<GetActionsContext>
) -> Void {
  wrappedMethod(outActions, context);
  if !IsEquipped(this.GetGameInstance()) ||
    !this.IsON() || this.IsMarkedAsQuest() ||
    !IsDefined(this.NESTNetworkMaster()) {
    return;
  }

  let i: Int32 = 0;
  while i < ArraySize(outActions) {
    let existing: ref<ScriptableDeviceAction> = outActions[i] as ScriptableDeviceAction;
    if IsDefined(existing) {
      let record: wref<ObjectAction_Record> = existing.GetObjectActionRecord();
      if IsDefined(record) && Equals(record.ActionName(), n"NESTInfiltration") {
        return;
      }
    }
    i += 1;
  }

  ArrayPush(outActions, this.NESTActionInfiltration());
  this.MarkActionsAsQuickHacks(outActions);
  this.SetActionsQuickHacksExecutioner(outActions);
}

@wrapMethod(ScriptableDeviceComponentPS)
public const func IsPotentiallyQuickHackable() -> Bool {
  if wrappedMethod() {
    return true;
  }
  return IsEquipped(this.GetGameInstance()) &&
    this.IsON() && !this.IsMarkedAsQuest() &&
    IsDefined(this.NESTNetworkMaster());
}

// Only act on the actual network members: no radius-based entity scan.
@addMethod(ScriptableDeviceComponentPS)
public func NESTInfiltrateNetwork() -> Void {
  if !IsEquipped(this.GetGameInstance()) || this.IsMarkedAsQuest() {
    return;
  }
  let master: ref<MasterControllerPS> = this.NESTNetworkMaster();
  if !IsDefined(master) {
    return;
  }

  let members: array<ref<DeviceComponentPS>>;
  master.GetAllDescendants(members);
  let i: Int32 = 0;
  while i < ArraySize(members) {
    let member: ref<ScriptableDeviceComponentPS> = members[i] as ScriptableDeviceComponentPS;
    if IsDefined(member) && member.IsON() && !member.IsMarkedAsQuest() {
      this.NESTApplyToMember(member);
    }
    i += 1;
  }
}

@addMethod(ScriptableDeviceComponentPS)
private func NESTApplyToMember(member: ref<ScriptableDeviceComponentPS>) -> Void {
  if IsDefined(member as SurveillanceCameraControllerPS) ||
    IsDefined(member as SecurityTurretControllerPS) ||
    IsDefined(member as ExplosiveTriggerDeviceControllerPS) {
    // Uses the existing game OFF event, including Camera Killer's camera
    // shutdown mechanism. No explosion or forced quest progression.
    this.QueuePSEvent(member, member.ActionSetDeviceOFF());
    return;
  }

  // Other regular devices cannot safely be guaranteed to power off.
  // Use the game's own Distract Enemies/Distraction event as fallback.
  // Masters, puppet links and quest-marked devices must never receive it.
  if IsDefined(member as MasterControllerPS) ||
    IsDefined(member as PuppetDeviceLinkPS) {
    return;
  }
  let distraction: ref<QuickHackDistraction> = member.ActionQuickHackDistraction();
  if IsDefined(distraction) {
    distraction.SetDurationValue(6.0);
    this.QueuePSEvent(member, distraction);
  }
}

// The game's action dispatcher is responsible for invoking this after the
// quickhack action is accepted. Do not set scanner or breach blackboards.
@addMethod(ScriptableDeviceComponentPS)
public func OnNESTInfiltration(evt: ref<NESTInfiltration>) -> EntityNotificationType {
  this.NESTInfiltrateNetwork();
  return EntityNotificationType.DoNotNotifyEntity;
}
