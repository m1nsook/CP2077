package.path = "./m1core/src/?.lua;" .. package.path
local Core = require("core")
local View = require("ui")
local writes=0
local registry=Core.new({
  load=function() return {} end,
  save=function() writes=writes+1 return true end
})
assert(registry:register("demo","Demo",{
  {id="enabled",type="boolean",default=true,description="A toggle"},
  {id="weight",type="integer",default=100,min=0,max=500,group="Gameplay"},
  {id="mode",type="choice",default="normal",choices={"normal","hard"}}
}))
local calls, stack = {}, 0
local function called(name) calls[name] = (calls[name] or 0)+1 end
local noop={__index=function(_,name)
  return function() called(name) end
end}
_G.ImGui = setmetatable({},noop)
_G.ImGuiCol = {Button=1,ButtonHovered=2,Text=3,ChildBg=4}
_G.ImGuiStyleVar = {WindowRounding=1}
_G.ImGuiWindowFlags = {NoCollapse=1}
_G.ImGuiCond = {FirstUseEver=1}
_G.ImGuiTableFlags={BordersInnerH=1}
_G.ImGuiTableColumnFlags={WidthStretch=1,WidthFixed=2}
function ImGui.Begin(_, open)
  called("Begin"); return open,true -- CET tuple order, not reversed!
end
function ImGui.BeginChild(...) called("BeginChild");return true end
function ImGui.BeginTable(...) called("BeginTable");return true end
function ImGui.InputText(_, value)
  called("InputText");return value,false
end
function ImGui.Checkbox(_, value)
  called("Checkbox");return value,false
end
function ImGui.SliderInt(_, value)
  called("SliderInt");return value,false
end
function ImGui.BeginCombo() called("BeginCombo");return false end
function ImGui.GetContentRegionAvail() return 900,550 end
function ImGui.Button() called("Button");return false end
function ImGui.Selectable() called("Selectable");return false end
function ImGui.PushStyleColor(...) stack=stack+1;called("PushStyleColor") end
function ImGui.PopStyleColor(n) stack=stack-(n or 1) end

local ui=View.new()
ui.selected="demo"
View.draw(registry,ui)
assert(ui.visible,"CET Begin open return order must not close the window")
assert(calls.Begin == 1 and calls.End == 1)
assert(calls.BeginChild == 3 and calls.EndChild == 3)
assert(calls.BeginTable == 1 and calls.EndTable == 1)
assert(calls.Checkbox == 1 and calls.SliderInt == 1)
assert(calls.BeginCombo == 1)
assert(stack==0,"unbalanced color stack")
assert(writes==0,"UI may not write saved settings without edits")
print("PASS CET-compatible ImGui call structure and return order")
