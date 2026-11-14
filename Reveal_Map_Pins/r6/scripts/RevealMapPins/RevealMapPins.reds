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

// Only explicit marker variants are eligible. A generic quest icon, map
// filter or position is never evidence that a marker is in scope.
// Do not create service, gig, job, stage, or player-property proxies.
public abstract class RMPAllowlist {
  public static func GroupOf(v: gamedataMappinVariant) -> gamedataWorldMapFilter {
    if Equals(v, gamedataMappinVariant.FastTravelVariant) {
      return gamedataWorldMapFilter.FastTravel;
    }
    if Equals(v, gamedataMappinVariant.FixerVariant) {
      return gamedataWorldMapFilter.Gigs;
    }
    if Equals(v, gamedataMappinVariant.HuntForPsychoVariant) {
      return gamedataWorldMapFilter.PsychoFilter;
    }
    if Equals(v, gamedataMappinVariant.TarotVariant) {
      return gamedataWorldMapFilter.TarotFilter;
    }
    // Candidate NCPD variants; the offline journal catalog will separate
    // Assault in Progress, Organized Crime and Reported Crime by journal ID.
    if Equals(v, gamedataMappinVariant.MinorActivityVariant)
      || Equals(v, gamedataMappinVariant.Zzz06_NCPDGigVariant)
      || Equals(v, gamedataMappinVariant.GangWatchVariant)
      || Equals(v, gamedataMappinVariant.OutpostVariant) {
      return gamedataWorldMapFilter.Story;
    }
    return gamedataWorldMapFilter.NoFilter;
  }
}

public abstract class RMPState {
  public static func Enabled(gi: GameInstance, t: gamedataWorldMapFilter) -> Bool {
    let q = GameInstance.GetQuestsSystem(gi);
    if !IsDefined(q) || q.GetFactStr("miso_rmp_all_disabled") == 1 { return false; }
    if Equals(t, gamedataWorldMapFilter.FastTravel) { return q.GetFactStr("miso_rmp_travel_disabled") != 1; }
    if Equals(t, gamedataWorldMapFilter.Gigs) { return q.GetFactStr("miso_rmp_fixer_disabled") != 1; }
    if Equals(t, gamedataWorldMapFilter.Story) { return q.GetFactStr("miso_rmp_ncpd_disabled") != 1; }
    if Equals(t, gamedataWorldMapFilter.PsychoFilter) { return q.GetFactStr("miso_rmp_psycho_disabled") != 1; }
    if Equals(t, gamedataWorldMapFilter.TarotFilter) { return q.GetFactStr("miso_rmp_tarot_disabled") != 1; }
    return false;
  }
}

// A POI may be displayed as a proxy ONLY if the exact vanilla variant
// is allowlisted. A QuestMappin is intentionally excluded: these include
// stage and objective markers, which the user explicitly does not want.
// FastTravelMappin inherits RuntimeMappin, so test that subtype first.
public abstract class RMPVanillaGuard {
  public static func ShouldReveal(gi: GameInstance, m: ref<IMappin>, filters: ref<RMPFilters>) -> Bool {
    if !IsDefined(m) || m.IsVisible() || m.IsPlayerTracked() {
      return false;
    }
    let group = RMPAllowlist.GroupOf(m.GetVariant());
    if Equals(group, gamedataWorldMapFilter.NoFilter) || !RMPState.Enabled(gi, group) {
      return false;
    }
    if IsDefined(m as FastTravelMappin) {
      return Equals(group, gamedataWorldMapFilter.FastTravel);
    }
    if IsDefined(m as RuntimeMappin) {
      return false;
    }
    // Only registered vanilla POI objects, never arbitrary quest stage pins.
    return IsDefined(m as PointOfInterestMappin);
  }
}

public abstract class RMPProxyDefinition {
  public static func TypeFor(group: gamedataWorldMapFilter) -> TweakDBID {
    if Equals(group, gamedataWorldMapFilter.FastTravel) {
      return t"Mappins.FastTravelStaticMappin";
    }
    return t"Mappins.QuestStaticMappinDefinition";
  }

