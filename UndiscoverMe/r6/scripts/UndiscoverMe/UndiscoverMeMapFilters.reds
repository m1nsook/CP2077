module MisoRevealMapPins

// UndiscoverMe: independent FIFTH top-level map quick-filter tab.
// No rows added to WorldMapCustomFiltersList: vanilla Custom remains vanilla.
// Uses the four existing game's native filter-list widgets in a separate view.
// Does not change catalog markers, completion detection or quest progress.

@addField(WorldMapMenuGameController)
private let m_udmQuickActive: Bool;

@addField(WorldMapMenuGameController)
private let m_udmSavedCustom: array<gamedataWorldMapFilter>;

@addMethod(WorldMapMenuGameController)
private final func UDMSlot(typ: gamedataWorldMapFilter) -> Int32 {
  if Equals(typ, gamedataWorldMapFilter.Story) { return 1; }
  if Equals(typ, gamedataWorldMapFilter.PsychoFilter) { return 2; }
  if Equals(typ, gamedataWorldMapFilter.TarotFilter) { return 3; }
  if Equals(typ, gamedataWorldMapFilter.FastTravel) { return 4; }
  return 0;
}

@addMethod(WorldMapMenuGameController)
private final func UDMFactEnabled(slot: Int32) -> Bool {
  if !IsDefined(this.m_player) { return false; }
  let qs = GameInstance.GetQuestsSystem(this.m_player.GetGame());
  if !IsDefined(qs) || qs.GetFactStr("miso_rmp_all_disabled") == 1 {
    return false;
  }
  let catalog = qs.GetFactStr("miso_rmp_catalog_enabled") == 1;
  if slot == 1 {
    return catalog && qs.GetFactStr("miso_rmp_ncpd_disabled") != 1;
  }
  if slot == 2 {
    return catalog && qs.GetFactStr("miso_rmp_psycho_disabled") != 1;
  }
  if slot == 3 {
    return catalog && qs.GetFactStr("miso_rmp_tarot_disabled") != 1;
  }
  if slot == 4 {
    return qs.GetFactStr("miso_rmp_travel_disabled") != 1;
  }
  return false;
}

// Update ONLY our category facts. CET's existing fact bridge synchronizes
// these with the same db.json used by the working PR #18 runtime.
@addMethod(WorldMapMenuGameController)
private final func UDMToggle(slot: Int32) -> Void {
  if !IsDefined(this.m_player) { return; }
  let qs = GameInstance.GetQuestsSystem(this.m_player.GetGame());
  if !IsDefined(qs) { return; }
  let enable = !this.UDMFactEnabled(slot);
  if enable {
    qs.SetFactStr("miso_rmp_all_disabled", 0);
    if slot != 4 {
      qs.SetFactStr("miso_rmp_catalog_enabled", 1);
    }
  }
  if slot == 1 { qs.SetFactStr("miso_rmp_ncpd_disabled", enable ? 0 : 1); }
  if slot == 2 { qs.SetFactStr("miso_rmp_psycho_disabled", enable ? 0 : 1); }
  if slot == 3 { qs.SetFactStr("miso_rmp_tarot_disabled", enable ? 0 : 1); }
  if slot == 4 { qs.SetFactStr("miso_rmp_travel_disabled", enable ? 0 : 1); }
  this.UDMApplyNativeFilters();
  this.UDMRefreshVisibleRows();
}

// The vanilla engine has exactly four built-in Custom-filter variants
// corresponding to our supported categories. We use those existing row
// widgets only in the new UndiscoverMe tab; we never add a Custom entry.
@addMethod(WorldMapMenuGameController)
private final func UDMRefreshVisibleRows() -> Void {
  if !this.m_udmQuickActive { return; }
  let i = 0;
  while i < ArraySize(this.m_customFiltersList) {
    let row = this.m_customFiltersList[i];
    if IsDefined(row) {
      let slot = this.UDMSlot(row.GetFilterType());
      row.GetRootWidget().SetVisible(slot > 0);
      if slot > 0 { row.EnableFilter(this.UDMFactEnabled(slot)); }
    }
    i += 1;
  }
}

@addMethod(WorldMapMenuGameController)
private final func UDMApplyNativeFilters() -> Void {
  if !this.m_udmQuickActive { return; }
  let i = 0;
  while i < ArraySize(this.m_customFiltersList) {
    let row = this.m_customFiltersList[i];
    if IsDefined(row) {
      let typ = row.GetFilterType();
      let slot = this.UDMSlot(typ);
      let enable = slot > 0 && this.UDMFactEnabled(slot);
      if enable && !this.HasCustomFilter(typ) { this.SetCustomFilter(typ); }
      if !enable && this.HasCustomFilter(typ) { this.ClearCustomFilter(typ); }
    }
    i += 1;
  }
}

