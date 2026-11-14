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
  local self = { rows = rows, owned = {}, ownedId = {}, clock = 0, errorShown = false,
    enabledLast = false, statistics = { catalog = rows and #rows or 0, added = 0, failed = 0, skipped = 0 } }
  if not rows then log("Catalog unavailable: " .. tostring(loadErr)) end

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
    self.clock = 0
    self.errorShown = false
    self.registrationErrorShown = false
    self.enabledLast = false
  end

  function self:stats()
    return string.format("catalog=%d active=%d skipped=%d failed=%d",
      self.statistics.catalog, self.statistics.added,
      self.statistics.skipped, self.statistics.failed)
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

    local active, skipped, failed = 0, 0, 0
    local completed = EnumInt(gamedataMappinPhase.CompletedPhase)
    local undiscovered = EnumInt(gamedataMappinPhase.UndiscoveredPhase)
    for _, entry in ipairs(self.rows) do
      local allowed = getSetting("enabled") and getSetting(entry.kind.key)
        and (ep1 or not entry.ep1)
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
