-- Free Chrome | cyberware Humanity / Capacity cost modifiers
-- Cyberpunk 2077 2.5 + Phantom Liberty | CET
--
-- The game uses shared "Variants.Humanity*Cost*_inline*.value" numeric TweakDB
-- modifier flats to calculate Capacity costs. Modifying an individual item's
-- variants does not reliably update the underlying stat calculation.
--
-- Original implementation, informed by comparing the 2.0-era ZeroHumanityCost
-- source supplied by the user. We generate potential cost flat paths rather
-- than copy its hardcoded 131 individual write statements.
local M = {}

-- Covers the stock 0-60 cost family and additional standard-named mod costs.
-- Developers can raise this or add custom paths using addCostFlat().
local MAX_HUMANITY_COST = 200
local TINKERER_SUFFIX = "TinkererVariant"

local function getNumber(path)
  local ok, result = pcall(function() return TweakDB:GetFlat(path) end)
  if ok and type(result) == "number" then return result end
  return nil
end

local function setNumber(path, value)
  -- CET 1.38.1 / game 2.5 bug: TweakDB writes executed INSIDE pcall()
  -- fail with "expected userdata, received nil" (CET issue #1081).
  -- Call SetFlat directly. The caller verifies that the numeric flat exists
  -- before writing, and this read-back confirms that the change applied.
  -- Do not add pcall around this function OR any patch:scan/setEnabled/restore
  -- caller: that indirectly puts SetFlat inside pcall and breaks every write.
  local result = TweakDB:SetFlat(path, value)
  if result == false then return false end
  return getNumber(path) == value
end

local function iterPaths(visit)
  for cost = 1, MAX_HUMANITY_COST do
    local family = "Variants.Humanity" .. cost .. "Cost"
    for index = 1, 2 do
      visit(family .. "_inline" .. index .. ".value")
    end
    for index = 1, 5 do
      visit(family .. TINKERER_SUFFIX .. "_inline" .. index .. ".value")
    end
  end
end

function M.new(logger)
  local self = {
    enabled = false,
    originals = {},    -- session-only, never included in db.json
    additional = {},   -- optional explicit cost flats from other mods
    firstScan = true,
    logger = logger or function() end,
  }

  -- For custom mod-defined Humanity records that do not use the standard
  -- naming family. Developers may add only numeric cost-related .value flats.
  function self:addCostFlat(path)
    if type(path) ~= "string" or not path:match("%.value$") then
      return false
    end
    self.additional[path] = true
    if self.enabled then self:scan() end
    return true
  end

  function self:scan()
    if not self.enabled then return 0, 0 end
    if not TweakDB or not TweakDB.GetFlat or not TweakDB.SetFlat then
      self.logger("ERROR: CET TweakDB flat API unavailable")
      return 0, 0
    end

    local scanned, found, newlyZeroed, failed = 0, 0, 0, 0
    local function apply(path)
      scanned = scanned + 1
      local value = getNumber(path)
      if value == nil then return end
      found = found + 1
      if value == 0 then return end
      if setNumber(path, 0) then
        -- Keep original values for OFF. If a different mod edits the same
        -- value while Free Chrome is enabled, retain the latest nonzero value.
        self.originals[path] = value
        newlyZeroed = newlyZeroed + 1
      else
        failed = failed + 1
      end
    end

    iterPaths(apply)
    for path in pairs(self.additional) do apply(path) end

    if self.firstScan or newlyZeroed > 0 or failed > 0 then
      self.logger("Humanity modifier fields found=" .. found ..
        ", newly zeroed=" .. newlyZeroed .. ", write failures=" .. failed ..
        " (scanned " .. scanned .. " candidate flats)")
      if found == 0 then
        self.logger("WARNING: no standard Humanity fields found; check game version / CET log")
      elseif failed > 0 then
        self.logger("WARNING: some Humanity fields could not be patched")
      end
    end
    self.firstScan = false
    return found, newlyZeroed
  end

  function self:restore()
    local restored, skipped, failed = 0, 0, 0
    for path, oldValue in pairs(self.originals) do
      local current = getNumber(path)
      if current == 0 then
        if setNumber(path, oldValue) then
          self.originals[path] = nil
          restored = restored + 1
        else
          failed = failed + 1
        end
      else
        -- An external mod modified it after our last scan; don't overwrite
        -- its new value or treat it as our own original anymore.
        self.originals[path] = nil
        skipped = skipped + 1
      end
    end
    self.logger("Humanity costs restored=" .. restored ..
      ", left to other mods=" .. skipped .. ", restore failures=" .. failed)
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
