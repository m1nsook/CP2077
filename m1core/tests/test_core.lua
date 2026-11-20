package.path = "./m1core/src/?.lua;" .. package.path
local Core = require("core")
local state = {schema=1, values={demo={active=false, gain=4.5}},profiles={}}
local store = {
  load=function() return state end,
  save=function(_, nextState) state = nextState return true end
}
local function check(ok, name)
  assert(ok, name)
  print("PASS " .. name)
end
local core = Core.new(store)
check(core:register("demo","Demo Mod",{
  {id="active",type="boolean",default=true,label="Enabled"},
  {id="level",type="integer",min=1,max=10,default=5,label="Level"},
  {id="gain",type="float",min=0,max=5,default=1.5,label="Gain"},
  {id="mode",type="choice",choices={"auto","manual"},default="auto"}
}),"register descriptors")
check(core:get("demo","active") == false,"restore persisted false")
check(core:get("demo","gain") == 4.5,"restore float")
local before = core:revisionToken()
check(core:set("demo","level",9) and core:get("demo","level")==9,"integer update")
check(core:revisionToken()==before+1,"revision increments")
check(core:set("demo","level",100) and core:get("demo","level")==10,"clamp high")
check(core:set("demo","gain",50) and core:get("demo","gain")==5,"clamp float")
check(not core:set("demo","level","bad"),"reject invalid types")
check(not core:set("demo","mode","invalid"),"reject unknown choice")
check(core:set("demo","mode","manual"),"choice update")
check(core:saveProfile("stealth"),"save profile")
check(core:set("demo","active",true),"toggle boolean")
check(core:loadProfile("stealth"),"load profile")
check(core:get("demo","active")==false,"profile restores false")
check(core:deleteProfile("stealth"),"delete profile")
check(#core:profileNames()==0,"deleted profile not listed")
check(core:reset("demo","level") and core:get("demo","level")==5,"reset setting")
local reopened=Core.new(store)
check(reopened:register("demo","Demo Mod",{
  {id="active",type="boolean",default=true},
  {id="level",type="integer",min=1,max=10,default=5}
}),"re-register after restart")
check(reopened:get("demo","active")==false,"boolean persists after restart")
check(reopened:get("demo","level")==5,"reset persists after restart")
check(not reopened:register("../../escape","Unsafe",{
  {id="x",type="boolean",default=true}
}),"reject invalid mod ids")
local locked=Core.new({load=function() error("broken JSON") end,
  save=function() error("must never overwrite unreadable config") end})
check(locked.error~=nil,"invalid saved config reported")
print("m1core CET core test suite passed")
