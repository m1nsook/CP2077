-- m1core v1 — CET-hosted. MIT, no native input hooks or DX12 detours.
local Core = require("core")
local Storage = require("storage")
local View = require("ui")

local registry = Core.new(Storage.new("settings.json"))
local state = View.new()
local api = {
  version = "1.0.0-dev.1",
  register = function(modId, name, settings)
    return registry:register(modId, name, settings)
  end,
  get = function(modId, id) return registry:get(modId, id) end,
  set = function(modId, id, value) return registry:set(modId, id, value) end,
  reset = function(modId, id) return registry:reset(modId, id) end,
  revision = function() return registry:revisionToken() end,
  save = function() return registry:save() end,
  saveProfile = function(name) return registry:saveProfile(name) end,
  loadProfile = function(name) return registry:loadProfile(name) end,
  deleteProfile = function(name) return registry:deleteProfile(name) end,
  profiles = function() return registry:profileNames() end,
  isVisible = function() return state.visible end,
  show = function() state.visible = true end,
  hide = function() state.visible = false end
}

-- CET's built-in input architecture handles the toggle key and OS cursor.
-- This hotkey has NO default; the player binds it in CET's Bindings tab.
registerHotkey("m1core_toggle", "M1CORE - Show / hide settings", function()
  state.visible = not state.visible
end)

-- CET owns when ImGui may be called. Never render from onUpdate or a hook.
registerForEvent("onDraw", function()
  if state.visible then View.draw(registry, state) end
end)

registerForEvent("onInit", function()
  registry:log("m1core initialized through CET; no independent input hooks")
end)

-- First time: M1CORE is visible whenever the player opens CET overlay.
-- Afterwards the native CET binding can hide/show it; no setup trap.
registerForEvent("onOverlayOpen", function()
  -- Opening CET must always provide a way back to settings even when
  -- no optional M1CORE hotkey has been assigned.
  state.visible = true
end)
registerForEvent("onOverlayClose", function() end)
registerForEvent("onShutdown", function() registry:save() end)

-- CET's GetMod("m1core") provides this API to other CET Lua mods.
return api
