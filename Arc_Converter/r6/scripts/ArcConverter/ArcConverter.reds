module MisoArcConverter

// Arc Converter - Automatic Item Processing
// Gameplay: REDscript. Settings: M1 Arc. No CET hooks.
// A new weapon must pass all safety checks twice before disassembly.

public class ArcConverterSystem extends ScriptableSystem {
  private let m_running: Bool;
  private let m_player: wref<PlayerPuppet>;
  private let m_callback: ref<ArcConverterInventoryCallback>;
  private let m_listener: ref<InventoryScriptListener>;
  private let m_warmupTicks: Int32;
  private let m_pending: array<ItemID>;
  private let m_ammoCooldown: Int32;

  private func OnAttach() -> Void {
    this.m_running = true;
    this.ScheduleNext();
  }

  private func OnDetach() -> Void {
    this.m_running = false;
    this.UnbindPlayer();
  }

  private func ScheduleNext() -> Void {
    if !this.m_running {
      return;
    };
    let callback: ref<ArcConverterTick> = new ArcConverterTick();
    callback.owner = this;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(callback, 0.50, false);
  }

  private func UnbindPlayer() -> Void {
    if IsDefined(this.m_player) && IsDefined(this.m_listener) {
      let ts: ref<TransactionSystem> = GameInstance.GetTransactionSystem(this.GetGameInstance());
      if IsDefined(ts) {
        ts.UnregisterInventoryListener(this.m_player, this.m_listener);
      };
    };
    this.m_listener = null;
    this.m_callback = null;
    this.m_player = null;
    this.m_warmupTicks = 0;
    this.m_ammoCooldown = 0;
    ArrayClear(this.m_pending);
  }

  private func BindPlayer() -> Bool {
    let player: ref<PlayerPuppet> = GetPlayer(this.GetGameInstance()) as PlayerPuppet;
    if !IsDefined(player) {
      this.UnbindPlayer();
      return false;
    };
    if IsDefined(this.m_player) && Equals(this.m_player.GetEntityID(), player.GetEntityID())
      && IsDefined(this.m_listener) {
      return true;
    };

    this.UnbindPlayer();
    let ts: ref<TransactionSystem> = GameInstance.GetTransactionSystem(this.GetGameInstance());
    if !IsDefined(ts) {
      return false;
    };
    let callback: ref<ArcConverterInventoryCallback> = new ArcConverterInventoryCallback();
    callback.owner = this;
    this.m_callback = callback;
    this.m_listener = ts.RegisterInventoryListener(player, callback);
    if !IsDefined(this.m_listener) {
      this.m_callback = null;
      return false;
    };
    this.m_player = player;
    // Do not process any inventory notifications produced during player load.
    this.m_warmupTicks = 2;
    return true;
  }

  private func IsMenuOpen() -> Bool {
    let bb: ref<IBlackboard> = GameInstance.GetBlackboardSystem(
      this.GetGameInstance()).Get(GetAllBlackboardDefs().UI_System);
    // Refuse destructive processing when menu state is unknown.
    if !IsDefined(bb) {
      return true;
    };
    return bb.GetBool(GetAllBlackboardDefs().UI_System.IsInMenu);
  }

  public func OnItemAdded(item: ItemID, data: wref<gameItemData>, silent: Bool) -> Void {
    // Register only newly added individual weapons, never existing inventory.
    // Skip silent internal grants and acquisitions while a menu/shop is open.
    if !this.m_running || this.m_warmupTicks > 0 || silent || this.IsMenuOpen()
      || !M1Arc_GetBool("m1nsook.arc-converter", "enabled")
      || !M1Arc_GetBool("m1nsook.arc-converter", "disassemble")
      || !ItemID.IsValid(item) || !IsDefined(data)
      || !RPGManager.IsItemWeapon(item) {
      return;
    };
    if !this.IsSafeWeapon(item, data) {
      return;
    };
    // Never disassemble within the transaction callback.
    if !ArrayContains(this.m_pending, item) && ArraySize(this.m_pending) < 64 {
      ArrayPush(this.m_pending, item);
    };
  }

