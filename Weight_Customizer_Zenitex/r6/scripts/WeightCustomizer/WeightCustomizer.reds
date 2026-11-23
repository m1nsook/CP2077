module MisoWeightCustomizer

// Live carry-capacity configuration from M1 Arc's native registry.
//
// Only the temporary additive modifier created by this system is removed.
// Never remove all modifiers: cyberware, perks, and other mods own theirs.
public class WeightCustomizerSystem extends ScriptableSystem {
  private let m_running: Bool;
  private let m_hasModifier: Bool;
  private let m_ownerEntity: EntityID;
  private let m_modifier: ref<gameConstantStatModifierData>;

  // Zenitex addon owns a second modifier; do not combine it with the
  // custom-capacity modifier or overwrite other mods' contributions.
  private let m_zenitexModifier: ref<gameConstantStatModifierData>;
  private let m_zenitexHasModifier: Bool;
  private let m_zenitexOwnerEntity: EntityID;
  private let m_zenitexBonus: Float;

  private func OnAttach() -> Void {
    this.m_running = true;
    this.ApplyCapacity();
    this.ScheduleNext();
  }

  private func OnDetach() -> Void {
    this.m_running = false;
    this.RemoveOwnedModifier();
    this.RemoveZenitexModifier();
  }

  private func ScheduleNext() -> Void {
    if !this.m_running {
      return;
    }
    let callback = new WeightCustomizerTick();
    callback.owner = this;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(callback, 0.50, false);
  }

  public func Tick() -> Void {
    if !this.m_running {
      return;
    }
    this.ApplyCapacity();
    this.ScheduleNext();
  }

  private func ForgetModifier() -> Void {
    this.m_modifier = null;
    this.m_hasModifier = false;
  }

  private func RemoveOwnedModifier() -> Void {
    if this.m_hasModifier && IsDefined(this.m_modifier) {
      let stats = GameInstance.GetStatsSystem(this.GetGameInstance());
      if IsDefined(stats) {
        // Do NOT remove the vanilla game stat or anyone else's modifiers.
        stats.RemoveModifier(Cast<StatsObjectID>(this.m_ownerEntity), this.m_modifier);
      }
    }
    this.ForgetModifier();
  }

  private func ForgetZenitexModifier() -> Void {
    this.m_zenitexModifier = null;
    this.m_zenitexHasModifier = false;
    this.m_zenitexBonus = 0.0;
  }

  private func RemoveZenitexModifier() -> Void {
    if this.m_zenitexHasModifier && IsDefined(this.m_zenitexModifier) {
      let stats = GameInstance.GetStatsSystem(this.GetGameInstance());
      if IsDefined(stats) {
        stats.RemoveModifier(Cast<StatsObjectID>(this.m_zenitexOwnerEntity), this.m_zenitexModifier);
      }
    }
    this.ForgetZenitexModifier();
  }

  // Equipment-EX represents appearance-only items as preview items in
  // OutfitSlots.Back. Never award a bonus just for owning a backpack.
  // Vanilla equipment slots already apply Zenitex's intrinsic stat mod,
  // so they must NOT receive an extra copy from this compatibility addon.
  private func GetZenitexBonus(player: ref<PlayerPuppet>) -> Float {
    let transaction = GameInstance.GetTransactionSystem(this.GetGameInstance());
    if !IsDefined(transaction) {
      return 0.0;
    }

    let backItem = transaction.GetItemInSlot(player, t"OutfitSlots.Back");
    if !IsDefined(backItem) {
      return 0.0;
    }

    let itemID = backItem.GetItemID();
    if !ItemID.IsValid(itemID) || !ItemID.HasFlag(itemID, gameEItemIDFlag.Preview) {
      return 0.0;
    }

    let record = TweakDBInterface.GetItemRecord(ItemID.GetTDBID(itemID));
    if !IsDefined(record) {
      return 0.0;
    }

    let tags = record.Tags();
    if ArrayContains(tags, n"ZenitexBackpackCustomModIconic") {
      return 250.0;
    }
    if !ArrayContains(tags, n"ZenitexBackpackCustomMod") {
      return 0.0;
    }

    let itemData = transaction.GetItemData(player, itemID);
    if !IsDefined(itemData) {
      return 0.0;
    }

    let quality = RPGManager.GetItemQuality(itemData);
    if Equals(quality, gamedataQuality.Legendary) {
      return 240.0;
    }
    if Equals(quality, gamedataQuality.Epic) {
      return 230.0;
    }
    if Equals(quality, gamedataQuality.Rare) {
      return 220.0;
    }
    if Equals(quality, gamedataQuality.Uncommon) {
      return 210.0;
    }
    if Equals(quality, gamedataQuality.Common) {
      return 200.0;
    }
    return 0.0;
  }

