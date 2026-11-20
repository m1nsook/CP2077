-- CET already supplies sandboxed IO and the global json codec.
-- All writes stay inside the m1core CET mod directory.
local Storage = {}
Storage.__index = Storage

function Storage.new(filename)
  return setmetatable({ filename = filename or "settings.json" }, Storage)
end

function Storage:load()
  local file = io.open(self.filename, "r")
  if not file then return {} end
  local text = file:read("*a")
  file:close()
  if type(text) ~= "string" or text == "" then return {} end
  local ok, parsed = pcall(json.decode, text)
  if not ok or type(parsed) ~= "table" then
    error("settings.json contains invalid JSON; original file preserved")
  end
  return parsed
end

function Storage:save(data)
  local ok, encoded = pcall(json.encode, data)
  if not ok or type(encoded) ~= "string" then
    return false, "unable to encode settings"
  end
  -- CET restricts the IO API to this mod's sandbox directory.
  local file, err = io.open(self.filename, "w")
  if not file then return false, err end
  local success, failure = file:write(encoded)
  file:close()
  return success ~= nil, failure
end

return Storage
