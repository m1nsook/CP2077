module MisoRevealMapPins

// Runtime map-pin *proxies* only. We do not unlock fast-travel destinations,
// complete quests, advance journals, or modify original map-pin state.
// Only pins already registered by the game can be displayed.
public class RMPFilters {
  public let variants: array<gamedataWorldMapFilter>;
  public let categoryOf: array<Int32>;

  public static func Build() -> ref<RMPFilters> {
    let f = new RMPFilters();
    let rec = TweakDBInterface.GetWorldMapFiltersListRecord(t"WorldMap.WorldMapCustomFiltersList");
    if !IsDefined(rec) { return f; }
    let size = EnumInt(gamedataMappinVariant.Count) + 1;
    ArrayResize(f.categoryOf, size);
    let z = 0;
    while z < size { f.categoryOf[z] = -1; z += 1; }
    let i = 0;
    while i < rec.GetListCount() {
      let entry = rec.GetListItem(i);
      if IsDefined(entry) {
        let ft = entry.FilterType();
        let index = ArraySize(f.variants);
        ArrayPush(f.variants, IsDefined(ft) ? ft.Type() : gamedataWorldMapFilter.CustomFilter);
        let j = 0;
        while j < entry.GetMappinsCount() {
          let mv = entry.GetMappinsItem(j);
          if IsDefined(mv) {
            let v = EnumInt(mv.Type());
            if v >= 0 && v < size && f.categoryOf[v] < 0 { f.categoryOf[v] = index; }
          }
          j += 1;
        }
      }
      i += 1;
    }
    return f;
  }

  public func TypeOf(variant: gamedataMappinVariant) -> gamedataWorldMapFilter {
    let v = EnumInt(variant);
    if v >= 0 && v < ArraySize(this.categoryOf) {
      let idx = this.categoryOf[v];
      if idx >= 0 && idx < ArraySize(this.variants) { return this.variants[idx]; }
    }
    return gamedataWorldMapFilter.NoFilter;
  }
}

public abstract class RMPState {
  public static func Enabled(gi: GameInstance, t: gamedataWorldMapFilter) -> Bool {
    let q = GameInstance.GetQuestsSystem(gi);
    if !IsDefined(q) { return true; }
    if q.GetFactStr("miso_rmp_all_disabled") == 1 { return false; }
    if Equals(t, gamedataWorldMapFilter.Quest) { return q.GetFactStr("miso_rmp_jobs_disabled") != 1; }
    if Equals(t, gamedataWorldMapFilter.Gigs) { return q.GetFactStr("miso_rmp_gigs_disabled") != 1; }
    if Equals(t, gamedataWorldMapFilter.PsychoFilter) { return q.GetFactStr("miso_rmp_psycho_disabled") != 1; }
    if Equals(t, gamedataWorldMapFilter.Story) { return q.GetFactStr("miso_rmp_ncpd_disabled") != 1; }
    if Equals(t, gamedataWorldMapFilter.FastTravel) { return q.GetFactStr("miso_rmp_travel_disabled") != 1; }
    if Equals(t, gamedataWorldMapFilter.RipperdocFilter) { return q.GetFactStr("miso_rmp_ripperdoc_disabled") != 1; }
    if Equals(t, gamedataWorldMapFilter.VendorFilter) { return q.GetFactStr("miso_rmp_vendor_disabled") != 1; }
    if Equals(t, gamedataWorldMapFilter.AutofixerFilter) { return q.GetFactStr("miso_rmp_autofixer_disabled") != 1; }
    if Equals(t, gamedataWorldMapFilter.DropPoint) { return q.GetFactStr("miso_rmp_drop_disabled") != 1; }
    if Equals(t, gamedataWorldMapFilter.ApartmentToPurchaseFilter) { return q.GetFactStr("miso_rmp_rent_disabled") != 1; }
    if Equals(t, gamedataWorldMapFilter.TarotFilter) { return q.GetFactStr("miso_rmp_tarot_disabled") != 1; }
    if Equals(t, gamedataWorldMapFilter.CommonFilter) { return q.GetFactStr("miso_rmp_property_disabled") != 1; }
    // Unknown variants are not cloned by default: avoid inventing markers.
    return false;
  }
}

public class RMPSystem extends ScriptableSystem {
  private let filters: ref<RMPFilters>;
  private let sourceIds: array<Uint64>;
  private let proxyIds: array<NewMappinID>;

  private func OnAttach() { this.Schedule(); }

  private func OnDetach() { this.ClearAll(); }

