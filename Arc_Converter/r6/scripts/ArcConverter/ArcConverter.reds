module MisoArcConverter

@if(ModuleExists("MuchBetterAmmo"))
import MuchBetterAmmo.MuchBetterAmmoRouter

// Arc Converter - Automatic Item Processing
// Gameplay: REDscript. Settings: M1 Arc. No CET hooks.
// Newly acquired weapons or clothes must pass safety checks before processing.

public class ArcConverterSystem extends ScriptableSystem {
  private let m_running: Bool;
  private let m_player: wref<PlayerPuppet>;
  private let m_callback: ref<ArcConverterInventoryCallback>;
  private let m_listener: ref<InventoryScriptListener>;
  private let m_warmupTicks: Int32;
  private let m_pending: array<ItemID>;
  private let m_ammoCooldown: Int32;
  private let m_ammoPreferLeft: Bool;
  private let m_processingSale: Bool;

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
    this.m_ammoPreferLeft = false;
    this.m_processingSale = false;
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

  private func IsWeapon(item: ItemID) -> Bool {
    // Some melee/throwing weapon records are not reported under the
    // generic Weapon category. Detect the actual vanilla item types too.
    let itemType: gamedataItemType = RPGManager.GetItemType(item);
    return RPGManager.IsItemWeapon(item)
      || UIInventoryItemsManager.IsItemTypeMeleeWeapon(itemType)
      || UIInventoryItemsManager.IsItemTypeRangedWeapon(itemType);
  }

  private func IsClothes(item: ItemID) -> Bool {
    let itemType: gamedataItemType = RPGManager.GetItemType(item);
    return RPGManager.IsItemClothing(item)
      || UIInventoryItemsManager.IsItemTypeCloting(itemType);
  }

  private func IsEquipmentType(item: ItemID) -> Bool {
    return this.IsWeapon(item) || this.IsClothes(item);
  }

  private func IsProcessingEnabled(item: ItemID) -> Bool {
    if this.IsWeapon(item) {
      // Keep the existing setting ID so weapon preferences survive upgrades.
      return M1Arc_GetBool("m1nsook.arc-converter", "disassemble");
    };
    if this.IsClothes(item) {
      return M1Arc_GetBool("m1nsook.arc-converter", "clothes_enabled");
    };
    return false;
  }

  private func WantsSell(item: ItemID) -> Bool {
    if this.IsWeapon(item) {
      return Equals(M1Arc_GetString("m1nsook.arc-converter", "weapon_action"), "Sell");
    };
    if this.IsClothes(item) {
      return Equals(M1Arc_GetString("m1nsook.arc-converter", "clothes_action"), "Sell");
    };
    return false;
  }

  public func OnItemAdded(item: ItemID, data: wref<gameItemData>, silent: Bool) -> Void {
    // Watch new individual weapon/clothing acquisitions only. Never scan
    // existing equipment. Ignore internal grants and vendor/menu activity.
    if !this.m_running || this.m_warmupTicks > 0 || silent || this.m_processingSale
      || this.IsMenuOpen() || !M1Arc_GetBool("m1nsook.arc-converter", "enabled")
      || !ItemID.IsValid(item) || !IsDefined(data)
      || !this.IsEquipmentType(item) || !this.IsProcessingEnabled(item) {
      return;
    };
    if !this.IsSafeEquipment(item, data) {
      return;
    };
    // No inventory mutation inside the transaction's inventory callback.
    if !ArrayContains(this.m_pending, item) && ArraySize(this.m_pending) < 64 {
      ArrayPush(this.m_pending, item);
    };
  }

  private func SelectedTier(item: ItemID, quality: gamedataQuality) -> Bool {
    if this.IsWeapon(item) {
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
    };
    if this.IsClothes(item) {
      switch quality {
        case gamedataQuality.Common:
        case gamedataQuality.CommonPlus:
          return M1Arc_GetBool("m1nsook.arc-converter", "clothes_tier1");
        case gamedataQuality.Uncommon:
        case gamedataQuality.UncommonPlus:
          return M1Arc_GetBool("m1nsook.arc-converter", "clothes_tier2");
        case gamedataQuality.Rare:
        case gamedataQuality.RarePlus:
          return M1Arc_GetBool("m1nsook.arc-converter", "clothes_tier3");
        case gamedataQuality.Epic:
        case gamedataQuality.EpicPlus:
          return M1Arc_GetBool("m1nsook.arc-converter", "clothes_tier4");
        case gamedataQuality.Legendary:
        case gamedataQuality.LegendaryPlus:
        case gamedataQuality.LegendaryPlusPlus:
          return M1Arc_GetBool("m1nsook.arc-converter", "clothes_tier5");
        default:
          return false;
      };
    };
    return false;
  }

