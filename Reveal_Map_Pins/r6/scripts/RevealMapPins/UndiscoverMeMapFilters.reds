module MisoRevealMapPins

// UndiscoverMe's optional native Custom-filter section.
// The TweakXL records have no mappin variants. This file hooks ONLY
// UndiscoverMe's own UI records; native/custom third-party filters pass
// through unchanged. Legacy internal IDs are intentionally retained.

// 0 = vanilla/other mod row, 1 = UndiscoverMe / All,
// 2 = NCPD, 3 = Cyberpsycho, 4 = Tarot, 5 = Fast Travel.
@addMethod(WorldMapFiltersListItem)
public final func UDMFilterSlot() -> Int32 {
  if !IsDefined(this.m_filterGroup) { return 0; }
  let id = this.m_filterGroup.GetID();
  if Equals(id, t"WorldMap.UndiscoverMeAll") { return 1; }
  if Equals(id, t"WorldMap.UndiscoverMeNCPD") { return 2; }
  if Equals(id, t"WorldMap.UndiscoverMePsycho") { return 3; }
  if Equals(id, t"WorldMap.UndiscoverMeTarot") { return 4; }
  if Equals(id, t"WorldMap.UndiscoverMeTravel") { return 5; }
  return 0;
}

// Use the game's actual FiltersListItem widget, animations, checkbox
// and selection states. Never inject a fake overlay or invent an icon.
@wrapMethod(WorldMapFiltersListItem)
public func SetFilterGroup(filterGroup: wref<MappinUIFilterGroup_Record>) -> Void {
  wrappedMethod(filterGroup);
  let slot = this.UDMFilterSlot();
  if slot == 1 {
    inkTextRef.SetText(this.m_filterName, "UNDISCOVERME / ALL");
  } else if slot == 2 {
    inkTextRef.SetText(this.m_filterName, "   NCPD SCANNER");
  } else if slot == 3 {
    inkTextRef.SetText(this.m_filterName, "   CYBERPSYCHO SIGHTINGS");
  } else if slot == 4 {
    inkTextRef.SetText(this.m_filterName, "   TAROT");
  } else if slot == 5 {
    inkTextRef.SetText(this.m_filterName, "   FAST TRAVEL");
  }
}

@addMethod(WorldMapMenuGameController)
private final func UDMIsEnabled(slot: Int32) -> Bool {
  if !IsDefined(this.m_player) { return false; }
  let qs = GameInstance.GetQuestsSystem(this.m_player.GetGame());
  if !IsDefined(qs) || qs.GetFactStr("miso_rmp_all_disabled") == 1 { return false; }
  let catalog = qs.GetFactStr("miso_rmp_catalog_enabled") == 1;
  if slot == 1 { return catalog; }
  if slot == 2 { return catalog && qs.GetFactStr("miso_rmp_ncpd_disabled") != 1; }
  if slot == 3 { return catalog && qs.GetFactStr("miso_rmp_psycho_disabled") != 1; }
  if slot == 4 { return catalog && qs.GetFactStr("miso_rmp_tarot_disabled") != 1; }
  // The existing registered fast-travel scanner is separate from the
  // classified NCPD/Cyberpsycho/Tarot catalog.
  if slot == 5 { return qs.GetFactStr("miso_rmp_travel_disabled") != 1; }
  return false;
}

@addMethod(WorldMapMenuGameController)
private final func UDMRefreshFilterRows() -> Void {
  let i = 0;
  while i < ArraySize(this.m_customFiltersList) {
    let row = this.m_customFiltersList[i];
    if IsDefined(row) {
      let slot = row.UDMFilterSlot();
      if slot > 0 { row.EnableFilter(this.UDMIsEnabled(slot)); }
    }
    i += 1;
  }
}

// Our rows are asynchronous and vanilla initializes all "CustomFilter"
// slots from the same native bit. Correct ONLY our checkbox state once each
// row has been spawned; never change another mod's filter state.
@wrapMethod(WorldMapMenuGameController)
protected cb func OnFilterListItemSpawned(filterWidget: ref<inkWidget>, userData: ref<IScriptable>) -> Bool {
  wrappedMethod(filterWidget, userData);
  if IsDefined(filterWidget) {
    let row = filterWidget.GetController() as WorldMapFiltersListItem;
    if IsDefined(row) {
      let slot = row.UDMFilterSlot();
      if slot > 0 { row.EnableFilter(this.UDMIsEnabled(slot)); }
    }
  }
  return true;
}

