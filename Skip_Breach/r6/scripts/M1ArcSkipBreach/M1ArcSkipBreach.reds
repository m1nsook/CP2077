module MisoSkipBreach

// Skip Breach: native REDscript + M1 Arc settings and guarded UI selector.
// First experimental integration for Cyberpunk 2077 2.5. Disabled by default.
// Does not use CET, Lua, forced daemon fulfillment or simulated mouse clicks.

public class M1ArcSkipBreachSystem extends ScriptableSystem {
  private let m_running: Bool;
  private let m_controller: wref<HackingMinigameGameController>;
  private let m_session: Int32;
  private let m_captureSerial: Int32;
  private let m_handledCapture: Int32;
  private let m_selectionSerial: Int32;
  private let m_lastRow: Int32;
  private let m_lastColumn: Int32;
  private let m_dimension: Int32;
  private let m_bufferSize: Int32;
  private let m_gameState: Int32;
  private let m_grid: array<Int32>;
  private let m_daemonOffsets: array<Int32>;
  private let m_daemonLengths: array<Int32>;
  private let m_daemonTokens: array<Int32>;
  private let m_ready: Bool;
  private let m_cancelled: Bool;

  private let m_active: Bool;
  private let m_paused: Bool;
  private let m_path: array<Int32>;
  private let m_nextStep: Int32;
  private let m_waiting: Bool;
  private let m_pendingSerial: Int32;
  private let m_waitTicks: Int32;
  private let m_delayTicks: Int32;
  private let m_lockedSession: Int32;
  private let m_lockedCapture: Int32;

  private let m_nodeCount: Int32;
  private let m_budgetHit: Bool;
  private let m_bestScore: Int32;
  private let m_bestPath: array<Int32>;
  private let m_lastDiagnostic: Int32;
  private let m_waitForBoardTicks: Int32;

  // Only log transitions. Do not fill REDscript.log every 100ms.
  private func Report(code: Int32, message: String) -> Void {
    if this.m_lastDiagnostic == code {
      return;
    };
    this.m_lastDiagnostic = code;
    LogChannel(n"DEBUG", "[Skip Breach] " + message);
  }

  public static func Get(gi: GameInstance) -> ref<M1ArcSkipBreachSystem> {
    // REDscript module classes have fully qualified RTTI names. An
    // unqualified Get(n"M1ArcSkipBreachSystem") resolves to no system,
    // preventing ALL controller hooks, capture, solver and input.
    return GameInstance.GetScriptableSystemsContainer(gi).Get(n"MisoSkipBreach.M1ArcSkipBreachSystem") as M1ArcSkipBreachSystem;
  }

  private func OnAttach() -> Void {
    this.m_running = true;
    this.m_handledCapture = -1;
    this.m_gameState = 0;
    LogChannel(n"DEBUG", "[Skip Breach] ScriptableSystem attached");
    this.ScheduleNext();
  }

  private func OnDetach() -> Void {
    this.m_running = false;
    this.StopExecution();
    this.m_controller = null;
  }

  private func ScheduleNext() -> Void {
    if !this.m_running {
      return;
    };
    let tick: ref<M1ArcSkipBreachTick> = new M1ArcSkipBreachTick();
    tick.owner = this;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(tick, 0.10, false);
  }

  public func BeginSession(ctrl: wref<HackingMinigameGameController>, size: Int32, buffer: Int32, gameState: Int32) -> Void {
    this.StopExecution();
    this.m_session += 1;
    this.m_controller = ctrl;
    this.m_dimension = size;
    this.m_bufferSize = buffer;
    this.m_gameState = gameState;
    this.m_ready = false;
    this.m_cancelled = false;
    this.m_waitForBoardTicks = 0;
    this.m_lastDiagnostic = 0;
    this.Report(10, "Breach controller initialized; waiting for puzzle capture");
    ArrayClear(this.m_grid);
    ArrayClear(this.m_daemonOffsets);
    ArrayClear(this.m_daemonLengths);
    ArrayClear(this.m_daemonTokens);
  }