  private func IsSafeEquipment(item: ItemID, data: wref<gameItemData>) -> Bool {
    if !ItemID.IsValid(item) || !IsDefined(this.m_player) || !IsDefined(data)
      || !this.IsEquipmentType(item) || !this.IsProcessingEnabled(item)
      || RPGManager.IsItemIconic(data) || RPGManager.IsItemCrafted(data)
      || data.HasTag(n"IconicWeapon") || data.HasTag(n"Quest")
      || data.HasTag(n"UnequipBlocked") || data.HasTag(n"LongLasting")
      || data.HasTag(n"base_fists")
      || RPGManager.IsItemEquipped(this.m_player, item)
      || !this.SelectedTier(item, RPGManager.GetItemQuality(data)) {
      return false;
    };
    if this.WantsSell(item) {
      // Do not send unpriced equipment through the conversion path.
      return RPGManager.CalculateSellPrice(this.GetGameInstance(),
        this.m_player, item) > 0;
    };
    let crafting: ref<CraftingSystem> = CraftingSystem.GetInstance(this.GetGameInstance());
    return IsDefined(crafting) && crafting.CanItemBeDisassembled(this.m_player, item);
  }

  private func SellEquipment(item: ItemID, data: wref<gameItemData>,
    ts: ref<TransactionSystem>) -> Void {
    // A vendor-free conversion matching Arc Converter's existing junk sales.
    // Item removal must succeed before eddies are credited.
    let price: Int32 = RPGManager.CalculateSellPrice(this.GetGameInstance(),
      this.m_player, item);
    if price <= 0 || price > 1000000000 {
      return;
    };
    if ts.GetItemQuantity(this.m_player, item) <= 0 {
      return;
    };
    let moneyID: ItemID = ItemID.CreateQuery(t"Items.money");
    let moneyBefore: Int32 = ts.GetItemQuantity(this.m_player, moneyID);
    if moneyBefore > 2000000000 - price {
      return;
    };

    // Ignore our own potential rollback inventory event.
    this.m_processingSale = true;
    if ts.RemoveItem(this.m_player, item, 1) {
      if !ts.GiveItemByTDBID(this.m_player, t"Items.money", price) {
        ts.GiveItem(this.m_player, item, 1);
      };
    };
    this.m_processingSale = false;
  }