  private func UpdateZenitex(
    player: ref<PlayerPuppet>,
    stats: ref<StatsSystem>,
    currentEntity: EntityID
  ) -> Void {
    let bonus = this.GetZenitexBonus(player);
    if this.m_zenitexHasModifier && this.m_zenitexBonus == bonus {
      return;
    }
    this.RemoveZenitexModifier();

    if bonus <= 0.0 {
      return;
    }

    let modifier = RPGManager.CreateStatModifier(
      gamedataStatType.CarryCapacity,
      gameStatModifierType.Additive,
      bonus
    ) as gameConstantStatModifierData;
    if !IsDefined(modifier) {
      return;
    }

    stats.AddModifier(Cast<StatsObjectID>(currentEntity), modifier);
    this.m_zenitexModifier = modifier;
    this.m_zenitexOwnerEntity = currentEntity;
    this.m_zenitexBonus = bonus;
    this.m_zenitexHasModifier = true;
  }

  private func ApplyCapacity() -> Void {
    let gi = this.GetGameInstance();
    let player = GetPlayer(gi) as PlayerPuppet;
    let stats = GameInstance.GetStatsSystem(gi);
    if !IsDefined(player) || !IsDefined(stats) {
      // On save transitions the old game entity is already being destroyed.
      // Never run an old modifier against the new save's player.
      this.ForgetModifier();
      this.ForgetZenitexModifier();
      return;
    }

    let currentEntity = player.GetEntityID();
    if this.m_hasModifier && NotEquals(currentEntity, this.m_ownerEntity) {
      // A different game session/player owns a different StatsObjectID.
      this.ForgetModifier();
    }
    if this.m_zenitexHasModifier && NotEquals(currentEntity, this.m_zenitexOwnerEntity) {
      this.ForgetZenitexModifier();
    }

    // Reconcile appearance-only Zenitex bonus before the fixed-total logic,
    // otherwise Weight Customizer would cancel the new capacity on its next tick.
    this.UpdateZenitex(player, stats, currentEntity);

    // Missing manifest/settings produces false/0, so the mod fails closed.
    let enabled = M1Arc_GetBool("m1nsook.weight-customizer", "enabled");
    let target = M1Arc_GetInt("m1nsook.weight-customizer", "limit");
    if !enabled || target < 50 {
      this.RemoveOwnedModifier();
      return;
    }

    let entity = Cast<StatsObjectID>(currentEntity);
    // Weight Customizer's configured total is the baseline, not the ceiling:
    // an equipped Zenitex backpack adds its own separate bonus on top.
    let wanted = Cast<Float>(target) + this.m_zenitexBonus;
    let actual = stats.GetStatValue(entity, gamedataStatType.CarryCapacity);

    // Recheck every half second: responds to settings changes *and* to
    // perk/cyberware stat changes without requiring the settings UI to open.
    if actual >= wanted - 0.50 && actual <= wanted + 0.50 {
      return;
    }

    // Rebuild ONLY our own additive modifier using the new vanilla+modded
    // baseline. This avoids stacking additional carry-capacity modifiers.
    this.RemoveOwnedModifier();
    let baseline = stats.GetStatValue(entity, gamedataStatType.CarryCapacity);
    let delta = wanted - baseline;
    if delta > -0.01 && delta < 0.01 {
      return;
    }

    let modifier = RPGManager.CreateStatModifier(
      gamedataStatType.CarryCapacity,
      gameStatModifierType.Additive,
      delta
    ) as gameConstantStatModifierData;
    if !IsDefined(modifier) {
      return;
    }
    stats.AddModifier(entity, modifier);
    this.m_modifier = modifier;
    this.m_ownerEntity = currentEntity;
    this.m_hasModifier = true;
  }
}

// Queue the next stat refresh through the game's DelaySystem.
public class WeightCustomizerTick extends DelayCallback {
  public let owner: wref<WeightCustomizerSystem>;

  public func Call() -> Void {
    if IsDefined(this.owner) {
      this.owner.Tick();
    }
  }
}
