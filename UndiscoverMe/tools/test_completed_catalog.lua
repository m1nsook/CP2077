-- Test the last confirmed working catalog behavior with fixers fully excluded.
-- Run from Reveal_Map_Pins: lua tools/test_completed_catalog.lua
-- No live game/save required. Does not assume journal APIs work on 2.5.
local root = (arg and arg[1]) or "bin/x64/plugins/cyber_engine_tweaks/mods/RevealMapPins/"
if root:sub(-1) ~= "/" and root:sub(-1) ~= "\\" then root = root .. "/" end

local owned, nextId = {}, 0
local completed = {[359900925]=true, [9669499]=true}
local nativePin = {
  IsVisible=function() return true end,
  GetWorldPosition=function() return {x=395.26306, y=1151.14099} end,
  GetVariant=function() return "TarotVariant" end,
  GetNewMappinID=function() return {value=400000} end,
}
local ms={}
function ms:GetMappins() return {nativePin} end
function ms:GetPointOfInterestMappinSavedState(hash)
  if completed[hash] then return true, 0, 0, false end
  return false, 0, 0, false
end
function ms:RegisterMappin(data, pos)
  assert(type(pos.x) == "number")
  nextId = nextId+1
  owned[nextId] = {data=data, position=pos}
  assert(data.variant ~= "FixerVariant", "Never create duplicate fixer overlays")
  return {value=nextId}
end
function ms:UnregisterMappin(id) owned[id.value] = nil end

Game = {
  GetMappinSystem=function() return ms end,
  GetPlayer=function() return {} end,
  IsEP1=function() return true end,
}
gamemappinsMappinTargetType={Map=1}
gamedataMappinPhase={CompletedPhase=0, UndiscoveredPhase=3}
gamedataMappinVariant={
  GangWatchVariant="GangWatchVariant", OutpostVariant="OutpostVariant",
  HiddenStashVariant="HiddenStashVariant",
  HuntForPsychoVariant="HuntForPsychoVariant",
  TarotVariant="TarotVariant", FixerVariant="FixerVariant",
}
MappinData={new=function() return {} end}
Vector4={new=function(x,y,z,w) return {x=x,y=y,z=z,w=w} end}
TweakDBID={new=function(x) return x end}
EnumInt=function(x) return x end

local settings={catalog=true,enabled=true,ncpd=true,psycho=true,tarot=true}
local catalog=dofile(root.."catalog_runtime.lua").new(
  root, function(key) return settings[key] end, print)
assert(catalog.rows and #catalog.rows == 212)
catalog:tick(0)
-- 212 non-fixer candidates - 2 completed native POIs - 1 visible original
assert(catalog.statistics.added == 209, catalog:stats())
assert(catalog.statistics.skipped == 3)
completed[148304100] = true
catalog:tick(12)
assert(catalog.statistics.added == 208, catalog:stats())
settings.catalog = false
catalog:tick(0)
assert(catalog.statistics.added == 0)
assert(next(owned) == nil)
print("PASS: fixers excluded, native originals untouched, registration and cleanup intact")