  public func EndSession(ctrl: wref<HackingMinigameGameController>) -> Void {
    if this.m_controller != ctrl {
      return;
    };
    this.StopExecution();
    this.m_controller = null;
    this.m_ready = false;
    this.m_gameState = 0;
  }

  public func RecordGameState(ctrl: wref<HackingMinigameGameController>, state: Int32) -> Void {
    if this.m_controller != ctrl {
      return;
    };
    this.m_gameState = state;
    if state != 1 {
      this.StopExecution();
    };
  }

  public func RecordSelection(ctrl: wref<HackingMinigameGameController>, position: Vector2) -> Void {
    if this.m_controller != ctrl {
      return;
    };
    this.m_lastRow = Cast<Int32>(position.X);
    this.m_lastColumn = Cast<Int32>(position.Y);
    this.m_selectionSerial += 1;
    if !this.m_active {
      // This is a confirmed MATRIX cell selection, unlike a generic
      // UI pointer click that can also be emitted while the puzzle opens.
      // Never start automated input on an already touched puzzle.
      this.m_cancelled = true;
      this.Report(11, "Player already selected a matrix cell; leaving this board manual");
      return;
    };
    if !this.m_waiting || this.m_nextStep >= ArraySize(this.m_path) {
      this.Report(12, "Selection event arrived out of order; stopping");
      this.StopExecution();
      return;
    };
    let expected: Int32 = this.m_path[this.m_nextStep];
    if this.m_selectionSerial <= this.m_pendingSerial
      || this.m_lastRow != expected / this.m_dimension
      || this.m_lastColumn != expected % this.m_dimension {
      this.Report(13, "Selection acknowledgement mismatch; stopping");
      this.StopExecution();
      return;
    };
    this.m_waiting = false;
    this.m_nextStep += 1;
    this.m_waitTicks = 0;
    this.m_delayTicks = 1; // allow one frame for animation
    if this.m_nextStep >= ArraySize(this.m_path) {
      // Final selection was accepted. The game's original callback must
      // still execute; it is responsible for programs/rewards/closing UI.
      this.Report(14, "All planned cells acknowledged; game applies rewards");
      this.StopExecution();
    };
  }

  public func Capture(ctrl: wref<HackingMinigameGameController>, size: Uint32,
                      grid: array<array<GridCell>>,
                      chains: array<CharactersChain>,
                      programs: array<UnlockableProgram>) -> Void {
    if this.m_controller != ctrl {
      return;
    };
    this.StopExecution();
    this.m_dimension = Cast<Int32>(size);
    ArrayClear(this.m_grid);
    ArrayClear(this.m_daemonOffsets);
    ArrayClear(this.m_daemonLengths);
    ArrayClear(this.m_daemonTokens);

    let row: Int32 = 0;
    while row < this.m_dimension {
      let col: Int32 = 0;
      while col < this.m_dimension {
        ArrayPush(this.m_grid, grid[row][col].rarityValue);
        col += 1;
      };
      row += 1;
    };

    // Programs have display order; CharactersChain.ownerId locates tokens.
    let owner: Int32 = 0;
    while owner < ArraySize(programs) && ArraySize(this.m_daemonLengths) < 4 {
      let chain: Int32 = this.FindChainForOwner(chains, owner);
      if !programs[owner].hidden
        && !programs[owner].isFulfilled
        && chain >= 0
        && !chains[chain].isFulfilled
        && ArraySize(chains[chain].rarities) > 0 {
        ArrayPush(this.m_daemonOffsets, ArraySize(this.m_daemonTokens));
        ArrayPush(this.m_daemonLengths, ArraySize(chains[chain].rarities));
        let token: Int32 = 0;
        while token < ArraySize(chains[chain].rarities) {
          ArrayPush(this.m_daemonTokens, Cast<Int32>(chains[chain].rarities[token]));
          token += 1;
        };
      };
      owner += 1;
    };
    this.m_ready = this.m_dimension >= 2 && this.m_dimension <= 8
      && this.m_bufferSize > 0 && this.m_bufferSize <= 12
      && ArraySize(this.m_grid) == this.m_dimension * this.m_dimension
      && ArraySize(this.m_daemonLengths) > 0;
    // Every generation increments the serial, even if it cannot be solved.
    this.m_captureSerial += 1;
    if this.m_ready {
      this.Report(20, "Puzzle captured; native readiness and solver are next");
    } else {
      this.Report(21, "Puzzle capture incomplete; no valid grid or daemons");
    };
  }