  private func Schedule() {
    let cb = new RMPPollTick();
    cb.owner = this;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 2.0, false);
  }

  private func ClearAll() {
    let ms = GameInstance.GetMappinSystem(this.GetGameInstance());
    if IsDefined(ms) {
      for id in this.proxyIds { ms.UnregisterMappin(id); }
    }
    ArrayClear(this.proxyIds);
    ArrayClear(this.sourceIds);
  }

  private func ContainsSource(id: Uint64) -> Bool {
    return ArrayContains(this.sourceIds, id);
  }

  private func ContainsProxy(id: Uint64) -> Bool {
    for p in this.proxyIds {
      if p.value == id { return true; }
    }
    return false;
  }

  public func Tick() {
    let gi = this.GetGameInstance();
    if !IsDefined(GetPlayer(gi)) { this.ClearAll(); this.Schedule(); return; }
    let ms = GameInstance.GetMappinSystem(gi);
    if !IsDefined(ms) { this.Schedule(); return; }
    if !IsDefined(this.filters) { this.filters = RMPFilters.Build(); }
    let arr: array<ref<IMappin>>;
    ms.GetMappins(gamemappinsMappinTargetType.Map, arr);

    // Remove proxies if source has become visible, disappeared, or is toggled off.
    let i = ArraySize(this.sourceIds) - 1;
    while i >= 0 {
      let needed = false;
      for source in arr {
        if IsDefined(source) && source.GetNewMappinID().value == this.sourceIds[i] {
          let t = this.filters.TypeOf(source.GetVariant());
          needed = !source.IsVisible() && RMPState.Enabled(gi, t);
          break;
        }
      }
      if !needed {
        ms.UnregisterMappin(this.proxyIds[i]);
        ArrayErase(this.proxyIds, i);
        ArrayErase(this.sourceIds, i);
      }
      i -= 1;
    }

    // Register a marker at the original's location without forcing discovery.
    for source in arr {
      if IsDefined(source) && !source.IsVisible() {
        let id = source.GetNewMappinID().value;
        if !this.ContainsSource(id) && !this.ContainsProxy(id) && RMPState.Enabled(gi, this.filters.TypeOf(source.GetVariant())) {
          let data: MappinData;
          data.mappinType = t"Mappins.DefaultStaticMappin";
          data.variant = source.GetVariant();
          data.visibleThroughWalls = false;
          data.active = true;
          let proxy = ms.RegisterMappin(data, source.GetWorldPosition());
          if proxy.value != 0ul {
            ArrayPush(this.sourceIds, id);
            ArrayPush(this.proxyIds, proxy);
          }
        }
      }
    }
    this.Schedule();
  }
}

public class RMPPollTick extends DelayCallback {
  public let owner: wref<RMPSystem>;
  public func Call() { if IsDefined(this.owner) { this.owner.Tick(); } }
}

