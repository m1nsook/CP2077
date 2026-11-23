// NETWORM-only skin for the vanilla HUDProgressBarController.
//
// Based on the player's original Cyberpunk 2077 2.5 WolvenKit export:
// base/gameplay/gui/widgets/hud_progress_bar/hud_progress_bar.inkwidget
// base/gameplay/gui/widgets/hud_progress_bar/progress_bar.inkstyle
//
// Native Neutral style maps Bar.Red to MainColors.Blue and
// Bar.ActiveRed to MainColors.ActiveBlue. The vanilla neutral icon
// still includes triangle/background/ghost layers. Only alter them
// while this particular progress bar belongs to NETWORM.
//
// This is runtime-scoped: DO NOT replace the base .inkwidget archive,
// which would restyle progress widgets for missions and other mods.
// The private NetDeck.archive must provide the networm_icons.inkatlas.
module NetDeck

@addField(HUDProgressBarController)
private let netDeckHudSkinApplied: Bool;

@addMethod(HUDProgressBarController)
private func NetDeckHudSkinShowNeutralPart(name: CName, visible: Bool) -> Void {
  let canvas: ref<inkCompoundWidget> =
    inkWidgetRef.Get(this.m_neutralIcon) as inkCompoundWidget;
  if !IsDefined(canvas) { return; }
  let widget: ref<inkWidget> = canvas.GetWidgetByPathName(name);
  if IsDefined(widget) {
    widget.SetVisible(visible);
  }
}

@addMethod(HUDProgressBarController)
private func NetDeckHudSkinApply(enable: Bool) -> Void {
  let image: ref<inkWidget> = inkImageRef.Get(this.m_neutralInIcon);
  if !IsDefined(image) { return; }

  if enable {
    if this.netDeckHudSkinApplied { return; }

    // Use the existing player's 32x32/64x64 multi-resolution quickhack icon.
    inkImageRef.SetAtlasResource(
      this.m_neutralInIcon,
      r"m1nsook\\netdeck\\ui\\networm_icons.inkatlas"
    );
    if !inkImageRef.SetTexturePart(this.m_neutralInIcon, n"networmhack") {
      // Gracefully fall back if the private icon .archive is not installed.
      inkImageRef.SetAtlasResource(
        this.m_neutralInIcon,
        r"base\\gameplay\\gui\\quests\\assets\\personal_link.inkatlas"
      );
      inkImageRef.SetTexturePart(this.m_neutralInIcon, n"neutral_!");
      return;
    }
    this.netDeckHudSkinApplied = true;

    // Suppress only the triangle artwork, retaining the neutral icon slot.
    this.NetDeckHudSkinShowNeutralPart(n"Neutral_Triangle_Icon", false);
    this.NetDeckHudSkinShowNeutralPart(n"Neutral_Background_Icon", false);
    this.NetDeckHudSkinShowNeutralPart(n"Neurtral_Small_Icon", false);
    this.NetDeckHudSkinShowNeutralPart(n"Neutral_Triangle_Icon_GHOST", false);

    image.BindProperty(n"tintColor", n"MainColors.ActiveBlue");
    let fill: ref<inkWidget> = inkWidgetRef.Get(this.m_bar);
    if IsDefined(fill) {
      fill.BindProperty(n"tintColor", n"MainColors.ActiveBlue");
    }
  } else {
    if !this.netDeckHudSkinApplied { return; }
    this.netDeckHudSkinApplied = false;

    // Restore all native neutral parts for other game/quest progress bars.
    inkImageRef.SetAtlasResource(
      this.m_neutralInIcon,
      r"base\\gameplay\\gui\\quests\\assets\\personal_link.inkatlas"
    );
    inkImageRef.SetTexturePart(this.m_neutralInIcon, n"neutral_!");
    this.NetDeckHudSkinShowNeutralPart(n"Neutral_Triangle_Icon", true);
    this.NetDeckHudSkinShowNeutralPart(n"Neutral_Background_Icon", true);
    this.NetDeckHudSkinShowNeutralPart(n"Neurtral_Small_Icon", true);
    this.NetDeckHudSkinShowNeutralPart(n"Neutral_Triangle_Icon_GHOST", true);

    image.BindProperty(n"tintColor", n"Bar.Red");
    let fill: ref<inkWidget> = inkWidgetRef.Get(this.m_bar);
    if IsDefined(fill) {
      fill.BindProperty(n"tintColor", n"Bar.ProgressLine");
    }
  }
}

// Use the same known, game-compatible HUD progress activation hook as
// Dark Future's REDscript integration. Unlike UpdateTimerHeader, this native
// method accepts Bool and has no String vs script_ref<String> discrepancy.
// Never replace the base game's .inkwidget or alter non-NETWORM HUD visuals.
@wrapMethod(HUDProgressBarController)
public final func UpdateProgressBarActive(active: Bool) -> Void {
  wrappedMethod(active);

  if !active {
    // NETWORM is done, or another quest has taken the progress widget.
    this.NetDeckHudSkinApply(false);
    return;
  }

  // This HUD controller already owns its progress blackboard and def.
  // inkHUDGameController does NOT provide GetGameInstance().
  if !IsDefined(this.m_progressBarBB) { return; }
  let header: String =
    this.m_progressBarBB.GetString(this.m_progressBarDef.Header);
  this.NetDeckHudSkinApply(StrBeginsWith(header, "NETWORM"));
}
