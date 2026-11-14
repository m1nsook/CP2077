-- Regression for native completed-quest enumeration, no game/save required.
-- Run from Reveal_Map_Pins: texlua tools/test_live_completion.lua
local folder = (arg and arg[1]) or "bin/x64/plugins/cyber_engine_tweaks/mods/RevealMapPins/"
if folder:sub(-1) ~= "/" and folder:sub(-1) ~= "\" then folder=folder.."/" end
local owned, nextId = {}, 0
local mappins = {}
function mappins:GetMappins() return {} end
function mappins:GetPointOfInterestMappinSavedState() return false, 3, 0, false end
function mappins:RegisterMappin(data, position)
  nextId = nextId + 1
  owned[nextId] = {data=data,position=position}
  return {value=nextId}
end
function mappins:UnregisterMappin(id) owned[id.value] = nil end
local finished = {"ma_wat_kab_02", "ma_wat_nid_12"}
local jm = {}
function jm:GetQuests(context)
  assert(context.stateFilter.succeeded and context.stateFilter.failed)
  local quests = {}
  for _,id in ipairs(finished) do
    quests[#quests+1] = {GetId=function() return id end}
  end
  return quests
end
function jm:GetEntryState() return 3 end
Game = {
  GetMappinSystem=function() return mappins end,
  GetPlayer=function() return {} end,
  IsEP1=function() return true end,
  GetJournalManager=function() return jm end,
}
MappinData = {new=function() return {} end}
TweakDBID = {new=function(record) return record end}
Vector4 = {new=function(x,y,z,w) return {x=x,y=y,z=z,w=w} end}
gamedataMappinPhase = {CompletedPhase=0, UndiscoveredPhase=3}
gamedataMappinVariant = {
  GangWatchVariant="A", OutpostVariant="O", HiddenStashVariant="R",
  HuntForPsychoVariant="C", TarotVariant="T",
}
gamemappinsMappinTargetType = {Map=1}
EnumInt = function(n) return n end
JournalRequestStateFilter = {new=function(options) return options end}
JournalRequestContext = {new=function(options) return options end}
local settings = {enabled=true,catalog=true,ncpd=true,psycho=true,tarot=true}
local instance = dofile(folder.."catalog_runtime.lua").new(
  folder, function(key) return settings[key] end, print
)
assert(instance.rows and #instance.rows == 212, "catalog changed")
instance:tick(0)
assert(instance.statistics.completed == 2, instance:stats())
assert(instance.statistics.journalMatched == 2, instance:stats())
assert(instance.statistics.added == 210, instance:stats())
-- A transient journal failure must not erase unknown markers.
Game.GetJournalManager = function() error("journal temporarily unavailable") end
instance:tick(12)
assert(instance.statistics.added == 210, instance:stats())
assert(instance.statistics.completed == 2, instance:stats())
settings.catalog = false
instance:tick(0)
assert(instance.statistics.added == 0 and next(owned) == nil)
print("PASS: terminal states hide only matching quests; unavailable journal preserves pins")