  private func FindChainForOwner(chains: array<CharactersChain>, owner: Int32) -> Int32 {
    let i: Int32 = 0;
    while i < ArraySize(chains) {
      if chains[i].ownerId == owner {
        return i;
      };
      i += 1;
    };
    return -1;
  }

  private func Consider(path: array<Int32>, codes: array<Int32>) -> Void {
    let score: Int32 = 0;
    let daemon: Int32 = 0;
    while daemon < ArraySize(this.m_daemonLengths) {
      let length: Int32 = this.m_daemonLengths[daemon];
      let offset: Int32 = this.m_daemonOffsets[daemon];
      let start: Int32 = 0;
      let matched: Bool = false;
      while start + length <= ArraySize(codes) && !matched {
        let j: Int32 = 0;
        matched = true;
        while j < length {
          if codes[start + j] != this.m_daemonTokens[offset + j] {
            matched = false;
            break;
          };
          j += 1;
        };
        start += 1;
      };
      if matched {
        score += 1;
      };
      daemon += 1;
    };
    if score > this.m_bestScore
      || (score == this.m_bestScore && score > 0
          && (ArraySize(this.m_bestPath) == 0
              || ArraySize(path) < ArraySize(this.m_bestPath))) {
      this.m_bestScore = score;
      this.m_bestPath = path;
    };
  }

  private func Search(path: array<Int32>, codes: array<Int32>,
                      row: Int32, col: Int32, pickColumn: Bool) -> Void {
    if this.m_nodeCount >= 50000 {
      this.m_budgetHit = true;
      return;
    };
    this.m_nodeCount += 1;
    this.Consider(path,codes);
    if ArraySize(path) >= this.m_bufferSize
      || ArraySize(path) >= ArraySize(this.m_grid)
      || this.m_bestScore >= ArraySize(this.m_daemonLengths) {
      return;
    };

    let i: Int32 = 0;
    while i < this.m_dimension && !this.m_budgetHit {
      let nextRow: Int32 = row;
      let nextCol: Int32 = col;
      if pickColumn {
        nextRow = i;
      } else {
        nextCol = i;
      };
      let flat: Int32 = nextRow * this.m_dimension + nextCol;
      let visited: Bool = false;
      let j: Int32 = 0;
      while j < ArraySize(path) {
        if path[j] == flat {
          visited = true;
          break;
        };
        j += 1;
      };
      if !visited {
        let nextPath: array<Int32> = path;
        let nextCodes: array<Int32> = codes;
        ArrayPush(nextPath, flat);
        ArrayPush(nextCodes, this.m_grid[flat]);
        this.Search(nextPath,nextCodes,nextRow,nextCol,!pickColumn);
      };
      i += 1;
    };
  }

  private func Plan() -> Bool {
    ArrayClear(this.m_bestPath);
    this.m_bestScore = 0;
    this.m_nodeCount = 0;
    this.m_budgetHit = false;
    let col: Int32 = 0;
    while col < this.m_dimension && !this.m_budgetHit {
      let path: array<Int32>;
      let codes: array<Int32>;
      ArrayPush(path,col); // first selection must be row 0
      ArrayPush(codes,this.m_grid[col]);
      this.Search(path,codes,0,col,true);
      col += 1;
    };
    // Every candidate is a legal native row/column path. If the search
    // budget expires, prefer a VERIFIED, rewarding best-found path over
    // silently discarding every successful candidate. It may not be optimal.
    if this.m_budgetHit && this.m_bestScore > 0 {
      this.Report(31, "Search budget reached; executing best legal path found (not guaranteed optimal)");
    };
    return this.m_bestScore > 0 && ArraySize(this.m_bestPath) > 0;
  }

