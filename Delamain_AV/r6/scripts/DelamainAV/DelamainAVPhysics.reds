// Native functions exported by DelamainAV.dll. No CET or DAV dependency.
public native func DelamainAV_PhysicsBegin(target: ref<VehicleObject>) -> Int32
public native func DelamainAV_PhysicsPulse(target: ref<VehicleObject>, verticalSpeed: Float) -> Int32
public native func DelamainAV_PhysicsStop(target: EntityID) -> Bool
