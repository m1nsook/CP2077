module MisoActionBlockedFix

// Standalone experimental recovery. Never replace vanilla eligibility
// checks, reset quest facts, or repeatedly remove newly applied effects.
public class ActionBlockedFixSystem extends ScriptableSystem {
  private let running: Bool;
  private let generation: Int32;
  private let samples: Int32;
  private let attempted: Bool;
  private let cleanSamples: Int32;
  private let owner: wref<PlayerPuppet>;
  private let lockedOut: Bool;
  private let probationTicks: Int32;
  private let lastEpoch: Int32;

  private func OnAttach() -> Void {
    this.generation += 1;
    this.running = true;
    this.Reset();
    this.Schedule();
  }

  private func OnDetach() -> Void {
    this.running = false;
    this.generation += 1;
    this.Reset();
  }

  private func Reset() -> Void {
    if IsDefined(this.owner) {
      this.owner.misoProbationActive = false;
      this.owner.misoReappliedDuringProbation = false;
    }
    this.owner = null;
    this.lockedOut = false;
    this.probationTicks = 0;
    this.lastEpoch = 0;
    this.samples = 0;
    this.attempted = false;
    this.cleanSamples = 0;
  }

  private func Schedule() -> Void {
    if !this.running { return; }
    let cb = new ActionBlockedFixTick();
    cb.owner = this;
    cb.generation = this.generation;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 1.00, false);
  }

  public func Tick(generation: Int32) -> Void {
    if !this.running || generation != this.generation { return; }
    this.Sample();
    this.Schedule();
  }

  // Refuse repair if any other GameplayRestriction-tagged record is
  // active. A pair alone does not establish ownership, but this avoids
  // clearing part of a broader scripted restriction.
  private func HasOnlyKnownPair(player: ref<PlayerPuppet>) -> Bool {
    let effects: array<ref<StatusEffect>>;
    let system = GameInstance.GetStatusEffectSystem(player.GetGame());
    if !IsDefined(system) { return false; }
    system.GetAppliedEffectsWithTag(player.GetEntityID(), n"GameplayRestriction", effects);
    let foundCombat: Bool = false;
    let foundZoom: Bool = false;
    for effect in effects {
      if !IsDefined(effect) { return false; }
      let record = effect.GetRecord();
      if !IsDefined(record) { return false; }
      if Equals(record.GetID(), t"GameplayRestriction.NoCombat") {
        foundCombat = true;
      } else {
        if Equals(record.GetID(), t"GameplayRestriction.NoZooming") {
          foundZoom = true;
        } else {
          return false;
        }
      }
    }
    return foundCombat && foundZoom;
  }

  // Independent status tags known to be used by scene/radio restrictions.
  // Do not clear these, even when both candidate records are present.
  private func HasContextLock(player: ref<PlayerPuppet>) -> Bool {
    return StatusEffectSystem.ObjectHasStatusEffectWithTag(player, n"PhoneCall")
      || StatusEffectSystem.ObjectHasStatusEffectWithTag(player, n"BlockFastTravel")
      || StatusEffectSystem.ObjectHasStatusEffectWithTag(player, n"FastForward")
      || StatusEffectSystem.ObjectHasStatusEffectWithTag(player, n"InDaClub")
      || StatusEffectSystem.ObjectHasStatusEffectWithTag(player, n"VehicleScene");
  }

  private func Sample() -> Void {
    let gi = this.GetGameInstance();
    let player = GetPlayer(gi) as PlayerPuppet;
    if !IsDefined(player) {
      this.Reset();
      return;
    }

    if this.owner != player {
      this.Reset();
      this.owner = player;
      this.lastEpoch = player.misoRestrictionEpoch;
      player.misoProbationActive = false;
      player.misoReappliedDuringProbation = false;
      player.misoRecoveryState = 0;
      player.misoFixCount = 0;
    }

    if this.lockedOut { return; }

    // Both the event ledger and the live status-effect system are checked.
    // A missed callback or failed removal must not permit another attempt.
    if this.probationTicks > 0 {
      if player.misoReappliedDuringProbation
        || player.misoRestrictionEpoch != this.lastEpoch
        || StatusEffectSystem.ObjectHasStatusEffect(player, t"GameplayRestriction.NoCombat")
        || StatusEffectSystem.ObjectHasStatusEffect(player, t"GameplayRestriction.NoZooming") {
        this.lockedOut = true;
        this.probationTicks = 0;
        player.misoProbationActive = false;
        player.misoRecoveryState = 4;
        return;
      }
      this.probationTicks -= 1;
      if this.probationTicks == 0 {
        player.misoProbationActive = false;
        player.misoRecoveryState = 3;
      }
      return;
    }

    if player.misoRestrictionEpoch != this.lastEpoch {
      this.lastEpoch = player.misoRestrictionEpoch;
      this.samples = 0;
    }

    let boards = GameInstance.GetBlackboardSystem(gi);
    if !IsDefined(boards) {
      this.samples = 0;
      this.cleanSamples = 0;
      return;
    }
    let psm = boards.GetLocalInstanced(player.GetEntityID(), GetAllBlackboardDefs().PlayerStateMachine);
    let vehicle = boards.Get(GetAllBlackboardDefs().UI_ActiveVehicleData);
    if !IsDefined(psm) || !IsDefined(vehicle) {
      this.samples = 0;
      this.cleanSamples = 0;
      return;
    }

    let defs = GetAllBlackboardDefs().PlayerStateMachine;
    let tier = psm.GetInt(defs.SceneTier);
    let highLevel = psm.GetInt(defs.HighLevel);
    let zone = psm.GetInt(defs.Zones);
    let upperBody = psm.GetInt(defs.UpperBody);
    let vitals = psm.GetInt(defs.Vitals);
    let mounted = vehicle.GetBool(GetAllBlackboardDefs().UI_ActiveVehicleData.IsPlayerMounted);
    let combat = player.IsInCombat();
    let blocked = StatusEffectSystem.ObjectHasStatusEffect(player, t"GameplayRestriction.NoCombat");
    let noZoom = StatusEffectSystem.ObjectHasStatusEffect(player, t"GameplayRestriction.NoZooming");

    // Match the reproduced bug, including normal exploration outside
    // combat. Zone 1 was captured on the affected save; zone 2 is safe.
    let normal = tier == 1 && highLevel == 1 && zone == 1
      && vitals == 0 && !mounted && !combat;
    let quests = GameInstance.GetQuestsSystem(gi);
    if !normal || !IsDefined(quests) || quests.GetFact(n"ForceEmptyHands") != 0
      || this.HasContextLock(player) {
      this.samples = 0;
      this.cleanSamples = 0;
      return;
    }

    // Re-arm after ten clean normal gameplay samples, never while
    // either restriction remains or is immediately reapplied.
    if !blocked && !noZoom && upperBody != 5 {
      if this.cleanSamples < 10 { this.cleanSamples += 1; }
      if this.cleanSamples >= 10 { this.attempted = false; }
    } else {
      this.cleanSamples = 0;
    }

    if this.attempted || !blocked || !noZoom || upperBody != 5
      || !this.HasOnlyKnownPair(player) {
      this.samples = 0;
      return;
    }

    // Two matching samples one gameplay second apart. The first sample
    // starts the wait; a changed application epoch resets it.
    // This is still a heuristic, not proof of quest ownership.
    this.samples += 1;
    if this.samples < 2 { return; }

    this.attempted = true;
    this.samples = 0;
    // Start probation BEFORE removal so synchronous reapplication is seen.
    this.lastEpoch = player.misoRestrictionEpoch;
    player.misoReappliedDuringProbation = false;
    player.misoProbationActive = true;
    player.misoRecoveryState = 2;
    player.misoFixCount += 1;
    this.probationTicks = 10;
    // Use only the two removals confirmed to recover the affected save.
    // Let the vanilla listeners restore upper-body state and Radioport.
    StatusEffectHelper.RemoveStatusEffect(player, t"GameplayRestriction.NoCombat");
    StatusEffectHelper.RemoveStatusEffect(player, t"GameplayRestriction.NoZooming");
  }
}