// Native world-map rendering still uses vanilla filter bits for its
// icon variants. On an explicit UndiscoverMe "ON" action, enable the
// corresponding vanilla filter if necessary. Never disable native filters
// and never alter/register/remove game-owned or third-party pins.
@addMethod(WorldMapMenuGameController)
private final func UDMEnableNativeFilter(typ: gamedataWorldMapFilter) -> Void {
  if this.HasCustomFilter(typ) { return; }
  this.UpdateCustomFilter(typ, true);
  let i = 0;
  while i < ArraySize(this.m_customFiltersList) {
    let row = this.m_customFiltersList[i];
    if IsDefined(row) && row.UDMFilterSlot() == 0 && Equals(row.GetFilterType(), typ) {
      row.EnableFilter(true);
    }
    i += 1;
  }
}

@addMethod(WorldMapMenuGameController)
private final func UDMSyncNativeFilters(slot: Int32) -> Void {
  if slot == 1 {
    if this.UDMIsEnabled(2) { this.UDMEnableNativeFilter(gamedataWorldMapFilter.Story); }
    if this.UDMIsEnabled(3) { this.UDMEnableNativeFilter(gamedataWorldMapFilter.PsychoFilter); }
    if this.UDMIsEnabled(4) { this.UDMEnableNativeFilter(gamedataWorldMapFilter.TarotFilter); }
    if this.UDMIsEnabled(5) { this.UDMEnableNativeFilter(gamedataWorldMapFilter.FastTravel); }
  } else if slot == 2 {
    this.UDMEnableNativeFilter(gamedataWorldMapFilter.Story);
  } else if slot == 3 {
    this.UDMEnableNativeFilter(gamedataWorldMapFilter.PsychoFilter);
  } else if slot == 4 {
    this.UDMEnableNativeFilter(gamedataWorldMapFilter.TarotFilter);
  } else if slot == 5 {
    this.UDMEnableNativeFilter(gamedataWorldMapFilter.FastTravel);
  }
}

@addMethod(WorldMapMenuGameController)
private final func UDMToggle(slot: Int32) -> Void {
  if !IsDefined(this.m_player) { return; }
  let qs = GameInstance.GetQuestsSystem(this.m_player.GetGame());
  if !IsDefined(qs) { return; }
  let turnOn = !this.UDMIsEnabled(slot);
  if slot == 1 {
    // The master row turns on the classified catalog, but switching it OFF
    // only disables our extra markers; game-owned markers stay visible.
    qs.SetFactStr("miso_rmp_all_disabled", turnOn ? 0 : 1);
    if turnOn { qs.SetFactStr("miso_rmp_catalog_enabled", 1); }
  } else {
    // Turning a category ON from the world map is an intentional request
    // to activate its parent and (for catalog POIs) the catalog itself.
    if turnOn {
      qs.SetFactStr("miso_rmp_all_disabled", 0);
      if slot != 5 { qs.SetFactStr("miso_rmp_catalog_enabled", 1); }
    }
    if slot == 2 { qs.SetFactStr("miso_rmp_ncpd_disabled", turnOn ? 0 : 1); }
    if slot == 3 { qs.SetFactStr("miso_rmp_psycho_disabled", turnOn ? 0 : 1); }
    if slot == 4 { qs.SetFactStr("miso_rmp_tarot_disabled", turnOn ? 0 : 1); }
    if slot == 5 { qs.SetFactStr("miso_rmp_travel_disabled", turnOn ? 0 : 1); }
  }
  this.UDMRefreshFilterRows();
  if turnOn { this.UDMSyncNativeFilters(slot); }
}

// Clicks on these five records are not passed to the vanilla CustomFilter
// handler: that would flip the shared native CustomFilter bit, affecting
// other mods. All unrelated rows use the original handler unchanged.
@wrapMethod(WorldMapMenuGameController)
protected cb func OnFilterSwitched(evt: ref<inkPointerEvent>) -> Bool {
  if this.canChangeCustomFilter {
    let row = evt.GetTarget().GetController() as WorldMapFiltersListItem;
    if IsDefined(row) {
      let slot = row.UDMFilterSlot();
      if slot > 0 {
        if evt.IsAction(n"click") || (evt.IsAction(n"tap") && row.IsFilterHovered()) {
          this.UDMToggle(slot);
          evt.Consume();
        }
        return true;
      }
    }
  }
  wrappedMethod(evt);
  return true;
}
