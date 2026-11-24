module MisoArcConverter

// Arc Converter - Automatic Item Processing
// Independent junk and consumable processor. Equipment remains acquisition-only.
//
// Matches the vanilla "Sell Junk" classification (Gen_Junk + Gen_Jewellery).
// This is a vendor-free exchange at the game's native sell price; it is
// not a real MarketSystem/vendor transaction and does not create buyback stock.

public class ArcConverterJunkSystem extends ScriptableSystem {
  private let m_running: Bool;
  private let m_player: wref<PlayerPuppet>;
  private let m_warmupTicks: Int32;

  private func OnAttach() -> Void {
    this.m_running = true;
    this.ScheduleNext();
  }

  private func OnDetach() -> Void {
    this.m_running = false;
    this.m_player = null;
    this.m_warmupTicks = 0;
  }

  private func ScheduleNext() -> Void {
    if !this.m_running {
      return;
    };
    let callback: ref<ArcConverterJunkTick> = new ArcConverterJunkTick();
    callback.owner = this;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(callback, 1.00, false);
  }

  private func IsMenuOpen() -> Bool {
    let board: ref<IBlackboard> = GameInstance.GetBlackboardSystem(
      this.GetGameInstance()).Get(GetAllBlackboardDefs().UI_System);
    // Never process items when the menu state is unavailable.
    if !IsDefined(board) {
      return true;
    };
    return board.GetBool(GetAllBlackboardDefs().UI_System.IsInMenu);
  }

  private func IsSafeStack(data: wref<gameItemData>) -> Bool {
    if !IsDefined(data) {
      return false;
    };
    let id: ItemID = data.GetID();
    if !ItemID.IsValid(id) || ItemID.HasFlag(id, gameEItemIDFlag.Preview)
      || data.HasTag(n"Quest") || data.HasTag(n"UnequipBlocked")
      || data.HasTag(n"IconicWeapon")
      || RPGManager.IsItemIconic(data) || RPGManager.IsItemEquipped(this.m_player, id) {
      return false;
    };
    return true;
  }

  private func IsSellableJunk(data: wref<gameItemData>) -> Bool {
    if !this.IsSafeStack(data) || data.HasTag(n"LongLasting") {
      return false;
    };
    let itemType: gamedataItemType = RPGManager.GetItemType(data.GetID());
    // Exactly what the game's "Sell Junk" button considers junk.
    return Equals(itemType, gamedataItemType.Gen_Junk)
      || Equals(itemType, gamedataItemType.Gen_Jewellery);
  }

  // REDscript has no 'continue' statement. Per-stack guards return from
  // this helper instead of trying to skip a while-loop iteration.
  private func SellJunkStack(gi: GameInstance, ts: ref<TransactionSystem>,
    data: wref<gameItemData>) -> Bool {
    if !this.IsSafeStack(data) {
      return false;
    };

    let item: ItemID = data.GetID();
    // Recheck the live stack, not the inventory snapshot.
    let quantity: Int32 = Min(data.GetQuantity(), ts.GetItemQuantity(this.m_player, item));
    if !this.IsSellableJunk(data) {
      quantity -= Max(0, M1Arc_GetInt("m1nsook.arc-converter", "consumables_keep"));
    };
    quantity = Min(128, quantity);
    if quantity <= 0 {
      return false;
    };

    // Same native sell-price calculation used by the inventory UI.
    let price: Int32 = RPGManager.CalculateSellPrice(
      gi, this.m_player, item);
    if price <= 0 || price > 1000000000 / quantity {
      // Never discard junk for free or allow Int32 payout overflow.
      return false;
    };

    let payment: Int32 = price * quantity;
    let moneyID: ItemID = ItemID.CreateQuery(t"Items.money");
    let moneyBefore: Int32 = ts.GetItemQuantity(this.m_player, moneyID);
    if moneyBefore > 2000000000 - payment {
      return false;
    };

    // Only pay if removal succeeded. Attempt to return the same junk on
    // the rare failure to grant currency rather than silently losing it.
    if ts.RemoveItem(this.m_player, item, quantity) {
      if !ts.GiveItemByTDBID(this.m_player, t"Items.money", payment) {
        ts.GiveItem(this.m_player, item, quantity);
        return false;
      };
      return true;
    };
    return false;
  }

  private func ConsumableTier(data: wref<gameItemData>) -> Int32 {
    switch RPGManager.GetItemQuality(data) {
      case gamedataQuality.Common:
      case gamedataQuality.CommonPlus: return 1;
      case gamedataQuality.Uncommon:
      case gamedataQuality.UncommonPlus: return 2;
      case gamedataQuality.Rare:
      case gamedataQuality.RarePlus: return 3;
      case gamedataQuality.Epic:
      case gamedataQuality.EpicPlus: return 4;
      case gamedataQuality.Legendary:
      case gamedataQuality.LegendaryPlus:
      case gamedataQuality.LegendaryPlusPlus: return 5;
    };
    return 0;
  }

