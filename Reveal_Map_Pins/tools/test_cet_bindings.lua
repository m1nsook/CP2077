-- Regression smoke test for CET registration during Lua script load.
-- Run: lua tools/test_cet_bindings.lua <mod-directory> [missing]
-- Requires standard Lua 5.1+; NO Cyberpunk runtime is needed.
local dir = assert(arg[1], "pass CET mod directory")
local missing = arg[2] == "missing"
local binds, callbacks, logs = {}, {}, {}
local nativePrint = print
print = function(...)
  local vals = {}
  for i = 1, select("#", ...) do vals[#vals+1] = tostring(select(i, ...)) end
  local s = table.concat(vals, "\t")
  logs[#logs+1] = s
  nativePrint(s)
end
json = {
  encode = function() return '{"schema":1,"settings":{}}' end,
  decode = function() return {settings={}} end,
}
registerHotkey = function(id, label, fn)
  assert(not binds[id], "duplicate hotkey " .. id)
  binds[id] = fn
end
registerForEvent = function(name, fn)
  assert(not callbacks[name], "duplicate event " .. name)
  callbacks[name] = fn
end
Game = {}
assert(TweakDBID == nil, "test should simulate unavailable game RTTI")
assert(loadfile(dir .. "/init.lua"))()
local count = 0
for _ in pairs(binds) do count = count + 1 end
assert(count == 4, "expected 4 hotkeys, got " .. count)
for _, id in ipairs({"RevealMapPins_Toggle","RevealMapPins_Catalog",
                       "RevealMapPins_Status","RevealMapPins_Preview"}) do
  assert(binds[id], "missing " .. id)
end
for _, name in ipairs({"onInit","onUpdate","onSessionStart","onSessionEnd","onShutdown"}) do
  assert(callbacks[name], "missing event " .. name)
end
callbacks.onInit()
local joined = table.concat(logs, "\n")
if missing then
  assert(joined:find("catalog startup FAILED", 1, true))
  assert(joined:find("hotkeys remain available", 1, true))
else
  assert(joined:find("Classified catalog loaded (catalog=219", 1, true))
end
assert(joined:find("four Reveal Map Pins bindings registered", 1, true))
nativePrint("PASS: four CET hotkeys load with " .. (missing and "missing" or "present") .. " catalog")
