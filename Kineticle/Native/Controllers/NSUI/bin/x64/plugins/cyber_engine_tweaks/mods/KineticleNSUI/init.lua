-- Kineticle NSUI controller. All gameplay and persistence live in Core.
-- Scope is supplied by the included REDscript bridge.
local api, ready, busy = nil, false, false
local options, pending = {}, nil
local timer, retry = 0, 0
local reported = false
local function warn(message)
    print("[Kineticle NSUI] " .. tostring(message))
end
local function call(name, ...)
    local args = {...}
    local ok, value = pcall(function() return Game[name](unpack(args)) end)
    if not ok then return nil end
    return value
end
local function hasFlag(flags, bit)
    return math.floor(flags / bit) % 2 == 1
end
local function read()
    local enabled = call("Kineticle_GetProbeEnabled")
    local mass = call("Kineticle_GetMassMultiplier")
    local bikes = call("Kineticle_GetIncludeMotorcycles")
    local flags = call("Kineticle_GetProtectionFlags")
    if type(enabled) ~= "boolean" or type(mass) ~= "number" or
       type(bikes) ~= "boolean" or type(flags) ~= "number" then return nil end
    return {probeEnabled=enabled, massMultiplier=mass, includeMotorcycles=bikes,
        mechanicalProtection=hasFlag(flags,1), gridProtection=hasFlag(flags,2),
        glassProtection=hasFlag(flags,4)}
end
local function sync()
    local state = read()
    if not state then return end
    busy = true
    for key, value in pairs(state) do
        if options[key] and not (key == "massMultiplier" and pending ~= nil) then
            api.setOption(options[key], value)
        end
    end
    busy = false
end
local function write(key, value)
    if busy or not ready then return end
    local ok
    if key == "probeEnabled" then
        ok = call("Kineticle_SetProbeEnabled",value)
    elseif key == "includeMotorcycles" then
        ok = call("Kineticle_SetIncludeMotorcycles",value)
    else
        local bit = ({mechanicalProtection=1,gridProtection=2,glassProtection=4})[key]
        local flags = call("Kineticle_GetProtectionFlags")
        if bit and type(flags) == "number" then
            if hasFlag(flags,bit) ~= value then flags = flags + (value and bit or -bit) end
            ok = call("Kineticle_SetProtectionFlags",flags)
        end
    end
    if ok ~= true then warn("Setting rejected: "..key) end
    sync()
end
local function commitMass()
    if pending == nil then return end
    local value = pending
    pending = nil
    if call("Kineticle_SetMassMultiplier",value) ~= true then warn("Mass setting rejected") end
    sync()
end
local function init()
    if ready then return end
    local ns = GetMod("nativeSettings")
    local state = read()
    if not ns or not state then
        if not reported then warn("Waiting for Native Settings UI and Kineticle Core") reported=true end
        return
    end
    api=ns
    api.addTab("/kineticle", "Kineticle", function() commitMass();sync() end)
    api.addSubcategory("/kineticle/physics","Physics")
    api.addSubcategory("/kineticle/protection","Protection")
    options.probeEnabled=api.addSwitch("/kineticle/physics","Enable Kineticle","Master switch for all Kineticle effects. Configuration is saved by Core.",state.probeEnabled,true,function(value) write("probeEnabled",value) end)
    options.massMultiplier=api.addRangeInt("/kineticle/physics","Chassis mass multiplier","Persistent 1–100x alternative chassis collision mass, capped at 100,000 kg. 1 = vanilla.",1,100,1,state.massMultiplier,1,function(value)
        if not busy and ready then pending=math.max(1,math.min(100,math.floor(value)));timer=0.5 end
    end)
    options.includeMotorcycles=api.addSwitch("/kineticle/physics","Include motorcycles","Allow Kineticle effects while V drives motorcycles.",state.includeMotorcycles,true,function(value) write("includeMotorcycles",value) end)
    options.mechanicalProtection=api.addSwitch("/kineticle/protection","Mechanical damage protection","Zero the computed vehicle hit values for V's actively driven road vehicle.",state.mechanicalProtection,false,function(value) write("mechanicalProtection",value) end)
    options.gridProtection=api.addSwitch("/kineticle/protection","Body deformation restoration","Reset the damage grid after the vanilla destruction callback.",state.gridProtection,false,function(value) write("gridProtection",value) end)
    options.glassProtection=api.addSwitch("/kineticle/protection","Glass restoration","Reset glass after the vanilla destruction callback.",state.glassProtection,false,function(value) write("glassProtection",value) end)
    ready=true
    if call("Kineticle_GetMassAvailable") ~= true then warn("Native mass unavailable; see Core logs") end
end
registerForEvent("onInit", init)
registerForEvent("onUpdate", function(dt)
    if not ready then
        retry=retry+dt
        if retry>=1 then retry=0;init() end
        return
    end
    if pending ~= nil then
        timer=timer-dt
        if timer<=0 then commitMass() end
    end
end)
registerForEvent("onShutdown", function() if ready then commitMass() end end)
