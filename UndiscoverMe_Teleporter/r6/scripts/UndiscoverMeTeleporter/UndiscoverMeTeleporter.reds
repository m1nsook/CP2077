module UndiscoverMeTeleporter

import MisoRevealMapPins.*

// Optional addon. Check the core's actual marker IDs, not icon or location.
@addField(WorldMapMenuGameController)
private let m_udmTeleportTriggered: Bool;

// Check the hovered pin itself. A variant or a coordinate is not proof
// that the marker belongs to UndiscoverMe.
@addMethod(WorldMapMenuGameController)
public final func UDMCanTeleportPin(pin: ref<IMappin>) -> Bool {
  if !IsDefined(this.m_player) || !IsDefined(pin) {
    return false;
  }

  let gi = this.m_player.GetGame();
  let qs = GameInstance.GetQuestsSystem(gi);
  if !IsDefined(qs) || qs.GetFactStr("miso_rmp_all_disabled") == 1 {
    return false;
  }

  let container = GameInstance.GetScriptableSystemsContainer(gi);
  if !IsDefined(container) { return false; }
  let owner = container.Get(n"MisoRevealMapPins.RMPSystem") as RMPSystem;
  return IsDefined(owner) && owner.UDMOwnsPin(pin.GetNewMappinID());
}

// For the map action, the pin must also be selected and the map ready.
@addMethod(WorldMapMenuGameController)
private final func UDMCanTeleportSelected() -> Bool {
  return this.HasSelectedMappin() && IsDefined(this.selectedMappin)
    && this.UDMCanTeleportPin(this.selectedMappin.GetMappin());
}

// Use the game's own Fast Travel widget, button glyph and hold-progress ring.
// This stays in the player's Cyberpunk UI language; no custom Ink or string
// is injected, and other markers follow the unchanged vanilla tooltip path.
@wrapMethod(WorldMapMenuGameController)
private final func UpdateTooltip(tooltipType: WorldMapTooltipType, controller: wref<BaseWorldMapMappinController>) -> Void {
  if !IsDefined(controller) || !this.UDMCanTeleportPin(controller.GetMappin()) {
    wrappedMethod(tooltipType, controller);
    return;
  }

  let tooltipData: WorldMapTooltipData;
  tooltipData.controller = controller;
  tooltipData.mappin = controller.GetMappin();
  tooltipData.journalEntry = this.GetMappinJournalEntry(tooltipData.mappin);
  tooltipData.readJournal = this.CanOpenJournalForMappin(tooltipData.mappin);
  tooltipData.moreInfo = this.CanOpenCodexPopup(tooltipData.mappin);
  tooltipData.isCollection = controller.IsCollection();
  tooltipData.fastTravelEnabled = true;

  // Preserve the native Delamain pricing fields when its UI is enabled.
  if this.IsDelamainTaxiEnabled() {
    tooltipData.delamainTaxiEnabled = true;
    tooltipData.travelCost = this.GetTravelCost();
    let transactionSystem = GameInstance.GetTransactionSystem(this.m_player.GetGame());
    tooltipData.playerMoney = transactionSystem.GetItemQuantity(this.m_player, MarketSystem.Money());
  }

  this.m_tooltipController.SetData(tooltipType, tooltipData, this);
}

// Some map-pin tooltip variants hide the input row even when its data says
// Fast Travel is enabled. Only force it visible for a registered UDM proxy.
@wrapMethod(WorldMapTooltipController)
public func SetData(const data: script_ref<WorldMapTooltipData>, menu: ref<WorldMapMenuGameController>) -> Void {
  wrappedMethod(data, menu);
  if IsDefined(menu) && menu.UDMCanTeleportPin(Deref(data).mappin) {
    inkWidgetRef.SetVisible(this.m_inputInteractContainer, Deref(data).fastTravelEnabled);
  }
}

// The world map routes the mouse/keyboard fast-travel hold through
// HandlePressInput -> HandleHoldInput -> TryFastTravel. Intercept those
// NATIVE methods, not OnPressInput / OnHoldInput pointer callbacks.
// Setting the start flags lets the vanilla hold machinery run and also
// works with the optional Accidental Fast Travel Bug Fix's hold guard.
@wrapMethod(WorldMapMenuGameController)
private final func HandlePressInput(e: ref<inkPointerEvent>) -> Void {
  if e.IsAction(n"world_map_menu_fast_travel") && this.UDMCanTeleportSelected() {
    this.m_audioSystem.Play(n"ui_menu_item_crafting_start");
    this.m_startedFastTraveling = true;
    this.m_isPanning = true;
  }
  wrappedMethod(e);
}

// Let the game's normal hold progress and release/cancel rules run.
// At completion, redirect ONLY UndiscoverMe-owned pins to our teleport.
@wrapMethod(WorldMapMenuGameController)
private final func TryFastTravel() -> Void {
  if !this.UDMCanTeleportSelected() {
    wrappedMethod();
    return;
  }

  let pin = this.selectedMappin.GetMappin();
  let player = GameInstance.GetPlayerSystem(this.m_player.GetGame()).GetLocalPlayerMainGameObject();
  let facility = GameInstance.GetTeleportationFacility(this.m_player.GetGame());
  if !IsDefined(pin) || !IsDefined(player) || !IsDefined(facility) {
    return;
  }

  let target = pin.GetWorldPosition();
  target.Z += 1.5;
  facility.Teleport(player, target, Vector4.ToRotation(player.GetWorldForward()));
  this.m_audioSystem.Play(n"ui_menu_item_crafting_done");
  if IsDefined(this.m_menuEventDispatcher) {
    this.m_menuEventDispatcher.SpawnEvent(n"OnCloseHubMenu");
  }
}
