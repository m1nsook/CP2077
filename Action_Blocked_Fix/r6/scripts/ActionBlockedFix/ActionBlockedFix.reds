module MisoActionBlockedFix

// Standalone experimental recovery. Never replace vanilla eligibility
// checks, reset quest facts, or repeatedly remove a newly applied effect.
public class ActionBlockedFixSystem extends ScriptableSystem {
  private let running: Bool;
  private let generation: Int32;
  private let samples: Int32;
  private let attempted: Bool;
  private let cleanSamples: Int32;
  private let owner: wref<PlayerPuppet>;

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
    this.owner = null;
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

    let tier = psm.GetInt(GetAllBlackboardDefs().PlayerStateMachine.SceneTier);
    let mounted = vehicle.GetBool(GetAllBlackboardDefs().UI_ActiveVehicleData.IsPlayerMounted);
    let combat = player.IsInCombat();
    let blocked = StatusEffectSystem.ObjectHasStatusEffect(player, t"GameplayRestriction.NoCombat");

    // Re-arm only after ten consecutive clean, unrestricted noncombat
    // samples. Never fight a restriction that the game keeps applying.
    if !blocked && !combat && tier == 1 && !mounted {
      if this.cleanSamples < 10 { this.cleanSamples += 1; }
      if this.cleanSamples >= 10 { this.attempted = false; }
    } else {
      this.cleanSamples = 0;
    }

    if this.attempted || !blocked || !combat || tier != 1 || mounted {
      this.samples = 0;
      return;
    }

    // NoCombat in sustained combat is a suspected stale restriction,
    // not definitive proof of an orphaned effect. This is a narrow
    // development heuristic pending vanilla quest/zone/phone validation.
    this.samples += 1;
    if this.samples < 8 { return; }

    // Set latch before removing, and never retry this encounter.
    this.attempted = true;
    this.samples = 0;
    StatusEffectHelper.RemoveStatusEffect(player, t"GameplayRestriction.NoCombat");
  }
}

public class ActionBlockedFixTick extends DelayCallback {
  public let owner: wref<ActionBlockedFixSystem>;
  public let generation: Int32;

  public func Call() -> Void {
    if IsDefined(this.owner) { this.owner.Tick(this.generation); }
  }
}