public class ActionBlockedFixTick extends DelayCallback {
  public let owner: wref<ActionBlockedFixSystem>;
  public let generation: Int32;

  public func Call() -> Void {
    if IsDefined(this.owner) { this.owner.Tick(this.generation); }
  }
}

// Volatile diagnostics, readable through CET during development only.
// No external logging headers or runtime framework required.
@addField(PlayerPuppet)
public let misoRestrictionEpoch: Int32;

@addField(PlayerPuppet)
public let misoProbationActive: Bool;

@addField(PlayerPuppet)
public let misoReappliedDuringProbation: Bool;

// 0 = no fix yet, 2 = probation, 3 = passed, 4 = session lockout.
@addField(PlayerPuppet)
public let misoRecoveryState: Int32;

@addField(PlayerPuppet)
public let misoFixCount: Int32;

@wrapMethod(PlayerPuppet)
protected cb func OnStatusEffectApplied(evt: ref<ApplyStatusEffectEvent>) -> Bool {
  // Observe only; never suppress or modify the original event.
  if IsDefined(evt) && IsDefined(evt.staticData) {
    let id = evt.staticData.GetID();
    if Equals(id, t"GameplayRestriction.NoCombat") || Equals(id, t"GameplayRestriction.NoZooming") {
      this.misoRestrictionEpoch += 1;
      if this.misoProbationActive {
        this.misoReappliedDuringProbation = true;
      }
    }
  }
  return wrappedMethod(evt);
}
