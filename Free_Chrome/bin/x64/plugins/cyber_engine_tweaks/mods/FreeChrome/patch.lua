-- Free Chrome - runtime TweakDB patcher (CET 1.38+, CP2077 2.5).
-- This module is intentionally separate from the settings so new cyberware
-- record types/slots can be supported without rewriting the UI.
local M = {}

local ZERO_VARIANT = "Variants.Humanity0Cost"
-- All standard cyberware items are gamedataItem_Record, not Cyberware_Record.
-- Use the second type only as a fallback for custom registrations.
local RECORD_TYPES = { "gamedataItem_Record", "gamedataCyberware_Record" }

-- Standard implant slots, including Phantom Liberty and operating systems.
-- The fallback *CW suffix and Cyb_ item types cover many added implant slots.
local CYBERWARE_AREAS = {
  FrontalCortexCW = true, ArmsCW = true, MusculoskeletalSystemCW = true,
  NervousSystemCW = true, IntegumentarySystemCW = true,
  SystemReplacementCW = true, EyesCW = true, HandsCW = true,
  CardiovascularSystemCW = true, LegsCW = true,
}

local function idName(id)
  if type(id) == "string" then return id end
  -- CET exposes record:GetID().value as a readable TweakDB record name.
  local ok, value = pcall(function() return id.value end)
  if ok and type(value) == "string" and value ~= "" then return value end
  local debugOK, debugName = pcall(function() return TDBID.ToStringDEBUG(id) end)
  if debugOK and type(debugName) == "string" and debugName ~= "" then return debugName end
  return nil
end

local function copyArray(value)
  if type(value) ~= "table" or #value == 0 then return nil end
  local out = {}
  for i = 1, #value do out[i] = value[i] end
  return out
end

local function sameArray(a, b)
  if type(a) ~= "table" or type(b) ~= "table" or #a ~= #b then return false end
  for i = 1, #a do
    local av, bv = idName(a[i]), idName(b[i])
    if av and bv then
      if av ~= bv then return false end
    elseif a[i] ~= b[i] and tostring(a[i]) ~= tostring(b[i]) then
      return false
    end
  end
  return true
end

-- Preserve every unrelated item variant, including variants supplied by other
-- mods. Only replace standard Humanity-cost entries.
local function zeroCostVariants(current)
  local output = copyArray(current)
  if not output then return nil end
  local found = false
  for i = 1, #output do
    local name = idName(output[i])
    if name and name:match("^Variants%.Humanity%d+Cost") then
      output[i] = TweakDBID.new(ZERO_VARIANT)
      found = true
    end
  end
  return found and output or nil
end

local function flatPath(id, property)
  local name = idName(id)
  if not name or name == "" then return nil end
  return name .. "." .. property
end

local function safeGetFlat(id, property)
  local path = flatPath(id, property)
  if not path then return nil end
  local ok, value = pcall(function() return TweakDB:GetFlat(path) end)
  return ok and value or nil
end

local function setVerified(id, variants)
  local path = flatPath(id, "variants")
  if not path then return false end
  local ok, result = pcall(function() return TweakDB:SetFlat(path, variants) end)
  return ok and result ~= false and sameArray(safeGetFlat(id, "variants"), variants)
end

local function recordName(value)
  if not value then return "" end
  return idName(value) or tostring(value)
end

local function isCyberware(record, id)
  -- The common item type is exactly gamedataItemType.Cyberware.
  -- The earlier scanner mistakenly looked for a Cyb_ prefix instead.
  local okItem, itemType = pcall(function() return record:ItemType():Type() end)
  if okItem and itemType then
    if itemType == gamedataItemType.Cyberware then return true end
    local kind = recordName(itemType)
    if kind == "Cyberware" or kind:match("^Cyb_") then return true end
  end

  local ok, area = pcall(function() return record:EquipArea() end)
  if ok and area then
    local okType, kind = pcall(function() return recordName(area:Type()) end)
    if okType and (CYBERWARE_AREAS[kind] or kind:match("CW$")) then return true end
  end

  local tags = safeGetFlat(id, "tags")
  if type(tags) == "table" then
    for _, tag in ipairs(tags) do
      if recordName(tag) == "Cyberware" then return true end
    end
  end
  return false
end

-- Stored originals are SESSION-ONLY, never saved to db.json. Disabling Free
-- Chrome restores the value that was present when this mod first patched it,
-- including values already added by TweakXL/other mods.
function M.new(logger)
  local self = { enabled = false, originals = {}, scanCount = 0, logger = logger or function() end }

  function self:scan()
    if not self.enabled then return 0, 0 end
    if not TweakDB or not TweakDB.GetRecords or not TweakDB.SetFlat then
      self.logger("TweakDB API unavailable; skipped patch")
      return 0, 0
    end
    local exists = pcall(function()
      assert(TweakDB:GetRecord(ZERO_VARIANT), "Variants.Humanity0Cost is missing")
    end)
    if not exists then
      self.logger("Missing " .. ZERO_VARIANT .. "; no records were changed")
      return 0, 0
    end

    local visited, scanned, matched, eligible, applied, failed = {}, 0, 0, 0, 0, 0
    for _, recordType in ipairs(RECORD_TYPES) do
      local ok, records = pcall(function() return TweakDB:GetRecords(recordType) end)
      if ok and type(records) == "table" then
        for _, record in ipairs(records) do
          scanned = scanned + 1
          local valid, id = pcall(function() return record:GetID() end)
          if not valid or not id then
            valid, id = pcall(function() return record:GetRecordID() end)
          end
          if valid and id then
            local key = idName(id)
            if key and not visited[key] then
              visited[key] = true
              if isCyberware(record, id) then
                matched = matched + 1
                local current = copyArray(safeGetFlat(id, "variants"))
                if current then
                  local zero = zeroCostVariants(current)
                  if zero then eligible = eligible + 1 end
                  if zero and not sameArray(current, zero) then
                    -- Capture the latest externally changed cost, preserving
                    -- every non-Humanity variant while Free Chrome is enabled.
                    if setVerified(id, zero) then
                      self.originals[key] = { id = id, variants = current }
                      applied = applied + 1
                    else
                      failed = failed + 1
                    end
                  end
                end
              end
            end
          end
        end
      end
    end
    if self.scanCount == 0 or applied > 0 or failed > 0 then
      self.logger("records scanned=" .. scanned .. ", cyberware=" .. matched
        .. ", matching Humanity variants=" .. eligible
        .. ", newly zeroed=" .. applied .. ", failed=" .. failed)
      if scanned == 0 or matched == 0 then
        self.logger("WARNING: no usable cyberware records found; check CET TweakDB API")
      elseif matched > 0 and eligible == 0 then
        self.logger("WARNING: no readable Humanity cost variants found; scanner did not modify costs")
      elseif failed > 0 then
        self.logger("WARNING: some variant writes were rejected; check CET scripting.log")
      end
    end
    self.scanCount = self.scanCount + 1
    return matched, applied
  end

  function self:restore()
    local restored, failed = 0, 0
    for key, original in pairs(self.originals) do
      if setVerified(original.id, original.variants) then
        self.originals[key] = nil
        restored = restored + 1
      else
        failed = failed + 1
      end
    end
    if restored > 0 or failed > 0 then
      self.logger("original cyberware variants restored=" .. restored .. ", failed=" .. failed)
    end
    return restored, failed
  end

  function self:setEnabled(enabled)
    self.enabled = enabled == true
    if self.enabled then return self:scan() end
    return self:restore()
  end

  return self
end

return M