  private func StopExecution() -> Void {
    this.m_active = false;
    this.m_waiting = false;
    if this.m_paused && IsDefined(this.m_controller) {
      this.m_controller.ResumeTheTimer();
    };
    this.m_paused = false;
    ArrayClear(this.m_path);
  }

  private func TryStart() -> Void {
    if !M1Arc_GetBool("m1nsook.skipbreach","enabled") {
      return;
    };
    if !this.m_ready {
      this.Report(41, "No captured, usable puzzle data; vanilla puzzle remains playable");
      return;
    };
    if this.m_gameState != 1 {
      this.Report(42, "Breach game state is not interactive (expected state 1)");
      return;
    };
    if this.m_cancelled || !IsDefined(this.m_controller) {
      this.Report(43, "Breach controller unavailable or already cancelled");
      return;
    };
    if !M1Arc_BreachInputAvailable() {
      this.Report(44, "Native selector unavailable: check m1arc entry in RED4ext log for version/signature reason");
      return;
    };
    if !this.Plan() {
      this.Report(45, "No rewarding path found within solver budget; manual puzzle remains playable");
      return;
    };
    this.m_lockedSession = this.m_session;
    this.m_lockedCapture = this.m_captureSerial;
    this.m_path = this.m_bestPath;
    this.m_nextStep = 0;
    this.m_waiting = false;
    this.m_waitTicks = 0;
    this.m_delayTicks = 1;
    this.m_controller.PauseTheTimer();
    this.m_paused = true;
    this.m_active = true;
    this.Report(46, "Valid path found; timer paused; starting cell selections");
  }

  public func Tick() -> Void {
    if !this.m_running {
      return;
    };
    if this.m_active {
      if !this.m_ready || this.m_cancelled || this.m_gameState != 1
        || this.m_session != this.m_lockedSession
        || this.m_captureSerial != this.m_lockedCapture
        || !M1Arc_BreachInputAvailable()
        || !M1Arc_GetBool("m1nsook.skipbreach","enabled") {
        this.StopExecution();
      } else {
        if this.m_waiting {
          this.m_waitTicks += 1;
          if this.m_waitTicks > 15 {
            // Never repeat a click without a native selection event.
            this.Report(51, "Game did not acknowledge selected cell after 1.5s; stopping");
            this.StopExecution();
          };
        } else {
          if this.m_delayTicks > 0 {
            this.m_delayTicks -= 1;
          } else {
            if this.m_nextStep < ArraySize(this.m_path) {
              let cell: Int32 = this.m_path[this.m_nextStep];
              let row: Int32 = cell / this.m_dimension;
              let col: Int32 = cell % this.m_dimension;
              let logic: wref<inkLogicController> =
                this.m_controller.GetControllerByType(n"gameuiHackingMinigameLogicController");
              if !IsDefined(logic) {
                this.Report(52, "Hacking logic controller not found; stopping");
                this.StopExecution();
              } else {
                this.m_pendingSerial = this.m_selectionSerial;
                this.m_waiting = true;
                this.m_waitTicks = 0;
                if !M1Arc_BreachSelectCell(logic, Vector2(Cast<Float>(row),Cast<Float>(col))) {
                  this.Report(53, "Native controller rejected cell selection (focus/type/coordinate check)");
                  this.StopExecution();
                };
              };
            };
          };
        };
      };
    } else {
      if M1Arc_GetBool("m1nsook.skipbreach","enabled")
        && IsDefined(this.m_controller) && !this.m_ready {
        this.m_waitForBoardTicks += 1;
        if this.m_waitForBoardTicks == 20 {
          this.Report(54, "No board captured after 2s. Enable Skip Breach BEFORE opening the minigame.");
        };
      };
      // A captured puzzle stays available when a user enables the setting
      // mid-puzzle. Previously a disabled setting marked the capture as
      // handled, forcing the player to exit/re-enter (mission failure).
      if this.m_captureSerial != this.m_handledCapture
        && this.m_gameState == 1
        && M1Arc_GetBool("m1nsook.skipbreach","enabled") {
        this.m_handledCapture = this.m_captureSerial;
        this.TryStart();
      };
    };
    this.ScheduleNext();
  }
}