  private func ProcessEquipment() -> Void {
    let items: array<ItemID> = this.m_pending;
    ArrayClear(this.m_pending);
    if !M1Arc_GetBool("m1nsook.arc-converter", "enabled")
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
      // Re-check live item, selected tier and action immediately before
      // any modification; items can become equipped or protected meanwhile.
      if this.IsSafeEquipment(item, data) {
        if this.WantsSell(item) {
          this.SellEquipment(item, data, ts);
        } else {
          let request: ref<DisassembleItemRequest> = new DisassembleItemRequest();
          request.target = this.m_player;
          request.itemID = item;
          request.amount = 1;
          crafting.QueueRequest(request);
        };
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
    let left: ref<WeaponObject>;
    if M1Arc_GetBool("m1nsook.arc-converter", "ammo_dual_wield") {
      let right: ref<WeaponObject> = GameObject.GetRightActiveWeapon(this.m_player);
      if IsDefined(right) {
        weapon = right;
      };
      left = GameObject.GetLeftActiveWeapon(this.m_player);
    };
    let distinctLeft: Bool = IsDefined(left);
    if distinctLeft && IsDefined(weapon) {
      distinctLeft = NotEquals(ItemID.GetTDBID(WeaponObject.GetAmmoType(left)),
        ItemID.GetTDBID(WeaponObject.GetAmmoType(weapon)));
    };
    // One recipe per tick, with alternating priority and shared-pool deduplication.
    if distinctLeft && this.m_ammoPreferLeft && this.TryCraftWeaponAmmo(left) {
      this.m_ammoPreferLeft = false;
      return;
    };
    if this.TryCraftWeaponAmmo(weapon) {
      this.m_ammoPreferLeft = true;
      return;
    };
    if distinctLeft && !this.m_ammoPreferLeft && this.TryCraftWeaponAmmo(left) {
      this.m_ammoPreferLeft = false;
    };
  }

  private func TryCraftWeaponAmmo(weapon: ref<WeaponObject>) -> Bool {
    if !IsDefined(weapon) {
      return false;
    };
    let ammoItem: ItemID = WeaponObject.GetAmmoType(weapon);
    if !ItemID.IsValid(ammoItem) {
      return false;
    };
    let ammoID: TweakDBID = ItemID.GetTDBID(ammoItem);
    if !this.IsVanillaAmmo(ammoID) && (!M1Arc_GetBool("m1nsook.arc-converter", "ammo_much_better") || !ArcConverterMBAIsCustom(ammoID)) {
      return false;
    };

    let crafting: ref<CraftingSystem> = CraftingSystem.GetInstance(this.GetGameInstance());
    let ts: ref<TransactionSystem> = GameInstance.GetTransactionSystem(this.GetGameInstance());
    if !IsDefined(crafting) || !IsDefined(ts) || !IsDefined(crafting.GetPlayerCraftBook()) {
      return false;
    };
    let record: ref<Item_Record> = TweakDBInterface.GetItemRecord(ammoID);
    if !IsDefined(record) || !IsDefined(record.CraftingData())
      || !crafting.IsRecipeKnown(ammoID, crafting.GetPlayerCraftBook()) {
      return false;
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
    // MBA's live configuration also covers empty inventory stacks.
    let modCapacity: Int32 = ArcConverterMBACapacity(ammoID);
    if modCapacity > 0 {
      capacity = modCapacity;
    };
    let batch: Int32 = CraftingSystem.GetAmmoBulletAmount(ammoID);
    if capacity <= 0 || batch <= 0 || current + batch > capacity {
      return false;
    };
    let threshold: Int32 = M1Arc_GetInt("m1nsook.arc-converter", "threshold");
    if threshold < 1 || threshold > 100 || current * 100 >= capacity * threshold {
      return false;
    };
    // Let vanilla calculate real component costs and apply crafting perks.
    if !crafting.CanItemBeCrafted(record) {
      return false;
    };

    let request: ref<CraftItemRequest> = new CraftItemRequest();
    request.target = this.m_player;
    request.itemRecord = record;
    request.amount = 1;
    request.bulletAmount = batch;
    crafting.QueueRequest(request);
    // Craft one vanilla recipe batch at a time; re-check resources on next tick.
    this.m_ammoCooldown = 1;
    return true;
  }

  public func Tick() -> Void {
    if !this.m_running {
      return;
    };
    if this.BindPlayer() {
      if this.m_warmupTicks > 0 {
        this.m_warmupTicks -= 1;
      } else {
        this.ProcessEquipment();
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

@if(ModuleExists("MuchBetterAmmo"))
private static func ArcConverterMBACapacity(ammo: TweakDBID) -> Int32 {
  return MuchBetterAmmoRouter.CapacityForAmmo(ammo);
}

@if(!ModuleExists("MuchBetterAmmo"))
private static func ArcConverterMBACapacity(ammo: TweakDBID) -> Int32 {
  return -1;
}

@if(ModuleExists("MuchBetterAmmo"))
private static func ArcConverterMBAIsCustom(ammo: TweakDBID) -> Bool {
  return Equals(ammo, t"MuchBetterAmmo.RevolverAmmo")
    || Equals(ammo, t"MuchBetterAmmo.SMGAmmo")
    || Equals(ammo, t"MuchBetterAmmo.MachineGunAmmo")
    || Equals(ammo, t"MuchBetterAmmo.PrecisionRifleAmmo");
}

@if(!ModuleExists("MuchBetterAmmo"))
private static func ArcConverterMBAIsCustom(ammo: TweakDBID) -> Bool {
  return false;
}
