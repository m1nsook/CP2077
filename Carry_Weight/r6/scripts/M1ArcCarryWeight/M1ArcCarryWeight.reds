module MisoCarryWeight

// Native REDscript consumer of the M1 Arc manifest settings registry.
// M1 Arc is required, and CET / Lua are not used by this mod.
//
// Only the temporary additive modifier created by this system is removed.
// Never remove all modifiers: cyberware, perks, and other mods own theirs.
public class M1ArcCarryWeightSystem extends ScriptableSystem {
  private let m_running: Bool;
  private let m_hasModifier: Bool;
  private let m_ownerEntity: EntityID;
  private let m_modifier: ref<gameConstantStatModifierData>;

  private func OnAttach() -> Void {
    this.m_running = true;
    this.ApplyCapacity();
    this.ScheduleNext();
  }

  private func OnDetach() -> Void {
    this.m_running = false;
    this.RemoveOwnedModifier();
  }

  private func ScheduleNext() -> Void {
    if !this.m_running {
      return;
    }
    let callback = new M1ArcCarryWeightTick();
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

  private func ApplyCapacity() -> Void {
    let gi = this.GetGameInstance();
    let player = GetPlayer(gi) as PlayerPuppet;
    let stats = GameInstance.GetStatsSystem(gi);
    if !IsDefined(player) || !IsDefined(stats) {
      // On save transitions the old game entity is already being destroyed.
      // Never run an old modifier against the new save's player.
      this.ForgetModifier();
      return;
    }

    let currentEntity = player.GetEntityID();
    if this.m_hasModifier && NotEquals(currentEntity, this.m_ownerEntity) {
      // A different game session/player owns a different StatsObjectID.
      this.ForgetModifier();
    }

    // Missing manifest/settings produces false/0, so the mod fails closed.
    let enabled = M1Arc_GetBool("m1nsook.weight", "enabled");
    let target = M1Arc_GetInt("m1nsook.weight", "limit");
    if !enabled || target < 50 {
      this.RemoveOwnedModifier();
      return;
    }

    let entity = Cast<StatsObjectID>(currentEntity);
    let wanted = Cast<Float>(target);
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

// Uses the ordinary REDscript DelaySystem, not a CET onUpdate handler.
public class M1ArcCarryWeightTick extends DelayCallback {
  public let owner: wref<M1ArcCarryWeightSystem>;

  public func Call() -> Void {
    if IsDefined(this.owner) {
      this.owner.Tick();
    }
  }
}