  private func SelectedTier(quality: gamedataQuality) -> Bool {
    switch quality {
      case gamedataQuality.Common:
      case gamedataQuality.CommonPlus:
        return M1Arc_GetBool("m1nsook.arc-converter", "tier1");
      case gamedataQuality.Uncommon:
      case gamedataQuality.UncommonPlus:
        return M1Arc_GetBool("m1nsook.arc-converter", "tier2");
      case gamedataQuality.Rare:
      case gamedataQuality.RarePlus:
        return M1Arc_GetBool("m1nsook.arc-converter", "tier3");
      case gamedataQuality.Epic:
      case gamedataQuality.EpicPlus:
        return M1Arc_GetBool("m1nsook.arc-converter", "tier4");
      case gamedataQuality.Legendary:
      case gamedataQuality.LegendaryPlus:
      case gamedataQuality.LegendaryPlusPlus:
        return M1Arc_GetBool("m1nsook.arc-converter", "tier5");
      default:
        return false;
    };
  }

  private func IsSafeWeapon(item: ItemID, data: wref<gameItemData>) -> Bool {
    if !IsDefined(this.m_player) || !IsDefined(data)
      || !RPGManager.IsItemWeapon(item)
      || RPGManager.IsItemIconic(data) || RPGManager.IsItemCrafted(data)
      || data.HasTag(n"IconicWeapon") || data.HasTag(n"Quest")
      || data.HasTag(n"UnequipBlocked") || data.HasTag(n"LongLasting")
      || RPGManager.IsItemEquipped(this.m_player, item)
      || !this.SelectedTier(RPGManager.GetItemQuality(data)) {
      return false;
    };
    let crafting: ref<CraftingSystem> = CraftingSystem.GetInstance(this.GetGameInstance());
    return IsDefined(crafting) && crafting.CanItemBeDisassembled(this.m_player, item);
  }

  private func ProcessDisassembly() -> Void {
    let items: array<ItemID> = this.m_pending;
    ArrayClear(this.m_pending);
    if !M1Arc_GetBool("m1nsook.arc-converter", "enabled")
      || !M1Arc_GetBool("m1nsook.arc-converter", "disassemble")
      || this.IsMenuOpen() {
      return;
    };
    let ts: ref<TransactionSystem> = GameInstance.GetTransactionSystem(this.GetGameInstance());
    let crafting: ref<CraftingSystem> = CraftingSystem.GetInstance(this.GetGameInstance());
    if !IsDefined(ts) || !IsDefined(crafting) {
      return;
    };
    let i: Int32 = 0;
    while i < ArraySize(items) {
      let item: ItemID = items[i];
      let data: wref<gameItemData> = ts.GetItemData(this.m_player, item);
      if this.IsSafeWeapon(item, data) {
        let request: ref<DisassembleItemRequest> = new DisassembleItemRequest();
        request.target = this.m_player;
        request.itemID = item;
        request.amount = 1;
        crafting.QueueRequest(request);
      };
      i += 1;
    };
  }

  private func IsVanillaAmmo(ammo: TweakDBID) -> Bool {
    return Equals(ammo, t"Ammo.HandgunAmmo") || Equals(ammo, t"Ammo.RifleAmmo")
      || Equals(ammo, t"Ammo.ShotgunAmmo") || Equals(ammo, t"Ammo.SniperRifleAmmo");
  }

  // Used only if an exhausted ammo stack has no inventory item data.
  private func VanillaAmmoCapacity(ammo: TweakDBID) -> Int32 {
    if Equals(ammo, t"Ammo.HandgunAmmo") {
      return 500;
    };
    if Equals(ammo, t"Ammo.RifleAmmo") {
      return 700;
    };
    if Equals(ammo, t"Ammo.ShotgunAmmo") || Equals(ammo, t"Ammo.SniperRifleAmmo") {
      return 100;
    };
    return 0;
  }