public class M1ArcSkipBreachTick extends DelayCallback {
  public let owner: wref<M1ArcSkipBreachSystem>;
  public func Call() -> Void {
    if IsDefined(this.owner) {
      this.owner.Tick();
    };
  }
}

public class M1ArcSkipBreachCaptureRule extends MinigameGenerationRule {
  protected func OnProcessRule(size: Uint32, out grid: array<array<GridCell>>) -> Bool {
    let ctrl: wref<HackingMinigameGameController> = this.minigameController;
    if !IsDefined(ctrl) || !IsDefined(ctrl.GetPlayerControlledObject()) {
      return true;
    };
    let sys: ref<M1ArcSkipBreachSystem> = M1ArcSkipBreachSystem.Get(
      ctrl.GetPlayerControlledObject().GetGame());
    if IsDefined(sys) {
      sys.Capture(ctrl,size,grid,ctrl.GetProgramsChains(),ctrl.GetUnlockablePrograms());
    };
    return true;
  }
}

@wrapMethod(HackingMinigameGameController)
protected cb func OnInitialize() -> Bool {
  wrappedMethod();
  let player: wref<GameObject> = this.GetPlayerControlledObject();
  let sys: ref<M1ArcSkipBreachSystem>;
  if IsDefined(player) {
    sys = M1ArcSkipBreachSystem.Get(player.GetGame());
  };
  if IsDefined(sys) {
    let board: ref<IBlackboard> = this.GetBlackboardSystem().Get(GetAllBlackboardDefs().HackingMinigame);
    let data: MinigameData = FromVariant<MinigameData>(
      board.GetVariant(GetAllBlackboardDefs().HackingMinigame.MinigameDefaults));
    sys.BeginSession(this,Cast<Int32>(data.gridSize),Cast<Int32>(data.bufferSize),
                     board.GetInt(GetAllBlackboardDefs().HackingMinigame.State));
    // Always install our passive generation observer, even while the
    // M1 Arc toggle is OFF. It does not change any cell or daemon.
    // First-puzzle capture and later opt-in must NEVER require exiting
    // and re-entering a gig's one-shot Breach Protocol attempt.
    let rule: ref<M1ArcSkipBreachCaptureRule> = new M1ArcSkipBreachCaptureRule();
    rule.minigameController = this;
    ArrayPush(data.rules,rule);
    board.SetVariant(GetAllBlackboardDefs().HackingMinigame.MinigameDefaults,data,true);
  };
}

@wrapMethod(HackingMinigameGameController)
protected cb func OnUninitialize() -> Bool {
  let player: wref<GameObject> = this.GetPlayerControlledObject();
  if IsDefined(player) {
    let sys: ref<M1ArcSkipBreachSystem> = M1ArcSkipBreachSystem.Get(player.GetGame());
    if IsDefined(sys) {
      sys.EndSession(this);
    };
  };
  wrappedMethod();
}

@wrapMethod(HackingMinigameGameController)
protected cb func OnPositionSelected(position: Vector2) -> Bool {
  let player: wref<GameObject> = this.GetPlayerControlledObject();
  if IsDefined(player) {
    let sys: ref<M1ArcSkipBreachSystem> = M1ArcSkipBreachSystem.Get(player.GetGame());
    if IsDefined(sys) {
      sys.RecordSelection(this,position);
    };
  };
  wrappedMethod(position);
}

@wrapMethod(HackingMinigameGameController)
protected cb func OnGameStateChanged(value: Int32) -> Bool {
  let player: wref<GameObject> = this.GetPlayerControlledObject();
  if IsDefined(player) {
    let sys: ref<M1ArcSkipBreachSystem> = M1ArcSkipBreachSystem.Get(player.GetGame());
    if IsDefined(sys) {
      sys.RecordGameState(this,value);
    };
  };
  wrappedMethod(value);
}

// Do not intercept generic UI click events: they can fire while the
// puzzle opens and are not proof that a matrix cell was selected.
// OnPositionSelected is the only reliable manual-selection signal.