// Original Custom choices are saved on entering this tab, and fully restored
// when leaving or closing the map. No persistent vanilla Custom changes.
@addMethod(WorldMapMenuGameController)
private final func UDMEnter() -> Void {
  if this.m_udmQuickActive { return; }
  ArrayClear(this.m_udmSavedCustom);
  let i = 0;
  while i < ArraySize(this.m_customFiltersList) {
    let row = this.m_customFiltersList[i];
    if IsDefined(row) && this.HasCustomFilter(row.GetFilterType()) {
      ArrayPush(this.m_udmSavedCustom, row.GetFilterType());
    }
    i += 1;
  }
  this.m_udmQuickActive = true;
  this.m_currentCustomFilterIndex = -1;
  this.UDMApplyNativeFilters();
  this.UDMRefreshVisibleRows();
}

@addMethod(WorldMapMenuGameController)
private final func UDMLeave() -> Void {
  if !this.m_udmQuickActive { return; }
  let i = 0;
  while i < ArraySize(this.m_customFiltersList) {
    let row = this.m_customFiltersList[i];
    if IsDefined(row) {
      let typ = row.GetFilterType();
      let originallyEnabled = ArrayContains(this.m_udmSavedCustom, typ);
      if originallyEnabled && !this.HasCustomFilter(typ) {
        this.SetCustomFilter(typ);
      }
      if !originallyEnabled && this.HasCustomFilter(typ) {
        this.ClearCustomFilter(typ);
      }
      row.GetRootWidget().SetVisible(true);
      row.EnableFilter(this.HasCustomFilter(typ));
    }
    i += 1;
  }
  this.m_udmQuickActive = false;
  ArrayClear(this.m_udmSavedCustom);
}

// The native quick-filter index has one entry per TweakDB quick-filter
// RECORD, but filter bits are enums. Custom and UndiscoverMe have the same
// engine type, so identify our tab using its TweakDB record ID.
@addMethod(WorldMapMenuGameController)
private final func UDMIsQuickTab() -> Bool {
  let list = TweakDBInterface.GetWorldMapFiltersListRecord(
    t"WorldMap.WorldMapQuickFiltersList"
  );
  if !IsDefined(list) { return false; }
  let idx = this.m_currentQuickFilterIndex;
  if idx < 0 || idx >= list.GetListCount() { return false; }
  let rec = list.GetListItem(idx);
  return IsDefined(rec) && Equals(rec.GetID(), t"WorldMap.UndiscoverMeQuickFilter");
}

@addMethod(WorldMapMenuGameController)
private final func UDMReconcileQuickTab() -> Void {
  if this.UDMIsQuickTab() {
    this.UDMEnter();
    inkTextRef.SetText(this.m_filterText, "UNDISCOVERME");
    inkWidgetRef.SetVisible(this.m_customFilters, true);
    this.UDMRefreshVisibleRows();
  } else {
    this.UDMLeave();
  }
}

// Preserve the base game's cycle and arrow sounds. Only detect our new tab
// after the engine advances the quick-filter index. This also catches Custom
// -> UndiscoverMe, even though both use the CustomFilter engine enum.
@wrapMethod(WorldMapMenuGameController)
private final func CycleWorldMapFilter(cycleNext: Bool) -> Void {
  wrappedMethod(cycleNext);
  this.UDMReconcileQuickTab();
}

@wrapMethod(WorldMapMenuGameController)
protected cb func OnQuickFilterChanged(filterGroup: wref<MappinUIFilterGroup_Record>) -> Bool {
  wrappedMethod(filterGroup);
  this.UDMReconcileQuickTab();
  return true;
}

// The shipped world_map.inkwidget contains four quick-filter indicator
// references under filterSelector/dots. Build a fifth native ink rectangle
// beside those original widgets at runtime rather than replacing the entire
// world-map asset (which would conflict with other UI archives).
// This access to the native parentWidget field does not require Codeware.
@addMethod(inkWidget)
public func UDMIndicatorParent() -> wref<inkWidget> {
  return this.parentWidget;
}

@addField(WorldMapMenuGameController)
private let m_udmFifthIndicator: wref<inkWidget>;

@addMethod(WorldMapMenuGameController)
private final func UDMCreateFifthIndicator() -> Void {
  if IsDefined(this.m_udmFifthIndicator) { return; }
  if ArraySize(this.m_quickFilterIndicators) != 4 { return; }

  let fourth = inkWidgetRef.Get(this.m_quickFilterIndicators[3]);
  if !IsDefined(fourth) { return; }
  let parent = fourth.UDMIndicatorParent() as inkCompoundWidget;
  if !IsDefined(parent) { return; }

  let extra = new inkRectangle();
  extra.SetName(n"udm_quick_filter_fifth");
  extra.SetSize(fourth.GetSize());
  extra.SetMargin(fourth.GetMargin());
  extra.SetTintColor(fourth.GetTintColor());
  extra.SetOpacity(0.35);
  extra.SetState(n"Default");
  extra.Reparent(parent);
  this.m_udmFifthIndicator = extra;
}

