module MisoLegendarySkills

// Legendary Skills: v0.1.0 diagnostic prototype.
// READ-ONLY: no XP/level changes, no effectors, no quest facts, no save writes.
// Hook verified against Cyberpunk 2077 2.50 scripts and the supplied ink widget.

@addField(StatsProgressController)
private let m_lsPreviewActive: Bool;

@addField(StatsProgressController)
private let m_lsRegisteredHold: Bool;

@addField(StatsProgressController)
private let m_lsOriginalLabelTint: HDRColor;

@addField(StatsProgressController)
private let m_lsOriginalBarTint: HDRColor;

@wrapMethod(StatsProgressController)
public final func SetProfiencyLevel(proficiency: ref<ProficiencyDisplayData>) -> Void {
  // Recycled skill-row widgets must not retain cyan styling for other skills.
  if this.m_lsPreviewActive {
    let oldLabel: wref<inkWidget> = inkWidgetRef.Get(this.m_labelRef);
    let oldBar: wref<inkWidget> = inkWidgetRef.Get(this.m_progressBarFill);
    if IsDefined(oldLabel) {
      oldLabel.SetTintColor(this.m_lsOriginalLabelTint);
    };
    if IsDefined(oldBar) {
      oldBar.SetTintColor(this.m_lsOriginalBarTint);
    };
    this.m_lsPreviewActive = false;
  };

  wrappedMethod(proficiency);

  if !IsDefined(proficiency)
    || NotEquals(proficiency.m_proficiency, gamedataProficiencyType.IntelligenceSkill)
    || proficiency.m_maxLevel != 60 || proficiency.m_level < 60 {
    return;
  };

  let label: wref<inkWidget> = inkWidgetRef.Get(this.m_labelRef);
  let bar: wref<inkWidget> = inkWidgetRef.Get(this.m_progressBarFill);
  if !IsDefined(label) || !IsDefined(bar) {
    return;
  };

  this.m_lsOriginalLabelTint = label.GetTintColor();
  this.m_lsOriginalBarTint = bar.GetTintColor();
  this.m_lsPreviewActive = true;

  inkTextRef.SetText(this.m_labelRef, "NETRUNNER // PRESTIGE");
  label.SetTintColor(new HDRColor(0.14, 0.86, 0.93, 1.00));
  bar.SetTintColor(new HDRColor(0.14, 0.86, 0.93, 1.00));

  if !this.m_lsRegisteredHold {
    // This is an interaction probe only; it never changes character progress.
    let root: wref<inkWidget> = this.GetRootWidget();
    if IsDefined(root) {
      root.SetInteractive(true);
      root.RegisterToCallback(n"OnHold", this, n"OnLegendaryNetrunnerPreviewHold");
      this.m_lsRegisteredHold = true;
    };
  };
}

@addMethod(StatsProgressController)
protected cb func OnLegendaryNetrunnerPreviewHold(evt: ref<inkPointerEvent>) -> Bool {
  if !this.m_lsPreviewActive || !IsDefined(evt) {
    return false;
  };
  if evt.GetHoldProgress() >= 1.00 {
    // Visual proof of hold completion. Do not prestige or write save state.
    inkTextRef.SetText(this.m_labelRef, "NETRUNNER // HOLD OK");
  };
  return false;
}
