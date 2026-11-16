-- Free Chrome - runtime TweakDB patcher (CET 1.38+, CP2077 2.5).
-- This module is intentionally separate from the settings so new cyberware
-- record types/slots can be supported without rewriting the UI.
local M = {}

local ZERO_VARIANT = "Variants.Humanity0Cost"
local RECORD_TYPES = { "gamedataItem_Record", "gamedataCyberware_Record" }

-- Standard implant slots, including Phantom Liberty and operating systems.
-- The fallback *CW suffix and Cyb_ item types cover many added implant slots.
local CYBERWARE_AREAS = {
  FrontalCortexCW = true, ArmsCW = true, MusculoskeletalSystemCW = true,
  NervousSystemCW = true, IntegumentarySystemCW = true,
  SystemReplacementCW = true, EyesCW = true, HandsCW = true,
  CardiovascularSystemCW = true, LegsCW = true,
}

local function copyArray(value)
  if type(value) ~= "table" or #value == 0 then return nil end
  local out = {}
  for i = 1, #value do out[i] = value[i] end
  return out
end

local function safeGetFlat(id, property)
  local ok, value = pcall(function() return TweakDB:GetFlat(id + "." .. property) end)
  return ok and value or nil
end

local function recordName(value)
  if not value then return "" end
  if type(value) == "string" then return value end
  local ok, v = pcall(function() return value.value end)
  return ok and type(v) == "string" and v or tostring(value)
end

local function isCyberware(record, id)
  local ok, area = pcall(function() return record:EquipArea() end)
  if ok and area then
    local okType, name = pcall(function() return recordName(area:Type()) end)
    if okType and (CYBERWARE_AREAS[name] or name:match("CW$")) then return true end
  end
  local okItem, itemType = pcall(function() return recordName(record:ItemType():Type()) end)
  if okItem and itemType:match("^Cyb_") then return true end

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
  local self = { enabled = false, originals = {}, logger = logger or function() end }

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

    local visited, matched, applied = {}, 0, 0
    for _, recordType in ipairs(RECORD_TYPES) do
      local ok, records = pcall(function() return TweakDB:GetRecords(recordType) end)
      if ok and type(records) == "table" then
        for _, record in ipairs(records) do
          local valid, id = pcall(function() return record:GetID() end)
          if valid and id then
            local key = tostring(id)
            if not visited[key] then
              visited[key] = true
              if isCyberware(record, id) then
                matched = matched + 1
                if not self.originals[key] then
                  local variants = copyArray(safeGetFlat(id, "variants"))
                  if variants then
                    local newVariants = { TweakDBID.new(ZERO_VARIANT) }
                    local changed, result = pcall(function()
                      return TweakDB:SetFlat(id + ".variants", newVariants)
                    end)
                    if changed and result ~= false then
                      self.originals[key] = { id = id, variants = variants }
                      applied = applied + 1
                    end
                  end
                end
              end
            end
          end
        end
      end
    end
    self.logger("cyberware records found=" .. matched .. ", newly zeroed=" .. applied)
    return matched, applied
  end

  function self:restore()
    local restored, failed = 0, 0
    for key, original in pairs(self.originals) do
      local ok, result = pcall(function()
        return TweakDB:SetFlat(original.id + ".variants", original.variants)
      end)
      if ok and result ~= false then
        self.originals[key] = nil
        restored = restored + 1
      else
        failed = failed + 1
      end
    end
    self.logger("original cyberware variants restored=" .. restored .. ", failed=" .. failed)
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
