module UndiscoverMeTeleporter

import MisoRevealMapPins.*

// Optional addon. Check the core's actual marker IDs, not icon or location.
@addField(WorldMapMenuGameController)
private let m_udmTeleportTriggered: Bool;

@addMethod(WorldMapMenuGameController)
private final func UDMCanTeleportSelected() -> Bool {
  if !this.m_readyToZoom || !IsDefined(this.m_player) || !IsDefined(this.selectedMappin) {
    return false;
  }

  let pin = this.selectedMappin.GetMappin();
  if !IsDefined(pin) { return false; }

  let gi = this.m_player.GetGame();
  let qs = GameInstance.GetQuestsSystem(gi);
  if !IsDefined(qs) || qs.GetFactStr("miso_rmp_all_disabled") == 1 {
    return false;
  }

  let container = GameInstance.GetScriptableSystemsContainer(gi);
  if !IsDefined(container) { return false; }
  let owner = container.Get(n"MisoRevealMapPins.RMPSystem") as RMPSystem;
  if !IsDefined(owner) { return false; }
  return owner.UDMOwnsPin(pin.GetNewMappinID());
}

// Suppress native fast-travel preparation ONLY for core-owned markers.
@wrapMethod(WorldMapMenuGameController)
protected cb func OnPressInput(e: ref<inkPointerEvent>) -> Bool {
  if e.IsAction(n"world_map_menu_fast_travel") && this.UDMCanTeleportSelected() {
    return true;
  }
  wrappedMethod(e);
  return true;
}

// Reuse the map's existing hold action and progress instead of a new bind.
@wrapMethod(WorldMapMenuGameController)
protected cb func OnHoldInput(e: ref<inkPointerEvent>) -> Bool {
  if e.IsAction(n"world_map_menu_fast_travel") && this.UDMCanTeleportSelected() {
    if !this.m_udmTeleportTriggered && e.GetHoldProgress() >= 1.0 {
      let pin = this.selectedMappin.GetMappin();
      let player = GameInstance.GetPlayerSystem(this.m_player.GetGame()).GetLocalPlayerMainGameObject();
      let facility = GameInstance.GetTeleportationFacility(this.m_player.GetGame());
      if IsDefined(pin) && IsDefined(player) && IsDefined(facility) {
        let target = pin.GetWorldPosition();
        target.Z += 1.5;
        this.m_udmTeleportTriggered = true;
        facility.Teleport(player, target, Vector4.ToRotation(player.GetWorldForward()));
        if IsDefined(this.m_menuEventDispatcher) {
          this.m_menuEventDispatcher.SpawnEvent(n"OnCloseHubMenu");
        }
        e.Handle();
        e.Consume();
      }
    }
    return true;
  }
  wrappedMethod(e);
  return true;
}

@wrapMethod(WorldMapMenuGameController)
protected cb func OnReleaseInput(e: ref<inkPointerEvent>) -> Bool {
  if e.IsAction(n"world_map_menu_fast_travel") {
    this.m_udmTeleportTriggered = false;
  }
  wrappedMethod(e);
  return true;
}