  // Preserve allowed source variants so a fixer, cyberpsycho, tarot or NCPD
  // marker isn't misrepresented as a generic quest/exclamation marker.
  public static func VariantFor(variant: gamedataMappinVariant, group: gamedataWorldMapFilter) -> gamedataMappinVariant {
    return variant;
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
    // The independently classified CET catalog owns its own marker IDs.
    // Never clone the same hidden original with the legacy scan concurrently.
    let facts = GameInstance.GetQuestsSystem(gi);
    if IsDefined(facts) && facts.GetFactStr("miso_rmp_catalog_enabled") == 1 {
      this.ClearAll();
      this.Schedule();
      return;
    }
    if !IsDefined(this.filters) { this.filters = RMPFilters.Build(); }
    let arr: array<ref<IMappin>>;
    ms.GetMappins(gamemappinsMappinTargetType.Map, arr);
    let hiddenCount = 0;
    let eligibleCount = 0;
    let failedCount = 0;

    // Remove proxies if source has become visible, disappeared, or is toggled off.
    let i = ArraySize(this.sourceIds) - 1;
    while i >= 0 {
      let needed = false;
      for source in arr {
        if IsDefined(source) && source.GetNewMappinID().value == this.sourceIds[i] {
          needed = RMPVanillaGuard.ShouldReveal(gi, source, this.filters);
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

    // Register proxies only for eligible game-managed sources.
    for source in arr {
      if IsDefined(source) && !source.IsVisible() { hiddenCount += 1; }
      if RMPVanillaGuard.ShouldReveal(gi, source, this.filters) {
        eligibleCount += 1;
        let id = source.GetNewMappinID().value;
        if id != 0ul && !this.ContainsSource(id) && !this.ContainsProxy(id) {
          let data: MappinData;
          let category = this.filters.TypeOf(source.GetVariant());
          data.mappinType = RMPProxyDefinition.TypeFor(category);
          data.variant = RMPProxyDefinition.VariantFor(source.GetVariant(), category);
          data.visibleThroughWalls = false;
          data.active = true;
          let proxy = ms.RegisterMappin(data, source.GetWorldPosition());
          if proxy.value != 0ul {
            ArrayPush(this.sourceIds, id);
            ArrayPush(this.proxyIds, proxy);
          } else {
            failedCount += 1;
          }
        }
      }
    }
    // CET diagnostic hotkey reads these separate, non-gameplay counters.
    let quests = GameInstance.GetQuestsSystem(gi);
    if IsDefined(quests) {
      quests.SetFactStr("miso_rmp_diag_seen", ArraySize(arr));
      quests.SetFactStr("miso_rmp_diag_hidden", hiddenCount);
      quests.SetFactStr("miso_rmp_diag_eligible", eligibleCount);
      quests.SetFactStr("miso_rmp_diag_active", ArraySize(this.proxyIds));
      quests.SetFactStr("miso_rmp_diag_failed", failedCount);
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
// Optional Mod Settings UI. CET db.json is the persistent source of
// truth; the fact bridge remains backward-compatible for retained keys.
public class RMPSettings extends ScriptableSystem {
  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "General")
  @runtimeProperty("ModSettings.displayName", "Reveal selected vanilla markers")
  public let enabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "Exploration")
  @runtimeProperty("ModSettings.displayName", "Fast travel locations")
  public let travel: Bool = true;

  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "Exploration")
  @runtimeProperty("ModSettings.displayName", "Fixer locations (not gigs)")
  public let fixer: Bool = true;

  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "Exploration")
  @runtimeProperty("ModSettings.displayName", "NCPD scanner hustles")
  public let ncpd: Bool = true;

  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "Exploration")
  @runtimeProperty("ModSettings.displayName", "Cyberpsycho sightings")
  public let psycho: Bool = true;

  @runtimeProperty("ModSettings.mod", "Reveal Map Pins")
  @runtimeProperty("ModSettings.category", "Exploration")
  @runtimeProperty("ModSettings.displayName", "Tarot cards")
  public let tarot: Bool = true;

  private let ready: Bool;
  private let previous: array<Bool>;
  private let keys: array<String>;

  private func Prepare() {
    ArrayPush(this.keys, "miso_rmp_all_disabled");
    ArrayPush(this.keys, "miso_rmp_travel_disabled");
    ArrayPush(this.keys, "miso_rmp_fixer_disabled");
    ArrayPush(this.keys, "miso_rmp_ncpd_disabled");
    ArrayPush(this.keys, "miso_rmp_psycho_disabled");
    ArrayPush(this.keys, "miso_rmp_tarot_disabled");
  }

  private func ValueAt(i: Int32) -> Bool {
    if i == 0 { return this.enabled; }
    if i == 1 { return this.travel; }
    if i == 2 { return this.fixer; }
    if i == 3 { return this.ncpd; }
    if i == 4 { return this.psycho; }
    return this.tarot;
  }

  private func SetAt(i: Int32, enabled: Bool) {
    if i == 0 { this.enabled = enabled; }
    else if i == 1 { this.travel = enabled; }
    else if i == 2 { this.fixer = enabled; }
    else if i == 3 { this.ncpd = enabled; }
    else if i == 4 { this.psycho = enabled; }
    else { this.tarot = enabled; }
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
