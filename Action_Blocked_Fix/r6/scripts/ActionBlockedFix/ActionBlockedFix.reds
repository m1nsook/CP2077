module MisoActionBlockedFix

// Experimental: observe state without overriding any vanilla eligibility check.
// Repair is deliberately a user-confirmed, single-record operation.
public class ActionBlockedFixSystem extends ScriptableSystem {
  private let running: Bool;
  private let generation: Int32;
  private let previous: String;
  private let samples: Int32;
  private let candidateLogged: Bool;
  private let owner: wref<PlayerPuppet>;
  private let verify: wref<PlayerPuppet>;

  private func OnAttach() -> Void {
    this.generation += 1;
    this.running = true;
    this.previous = "";
    this.samples = 0;
    this.candidateLogged = false;
    this.owner = null;
    this.verify = null;
    this.Schedule();
  }

  private func OnDetach() -> Void {
    this.running = false;
    this.generation += 1;
    this.owner = null;
    this.verify = null;
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

  private func Log(level: String, message: String) -> Void {
    M1Arc_Log("m1nsook.action-blocked-fix", level, message);
  }

  private func Consume(id: String) -> Bool {
    let clicked: Bool = false;
    let i: Int32 = 0;
    // Collapse duplicate button clicks; never schedule a batch of repairs.
    while i < 16 && M1Arc_ConsumeAction("m1nsook.action-blocked-fix", id) {
      clicked = true;
      i += 1;
    }
    return clicked;
  }

  private func ResetCandidate() -> Void {
    this.samples = 0;
    this.candidateLogged = false;
  }

  private func Sample() -> Void {
    let inspect = this.Consume("inspect");
    let repair = this.Consume("repair");
    let gi = this.GetGameInstance();
    let player = GetPlayer(gi) as PlayerPuppet;
    if !IsDefined(player) {
      this.owner = null;
      this.verify = null;
      this.previous = "";
      this.ResetCandidate();
      if inspect || repair { this.Log("WARNING", "ABF_NO_PLAYER: request discarded"); }
      return;
    }

    if this.owner != player {
      this.owner = player;
      this.verify = null;
      this.previous = "";
      this.ResetCandidate();
      // A queued repair must never follow a player/save transition.
      if repair {
        this.Log("WARNING", "ABF_PLAYER_CHANGED: repair discarded; retry in gameplay");
        repair = false;
      }
    }

    let boards = GameInstance.GetBlackboardSystem(gi);
    if !IsDefined(boards) {
      this.ResetCandidate();
      if inspect || repair { this.Log("WARNING", "ABF_NO_BLACKBOARDS: request discarded"); }
      return;
    }
    let psm = boards.GetLocalInstanced(player.GetEntityID(), GetAllBlackboardDefs().PlayerStateMachine);
    let vehicle = boards.Get(GetAllBlackboardDefs().UI_ActiveVehicleData);
    if !IsDefined(psm) || !IsDefined(vehicle) {
      this.ResetCandidate();
      if inspect || repair { this.Log("WARNING", "ABF_CONTEXT_UNKNOWN: request discarded"); }
      return;
    }

    let tier = psm.GetInt(GetAllBlackboardDefs().PlayerStateMachine.SceneTier);
    let mounted = vehicle.GetBool(GetAllBlackboardDefs().UI_ActiveVehicleData.IsPlayerMounted);
    let combat = player.IsInCombat();
    let blocked = StatusEffectSystem.ObjectHasStatusEffect(player, t"GameplayRestriction.NoCombat");
    let snapshot = "NoCombat=" + ToString(blocked) + " combat=" + ToString(combat)
      + " sceneTier=" + ToString(tier) + " mounted=" + ToString(mounted);
    if inspect || NotEquals(this.previous, snapshot) {
      if M1Arc_Log("m1nsook.action-blocked-fix", "INFO", "ABF_STATE: " + snapshot) {
        this.previous = snapshot;
      }
    }

    if IsDefined(this.verify) {
      if this.verify == player {
        if blocked {
          this.Log("WARNING", "ABF_STILL_ACTIVE: NoCombat remains or was reapplied; no retry");
        } else {
          this.Log("INFO", "ABF_EFFECT_CLEARED: test weapon/cyberware input; gameplay recovery not yet proven");
        }
      }
      this.verify = null;
    }

    // A contradiction is evidence for diagnosis, not proof of a stale lock.
    if blocked && combat && tier == 1 && !mounted {
      if this.samples < 3 { this.samples += 1; }
      if this.samples >= 3 && !this.candidateLogged {
        this.Log("WARNING", "ABF_CANDIDATE: NoCombat during full-gameplay combat for three samples; no automatic removal");
        this.candidateLogged = true;
      }
    } else {
      this.ResetCandidate();
    }

    if !repair { return; }
    if !blocked {
      this.Log("INFO", "ABF_NOT_PRESENT: NoCombat is absent; no change");
      return;
    }
    if tier != 1 || mounted || !combat || this.samples < 3 {
      this.Log("WARNING", "ABF_GUARD: requires sustained on-foot combat in scene tier 1; no change");
      return;
    }
    // The user must confirm that V is outside any intentionally restricted
    // quest, phone call, safe zone or scripted encounter. Those independent
    // guards have not been verified on 2.5, so no unattended repair exists.
    StatusEffectHelper.RemoveStatusEffect(player, t"GameplayRestriction.NoCombat");
    this.verify = player;
    this.ResetCandidate();
    this.Log("INFO", "ABF_REPAIR_REQUESTED: single NoCombat removal; checking next sample");
  }
}

public class ActionBlockedFixTick extends DelayCallback {
  public let owner: wref<ActionBlockedFixSystem>;
  public let generation: Int32;

  public func Call() -> Void {
    if IsDefined(this.owner) { this.owner.Tick(this.generation); }
  }
}
