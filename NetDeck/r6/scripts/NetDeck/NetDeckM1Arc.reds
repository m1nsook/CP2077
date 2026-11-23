module NetDeck

// M1 Arc owns the UI, button queue and persisted JSON values.
// This script only applies settings to NETWORM and handles one-time item grants.
// No CET or separate DLL; never hook NPC/vehicle streaming or game input.
public class NetDeckM1ArcSystem extends ScriptableSystem {
  private let m_running: Bool;
  private let m_configured: Bool;
  private let m_owner: EntityID;
  private let m_lastRevision: Int32;

  private func OnAttach() -> Void {
    this.m_running = true;
    this.ScheduleNext();
  }

  private func OnDetach() -> Void {
    this.m_running = false;
  }

  private func ScheduleNext() -> Void {
    if !this.m_running { return; }
    let cb = new NetDeckM1ArcTick();
    cb.owner = this;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 0.50, false);
  }

  public func Tick() -> Void {
    if !this.m_running { return; }
    // Consume inventory action once; never replay on later save loads.
    let giveDeck = M1Arc_ConsumeAction("m1nsook.infiltration", "give_deck");
    let player = GetPlayer(this.GetGameInstance()) as PlayerPuppet;
    if IsDefined(player) {
      if giveDeck {
        let ts = GameInstance.GetTransactionSystem(this.GetGameInstance());
        if IsDefined(ts) {
          ts.GiveItemByTDBID(player, t"Items.NESTCyberdeck", 1);
        }
      }
      // Apply only after M1 Arc loads valid values. A new save/player
      // receives its own settings, even when no slider value changed.
      let revision = M1Arc_Revision();
      if !this.m_configured || NotEquals(this.m_owner, player.GetEntityID()) ||
        this.m_lastRevision != revision {
        let seconds = M1Arc_GetFloat("m1nsook.infiltration", "uploadSeconds");
        let scan = M1Arc_GetFloat("m1nsook.infiltration", "scanRadius");
        let hop = M1Arc_GetFloat("m1nsook.infiltration", "hopRadius");
        let interval = M1Arc_GetFloat("m1nsook.infiltration", "spreadInterval");
        let jumps = M1Arc_GetInt("m1nsook.infiltration", "maxJumps");
        let targets = M1Arc_GetInt("m1nsook.infiltration", "maxTargets");
        let fanout = M1Arc_GetInt("m1nsook.infiltration", "fanout");
        // Missing/invalid manifests return zeros. Do not overwrite the
        // tested REDscript defaults with zeroed config.
        if seconds >= 0.7 && scan >= 30.0 && hop >= 15.0 &&
          interval >= 0.10 && jumps >= 4 && targets >= 8 && fanout >= 1 {
          player.NetDeckConfigure(seconds, scan, hop, interval, jumps, targets, fanout);
          this.m_owner = player.GetEntityID();
          this.m_lastRevision = revision;
          this.m_configured = true;
        }
      }
    } else {
      this.m_configured = false;
    }
    this.ScheduleNext();
  }

}

public class NetDeckM1ArcTick extends DelayCallback {
  public let owner: wref<NetDeckM1ArcSystem>;
  public func Call() -> Void {
    if IsDefined(this.owner) { this.owner.Tick(); }
  }
}
