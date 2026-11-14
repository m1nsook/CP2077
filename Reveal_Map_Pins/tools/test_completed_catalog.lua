-- Run from Reveal_Map_Pins/ with Lua 5.4:
-- lua tools/test_completed_catalog.lua
-- No Cyberpunk game files or player save required.
local folder = (arg and arg[1]) or "bin/x64/plugins/cyber_engine_tweaks/mods/RevealMapPins/"
if folder:sub(-1) ~= "/" and folder:sub(-1) ~= "\\" then folder=folder.."/" end
local mappins, count = {}, 0
local ms = {}
function ms:GetMappins() return {} end
function ms:GetPointOfInterestMappinSavedState(hash)
  if hash == 359900925 then return true, 0, 1, false end
  return false, 0, 0, false
end
function ms:RegisterMappin(data, position)
  count = count + 1
  local id = {value=count}
  mappins[count] = {data=data,position=position,id=id}
  return id
end
function ms:UnregisterMappin(id) mappins[id.value] = nil end

local terminal = {
  ["quests/minor_quest/ma_pac_cvi_08"] = 3,
  ["quests/minor_quest/ma_wat_nid_03"] = 4,
  ["quests/minor_activities/watson/little_china/ma_wat_lch_03"] = 3,
}
local journal = {}
function journal:GetEntry(hash) return nil end
function journal:GetEntryByString(path, cl)
  assert(cl == "gameJournalQuest")
  if terminal[path] then return {path=path} end
  return nil
end
function journal:GetEntryState(entry) return terminal[entry.path] end

local settings = {enabled=true,catalog=true,ncpd=true,fixer=true,psycho=true,tarot=true}
Game = {
  GetMappinSystem=function() return ms end,
  GetPlayer=function() return {} end,
  IsEP1=function() return true end,
  GetJournalManager=function() return journal end,
}
MappinData = {new=function() return {} end}
TweakDBID = {new=function(record) return record end}
Vector4 = {new=function(x,y,z,w) return {x=x,y=y,z=z,w=w} end}
gamedataMappinPhase = {CompletedPhase=0,UndiscoveredPhase=3}
gamedataMappinVariant = {
  GangWatchVariant="A", OutpostVariant="O", HiddenStashVariant="R",
  HuntForPsychoVariant="C", TarotVariant="T", FixerVariant="F",
}
gamemappinsMappinTargetType = {Map=1}
EnumInt = function(x) return x end

local runtime = dofile(folder.."catalog_runtime.lua").new(
  folder, function(key) return settings[key] end, function() end
)
assert(runtime.rows and #runtime.rows == 219)
runtime:tick(0)
-- One native saved completion, two cyberpsychos, one NCPD completed quest.
assert(runtime.statistics.added == 215, runtime:stats())
assert(runtime.statistics.completed == 4)
assert(runtime.statistics.unverified > 100, "mock deliberately has unknown journals")
assert(runtime.statistics.questResolved >= 3)

-- A newly completed cyberpsycho should disappear on the next refresh.
terminal["quests/minor_quest/ma_bls_ina_se1_08"] = 3
runtime:tick(12)
assert(runtime.statistics.added == 214, runtime:stats())
assert(runtime.statistics.completed == 5)

-- A transient unavailable journal must not destroy the map or resurrect
-- previously VERIFIED finished quests within this session.
Game.GetJournalManager = function() return nil end
runtime:tick(12)
assert(runtime.statistics.added == 214, runtime:stats())
assert(runtime.statistics.completed == 5)

settings.catalog = false
runtime:tick(0)
assert(runtime.statistics.added == 0)
for _ in pairs(mappins) do error("leaked registered mappin") end
print("PASS: unknown states preserve pins; verified completion hides; session cleanup")