// The engine still expects its four authored references. Keep its index
// bounded, then independently reflect the fifth logical tab on our extra
// indicator. Never access m_quickFilterIndicators[4].
@wrapMethod(WorldMapMenuGameController)
private final func SetQuickFilterIndicator(index: Int32) -> Void {
  let count = ArraySize(this.m_quickFilterIndicators);
  if count <= 0 { return; }

  this.UDMCreateFifthIndicator();
  if index < 0 {
    wrappedMethod(0);
  } else {
    wrappedMethod(index >= count ? count - 1 : index);
  }

  if !IsDefined(this.m_udmFifthIndicator) { return; }

  if index == 4 && count == 4 {
    let fourth = inkWidgetRef.Get(this.m_quickFilterIndicators[3]);
    if IsDefined(fourth) {
      // Copy the native selected tint before resetting the fourth indicator.
      this.m_udmFifthIndicator.SetTintColor(fourth.GetTintColor());
      fourth.SetState(n"Default");
    }
    this.m_udmFifthIndicator.SetState(n"Selected");
    this.m_udmFifthIndicator.SetOpacity(1.0);
  } else {
    this.m_udmFifthIndicator.SetState(n"Default");
    this.m_udmFifthIndicator.SetOpacity(0.35);
  }
}

// Called after each of the game's original Custom filter widgets has spawned.
// This keeps the UndiscoverMe tab responsive even on late asynchronous loads.
@wrapMethod(WorldMapMenuGameController)
protected cb func OnFilterListItemSpawned(
  filterWidget: ref<inkWidget>, userData: ref<IScriptable>
) -> Bool {
  wrappedMethod(filterWidget, userData);
  if this.m_udmQuickActive {
    this.UDMApplyNativeFilters();
    this.UDMRefreshVisibleRows();
  }
  return true;
}

// Mouse clicks in Custom remain 100% vanilla. Only clicks while the new
// quick filter tab is selected change UndiscoverMe's own category facts.
@wrapMethod(WorldMapMenuGameController)
protected cb func OnFilterSwitched(evt: ref<inkPointerEvent>) -> Bool {
  if this.m_udmQuickActive && this.canChangeCustomFilter {
    let row = evt.GetTarget().GetController() as WorldMapFiltersListItem;
    if IsDefined(row) {
      let slot = this.UDMSlot(row.GetFilterType());
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

// The original keyboard/controller navigation cycles all 13 Custom rows.
// In UndiscoverMe's quick tab, skip hidden rows and select only our four.
@wrapMethod(WorldMapMenuGameController)
private final func NavigateCustomFilters(option: ECustomFilterDPadNavigationOption) -> Void {
  if !this.m_udmQuickActive {
    wrappedMethod(option);
    return;
  }
  let count = ArraySize(this.m_customFiltersList);
  if count == 0 { return; }
  let index = this.m_currentCustomFilterIndex;
  if Equals(option, ECustomFilterDPadNavigationOption.Toggle) {
    if index >= 0 && index < count {
      let row = this.m_customFiltersList[index];
      if IsDefined(row) {
        let slot = this.UDMSlot(row.GetFilterType());
        if slot > 0 { this.UDMToggle(slot); }
      }
    }
    return;
  }
  if index >= 0 && index < count {
    let previous = this.m_customFiltersList[index];
    if IsDefined(previous) {
      previous.SetFilterState(previous.IsFilterEnabled() ? n"Selected" : n"Default");
    }
  }
  let next = index;
  let step = Equals(option, ECustomFilterDPadNavigationOption.SelectPrev) ? -1 : 1;
  if next < 0 { next = step < 0 ? 0 : count - 1; }
  let checked = 0;
  while checked < count {
    next = (next + step + count) % count;
    let row = this.m_customFiltersList[next];
    if IsDefined(row) && this.UDMSlot(row.GetFilterType()) > 0 {
      this.m_currentCustomFilterIndex = next;
      row.SetFilterState(n"Hover");
      return;
    }
    checked += 1;
  }
}

// Restore original Custom filter bitmask BEFORE native OnUninitialize saves
// the map settings. This must work even if the user closes the map while
// UndiscoverMe is currently selected.
@wrapMethod(WorldMapMenuGameController)
protected cb func OnUninitialize() -> Bool {
  this.UDMLeave();
  wrappedMethod();
  return true;
}
