-- Reveal Map Pins: opt-in, game-derived POI catalog (Cyberpunk 2077 2.5).
-- Owns only the NewMappinIDs that it registers; never edits vanilla pins.
local Runtime = {}

local TYPES = {
  A = { key = "ncpd", variant = "GangWatchVariant", name = "Assault in Progress" },
  O = { key = "ncpd", variant = "OutpostVariant", name = "Organized Crime" },
  R = { key = "ncpd", variant = "HiddenStashVariant", name = "Reported Crime" },
  C = { key = "psycho", variant = "HuntForPsychoVariant", name = "Cyberpsycho Sighting" },
  T = { key = "tarot", variant = "TarotVariant", name = "Tarot Card" },
}
-- TweakDBID uses game RTTI, which is unavailable before CET onInit.
-- Construct it during an in-game registration, never at module load time.
local POI_DEFINITION = "Mappins.StaticPointOfInterestMappinDefinition"
local POLL_SECONDS = 12
local DUPLICATE_METERS = 3
local JOURNAL_QUEST_IDS = 175

local function ident(id)
  local ok, value = pcall(function() return id.value end)
  return ok and value and tostring(value) or nil
end

local function readCatalog(filename)
  local rows, seen = {}, {}
  local file, err = io.open(filename, "r")
  if not file then return nil, "cannot read catalog: " .. tostring(err) end
  for line in file:lines() do
    if line:sub(1, 1) ~= "#" and line:match("%S") then
      local hash, code, x, y, z, origin = line:match("^(%d+)\t([A-Z])\t([%-%d%.]+)\t([%-%d%.]+)\t([%-%d%.]+)\t([be])$")
      local kind = TYPES[code]
      hash, x, y, z = tonumber(hash), tonumber(x), tonumber(y), tonumber(z)
      if not kind or not hash or not x or not y or not z or not origin
        or hash < 1 or hash > 4294967295 or seen[hash]
        or x ~= x or y ~= y or z ~= z
        or math.abs(x) > 10000 or math.abs(y) > 10000 or math.abs(z) > 5000 then
        file:close()
        return nil, "invalid/duplicate catalog line: " .. line:sub(1, 140)
      end
      seen[hash] = true
      rows[#rows + 1] = { hash = hash, kind = kind, x = x, y = y, z = z, ep1 = origin == "e" }
    end
  end
  file:close()
  if #rows ~= 212 then return nil, "catalog has " .. #rows .. " rows; expected 212" end
  return rows
end

-- Map only the 175 NCPD/Cyberpsycho candidates to game quest IDs.
-- These are source-data IDs (ma_*), NOT the user's completed save data.
local function attachQuestIds(filename, rows)
  local file, err = io.open(filename, "r")
  if not file then return false, "quest ID table missing: " .. tostring(err) end
  local byHash, seenIds, count = {}, {}, 0
  for _, row in ipairs(rows) do byHash[row.hash] = row end
  for line in file:lines() do
    if line:sub(1, 1) ~= "#" and line:match("%S") then
      local hashText, questId = line:match("^(%d+)\t(ma_[%w_]+)$")
      local hash = tonumber(hashText)
      if not hash or not questId or not byHash[hash]
        or byHash[hash].kind.key == "tarot" or seenIds[questId]
        or byHash[hash].questId then
        file:close()
        return false, "invalid/duplicate quest ID: " .. line:sub(1, 100)
      end
      byHash[hash].questId = questId
      seenIds[questId] = true
      count = count + 1
    end
  end
  file:close()
  if count ~= JOURNAL_QUEST_IDS then
    return false, "expected 175 quest IDs, got " .. count
  end
  return true
end

-- CET supports JournalRequestContext.GetQuests with state filters.
-- Only positively terminal states are accepted; unavailable/missing APIs
-- cannot hide otherwise valid map pins (regression safety from PR #14).
local function readCompletedQuests()
  local ok, finished, count = pcall(function()
    local jm = Game.GetJournalManager()
    if not jm then return nil, 0 end
    local ctx = JournalRequestContext.new({
      stateFilter = JournalRequestStateFilter.new({
        succeeded = true, failed = true,
      }),
    })
    local quests = jm:GetQuests(ctx)
    if type(quests) ~= "table" then return nil, 0 end
    local ids, scanned = {}, 0
    for _, quest in ipairs(quests) do
      local qok, qid, state = pcall(function()
        return quest:GetId(), jm:GetEntryState(quest)
      end)
      if qok then
        scanned = scanned + 1
        local enumOK, numericState = pcall(EnumInt, state)
        numericState = enumOK and tonumber(numericState) or tonumber(state)
        if type(qid) == "string" and (numericState == 3 or numericState == 4) then
          ids[qid] = true
        end
      end
    end
    return ids, scanned
  end)
  if not ok then return nil, 0 end
  return finished, count or 0
end

local function existingPins(ms)
  local ok, pins = pcall(function()
    return ms:GetMappins(gamemappinsMappinTargetType.Map)
  end)
  if not ok or type(pins) ~= "table" then
    ok, pins = pcall(function() return ms:GetAllMappins() end)
  end
  if not ok or type(pins) ~= "table" then return nil end
  local result = {}
  for _, pin in ipairs(pins) do
    -- Depending on CET reflection, the return value may contain IMappins or
    -- MappinEntry structures. Resolve the latter rather than assuming one.
    local okPin, resolved = pcall(function()
      if pin.GetVariant then return pin end
      if pin.id then return ms:GetMappin(pin.id) end
      return nil
    end)
    if okPin and resolved then result[#result + 1] = resolved end
  end
  return result
end

function Runtime.new(directory, getSetting, log)
  local rows, loadErr = readCatalog(directory .. "catalog.tsv")
  local questIdsLoaded, questErr = false, nil
  if rows then questIdsLoaded, questErr = attachQuestIds(directory .. "quest_ids.tsv", rows) end
  local self = { rows = rows, owned = {}, ownedId = {}, completedKnown = {},
    clock = 0, errorShown = false, journalWarningIssued = false,
    enabledLast = false, statistics = { catalog = rows and #rows or 0,
      added = 0, failed = 0, skipped = 0, completed = 0,
      journalQuests = 0, journalMatched = 0 } }
  if not rows then log("Catalog unavailable: " .. tostring(loadErr)) end
  if rows and not questIdsLoaded then
    log("Completion ID lookup unavailable: " .. tostring(questErr) .. "; existing map pins unaffected")
  end

  function self:clear()
    -- Session-end and game shutdown may happen after MappinSystem teardown.
    local ok, ms = pcall(Game.GetMappinSystem)
    if not ok then ms = nil end
    for hash, id in pairs(self.owned) do
      if ms then pcall(function() ms:UnregisterMappin(id) end) end
      self.owned[hash] = nil
    end
    self.ownedId = {}
    self.statistics.added = 0
  end

  function self:reset()
    self:clear()
    self.completedKnown = {} -- session scoped, not a cross-save completion list
    self.clock = 0
    self.errorShown = false
    self.registrationErrorShown = false
    self.journalWarningIssued = false
    self.enabledLast = false
  end

  function self:stats()
    return string.format("catalog=%d active=%d skipped=%d completed=%d journalQuests=%d journalMatched=%d failed=%d",
      self.statistics.catalog, self.statistics.added,
      self.statistics.skipped, self.statistics.completed,
      self.statistics.journalQuests, self.statistics.journalMatched,
      self.statistics.failed)
  end

  function self:refresh()
    if not self.rows then return end
    local ms = Game.GetMappinSystem()
    if not ms then return end
    local pins = existingPins(ms)
    if not pins then
      if not self.errorShown then
        log("Catalog waiting: CET cannot enumerate map pins safely")
        self.errorShown = true
      end
      return  -- fail closed instead of cloning unknown originals
    end
    self.errorShown = false
    local ep1 = false
    local expansionOk, expansion = pcall(Game.IsEP1)
    if expansionOk and expansion == true then ep1 = true end
    local occupied = {}
    for _, pin in ipairs(pins) do
      local ok, visible, position, variant, id = pcall(function()
        return pin:IsVisible(), pin:GetWorldPosition(), pin:GetVariant(), pin:GetNewMappinID()
      end)
      if ok and visible and position then
        local key = ident(id)
        if not key or not self.ownedId[key] then
          occupied[#occupied + 1] = { x = position.x, y = position.y, variant = variant }
        end
      end
    end

    local finishedIds, scannedQuests = readCompletedQuests()
    if not finishedIds and not self.journalWarningIssued then
      log("Live quest completion unavailable; using last confirmed marker behavior")
      self.journalWarningIssued = true
    end
    local matched = 0
    if finishedIds then
      for _, row in ipairs(self.rows) do
        if row.questId and finishedIds[row.questId] then
          self.completedKnown[row.hash] = true
          matched = matched + 1
        end
      end
    end
    local active, skipped, failed, completedCount = 0, 0, 0, 0
    local completed = EnumInt(gamedataMappinPhase.CompletedPhase)
    local undiscovered = EnumInt(gamedataMappinPhase.UndiscoveredPhase)
    for _, entry in ipairs(self.rows) do
      local allowed = getSetting("enabled") and getSetting(entry.kind.key)
        and (ep1 or not entry.ep1)
      if allowed and self.completedKnown[entry.hash] then
        allowed = false
        completedCount = completedCount + 1
      end
      if allowed then
        local ok, found, phase, _, stateActive = pcall(function()
          return ms:GetPointOfInterestMappinSavedState(entry.hash)
        end)
        if not ok then
          -- Read-only saved state is mandatory: never expose completed content
          -- if current game APIs cannot verify its phase.
          allowed = false
          failed = failed + 1
        elseif found == true and (phase == completed or (phase ~= undiscovered and stateActive == false)) then
          allowed = false
        end
      end
      if allowed then
        local kind = gamedataMappinVariant[entry.kind.variant]
        if not kind then
          allowed = false
          failed = failed + 1
        else
          for _, pin in ipairs(occupied) do
            if pin.variant == kind and math.abs(pin.x - entry.x) < DUPLICATE_METERS
              and math.abs(pin.y - entry.y) < DUPLICATE_METERS then
              allowed = false
              break
            end
          end
        end
      end
      if not allowed then
        if self.owned[entry.hash] then
          local id = self.owned[entry.hash]
          pcall(function() ms:UnregisterMappin(id) end)
          local key = ident(id)
          if key then self.ownedId[key] = nil end
          self.owned[entry.hash] = nil
        end
        skipped = skipped + 1
      else
        if not self.owned[entry.hash] then
          local ok, id = pcall(function()
            local data = MappinData.new()
            data.mappinType = TweakDBID.new(POI_DEFINITION)
            data.variant = gamedataMappinVariant[entry.kind.variant]
            data.active = true
            data.visibleThroughWalls = false
            data.debugCaption = entry.kind.name
            return ms:RegisterMappin(data, Vector4.new(entry.x, entry.y, entry.z, 1))
          end)
          local key = ok and ident(id) or nil
          if key and key ~= "0" then
            self.owned[entry.hash] = id
            self.ownedId[key] = true
          else
            failed = failed + 1
            if not self.registrationErrorShown then
              log("Map marker registration failed (first error): " .. tostring(ok and key or id))
              self.registrationErrorShown = true
            end
          end
        end
        if self.owned[entry.hash] then active = active + 1 end
      end
    end
    self.statistics.added = active
    self.statistics.skipped = skipped
    self.statistics.completed = completedCount
    self.statistics.journalQuests = scannedQuests
    self.statistics.journalMatched = matched
    self.statistics.failed = failed
  end

  function self:tick(delta)
    if not self.rows then return end
    local enabled = getSetting("catalog") and getSetting("enabled")
    if not enabled or not Game.GetPlayer() then
      if self.enabledLast then self:clear() end
      self.enabledLast = false
      self.clock = 0
      return
    end
    if not self.enabledLast then
      self.enabledLast = true
      self.clock = POLL_SECONDS
    end
    self.clock = self.clock + (delta or 0)
    if self.clock >= POLL_SECONDS then
      self.clock = 0
      local ok, err = pcall(function() self:refresh() end)
      if not ok and not self.errorShown then
        log("Catalog refresh failed: " .. tostring(err))
        self.errorShown = true
      end
    end
  end
  return self
end
return Runtime