  private func CraftEquippedAmmo() -> Void {
    if !M1Arc_GetBool("m1nsook.arc-converter", "enabled")
      || !M1Arc_GetBool("m1nsook.arc-converter", "ammo")
      || this.IsMenuOpen() {
      return;
    };
    if this.m_ammoCooldown > 0 {
      this.m_ammoCooldown -= 1;
      return;
    };

    let weapon: ref<WeaponObject> = ScriptedPuppet.GetActiveWeapon(this.m_player);
    if !IsDefined(weapon) {
      return;
    };
    let ammoItem: ItemID = WeaponObject.GetAmmoType(weapon);
    if !ItemID.IsValid(ammoItem) {
      return;
    };
    let ammoID: TweakDBID = ItemID.GetTDBID(ammoItem);
    if !this.IsVanillaAmmo(ammoID) {
      return;
    };

    let crafting: ref<CraftingSystem> = CraftingSystem.GetInstance(this.GetGameInstance());
    let ts: ref<TransactionSystem> = GameInstance.GetTransactionSystem(this.GetGameInstance());
    if !IsDefined(crafting) || !IsDefined(ts) || !IsDefined(crafting.GetPlayerCraftBook()) {
      return;
    };
    let record: ref<Item_Record> = TweakDBInterface.GetItemRecord(ammoID);
    if !IsDefined(record) || !IsDefined(record.CraftingData())
      || !crafting.IsRecipeKnown(ammoID, crafting.GetPlayerCraftBook()) {
      return;
    };

    let current: Int32 = ts.GetItemQuantity(this.m_player, ammoItem);
    let capacity: Int32 = this.VanillaAmmoCapacity(ammoID);
    let data: wref<gameItemData> = ts.GetItemData(this.m_player, ammoItem);
    if IsDefined(data) {
      let liveCap: Int32 = Cast<Int32>(data.GetStatValueByType(gamedataStatType.Quantity));
      if liveCap > 0 {
        capacity = liveCap;
      };
    };
    let batch: Int32 = CraftingSystem.GetAmmoBulletAmount(ammoID);
    if capacity <= 0 || batch <= 0 || current + batch > capacity {
      return;
    };
    let threshold: Int32 = M1Arc_GetInt("m1nsook.arc-converter", "threshold");
    if threshold < 1 || threshold > 100 || current * 100 >= capacity * threshold {
      return;
    };
    // Let vanilla calculate real component costs and apply crafting perks.
    if !crafting.CanItemBeCrafted(record) {
      return;
    };

    let request: ref<CraftItemRequest> = new CraftItemRequest();
    request.target = this.m_player;
    request.itemRecord = record;
    request.amount = 1;
    request.bulletAmount = batch;
    crafting.QueueRequest(request);
    // Craft one vanilla recipe batch at a time; re-check resources on next tick.
    this.m_ammoCooldown = 1;
  }

  public func Tick() -> Void {
    if !this.m_running {
      return;
    };
    if this.BindPlayer() {
      if this.m_warmupTicks > 0 {
        this.m_warmupTicks -= 1;
      } else {
        this.ProcessDisassembly();
        this.CraftEquippedAmmo();
      };
    };
    this.ScheduleNext();
  }
}

public class ArcConverterInventoryCallback extends InventoryScriptCallback {
  public let owner: wref<ArcConverterSystem>;

  public func OnItemAdded(item: ItemID, itemData: wref<gameItemData>, flaggedAsSilent: Bool) -> Void {
    if IsDefined(this.owner) {
      this.owner.OnItemAdded(item, itemData, flaggedAsSilent);
    };
  }
}

public class ArcConverterTick extends DelayCallback {
  public let owner: wref<ArcConverterSystem>;

  public func Call() -> Void {
    if IsDefined(this.owner) {
      this.owner.Tick();
    };
  }
}
