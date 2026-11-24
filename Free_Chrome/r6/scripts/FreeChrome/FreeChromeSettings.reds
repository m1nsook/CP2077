module MisoFreeChrome

// Native TweakXL access; M1 Arc is the sole settings authority.
// Keep exact Float/Int32 type and restore only flats we actually zeroed.
public class FreeChromeSettings extends ScriptableSystem {
  private let running: Bool;
  private let ready: Bool;
  private let enabled: Bool;
  private let ticks: Int32;
  private let owned: array<TweakDBID>;
  private let original: array<Variant>;
  private let found: Int32;
  private let changed: Int32;
  private let failed: Int32;

  private func OnAttach() -> Void {
    this.running = true;
    this.Tick();
  }

  private func OnDetach() -> Void {
    this.running = false;
    this.Restore();
  }

  public func Tick() -> Void {
    if !this.running { return; }
    let enabled = M1Arc_GetBool("m1nsook.free-chrome", "enabled");
    if !this.ready || NotEquals(enabled, this.enabled) {
      this.enabled = enabled;
      this.ready = M1Arc_Log("m1nsook.free-chrome", "INFO", "Zero Humanity cost enabled=" + ToString(enabled) + "; reload a save after switching.");
      this.ticks = 0;
      if enabled {
        this.Scan();
      } else {
        this.Restore();
      }
    } else {
      this.ticks += 1;
      if this.ticks >= 60 {
        this.ticks = 0;
        if enabled { this.Scan(); }
        else { if ArraySize(this.owned) > 0 { this.Restore(); } }
      }
    }
    let cb = new FreeChromeSettingsTick();
    cb.owner = this;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 0.50, false);
  }

  private func Scan() -> Void {
    this.found = 0;
    this.changed = 0;
    this.failed = 0;
    let cost = 1;
    while cost <= 200 {
      let family = "Variants.Humanity" + ToString(cost) + "Cost";
      let i = 1;
      while i <= 2 {
        this.Apply(TDBID.Create(family + "_inline" + ToString(i) + ".value"));
        i += 1;
      }
      i = 1;
      while i <= 5 {
        this.Apply(TDBID.Create(family + "TinkererVariant_inline" + ToString(i) + ".value"));
        i += 1;
      }
      cost += 1;
    }
    if this.found == 0 || this.failed > 0 {
      M1Arc_Log("m1nsook.free-chrome", "WARNING", "Humanity fields found=" + ToString(this.found) + ", zeroed=" + ToString(this.changed) + ", failures=" + ToString(this.failed));
    } else {
      if this.changed > 0 {
        M1Arc_Log("m1nsook.free-chrome", "INFO", "Humanity fields found=" + ToString(this.found) + ", zeroed=" + ToString(this.changed));
      }
    }
  }

  private func IsNumeric(value: Variant) -> Bool {
    let typ = VariantTypeName(value);
    return Equals(typ, n"Float") || Equals(typ, n"Int32");
  }

  private func IsZero(value: Variant) -> Bool {
    if Equals(VariantTypeName(value), n"Float") {
      return FromVariant<Float>(value) == 0.0;
    }
    if Equals(VariantTypeName(value), n"Int32") {
      return FromVariant<Int32>(value) == 0;
    }
    return false;
  }

  private func Apply(id: TweakDBID) -> Void {
    let value = TweakDBInterface.GetFlat(id);
    if !this.IsNumeric(value) { return; }
    this.found += 1;
    if this.IsZero(value) { return; }
    let zero: Variant;
    if Equals(VariantTypeName(value), n"Float") {
      zero = ToVariant(0.0);
    } else {
      zero = ToVariant(0);
    }
    if !TweakDBManager.SetFlat(id, zero) {
      this.failed += 1;
      return;
    }
    // Keep the original even if the read-back fails; restoration checks ownership.
    let i = ArrayFindFirst(this.owned, id);
    if i < 0 {
      ArrayPush(this.owned, id);
      ArrayPush(this.original, value);
    } else {
      this.original[i] = value;
    }
    if this.IsZero(TweakDBInterface.GetFlat(id)) {
      this.changed += 1;
    } else {
      this.failed += 1;
    }
  }

  private func Restore() -> Void {
    let restored = 0;
    let skipped = 0;
    let failed = 0;
    let i = ArraySize(this.owned) - 1;
    while i >= 0 {
      let id = this.owned[i];
      let current = TweakDBInterface.GetFlat(id);
      if this.IsZero(current) && Equals(VariantTypeName(current), VariantTypeName(this.original[i])) {
        if TweakDBManager.SetFlat(id, this.original[i]) {
          restored += 1;
          ArrayErase(this.owned, i);
          ArrayErase(this.original, i);
        } else {
          failed += 1;
        }
      } else {
        // A different mod now owns the value; do not overwrite it.
        skipped += 1;
        ArrayErase(this.owned, i);
        ArrayErase(this.original, i);
      }
      i -= 1;
    }
    if restored + skipped + failed > 0 {
      M1Arc_Log("m1nsook.free-chrome", failed > 0 ? "WARNING" : "INFO", "Humanity fields restored=" + ToString(restored) + ", external changes preserved=" + ToString(skipped) + ", failures=" + ToString(failed));
    }
  }
}

public class FreeChromeSettingsTick extends DelayCallback {
  public let owner: wref<FreeChromeSettings>;
  public func Call() -> Void {
    if IsDefined(this.owner) { this.owner.Tick(); }
  }
}
