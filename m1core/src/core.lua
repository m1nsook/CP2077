-- m1core v1: settings registry built on CET's real scripting API.
-- MIT: independent Lua mod; CET itself remains separately GPLv3.
local Core = {}
Core.__index = Core

local function valid_id(id)
  return type(id) == "string" and #id > 0 and #id <= 96
     and id:match("^[%w_.%-]+$") ~= nil
end
local function clone(value)
  if type(value) ~= "table" then return value end
  local result = {}
  for k,v in pairs(value) do result[k] = clone(v) end
  return result
end
local function normalize(setting, value)
  local kind = setting.type
  if kind == "boolean" then
    if type(value) ~= "boolean" then return nil, "expected boolean" end
    return value
  elseif kind == "integer" or kind == "float" then
    if type(value) ~= "number" or value ~= value or
       value == math.huge or value == -math.huge then
      return nil, "expected finite number"
    end
    local low, high = setting.min or 0, setting.max or 100
    if type(low) ~= "number" or type(high) ~= "number" or low > high then
      return nil, "invalid range"
    end
    value = math.max(low, math.min(high, value))
    if kind == "integer" then value = math.floor(value + 0.5) end
    return value
  elseif kind == "choice" then
    if type(value) ~= "string" then return nil, "expected choice string" end
    for _,choice in ipairs(setting.choices or {}) do
      if choice == value then return value end
    end
    return nil, "not a valid choice"
  end
  return nil, "unsupported setting type"
end

function Core.new(store)
  assert(store and type(store.load) == "function" and
         type(store.save) == "function", "storage adapter required")
  local self = setmetatable({
    store = store, mods = {}, order = {}, logs = {}, revision = 0,
    saved = {}, profiles = {}, error = nil, readOnly = false
  }, Core)
  local ok, data = pcall(store.load, store)
  if ok and type(data) == "table" then
    self.saved = type(data.values) == "table" and data.values or {}
    self.profiles = type(data.profiles) == "table" and data.profiles or {}
  elseif not ok then
    self.error = "Unable to read saved settings; file will not be overwritten"
    self.readOnly = true
  end
  return self
end

function Core:log(message)
  self.logs[#self.logs+1] = tostring(message)
  if #self.logs > 120 then table.remove(self.logs, 1) end
end

function Core:save()
  if self.readOnly then return false, self.error end
  local ok, success, reason = pcall(self.store.save, self.store, {
    schema = 1, values = self.saved, profiles = self.profiles
  })
  if not ok or success == false then
    self.error = tostring(reason or success or "failed to write settings")
    self:log("Save error: " .. self.error)
    return false, self.error
  end
  self.error = nil
  return true
end

function Core:register(modId, displayName, descriptors)
  if not valid_id(modId) or type(displayName) ~= "string" or
     #displayName == 0 or type(descriptors) ~= "table" then
    return false, "invalid registration"
  end
  local mod = self.mods[modId]
  if not mod then
    mod = { id = modId, name = displayName, settings = {}, order = {} }
    self.mods[modId] = mod
    self.order[#self.order+1] = modId
  else
    mod.name = displayName
  end

  local registered = 0
  for _,input in ipairs(descriptors) do
    if type(input) == "table" and valid_id(input.id) and
       type(input.type) == "string" and
       type(input.default) ~= "nil" then
      local setting = clone(input)
      setting.label = type(setting.label) == "string" and
                      setting.label or setting.id
      setting.description = type(setting.description) == "string" and
                            setting.description or ""
      setting.group = type(setting.group) == "string" and setting.group or "General"
      local default = normalize(setting, setting.default)
      if default ~= nil then
        setting.default = default
        local previous = self.saved[modId]
        local stored = previous and previous[setting.id]
        local restored = nil
        if stored ~= nil then restored = normalize(setting, stored) end
        if restored ~= nil then setting.value = restored
        else setting.value = default end
        if not mod.settings[setting.id] then
          mod.order[#mod.order+1] = setting.id
        end
        mod.settings[setting.id] = setting
        registered = registered + 1
      end
    end
  end
  if registered == 0 then return false, "no valid settings" end
  table.sort(self.order, function(a,b)
    return self.mods[a].name:lower() < self.mods[b].name:lower()
  end)
  self:log("Registered " .. modId .. " (" .. registered .. " settings)")
  return true
end

function Core:get(modId, settingId)
  local mod = self.mods[modId]
  local setting = mod and mod.settings[settingId]
  if not setting then return nil end
  return setting.value
end

function Core:set(modId, settingId, value)
  local mod = self.mods[modId]
  local setting = mod and mod.settings[settingId]
  if not setting then return false, "unknown setting" end
  local parsed, err = normalize(setting, value)
  if parsed == nil then return false, err end
  if parsed == setting.value then return true end
  setting.value = parsed
  self.saved[modId] = self.saved[modId] or {}
  self.saved[modId][settingId] = parsed
  self.revision = self.revision + 1
  self:log("Changed " .. modId .. "." .. settingId)
  local ok, reason = self:save()
  return ok, reason
end

function Core:reset(modId, settingId)
  local mod = self.mods[modId]
  if not mod then return false, "unknown mod" end
  if settingId then
    local setting = mod.settings[settingId]
    if not setting then return false, "unknown setting" end
    return self:set(modId, settingId, setting.default)
  end
  local ok = true
  for _,id in ipairs(mod.order) do
    if not self:set(modId, id, mod.settings[id].default) then ok = false end
  end
  return ok
end

function Core:revisionToken()
  return self.revision
end

function Core:modsList()
  local result = {}
  for _,id in ipairs(self.order) do result[#result+1] = self.mods[id] end
  return result
end

function Core:profileNames()
  local result = {}
  for id in pairs(self.profiles) do result[#result+1] = id end
  table.sort(result)
  return result
end

function Core:saveProfile(name)
  if not valid_id(name) then return false, "invalid profile name" end
  local snapshot = {}
  for modId,mod in pairs(self.mods) do
    snapshot[modId] = {}
    for _,id in ipairs(mod.order) do
      snapshot[modId][id] = mod.settings[id].value
    end
  end
  self.profiles[name] = snapshot
  return self:save()
end

function Core:loadProfile(name)
  local profile = self.profiles[name]
  if type(profile) ~= "table" then return false, "unknown profile" end
  -- Validate every value before applying anything: don't partially import.
  local pending = {}
  for modId,values in pairs(profile) do
    local mod = self.mods[modId]
    if mod and type(values) == "table" then
      for settingId,value in pairs(values) do
        local setting = mod.settings[settingId]
        if setting then
          local parsed = normalize(setting, value)
          if parsed == nil then return false, "invalid profile value" end
          pending[#pending+1] = {modId, settingId, parsed}
        end
      end
    end
  end
  for _,entry in ipairs(pending) do
    local modId, settingId, value = entry[1], entry[2], entry[3]
    self.mods[modId].settings[settingId].value = value
    self.saved[modId] = self.saved[modId] or {}
    self.saved[modId][settingId] = value
  end
  self.revision = self.revision + 1
  self:log("Loaded profile " .. name)
  return self:save()
end

function Core:deleteProfile(name)
  if not self.profiles[name] then return false, "unknown profile" end
  self.profiles[name] = nil
  return self:save()
end

return Core
