-- M1CORE settings persistence is owned by the RED4ext DLL.
-- CET Lua only marshals JSON through native RTTI functions. No custom hooks.
local Storage = {}
Storage.__index = Storage

function Storage.new()
  local success, version = pcall(function() return Game.M1Core_NativeVersion() end)
  if not success or type(version) ~= "string" then
    error("m1core.dll is required. Install the matching red4ext/ folder.")
  end
  local readyOk, ready = pcall(function() return Game.M1Core_NativeReady() end)
  if not readyOk or not ready then
    local errOk, err = pcall(function() return Game.M1Core_NativeLastError() end)
    error("m1core.dll settings unavailable: " .. tostring(errOk and err or "unknown error"))
  end
  return setmetatable({},Storage)
end

function Storage:load()
  local content = Game.M1Core_NativeGetState()
  if type(content) ~= "string" then error("Native DLL returned no settings state") end
  local ok, data = pcall(json.decode, content)
  if not ok or type(data) ~= "table" then
    error("Native DLL returned invalid JSON")
  end
  return data
end

function Storage:save(data)
  local ok, encoded = pcall(json.encode, data)
  if not ok or type(encoded) ~= "string" then
    return false, "Unable to encode settings"
  end
  local success, updated = pcall(function() return Game.M1Core_NativeSetState(encoded) end)
  if not success or not updated then
    local errOk, err = pcall(function() return Game.M1Core_NativeLastError() end)
    return false, tostring((errOk and err ~= "" and err) or "native save failed")
  end
  return true
end

return Storage
