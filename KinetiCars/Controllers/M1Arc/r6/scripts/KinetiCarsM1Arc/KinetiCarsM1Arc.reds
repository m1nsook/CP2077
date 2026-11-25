// Optional controller for the development probe. The Core never imports M1 Arc.
public class KinetiCarsM1ArcSystem extends ScriptableSystem {
  private let running: Bool;
  private let epoch: Int32;

  private func OnAttach() -> Void {
    this.epoch += 1;
    this.running = true;
    this.Schedule();
  }

  private func OnDetach() -> Void {
    this.running = false;
    this.epoch += 1;
  }

  private func Schedule() -> Void {
    if !this.running { return; }
    let cb = new KinetiCarsM1ArcTick();
    cb.owner = this;
    cb.epoch = this.epoch;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 0.50, false);
  }

  public func Tick(epoch: Int32) -> Void {
    if !this.running || this.epoch != epoch { return; }
    let id = "m1nsook.kineticars";
    let value = M1Arc_GetJson(id, "probe");
    let player = GetPlayer(this.GetGameInstance()) as PlayerPuppet;
    // If the controller manifest cannot be read, leave the core alone.
    if IsDefined(player) && (Equals(value, "true") || Equals(value, "false")) {
      player.kineticarsProbeDisabled = !M1Arc_GetBool(id, "probe");
      if M1Arc_ConsumeAction(id, "reset_probe") {
        player.KinetiCarsResetProbe();
        M1Arc_Log(id, "INFO", "Collision probe counters reset.");
      }
      if M1Arc_ConsumeAction(id, "report_probe") {
        M1Arc_Log(id, "INFO", "Read-only probe; outgoing events=" + ToString(player.kineticarsOutgoingBumps)
          + "; incoming events=" + ToString(player.kineticarsIncomingBumps)
          + "; last delta-V=" + ToString(player.kineticarsLastDeltaV)
          + "; normal XYZ=" + ToString(player.kineticarsLastNormal.X) + ", "
          + ToString(player.kineticarsLastNormal.Y) + ", " + ToString(player.kineticarsLastNormal.Z));
      }
    }
    this.Schedule();
  }
}

public class KinetiCarsM1ArcTick extends DelayCallback {
  public let owner: wref<KinetiCarsM1ArcSystem>;
  public let epoch: Int32;

  public func Call() -> Void {
    if IsDefined(this.owner) { this.owner.Tick(this.epoch); }
  }
}
