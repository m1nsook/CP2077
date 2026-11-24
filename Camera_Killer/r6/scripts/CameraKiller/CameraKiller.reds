// m1nsook
// Stops our propagated OFF events from creating another wave.
@addField(SurveillanceCameraControllerPS)
public let m_cameraKillerIncoming: Bool;

@addMethod(SurveillanceCameraControllerPS)
public func CameraKillerToNetwork() -> Void {
  // The original OFF action has already run. Only gate propagation.
  if !M1Arc_GetBool("m1nsook.camera-killer", "enabled") {
    return;
  }
  let accessPoints: array<ref<AccessPointControllerPS>> = this.GetAccessPoints();
  let i: Int32 = 0;

  // Prefer explicit physical access-point links over broad security systems.
  while i < ArraySize(accessPoints) {
    if IsDefined(accessPoints[i]) {
      this.CameraKillerShutDownChildren(accessPoints[i]);
    }
    i += 1;
  }
  if ArraySize(accessPoints) > 0 {
    return;
  }

  // Some cameras are attached below a virtual surveillance system, not
  // directly below an access point. Search their established ancestors.
  let ancestors: array<ref<DeviceComponentPS>>;
  this.GetAncestors(ancestors);
  i = 0;
  while i < ArraySize(ancestors) {
    let surveillance: ref<SurveillanceSystemControllerPS> = ancestors[i] as SurveillanceSystemControllerPS;
    if IsDefined(surveillance) {
      this.CameraKillerShutDownChildren(surveillance);
      return;
    }
    i += 1;
  }
  i = 0;
  while i < ArraySize(ancestors) {
    let accessPoint: ref<AccessPointControllerPS> = ancestors[i] as AccessPointControllerPS;
    if IsDefined(accessPoint) {
      this.CameraKillerShutDownChildren(accessPoint);
      return;
    }
    i += 1;
  }
}

@addMethod(SurveillanceCameraControllerPS)
public func CameraKillerShutDownChildren(master: ref<MasterControllerPS>) -> Void {
  if !IsDefined(master) {
    return;
  }
  let members: array<ref<DeviceComponentPS>>;
  master.GetAllDescendants(members);
  let i: Int32 = 0;
  while i < ArraySize(members) {
    let camera: ref<SurveillanceCameraControllerPS> = members[i] as SurveillanceCameraControllerPS;
    if IsDefined(camera) && camera != this && camera.IsON() && !camera.IsMarkedAsQuest() {
      camera.m_cameraKillerIncoming = true;
      this.QueuePSEvent(camera, camera.ActionSetDeviceOFF());
    }
    i += 1;
  }
}

// The common camera quickhack uses QuickHackToggleON -> ToggleON, not
// SetDeviceOFF, so we need this hook as well.
@wrapMethod(SurveillanceCameraControllerPS)
public func OnToggleON(evt: ref<ToggleON>) -> EntityNotificationType {
  let wasOn: Bool = this.IsON();
  let result: EntityNotificationType = wrappedMethod(evt);
  if wasOn && this.IsOFF() && !this.m_cameraKillerIncoming && !this.IsMarkedAsQuest() {
    this.CameraKillerToNetwork();
  }
  return result;
}

// Breach Protocol's Network Camera Shutdown, engineering actions and
// some other device interactions turn cameras off with SetDeviceOFF.
// Hook the inherited handler once, but only act for cameras.
@wrapMethod(ScriptableDeviceComponentPS)
protected func OnSetDeviceOFF(evt: ref<SetDeviceOFF>) -> EntityNotificationType {
  let camera: ref<SurveillanceCameraControllerPS> = this as SurveillanceCameraControllerPS;
  let wasOn: Bool = IsDefined(camera) && camera.IsON();
  let result: EntityNotificationType = wrappedMethod(evt);
  if IsDefined(camera) {
    if camera.m_cameraKillerIncoming {
      camera.m_cameraKillerIncoming = false;
    } else {
      if wasOn && camera.IsOFF() && !camera.IsMarkedAsQuest() {
        camera.CameraKillerToNetwork();
      }
    }
  }
  return result;
}

// Report the effective setting once at startup and when it changes.
public class MisoCameraKillerDiagnostics extends ScriptableSystem {
  private let running: Bool;
  private let ready: Bool;
  private let previous: Bool;

  private func OnAttach() -> Void {
    this.running = true;
    this.Tick();
  }

  private func OnDetach() -> Void {
    this.running = false;
  }

  public func Tick() -> Void {
    if !this.running { return; }
    let enabled = M1Arc_GetBool("m1nsook.camera-killer", "enabled");
    if !this.ready || NotEquals(enabled, this.previous) {
      if M1Arc_Log("m1nsook.camera-killer", "INFO", "Camera network shutdown enabled=" + ToString(enabled)) {
        this.ready = true;
        this.previous = enabled;
      }
    }
    let cb = new MisoCameraKillerDiagnosticsTick();
    cb.owner = this;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(cb, 1.00, false);
  }
}

public class MisoCameraKillerDiagnosticsTick extends DelayCallback {
  public let owner: wref<MisoCameraKillerDiagnostics>;
  public func Call() -> Void {
    if IsDefined(this.owner) { this.owner.Tick(); }
  }
}
