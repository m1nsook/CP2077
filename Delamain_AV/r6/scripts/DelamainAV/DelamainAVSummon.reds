module DelamainAV

// Use the game's existing vehicle popup. Do not unlock/register a drivable
// vehicle or replace the real Delamain Cab. The row exists only in the popup.
public static func AVSummonRows(player: ref<GameObject>) -> array<ref<IScriptable>> {
  let rows = VehiclesManagerDataHelper.GetVehicles(player);
  let record = TweakDBInterface.GetVehicleRecord(t"Vehicle.m1nsook_delamain_av");
  let cab = TweakDBInterface.GetVehicleRecord(t"Vehicle.v_delamain_taxi");
  if !IsDefined(record) || !IsDefined(cab) { return rows; };
  let row = new VehicleListItemData();
  let data: PlayerVehicle;
  data.name = n"Delamain AV";
  data.recordID = t"Vehicle.m1nsook_delamain_av";
  data.vehicleType = gamedataVehicleType.Car;
  data.isUnlocked = true;
  data.uiFavoriteIndex = -1;
  data.overrideDisplay = true;
  data.icon = n"delamain";
  data.defaultState = n"Delamain";
  data.activeState = n"DelamainActive";
  data.forcedFavorite = true;
  data.spawnOnlyOnValidRoad = false;
  row.m_data = data;
  row.m_displayName = n"Delamain AV";
  row.m_icon = cab.Icon();
  row.m_repairTimeRemaining = 0.0;
  row.m_canBeActive = false;
  ArrayPush(rows, row);
  return rows;
}

@wrapMethod(VehiclesManagerDataView)
public func SortItem(lhs: ref<IScriptable>, rhs: ref<IScriptable>) -> Bool {
  let left = lhs as VehicleListItemData;
  let right = rhs as VehicleListItemData;
  if IsDefined(left) && IsDefined(right) {
    if Equals(left.m_data.recordID, t"Vehicle.m1nsook_delamain_av") { return NotEquals(right.m_data.recordID, t"Vehicle.m1nsook_delamain_av"); };
    if Equals(right.m_data.recordID, t"Vehicle.m1nsook_delamain_av") { return false; };
  };
  return wrappedMethod(lhs, rhs);
}

@wrapMethod(VehiclesManagerPopupGameController)
protected func SetupData() -> Void {
  wrappedMethod();
  this.m_dataSource.Reset(AVSummonRows(this.m_playerPuppet));
}

@wrapMethod(VehiclesManagerPopupGameController)
private func ToggleFavorite() -> Void {
  let item = this.m_listController.GetSelectedItem() as VehiclesManagerListItemController;
  if IsDefined(item) {
    let data = item.GetVehicleData();
    if IsDefined(data) && Equals(data.m_data.recordID, t"Vehicle.m1nsook_delamain_av") { return; };
  };
  wrappedMethod();
  let index = this.m_listController.GetSelectedIndex();
  this.m_dataSource.Reset(AVSummonRows(this.m_playerPuppet));
  this.m_listController.SelectItem(index);
}

@wrapMethod(VehiclesManagerListItemController)
protected cb func OnDataChanged(value: Variant) -> Bool {
  let result = wrappedMethod(value);
  if IsDefined(this.m_vehicleData) && Equals(this.m_vehicleData.m_data.recordID, t"Vehicle.m1nsook_delamain_av") {
    inkTextRef.SetText(this.m_label, "Delamain AV");
  };
  return result;
}

@wrapMethod(VehiclesManagerPopupGameController)
protected func Activate() -> Void {
  let selected = this.m_listController.GetSelectedItem() as VehiclesManagerListItemController;
  if IsDefined(selected) {
    let data = selected.GetVehicleData();
    if IsDefined(data) && Equals(data.m_data.recordID, t"Vehicle.m1nsook_delamain_av") {
      if IsDefined(this.m_selectAnimProxy) { return; };
      let player = this.m_playerPuppet as PlayerPuppet;
      if !IsDefined(player) { return; };
      let system = GameInstance.GetScriptableSystemsContainer(player.GetGame()).Get(n"DelamainAV.DelamainAVSystem") as DelamainAVSystem;
      if IsDefined(system) { system.QueueSummon(); };
      this.Close();
      this.PlaySound(n"Holocall", n"OnHangUp");
      return;
    };
  };
  wrappedMethod();
}