  private func ConsumableAction(data: wref<gameItemData>) -> String {
    let ty: gamedataItemType = RPGManager.GetItemType(data.GetID());
    let tier: Int32 = this.ConsumableTier(data);
    if Equals(ty, gamedataItemType.Con_Edible) && M1Arc_GetBool("m1nsook.arc-converter", "food_enabled") {
      switch tier {
        case 1: return M1Arc_GetString("m1nsook.arc-converter", "food_tier1");
        case 2: return M1Arc_GetString("m1nsook.arc-converter", "food_tier2");
        case 3: return M1Arc_GetString("m1nsook.arc-converter", "food_tier3");
        case 4: return M1Arc_GetString("m1nsook.arc-converter", "food_tier4");
        case 5: return M1Arc_GetString("m1nsook.arc-converter", "food_tier5");
      };
    };
    if Equals(ty, gamedataItemType.Con_LongLasting) && M1Arc_GetBool("m1nsook.arc-converter", "boosters_enabled") {
      switch tier {
        case 1: return M1Arc_GetString("m1nsook.arc-converter", "boosters_tier1");
        case 2: return M1Arc_GetString("m1nsook.arc-converter", "boosters_tier2");
        case 3: return M1Arc_GetString("m1nsook.arc-converter", "boosters_tier3");
        case 4: return M1Arc_GetString("m1nsook.arc-converter", "boosters_tier4");
        case 5: return M1Arc_GetString("m1nsook.arc-converter", "boosters_tier5");
      };
    };
    return "Keep";
  }

  private func ProcessStack(gi: GameInstance, ts: ref<TransactionSystem>, data: wref<gameItemData>) -> Bool {
    if !this.IsSafeStack(data) {
      return false;
    };
    let item: ItemID = data.GetID();
    let action: String;
    let keep: Int32 = 0;
    if this.IsSellableJunk(data) {
      if !M1Arc_GetBool("m1nsook.arc-converter", "sell_junk") {
        return false;
      };
      action = M1Arc_GetString("m1nsook.arc-converter", "junk_action");
      if Equals(action, "By value") {
        let price: Int32 = RPGManager.CalculateSellPrice(gi, this.m_player, item);
        if price <= M1Arc_GetInt("m1nsook.arc-converter", "junk_price_threshold") {
          action = "Disassemble";
        } else {
          action = "Sell";
        };
      };
    } else {
      action = this.ConsumableAction(data);
      keep = Max(0, M1Arc_GetInt("m1nsook.arc-converter", "consumables_keep"));
    };
    if Equals(action, "Sell") {
      return this.SellJunkStack(gi, ts, data);
    };
    if !Equals(action, "Disassemble") {
      return false;
    };
    let crafting: ref<CraftingSystem> = CraftingSystem.GetInstance(gi);
    let quantity: Int32 = Min(128, Min(data.GetQuantity(), ts.GetItemQuantity(this.m_player, item)) - keep);
    if quantity <= 0 || !IsDefined(crafting) || !crafting.CanItemBeDisassembled(this.m_player, item) {
      return false;
    };
    let request: ref<DisassembleItemRequest> = new DisassembleItemRequest();
    request.target = this.m_player;
    request.itemID = item;
    request.amount = quantity;
    crafting.QueueRequest(request);
    return true;
  }

  private func SellJunk() -> Void {
    if (!M1Arc_GetBool("m1nsook.arc-converter", "sell_junk")
      && !M1Arc_GetBool("m1nsook.arc-converter", "food_enabled")
      && !M1Arc_GetBool("m1nsook.arc-converter", "boosters_enabled"))
      || !M1Arc_GetBool("m1nsook.arc-converter", "enabled")
      || this.IsMenuOpen() {
      return;
    };

    let gi: GameInstance = this.GetGameInstance();
    let ts: ref<TransactionSystem> = GameInstance.GetTransactionSystem(gi);
    if !IsDefined(ts) || !IsDefined(this.m_player) {
      return;
    };

    let items: array<wref<gameItemData>>;
    ts.GetItemList(this.m_player, items);

    let processed: Int32 = 0;
    let i: Int32 = 0;
    // Bound per-tick processing and allow the inventory to settle between ticks.
    while i < ArraySize(items) && processed < 12 {
      let data: wref<gameItemData> = items[i];
      i += 1;
      if this.ProcessStack(gi, ts, data) {
        processed += 1;
      };
    };
  }

  public func Tick() -> Void {
    if !this.m_running {
      return;
    };

    let player: ref<PlayerPuppet> = GetPlayer(this.GetGameInstance()) as PlayerPuppet;
    if !IsDefined(player) {
      this.m_player = null;
      this.m_warmupTicks = 0;
    } else {
      if !IsDefined(this.m_player) ||
        NotEquals(this.m_player.GetEntityID(), player.GetEntityID()) {
        this.m_player = player;
        // Prevent inventory changes during player/save initialization.
        this.m_warmupTicks = 3;
      } else {
        if this.m_warmupTicks > 0 {
          this.m_warmupTicks -= 1;
        } else {
          this.SellJunk();
        };
      };
    };
    this.ScheduleNext();
  }
}

public class ArcConverterJunkTick extends DelayCallback {
  public let owner: wref<ArcConverterJunkSystem>;

  public func Call() -> Void {
    if IsDefined(this.owner) {
      this.owner.Tick();
    };
  }
}
