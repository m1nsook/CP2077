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
  public static func Get(key: String) -> Bool {
    return M1Arc_GetBool("m1nsook.undiscoverme", key);
  }

  public static func Enabled(gi: GameInstance, t: gamedataWorldMapFilter) -> Bool {
    if !RMPState.Get("enabled") { return false; }
    if Equals(t, gamedataWorldMapFilter.FastTravel) { return RMPState.Get("travel"); }
    if Equals(t, gamedataWorldMapFilter.Story) { return RMPState.Get("ncpd"); }
    if Equals(t, gamedataWorldMapFilter.PsychoFilter) { return RMPState.Get("psycho"); }
    if Equals(t, gamedataWorldMapFilter.TarotFilter) { return RMPState.Get("tarot"); }
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
    // The external catalog handles all classified POI types when enabled;
    // keep the registered fast-travel path alive because it is not in that
    // catalog and its originals cannot conflict with NCPD/Tarot pins.
    if RMPState.Get("catalog")
      && !Equals(group, gamedataWorldMapFilter.FastTravel) {
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

  // Preserve allowed source variants so a cyberpsycho, tarot or NCPD
  // marker isn't misrepresented as a generic quest/exclamation marker.
  public static func VariantFor(variant: gamedataMappinVariant, group: gamedataWorldMapFilter) -> gamedataMappinVariant {
    return variant;
  }
}

public class RMPSystem extends ScriptableSystem {
  private let filters: ref<RMPFilters>;
  private let sourceIds: array<Uint64>;
  private let proxyIds: array<NewMappinID>;
  private let catalogIds: array<NewMappinID>;
  private let rows: array<ref<UDMCatalogRow>>;
  private let running: Bool;
  private let catalogTicks: Int32;
  private let lastRevision: Int32;
  private let diagnosticReady: Bool;
  private let lastActive: Int32;
  private let lastFailed: Int32;

  // Public runtime API; deliberately excludes original vanilla mappins.
  public func UDMRegisterCatalogPin(id: NewMappinID) -> Void {
    if id.value == 0ul { return; }
    for existing in this.catalogIds {
      if existing.value == id.value { return; }
    }
    ArrayPush(this.catalogIds, id);
  }

  public func UDMUnregisterCatalogPin(id: NewMappinID) -> Void {
    let i = ArraySize(this.catalogIds) - 1;
    while i >= 0 {
      if this.catalogIds[i].value == id.value {
        ArrayErase(this.catalogIds, i);
        return;
      }
      i -= 1;
    }
  }

  public func UDMOwnsPin(id: NewMappinID) -> Bool {
    if id.value == 0ul { return false; }
    for proxy in this.proxyIds {
      if proxy.value == id.value { return true; }
    }
    for catalog in this.catalogIds {
      if catalog.value == id.value { return true; }
    }
    return false;
  }

  private func OnAttach() {
    this.running = true;
    this.rows = UDMCatalogRow.Build();
    this.catalogTicks = 6;
    this.Schedule();
  }

  private func OnDetach() {
    this.running = false;
    this.ClearAll();
  }

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
    this.ClearCatalog();
    this.catalogTicks = 6;
    this.diagnosticReady = false;
    for row in this.rows { row.completed = false; }
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

  // Legacy mod-owned fact mirror only for the existing Teleporter addon.
  // M1 Arc is authoritative; these are not vanilla quest progression facts.
  private func SyncLegacyFact() -> Void {
    let qs = GameInstance.GetQuestsSystem(this.GetGameInstance());
    if IsDefined(qs) {
      let value = RMPState.Get("enabled") ? 0 : 1;
      if qs.GetFactStr("miso_rmp_all_disabled") != value {
        qs.SetFactStr("miso_rmp_all_disabled", value);
      }
    }
  }

  private func ClearCatalog() -> Void {
    let ms = GameInstance.GetMappinSystem(this.GetGameInstance());
    for row in this.rows {
      if row.owned.value != 0ul {
        if IsDefined(ms) { ms.UnregisterMappin(row.owned); }
        row.owned.value = 0ul;
      }
    }
    ArrayClear(this.catalogIds);
  }

  private func RefreshCatalog(arr: array<ref<IMappin>>) -> Void {
    let gi = this.GetGameInstance();
    let ms = GameInstance.GetMappinSystem(gi);
    let journal = GameInstance.GetJournalManager(gi);
    if !IsDefined(ms) || !IsDefined(journal) { this.ClearCatalog(); return; }
    let context: JournalRequestContext;
    context.stateFilter.succeeded = true;
    context.stateFilter.failed = true;
    let finished: array<wref<JournalEntry>>;
    journal.GetQuests(context, finished);
    let ids: array<String>;
    for entry in finished {
      if IsDefined(entry) {
        let state = journal.GetEntryState(entry);
        if Equals(state, gameJournalEntryState.Succeeded) || Equals(state, gameJournalEntryState.Failed) {
          ArrayPush(ids, entry.GetId());
        }
      }
    }
    let active = 0;
    let failed = 0;
    let completed = 0;
    for row in this.rows {
      if NotEquals(row.questId, "") && ArrayContains(ids, row.questId) { row.completed = true; }
      if row.completed { completed += 1; }
      let allowed = RMPState.Get(row.key) && !row.completed && (IsEP1() || !row.ep1);
      if allowed {
        let phase: Uint16;
        let variant: Uint16;
        let stateActive: Bool;
        if ms.GetPointOfInterestMappinSavedState(row.hash, phase, variant, stateActive) {
          if Equals(phase, Cast<Uint16>(EnumInt(gamedataMappinPhase.CompletedPhase))) ||
            (NotEquals(phase, Cast<Uint16>(EnumInt(gamedataMappinPhase.UndiscoveredPhase))) && !stateActive) {
            allowed = false;
          }
        }
      }
      if allowed {
        for pin in arr {
          if IsDefined(pin) && pin.IsVisible() && !this.UDMOwnsPin(pin.GetNewMappinID()) && Equals(pin.GetVariant(), row.variant) {
            let p = pin.GetWorldPosition();
            if AbsF(p.X - row.position.X) < 3.0 && AbsF(p.Y - row.position.Y) < 3.0 {
              allowed = false;
              break;
            }
          }
        }
      }
      if !allowed {
        if row.owned.value != 0ul {
          this.UDMUnregisterCatalogPin(row.owned);
          ms.UnregisterMappin(row.owned);
          row.owned.value = 0ul;
        }
      } else {
        if row.owned.value == 0ul {
          let data: MappinData;
          data.mappinType = t"Mappins.StaticPointOfInterestMappinDefinition";
          data.variant = row.variant;
          data.active = true;
          data.visibleThroughWalls = false;
          data.debugCaption = row.caption;
          row.owned = ms.RegisterMappin(data, row.position);
          if row.owned.value != 0ul { this.UDMRegisterCatalogPin(row.owned); }
          else { failed += 1; }
        }
        if row.owned.value != 0ul { active += 1; }
      }
    }
    if !this.diagnosticReady || active != this.lastActive || failed != this.lastFailed {
      this.diagnosticReady = M1Arc_Log("m1nsook.undiscoverme", failed > 0 ? "WARNING" : "INFO", "Catalog=212, active=" + ToString(active) + ", completed journal matches=" + ToString(completed) + ", failed=" + ToString(failed));
      this.lastActive = active;
      this.lastFailed = failed;
    }
  }

  public func Tick() {
    if !this.running { return; }
    let gi = this.GetGameInstance();
    if !IsDefined(GetPlayer(gi)) { this.ClearAll(); this.Schedule(); return; }
    let ms = GameInstance.GetMappinSystem(gi);
    if !IsDefined(ms) { this.Schedule(); return; }
    if !IsDefined(this.filters) { this.filters = RMPFilters.Build(); }
    let arr: array<ref<IMappin>>;
    ms.GetMappins(gamemappinsMappinTargetType.Map, arr);

    this.SyncLegacyFact();
    let revision = M1Arc_Revision();
    if !RMPState.Get("enabled") || !RMPState.Get("catalog") {
      if ArraySize(this.catalogIds) > 0 { this.ClearCatalog(); }
      this.catalogTicks = 6;
      this.diagnosticReady = false;
    } else {
      this.catalogTicks += 1;
      if this.catalogTicks >= 6 || revision != this.lastRevision {
        this.catalogTicks = 0;
        this.RefreshCatalog(arr);
      }
    }
    this.lastRevision = revision;

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
      if RMPVanillaGuard.ShouldReveal(gi, source, this.filters) {
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

