module MisoFreeChrome

// TweakDB lives for the process, not the save. Backup flats are namespaced,
// process-local and never saved into a player profile or game save.
// Nonzero backup = this mod owns a change. Zero = no outstanding change.
public class FreeChromePatch {
  private let found: Int32;
  private let changed: Int32;
  private let restored: Int32;
  private let skipped: Int32;
  private let failed: Int32;

  private func IsNumeric(value: Variant) -> Bool {
    if !IsDefined(value) { return false; }
    let typ = VariantTypeName(value);
    return Equals(typ, n"Float") || Equals(typ, n"Int32");
  }

  private func IsZero(value: Variant) -> Bool {
    if !this.IsNumeric(value) { return false; }
    if Equals(VariantTypeName(value), n"Float") { return FromVariant<Float>(value) == 0.0; }
    return FromVariant<Int32>(value) == 0;
  }

  private func Same(a: Variant, b: Variant) -> Bool {
    if !this.IsNumeric(a) || !this.IsNumeric(b) { return false; }
    if NotEquals(VariantTypeName(a), VariantTypeName(b)) { return false; }
    if Equals(VariantTypeName(a), n"Float") { return FromVariant<Float>(a) == FromVariant<Float>(b); }
    return FromVariant<Int32>(a) == FromVariant<Int32>(b);
  }

  private func Zero(value: Variant) -> Variant {
    if Equals(VariantTypeName(value), n"Float") { return ToVariant(0.0); }
    return ToVariant(0);
  }

  private func Write(id: TweakDBID, value: Variant) -> Bool {
    if !TweakDBManager.SetFlat(id, value) { return false; }
    return this.Same(TweakDBInterface.GetFlat(id), value);
  }

  private func Visit(recordName: String, enabled: Bool) -> Void {
    let record = TDBID.Create(recordName);
    let id = TDBID.Create(recordName + ".value");
    let backupId = TDBID.Create("M1nsook.FreeChrome.Backup." + recordName + ".value");
    let value = TweakDBInterface.GetFlat(id);
    if !this.IsNumeric(value) { return; }
    this.found += 1;
    let original = TweakDBInterface.GetFlat(backupId);
    let owned = this.IsNumeric(original) && !this.IsZero(original);

    if enabled {
      if this.IsZero(value) { return; }
      // Record the previous value BEFORE changing the real flat. Do not
      // touch the real flat if the backup cannot be written and verified.
      if !this.Write(backupId, value) {
        this.failed += 1;
        return;
      }
      if !this.Write(id, this.Zero(value)) {
        this.failed += 1;
        return;
      }
      if !TweakDBManager.UpdateRecord(record) {
        this.failed += 1;
        return;
      }
      this.changed += 1;
      return;
    }

    if !owned { return; }
    if this.IsZero(value) || this.Same(value, original) {
      // Keep the backup if a record refresh fails so restoration can retry.
      if !this.Same(value, original) && !this.Write(id, original) {
        this.failed += 1;
        return;
      }
      if !TweakDBManager.UpdateRecord(record) {
        this.failed += 1;
        return;
      }
      if !this.Write(backupId, this.Zero(original)) {
        this.failed += 1;
        return;
      }
      this.restored += 1;
    } else {
      // A different mod has changed the field. Preserve its new value.
      if !this.Write(backupId, this.Zero(original)) {
        this.failed += 1;
        return;
      }
      this.skipped += 1;
    }
  }

  private func Probe(stage: String) -> Void {
    let value = TweakDBInterface.GetFlat(t"Variants.Humanity16Cost_inline2.value");
    if !IsDefined(value) {
      M1Arc_Log("m1nsook.free-chrome", "WARNING", stage + ": known Humanity16 flat missing");
      return;
    }
    let typ = VariantTypeName(value);
    let message = stage + ": Humanity16Cost_inline2.value type=" + NameToString(typ);
    if Equals(typ, n"Float") {
      message += ", value=" + ToString(FromVariant<Float>(value));
    } else {
      if Equals(typ, n"Int32") { message += ", value=" + ToString(FromVariant<Int32>(value)); }
    }
    M1Arc_Log("m1nsook.free-chrome", "INFO", message);
  }

  public func Run(enabled: Bool, stage: String, report: Bool) -> Void {
    this.found = 0;
    this.changed = 0;
    this.restored = 0;
    this.skipped = 0;
    this.failed = 0;
    if report { this.Probe(stage + " before"); }
    let cost = 1;
    while cost <= 200 {
      let family = "Variants.Humanity" + ToString(cost) + "Cost";
      let i = 1;
      while i <= 2 {
        this.Visit(family + "_inline" + ToString(i), enabled);
        i += 1;
      }
      i = 1;
      while i <= 5 {
        this.Visit(family + "TinkererVariant_inline" + ToString(i), enabled);
        i += 1;
      }
      cost += 1;
    }
    if report || this.changed > 0 || this.restored > 0 || this.failed > 0 || this.found == 0 {
      M1Arc_Log("m1nsook.free-chrome", this.failed > 0 || this.found == 0 ? "WARNING" : "INFO", stage + ": enabled=" + ToString(enabled) + ", found=" + ToString(this.found) + ", zeroed=" + ToString(this.changed) + ", restored=" + ToString(this.restored) + ", external preserved=" + ToString(this.skipped) + ", failures=" + ToString(this.failed));
    }
    if report { this.Probe(stage + " after"); }
  }
}

// TweakXL executes ScriptableTweaks while loading the TweakDB, before
// the player's first save is initialized. This also works without CET.
public class FreeChromeStartup extends ScriptableTweak {
  protected cb func OnApply() -> Void {
    let patch = new FreeChromePatch();
    patch.Run(M1Arc_GetBool("m1nsook.free-chrome", "enabled"), "TweakXL startup", true);
  }
}

public class FreeChromeSettings extends ScriptableSystem {
  private let running: Bool;
  private let ready: Bool;
  private let enabled: Bool;
  private let ticks: Int32;

  private func OnAttach() -> Void {
    this.running = true;
    this.ready = false;
    this.Tick();
  }

  private func OnDetach() -> Void {
    this.running = false;
    // Never restore on save reload. The next session must see the same
    // patched database before its cyberware is initialized.
    M1Arc_Log("m1nsook.free-chrome", "DEBUG", "Session detached; Humanity flats preserved across save loads.");
  }

  public func Tick() -> Void {
    if !this.running { return; }
    let enabled = M1Arc_GetBool("m1nsook.free-chrome", "enabled");
    if !this.ready || NotEquals(enabled, this.enabled) {
      let stage = this.ready ? "Setting changed" : "Session attach";
      this.ready = true;
      this.enabled = enabled;
      this.ticks = 0;
      let patch = new FreeChromePatch();
      patch.Run(enabled, stage, true);
    } else {
      this.ticks += 1;
      if this.ticks >= 60 {
        this.ticks = 0;
        let patch = new FreeChromePatch();
        patch.Run(enabled, "Periodic refresh", false);
      }
    }
    let cb = new FreeChromeSettingsTick();
    cb.owner = this;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 0.50, false);
  }
}

public class FreeChromeSettingsTick extends DelayCallback {
  public let owner: wref<FreeChromeSettings>;
  public func Call() -> Void {
    if IsDefined(this.owner) { this.owner.Tick(); }
  }
}
