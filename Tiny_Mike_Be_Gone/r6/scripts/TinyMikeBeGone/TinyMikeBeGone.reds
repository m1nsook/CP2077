// Tiny Mike Be Gone 1.0.0
// REDscript only. Never alter the character record or quest facts.
public class TinyMikeBeGoneCheckEvent extends Event {
  public let attemptsLeft: Int32;
}

@wrapMethod(NPCPuppet)
protected cb func OnGameAttached() -> Bool {
  let result: Bool = wrappedMethod();
  if Equals(this.GetRecordID(), t"Character.sts_wat_kab_01_tiny_mike") {
    let evt: ref<TinyMikeBeGoneCheckEvent> = new TinyMikeBeGoneCheckEvent();
    evt.attemptsLeft = 4;
    GameInstance.GetDelaySystem(this.GetGame()).DelayEvent(this, evt, 0.20);
  }
  return result;
}

@addMethod(NPCPuppet)
protected cb func OnTinyMikeBeGoneCheckEvent(evt: ref<TinyMikeBeGoneCheckEvent>) -> Bool {
  if !Equals(this.GetRecordID(), t"Character.sts_wat_kab_01_tiny_mike") {
    return true;
  }

  let pos: Vector4 = this.GetWorldPosition();
  let dx: Float = pos.X - (-1436.340454);
  let dy: Float = pos.Y - 1014.116760;
  let dz: Float = pos.Z - 16.549995;
  // Match only the confirmed Afterlife seat within a 4 m sphere.
  if dx * dx + dy * dy + dz * dz <= 16.0 {
    // Both native signatures are present in the supplied 2.50 game bundle.
    EntityGameInterface.Destroy(this.GetEntity());
    return true;
  }

  // Spawn transforms may settle after attachment; retry only this record.
  if evt.attemptsLeft > 1 {
    let nextCheck: ref<TinyMikeBeGoneCheckEvent> = new TinyMikeBeGoneCheckEvent();
    nextCheck.attemptsLeft = evt.attemptsLeft - 1;
    GameInstance.GetDelaySystem(this.GetGame()).DelayEvent(this, nextCheck, 0.50);
  }
  return true;
}