// Optional Mod Settings interface. CET JSON database and the Mod Settings panel
// share an internal fact bridge; no hard dependency on the Mod Settings module.
public class RMPSettings extends ScriptableSystem {
  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "General")
  @runtimeProperty("ModSettings.displayName", "Reveal available map markers")
  public let enabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "Activities")
  @runtimeProperty("ModSettings.displayName", "Jobs")
  public let jobs: Bool = true;

  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "Activities")
  @runtimeProperty("ModSettings.displayName", "Gigs")
  public let gigs: Bool = true;

  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "Activities")
  @runtimeProperty("ModSettings.displayName", "NCPD scanner")
  public let ncpd: Bool = true;

  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "Activities")
  @runtimeProperty("ModSettings.displayName", "Cyberpsycho")
  public let psycho: Bool = true;

  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "Travel")
  @runtimeProperty("ModSettings.displayName", "Fast travel / NCART")
  public let travel: Bool = true;

  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "Services")
  @runtimeProperty("ModSettings.displayName", "Ripperdocs")
  public let ripperdoc: Bool = true;

  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "Services")
  @runtimeProperty("ModSettings.displayName", "Vendors")
  public let vendor: Bool = true;

  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "Services")
  @runtimeProperty("ModSettings.displayName", "Autofixer")
  public let autofixer: Bool = true;

  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "Services")
  @runtimeProperty("ModSettings.displayName", "Drop points")
  public let drop: Bool = true;

  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "Misc")
  @runtimeProperty("ModSettings.displayName", "Apartments for rent")
  public let rent: Bool = true;

  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "Misc")
  @runtimeProperty("ModSettings.displayName", "Tarot")
  public let tarot: Bool = true;

  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "Misc")
  @runtimeProperty("ModSettings.displayName", "Owned apartments / vehicles")
  public let ownedProperty: Bool = true;

  private let ready: Bool;
  private let previous: array<Bool>;
  private let keys: array<String>;

  private func Prepare() {
    ArrayPush(this.keys, "miso_rmp_all_disabled");
    ArrayPush(this.keys, "miso_rmp_jobs_disabled");
    ArrayPush(this.keys, "miso_rmp_gigs_disabled");
    ArrayPush(this.keys, "miso_rmp_ncpd_disabled");
    ArrayPush(this.keys, "miso_rmp_psycho_disabled");
    ArrayPush(this.keys, "miso_rmp_travel_disabled");
    ArrayPush(this.keys, "miso_rmp_ripperdoc_disabled");
    ArrayPush(this.keys, "miso_rmp_vendor_disabled");
    ArrayPush(this.keys, "miso_rmp_autofixer_disabled");
    ArrayPush(this.keys, "miso_rmp_drop_disabled");
    ArrayPush(this.keys, "miso_rmp_rent_disabled");
    ArrayPush(this.keys, "miso_rmp_tarot_disabled");
    ArrayPush(this.keys, "miso_rmp_property_disabled");
  }

  private func ValueAt(i: Int32) -> Bool {
    if i == 0 { return this.enabled; }
    if i == 1 { return this.jobs; }
    if i == 2 { return this.gigs; }
    if i == 3 { return this.ncpd; }
    if i == 4 { return this.psycho; }
    if i == 5 { return this.travel; }
    if i == 6 { return this.ripperdoc; }
    if i == 7 { return this.vendor; }
    if i == 8 { return this.autofixer; }
    if i == 9 { return this.drop; }
    if i == 10 { return this.rent; }
    if i == 11 { return this.tarot; }
    return this.ownedProperty;
  }

  private func SetAt(i: Int32, enabled: Bool) {
    if i == 0 { this.enabled = enabled; }
    else if i == 1 { this.jobs = enabled; }
    else if i == 2 { this.gigs = enabled; }
    else if i == 3 { this.ncpd = enabled; }
    else if i == 4 { this.psycho = enabled; }
    else if i == 5 { this.travel = enabled; }
    else if i == 6 { this.ripperdoc = enabled; }
    else if i == 7 { this.vendor = enabled; }
    else if i == 8 { this.autofixer = enabled; }
    else if i == 9 { this.drop = enabled; }
    else if i == 10 { this.rent = enabled; }
    else if i == 11 { this.tarot = enabled; }
    else { this.ownedProperty = enabled; }
  }

  private func OnAttach() {
    this.Prepare();
    let qs = GameInstance.GetQuestsSystem(this.GetGameInstance());
    let i = 0;
    while i < ArraySize(this.keys) {
      let value = !IsDefined(qs) || qs.GetFactStr(this.keys[i]) != 1;
      this.SetAt(i, value);
      ArrayPush(this.previous, value);
      i += 1;
    }
    RMPRegister(this);
    this.Schedule();
  }

  private func OnDetach() { RMPUnregister(this); }

  private func Schedule() {
    let cb = new RMPSettingsTick();
    cb.owner = this;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 0.75, false);
  }

  public func Tick() {
    let qs = GameInstance.GetQuestsSystem(this.GetGameInstance());
    if IsDefined(qs) {
      let i = 0;
      while i < ArraySize(this.keys) {
        let curr = this.ValueAt(i);
        if NotEquals(curr, this.previous[i]) {
          qs.SetFactStr(this.keys[i], curr ? 0 : 1);
          this.previous[i] = curr;
        } else {
          let bridge = qs.GetFactStr(this.keys[i]) != 1;
          if NotEquals(curr, bridge) {
            this.SetAt(i, bridge);
            this.previous[i] = bridge;
          }
        }
        i += 1;
      }
    }
    this.Schedule();
  }
}

public class RMPSettingsTick extends DelayCallback {
  public let owner: wref<RMPSettings>;
  public func Call() { if IsDefined(this.owner) { this.owner.Tick(); } }
}

@if(ModuleExists("ModSettingsModule"))
public func RMPRegister(s: ref<RMPSettings>) { ModSettings.RegisterListenerToClass(s); }
@if(!ModuleExists("ModSettingsModule"))
public func RMPRegister(s: ref<RMPSettings>) {}
@if(ModuleExists("ModSettingsModule"))
public func RMPUnregister(s: ref<RMPSettings>) { ModSettings.UnregisterListenerToClass(s); }
@if(!ModuleExists("ModSettingsModule"))
public func RMPUnregister(s: ref<RMPSettings>) {}
