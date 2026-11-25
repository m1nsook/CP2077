// Native functions exported by DelamainAV.dll. No CET or DAV dependency.
public native func DelamainAV_PhysicsBegin(target: ref<VehicleObject>) -> Int32
public native func DelamainAV_PhysicsPulse(target: ref<VehicleObject>, verticalSpeed: Float) -> Int32
public native func DelamainAV_PhysicsStop(target: EntityID) -> Bool

// Persistent local diagnostic file, independent of M1 Arc's log history.
public native func DelamainAV_DiagnosticLog(level: String, message: String) -> Bool
public native func DelamainAV_DiagnosticSnapshot(target: ref<VehicleObject>, stage: String) -> Bool
public native func DelamainAV_DiagnosticEnvironment(stage: String) -> Bool

public native func DelamainAV_WorldState(attached: Bool) -> Bool
