module MisoSkipBreach

// Skip Breach: native REDscript + M1 Arc settings and guarded UI selector.
// First experimental integration for Cyberpunk 2077 2.5. Disabled by default.
// Does not use CET, Lua, forced daemon fulfillment or simulated mouse clicks.

// First puzzle UI event pump. Game-time DelayCallback polling could stall
// while the vanilla hacking clock waits for the first input.
public class M1ArcSkipBreachPulseEvent extends Event {
  public let session: Int32;
  public let captureSerial: Int32;
}

public class M1ArcSkipBreachSystem extends ScriptableSystem {
  private let m_running: Bool;
  private let m_controller: wref<HackingMinigameGameController>;
  private let m_session: Int32;
  private let m_captureSerial: Int32;
  private let m_handledCapture: Int32;
  private let m_plannedCapture: Int32;
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
  private let m_bestMask: Int32;
  private let m_bestPath: array<Int32>;
  private let m_lastDiagnostic: Int32;
  private let m_waitForBoardTicks: Int32;
  private let m_firstCellRejects: Int32;
  private let m_pulseQueued: Bool;
  private let m_pulseCount: Int32;
  private let m_inProgressTicks: Int32;
  private let m_unknownTicks: Int32;
  private let m_terminalTicks: Int32;
  private let m_earlyTwoLogged: Bool;
  private let m_ackRetries: Int32;

  // Only log transitions. Do not fill REDscript.log every 100ms.
  private func Report(code: Int32, message: String) -> Void {
    if this.m_lastDiagnostic == code {
      return;
    };
    this.m_lastDiagnostic = code;
    M1Arc_BreachTrace(message);
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
    this.m_plannedCapture = -1;
    this.m_gameState = 0;
    M1Arc_BreachTrace("ScriptableSystem attached; UI event driven");
  }

  private func OnDetach() -> Void {
    this.m_running = false;
    this.StopExecution();
    this.m_controller = null;
  }

  // UISystem events continue to dispatch when the game's timer waits for
  // an initial Breach selection. Limit event requests, and tag each one with
  // the exact session and generation to prevent cross-puzzle input.
  private func ScheduleNext() -> Void {
    if !this.m_running || !IsDefined(this.m_controller) || this.m_pulseQueued {
      return;
    };
    if this.m_pulseCount >= 1500 {
      this.Report(57, "UI event safety limit exceeded; leaving puzzle manual");
      this.StopExecution();
      this.m_handledCapture = this.m_captureSerial;
      return;
    };
    let ui: ref<UISystem> = GameInstance.GetUISystem(this.GetGameInstance());
    if !IsDefined(ui) {
      this.Report(58, "UI event system unavailable; leaving puzzle manual");
      return;
    };
    let event: ref<M1ArcSkipBreachPulseEvent> = new M1ArcSkipBreachPulseEvent();
    event.session = this.m_session;
    event.captureSerial = this.m_captureSerial;
    this.m_pulseQueued = true;
    this.m_pulseCount += 1;
    ui.QueueEvent(event);
  }

  public func ReceivePulse(ctrl: wref<HackingMinigameGameController>,
                           session: Int32, capture: Int32) -> Void {
    // Stale events do not clear newer in-flight pulses.
    if this.m_controller != ctrl || this.m_session != session
      || this.m_captureSerial != capture {
      return;
    };
    this.m_pulseQueued = false;
    this.Tick();
  }

  public func BeginSession(ctrl: wref<HackingMinigameGameController>, size: Int32, buffer: Int32, gameState: Int32) -> Void {
    // A generation observer may run before the enclosing OnInitialize
    // wrapper reaches BeginSession. Preserve an already-captured board
    // rather than resetting its first (and potentially only) generation.
    if this.m_controller == ctrl {
      this.m_dimension = size;
      this.m_bufferSize = buffer;
      if gameState == 1 {
        this.m_gameState = gameState;
      };
      return;
    };
    this.StopExecution();
    this.m_session += 1;
    this.m_controller = ctrl;
    this.m_dimension = size;
    this.m_bufferSize = buffer;
    this.m_gameState = gameState;
    this.m_ready = false;
    this.m_plannedCapture = -1;
    this.m_cancelled = false;
    this.m_waitForBoardTicks = 0;
    this.m_firstCellRejects = 0;
    this.m_pulseQueued = false;
    this.m_pulseCount = 0;
    this.m_inProgressTicks = 0;
    this.m_unknownTicks = 0;
    this.m_terminalTicks = 0;
    this.m_earlyTwoLogged = false;
    // Per-session selection bookkeeping: 0 means no cell was selected yet.
    this.m_selectionSerial = 0;
    this.m_pendingSerial = 0;
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
    this.m_inProgressTicks = 0;
  }

  // Live blackboard value, independent of any (possibly stale) listener callback.
  private func LiveState() -> Int32 {
    let board: ref<IBlackboard> = GameInstance.GetBlackboardSystem(
      this.GetGameInstance()).Get(GetAllBlackboardDefs().HackingMinigame);
    if !IsDefined(board) {
      return -1;
    };
    return board.GetInt(GetAllBlackboardDefs().HackingMinigame.State);
  }

  public func RecordGameState(ctrl: wref<HackingMinigameGameController>, state: Int32) -> Void {
    if this.m_controller != ctrl {
      return;
    };
    // FIX: on the first breach after launch the blackboard reports Succeeded(2)
    // ~90ms after InProgress and keeps it there, yet the puzzle still accepts
    // cell selections (logs: cell 0 acknowledged while live=2) and only ends
    // later with Failed(3) when the timer runs out. A genuine success cannot
    // happen before any cell is selected, nor before our planned path has been
    // fully acknowledged, so a 2 in either situation must not abort the run.
    if state == 2 && (this.m_selectionSerial == 0
        || (this.m_active && this.m_nextStep < ArraySize(this.m_path))) {
      if !this.m_earlyTwoLogged {
        this.m_earlyTwoLogged = true;
        M1Arc_BreachTrace("[diag] ignoring Succeeded(2): no cell selected yet or planned path incomplete; puzzle still live");
      };
      this.ScheduleNext();
      return;
    };
    // Event-based trace, not frame polling: identify first-screen Unknown
    // versus interactive/terminal states in the *same* native log file.
    if state != this.m_gameState {
      M1Arc_BreachTrace("[diag] state " + IntToString(this.m_gameState)
        + " -> " + IntToString(state) + ", live blackboard=" + IntToString(this.LiveState()));
      this.m_inProgressTicks = 0;
      this.m_terminalTicks = 0;
      if state == 0 {
        M1Arc_BreachTrace("Minigame entered Unknown state (first interaction pending)");
      } else {
        if state == 1 {
          M1Arc_BreachTrace("Minigame entered InProgress state");
        } else {
          M1Arc_BreachTrace("Minigame entered terminal state (success/failure), raw=" + IntToString(state));
        };
      };
    };
    this.m_gameState = state;
    // Unknown=0 before the first cell may be a valid UI state.
    // 2/3 are terminal Success/Failure and must always abort.
    if state > 1 {
      this.StopExecution();
    };
    // FIX: never let a terminal value kill the pulse pump. The first breach after
    // launch reports state 2 in the same millisecond as InProgress while the puzzle
    // keeps running (it later ended with 3). Keep pulsing so Tick() can re-read the
    // live blackboard and recover; the pump stops itself after a bounded time.
    this.ScheduleNext();
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
    // Matching OnPositionSelected callback observed for the requested cell.
    // Record every step, including later puzzles, not only state changes.
    M1Arc_BreachTrace("Matching OnPositionSelected callback received");
    this.m_waiting = false;
    this.m_nextStep += 1;
    this.m_waitTicks = 0;
    this.m_delayTicks = 2; // allow a later UI frame for animation
    this.ScheduleNext();
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
      // On a fresh game UI, the passive generation rule can fire before
      // OnInitialize's system handshake. Adopt only an unowned session;
      // never steal an active puzzle belonging to another controller.
      if IsDefined(this.m_controller) || !IsDefined(ctrl) {
        return;
      };
      let board: ref<IBlackboard> = GameInstance.GetBlackboardSystem(
        this.GetGameInstance()).Get(GetAllBlackboardDefs().HackingMinigame);
      if !IsDefined(board) {
        return;
      };
      let defaults: MinigameData = FromVariant<MinigameData>(
        board.GetVariant(GetAllBlackboardDefs().HackingMinigame.MinigameDefaults));
      this.BeginSession(ctrl,Cast<Int32>(size),Cast<Int32>(defaults.bufferSize),
                        board.GetInt(GetAllBlackboardDefs().HackingMinigame.State));
      this.Report(22, "First-board capture initialized its own session");
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
    this.m_pulseQueued = false;
    this.m_pulseCount = 0;
    this.m_inProgressTicks = 0;
    this.m_plannedCapture = -1;
    if this.m_ready {
      this.Report(20, "Puzzle captured; native readiness and solver are next");
      this.ScheduleNext();
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

  // Source-informed native REDscript planner. The supplied AutoBreach
  // solver.lua uses daemon match masks, ranked search, optimistic bounds
  // and shortest-path reconstruction. It is a Lua 5.1 implementation,
  // so we reproduce the ALGORITHM here without requiring CET or copying
  // Lua into the game. The full Lua automaton/memo solver is more complete;
  // this bounded native planner is not claimed to be an exact port.

  // Each displayed daemon is a bit of a four-bit mask (A=1 .. D=8).
  // Match contiguous sequences, including those that overlap.
  private func MatchMask(codes: array<Int32>) -> Int32 {
    let mask: Int32 = 0;
    let bit: Int32 = 1;
    let daemon: Int32 = 0;
    while daemon < ArraySize(this.m_daemonLengths) {
      let length: Int32 = this.m_daemonLengths[daemon];
      let offset: Int32 = this.m_daemonOffsets[daemon];
      let start: Int32 = 0;
      while start + length <= ArraySize(codes) {
        let matched: Bool = true;
        let j: Int32 = 0;
        while j < length {
          if codes[start + j] != this.m_daemonTokens[offset + j] {
            matched = false;
            break;
          };
          j += 1;
        };
        if matched {
          mask += bit;
          break;
        };
        start += 1;
      };
      bit *= 2;
      daemon += 1;
    };
    return mask;
  }

  private func CountMask(mask: Int32) -> Int32 {
    let count: Int32 = 0;
    while mask > 0 {
      count += mask % 2;
      mask /= 2;
    };
    return count;
  }

  // Rank matches by number of completed daemons, then display-order mask,
  // matching the working reference's rankMask() priority.
  private func RankMask(mask: Int32) -> Int32 {
    return this.CountMask(mask) * 16 + mask;
  }

  // Longest suffix of selections that is also this daemon's prefix.
  // Supports overlapping daemon sequences without requiring an exact
  // concatenation or spending every buffer slot on a single daemon.
  private func PrefixLength(codes: array<Int32>, daemon: Int32) -> Int32 {
    let length: Int32 = this.m_daemonLengths[daemon];
    let offset: Int32 = this.m_daemonOffsets[daemon];
    let limit: Int32 = Min(length, ArraySize(codes));
    while limit > 0 {
      let ok: Bool = true;
      let i: Int32 = 0;
      while i < limit {
        if codes[ArraySize(codes) - limit + i] != this.m_daemonTokens[offset + i] {
          ok = false;
          break;
        };
        i += 1;
      };
      if ok {
        return limit;
      };
      limit -= 1;
    };
    return 0;
  }

  // A geometry-free optimistic upper bound. If a daemon needs more
  // tokens than remaining buffer slots, it cannot improve this branch.
  // Allow overlaps freely: that makes this bound safe, not pessimistic.
  private func UpperScore(codes: array<Int32>, mask: Int32, remaining: Int32) -> Int32 {
    let result: Int32 = this.CountMask(mask);
    let daemon: Int32 = 0;
    let bit: Int32 = 1;
    while daemon < ArraySize(this.m_daemonLengths) {
      if (mask / bit) % 2 == 0 {
        let needed: Int32 = this.m_daemonLengths[daemon] - this.PrefixLength(codes,daemon);
        if needed <= remaining {
          result += 1;
        };
      };
      bit *= 2;
      daemon += 1;
    };
    return result;
  }

  // Ordering heuristic from the working solver's rank-first candidate
  // ordering: completed daemon masks first, then partial prefixes.
  private func CandidatePriority(codes: array<Int32>) -> Int32 {
    let mask: Int32 = this.MatchMask(codes);
    let progress: Int32 = 0;
    let daemon: Int32 = 0;
    let bit: Int32 = 1;
    while daemon < ArraySize(this.m_daemonLengths) {
      if (mask / bit) % 2 == 0 {
        progress += this.PrefixLength(codes,daemon);
      };
      bit *= 2;
      daemon += 1;
    };
    return this.RankMask(mask) * 256 + progress;
  }

  private func Consider(path: array<Int32>, mask: Int32) -> Void {
    let score: Int32 = this.CountMask(mask);
    if score == 0 {
      return;
    };
    let newRank: Int32 = this.RankMask(mask);
    let bestRank: Int32 = this.RankMask(this.m_bestMask);
    if newRank > bestRank
      || (newRank == bestRank
          && (ArraySize(this.m_bestPath) == 0
              || ArraySize(path) < ArraySize(this.m_bestPath))) {
      this.m_bestMask = mask;
      this.m_bestScore = score;
      this.m_bestPath = path;
    };
  }

  private func Search(path: array<Int32>, codes: array<Int32>,
                      row: Int32, col: Int32, pickColumn: Bool,
                      depthLimit: Int32) -> Void {
    if this.m_nodeCount >= 50000 {
      this.m_budgetHit = true;
      return;
    };
    this.m_nodeCount += 1;
    let mask: Int32 = this.MatchMask(codes);
    this.Consider(path,mask);
    if this.m_bestScore >= ArraySize(this.m_daemonLengths)
      || ArraySize(path) >= depthLimit
      || ArraySize(path) >= ArraySize(this.m_grid) {
      return;
    };

    let remaining: Int32 = depthLimit - ArraySize(path);
    if this.UpperScore(codes,mask,remaining) < this.m_bestScore {
      return; // no completion can outrank the best existing mask
    };

    // 7x7 game boards have at most seven candidates per alternation.
    // Stable rank-first insertion ensures useful daemon prefixes win over
    // unproductive cells before we spend the bounded 50k-state budget.
    let candidates: array<Int32>;
    let priorities: array<Int32>;
    let i: Int32 = 0;
    while i < this.m_dimension {
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
        let preview: array<Int32> = codes;
        ArrayPush(preview,this.m_grid[flat]);
        let priority: Int32 = this.CandidatePriority(preview);
        let position: Int32 = ArraySize(priorities);
        while position > 0 && priority > priorities[position - 1] {
          position -= 1;
        };
        ArrayInsert(candidates,position,flat);
        ArrayInsert(priorities,position,priority);
      };
      i += 1;
    };

    let index: Int32 = 0;
    while index < ArraySize(candidates) && !this.m_budgetHit {
      let flat: Int32 = candidates[index];
      let nextPath: array<Int32> = path;
      let nextCodes: array<Int32> = codes;
      ArrayPush(nextPath,flat);
      ArrayPush(nextCodes,this.m_grid[flat]);
      this.Search(nextPath,nextCodes,flat / this.m_dimension,
                  flat % this.m_dimension,!pickColumn,depthLimit);
      if this.m_bestScore >= ArraySize(this.m_daemonLengths) {
        return;
      };
      index += 1;
    };
  }

  private func Plan() -> Bool {
    ArrayClear(this.m_bestPath);
    this.m_bestMask = 0;
    this.m_bestScore = 0;
    this.m_nodeCount = 0;
    this.m_budgetHit = false;

    // Iterative deepening, similar to the working solver's shortest
    // completion-depth phase: inspect all first-row starts at each depth,
    // instead of exhausting one arbitrary starting cell first.
    let limit: Int32 = 1;
    let maxDepth: Int32 = Min(this.m_bufferSize,ArraySize(this.m_grid));
    while limit <= maxDepth && !this.m_budgetHit {
      let col: Int32 = 0;
      while col < this.m_dimension && !this.m_budgetHit {
        let path: array<Int32>;
        let codes: array<Int32>;
        ArrayPush(path,col);
        ArrayPush(codes,this.m_grid[col]);
        this.Search(path,codes,0,col,true,limit);
        if this.m_bestScore >= ArraySize(this.m_daemonLengths) {
          return true;
        };
        col += 1;
      };
      limit += 1;
    };
    if this.m_budgetHit && this.m_bestScore > 0 {
      this.Report(31,"Search budget reached; using best verified legal path (may not be optimal)");
    };
    return this.m_bestScore > 0 && ArraySize(this.m_bestPath) > 0;
  }

  private func StopExecution() -> Void {
    this.m_active = false;
    this.m_waiting = false;
    // No timer pause is introduced by Skip Breach. We intentionally leave
    // the vanilla countdown untouched, especially during gig stealth.
    this.m_paused = false;
    ArrayClear(this.m_path);
  }

  // Source: 2.5 hacking_minigame.inkwidget GridContainer/Grid.
  // Coordinates come from live widgets, never fixed 1080p pixels.
  private func ResolveClickPoint(row: Int32, col: Int32) -> Vector2 {
    let invalid: Vector2 = Vector2(-1.0,-1.0);
    if !IsDefined(this.m_controller) || this.m_dimension < 2
      || row < 0 || col < 0 ||
      row >= this.m_dimension || col >= this.m_dimension {
      return invalid;
    };
    let root: wref<inkCompoundWidget> = this.m_controller.GetRootCompoundWidget();
    if !IsDefined(root) {
      return invalid;
    };
    let grid: wref<inkUniformGrid> = root.GetWidgetByPathName(
      n"AllParts/Top/NetworkCodeMapAndButton/wrapper/NetworkCodeMap/NetworkCodeMap/ContentWithBorder/ContentContainer/GridContainer/Grid") as inkUniformGrid;
    if !IsDefined(grid)
      || grid.GetWrappingWidgetCount() != Cast<Uint32>(this.m_dimension)
      || grid.GetNumChildren() < this.m_dimension * this.m_dimension {
      return invalid;
    };
    let first: wref<inkWidget> = grid.GetWidgetByIndex(0);
    let right: wref<inkWidget> = grid.GetWidgetByIndex(1);
    let down: wref<inkWidget> = grid.GetWidgetByIndex(this.m_dimension);
    let cell: wref<inkWidget> = grid.GetWidgetByIndex(
      row * this.m_dimension + col);
    if !IsDefined(first) || !IsDefined(right) || !IsDefined(down)
      || !IsDefined(cell) || !cell.IsVisible() ||
      !first.IsVisible() || !right.IsVisible() || !down.IsVisible() {
      return invalid;
    };
    let rootRect: RectF = GetScreenPosition(root);
    let firstRect: RectF = GetScreenPosition(first);
    let rightRect: RectF = GetScreenPosition(right);
    let downRect: RectF = GetScreenPosition(down);
    let cellRect: RectF = GetScreenPosition(cell);
    let width: Float = rootRect.Right - rootRect.Left;
    let height: Float = rootRect.Bottom - rootRect.Top;
    let stepX: Float = (rightRect.Left+rightRect.Right -
      firstRect.Left-firstRect.Right)*0.5;
    let stepY: Float = (downRect.Top+downRect.Bottom -
      firstRect.Top-firstRect.Bottom)*0.5;
    let cellWidth: Float = firstRect.Right - firstRect.Left;
    let cellHeight: Float = firstRect.Bottom - firstRect.Top;
    if width < 640.0 || height < 480.0
      || cellWidth < 8.0 || cellHeight < 8.0
      || stepX < cellWidth*0.5 || stepY < cellHeight*0.5 {
      return invalid;
    };
    let x: Float = (cellRect.Left+cellRect.Right)*0.5;
    let y: Float = (cellRect.Top+cellRect.Bottom)*0.5;
    let expectedX: Float = (firstRect.Left+firstRect.Right)*0.5 +
      Cast<Float>(col)*stepX;
    let expectedY: Float = (firstRect.Top+firstRect.Bottom)*0.5 +
      Cast<Float>(row)*stepY;
    if x < expectedX-cellWidth*0.4 || x > expectedX+cellWidth*0.4
      || y < expectedY-cellHeight*0.4 || y > expectedY+cellHeight*0.4
      || x <= rootRect.Left || x >= rootRect.Right
      || y <= rootRect.Top || y >= rootRect.Bottom {
      return invalid;
    };
    return Vector2((x-rootRect.Left)/width,(y-rootRect.Top)/height);
  }

  private func TryStart() -> Void {
    if !M1Arc_GetBool("m1nsook.skipbreach","enabled") {
      return;
    };
    if !this.m_ready {
      this.Report(41, "No captured, usable puzzle data; vanilla puzzle remains playable");
      return;
    };
    // The FIRST board can stay Unknown until its first accepted grid selection.
    // No guessed intro timer; use live rendered grid as a readiness guard.
    if this.m_gameState < 0 || this.m_gameState > 1 {
      this.Report(43, "Breach is terminal; leaving puzzle manual");
      return;
    };
    // FIX: the intro animation keeps the state at Unknown (0) for ~2s while the
    // grid is already laid out and visible, so a visible grid is NOT proof the
    // game accepts input. Wait for InProgress; only fall back to Unknown after
    // ~600 UI updates in case a build really stays Unknown until first input.
    if this.m_gameState == 0 && this.m_unknownTicks < 600 {
      this.Report(60, "Waiting for InProgress; grid is visible but intro is still running");
      return;
    };
    if this.m_gameState == 1 && this.m_inProgressTicks < 20 {
      this.Report(59, "Waiting for stable InProgress before native input");
      return;
    };
    let visual: Vector2 = this.ResolveClickPoint(0,0);
    if visual.X < 0.0 {
      this.Report(42, "Waiting for rendered grid cells on FIRST puzzle");
      return;
    };
    if this.m_cancelled || !IsDefined(this.m_controller) {
      this.Report(48, "Breach controller unavailable or already cancelled");
      this.m_handledCapture = this.m_captureSerial;
      return;
    };
    if !M1Arc_BreachInputAvailable() {
      this.Report(44, "Native input not ready: check selector version, game focus and M1 Arc overlay in RED4ext log");
      return;
    };
    // First-time UI controllers can become available AFTER board capture.
    // Do not consume the only stealth puzzle before there is a place to
    // submit its first cell. Retry on the SAME board while it is untouched.
    let logic: wref<inkLogicController> =
      this.m_controller.GetControllerByType(n"gameuiHackingMinigameLogicController");
    if !IsDefined(logic) {
      this.Report(47, "Waiting for hacking UI controller (same puzzle, no exit needed)");
      return;
    };
    // Solve exactly once per generated puzzle, even if the selected
    // first cell needs more UI layout time than cell (0,0). Replanning on
    // every UI pulse can consume the entire initial input window.
    if this.m_plannedCapture != this.m_captureSerial {
      if !this.Plan() {
        this.Report(45, "No rewarding path found; leaving original puzzle manual");
        this.m_handledCapture = this.m_captureSerial;
        return;
      };
      this.m_plannedCapture = this.m_captureSerial;
    };
    let firstIndex: Int32 = this.m_bestPath[0];
    let actualFirst: Vector2 = this.ResolveClickPoint(
      firstIndex / this.m_dimension, firstIndex % this.m_dimension);
    if actualFirst.X < 0.0 {
      this.Report(62, "Waiting for the SOLVER'S first cell to have valid live grid coordinates");
      return;
    };
    this.m_handledCapture = this.m_captureSerial;
    this.m_firstCellRejects = 0;
    this.m_ackRetries = 0;
    this.m_lockedSession = this.m_session;
    this.m_lockedCapture = this.m_captureSerial;
    this.m_path = this.m_bestPath;
    this.m_nextStep = 0;
    this.m_waiting = false;
    this.m_waitTicks = 0;
    this.m_delayTicks = 0;
    // Crucial first-attempt safety: DO NOT pause the vanilla timer here.
    // The first launch can stall between preparation and input while the
    // controller is still becoming interactive. The existing DelaySystem
    // tick must continue; only game-acknowledged cell selections advance.
    this.m_active = true;
    this.Report(46, "Valid path ready; starting native grid selections with vanilla timer running");
  }

  public func Tick() -> Void {
    if !this.m_running {
      return;
    };
    if IsDefined(this.m_controller) {
      // The state listener is delayed. In particular, the FIRST opening
      // can initialize while the Blackboard still says Unknown (0).
      // Read the live state every tick rather than requiring a second
      // minigame to deliver a new callback. This never changes game state.
      let stateBoard: ref<IBlackboard> = GameInstance.GetBlackboardSystem(
        this.GetGameInstance()).Get(GetAllBlackboardDefs().HackingMinigame);
      if IsDefined(stateBoard) {
        let observedState: Int32 = stateBoard.GetInt(
          GetAllBlackboardDefs().HackingMinigame.State);
        if observedState != this.m_gameState {
          this.RecordGameState(this.m_controller,observedState);
        };
      };
    };
    if this.m_gameState == 1 && this.m_inProgressTicks < 20 {
      this.m_inProgressTicks += 1;
    };
    if this.m_gameState == 0 && this.m_unknownTicks < 1000 {
      this.m_unknownTicks += 1;
    };
    if this.m_gameState > 1 && this.m_terminalTicks < 300 {
      this.m_terminalTicks += 1;
    };
    if this.m_active {
      if !this.m_ready || this.m_cancelled
        || this.m_gameState < 0 || this.m_gameState > 1
        || this.m_session != this.m_lockedSession
        || this.m_captureSerial != this.m_lockedCapture
        || !M1Arc_BreachInputAvailable()
        || !M1Arc_GetBool("m1nsook.skipbreach","enabled") {
        this.StopExecution();
      } else {
        if this.m_waiting {
          this.m_waitTicks += 1;
          if this.m_waitTicks > 120 {
            // Frame-driven events, not 0.1-second DelayCallback ticks.
            // Only the FIRST cell may be re-sent, and only if RecordSelection saw
            // no OnPositionSelected at all since dispatch (serial unchanged), so
            // the board is provably untouched. Later cells are never repeated.
            if this.m_nextStep == 0 && this.m_selectionSerial == this.m_pendingSerial
              && this.m_ackRetries < 3 {
              this.m_ackRetries += 1;
              this.m_waiting = false;
              this.m_waitTicks = 0;
              this.m_delayTicks = 30;
              this.Report(63, "No acknowledgement for first cell; board untouched, retrying");
            } else {
              this.Report(51, "No native selection acknowledgement after 120 UI updates");
              this.StopExecution();
            };
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
                if this.m_nextStep == 0 && this.m_firstCellRejects < 20 {
                  this.m_firstCellRejects += 1;
                  this.m_delayTicks = 2;
                  this.Report(52, "Waiting for first-try hacking UI controller; will retry without exiting puzzle");
                } else {
                  this.Report(55, "Hacking UI controller disappeared; leaving manual puzzle");
                  this.StopExecution();
                };
              } else {
                let target: Vector2 = this.ResolveClickPoint(row,col);
                if target.X < 0.0 {
                  // Layout is transient, NOT a native click rejection.
                  // Do not send (-1,-1) to M1 Arc or spend the first-cell
                  // retry allowance before any input has been attempted.
                  this.m_delayTicks = 2;
                  this.Report(61, "Waiting for the exact next grid cell to be visible; no click sent");
                } else {
                  this.m_pendingSerial = this.m_selectionSerial;
                  this.m_waiting = true;
                  this.m_waitTicks = 0;
                  // Win32 SendInput dispatched but never produced a real
                  // OnPositionSelected on either board (08:46 user log).
                  // Restore the game-native selector, which previously
                  // acknowledged every cell on the SECOND puzzle. Unlike
                  // that older revision, first-board execution now starts
                  // from an Unknown state once live grid layout is ready.
                  if !M1Arc_BreachSelectCell(
                    logic,Vector2(Cast<Float>(row),Cast<Float>(col))) {
                    this.m_waiting = false;
                    if this.m_nextStep == 0 && this.m_firstCellRejects < 20 {
                      // FALSE means native input was rejected before dispatch.
                      // A TRUE dispatch without acknowledgement is NEVER
                      // retried: it might have changed the actual puzzle.
                      this.m_firstCellRejects += 1;
                      this.m_delayTicks = 2;
                      this.Report(53, "Native grid input not dispatched; retrying same untouched board");
                    } else {
                      this.Report(56, "Native grid selector rejected cell; leaving puzzle manual");
                      this.StopExecution();
                    };
                  };
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
        if this.m_waitForBoardTicks == 120 {
          this.Report(54, "No board captured after 120 UI updates; manual puzzle remains playable");
        };
      };
      // Preserve the first generated board until ink grid geometry is ready.
      // A real pointer input starts vanilla interaction (Unknown is valid).
      if this.m_ready && this.m_captureSerial != this.m_handledCapture
        && this.m_gameState >= 0 && this.m_gameState <= 1
        && M1Arc_GetBool("m1nsook.skipbreach","enabled") {
        // TryStart consumes this generation only AFTER controller readiness,
        // or if it is truly unsolvable/previously touched. Early UI startup
        // must not cause a permanent skip until the player exits the puzzle.
        this.TryStart();
      };
    };
    // No idle background poller. Deliver one pulse at a time while
    // waiting for Unknown -> InProgress on the SAME captured board.
    // Do not stop the pulse or mark the capture handled during intro.
    if this.m_active
      || (this.m_gameState > 1 && this.m_terminalTicks < 300
          && this.m_captureSerial != this.m_handledCapture)
      || (this.m_ready
      && this.m_captureSerial != this.m_handledCapture
      && !this.m_cancelled
      && this.m_gameState >= 0 && this.m_gameState <= 1
      && M1Arc_GetBool("m1nsook.skipbreach","enabled")) {
      this.ScheduleNext();
    };
  }
}

public class M1ArcSkipBreachCaptureRule extends MinigameGenerationRule {
  protected func OnProcessRule(size: Uint32, out grid: array<array<GridCell>>) -> Bool {
    M1Arc_BreachTrace("Passive generation observer invoked");
    let ctrl: wref<HackingMinigameGameController> = this.minigameController;
    if !IsDefined(ctrl) {
      M1Arc_BreachTrace("Capture skipped: missing minigame controller");
      return true;
    };
    let player: wref<GameObject> = ctrl.GetPlayerControlledObject();
    if !IsDefined(player) {
      M1Arc_BreachTrace("Capture skipped: player object unavailable");
      return true;
    };
    let sys: ref<M1ArcSkipBreachSystem> = M1ArcSkipBreachSystem.Get(player.GetGame());
    if IsDefined(sys) {
      sys.Capture(ctrl,size,grid,ctrl.GetProgramsChains(),ctrl.GetUnlockablePrograms());
    } else {
      M1Arc_BreachTrace("Capture skipped: ScriptableSystem unavailable");
    };
    return true;
  }
}

@wrapMethod(HackingMinigameGameController)
protected cb func OnInitialize() -> Bool {
  // First-run diagnostics: differentiate a hook that never ran from
  // a missing ScriptableSystem or a missing generation callback.
  M1Arc_BreachTrace("Hacking UI OnInitialize entered");
  wrappedMethod();
  let player: wref<GameObject> = this.GetPlayerControlledObject();
  let sys: ref<M1ArcSkipBreachSystem>;
  if IsDefined(player) {
    sys = M1ArcSkipBreachSystem.Get(player.GetGame());
  };
  let board: ref<IBlackboard> = this.GetBlackboardSystem().Get(GetAllBlackboardDefs().HackingMinigame);
  if !IsDefined(board) {
    M1Arc_BreachTrace("No HackingMinigame Blackboard at OnInitialize");
    return true;
  };
  let data: MinigameData = FromVariant<MinigameData>(
    board.GetVariant(GetAllBlackboardDefs().HackingMinigame.MinigameDefaults));
  if IsDefined(sys) {
    sys.BeginSession(this,Cast<Int32>(data.gridSize),Cast<Int32>(data.bufferSize),
                     board.GetInt(GetAllBlackboardDefs().HackingMinigame.State));
  } else {
    // System attachment may lag the FIRST UI initialization. Do not
    // withhold the capture rule merely because Get() was not ready yet.
    M1Arc_BreachTrace("System lookup delayed; observer still installed");
  };
  // Install the observer regardless of settings AND system readiness.
  // It never edits grid cells; Capture() can adopt an unowned session.
  let rule: ref<M1ArcSkipBreachCaptureRule> = new M1ArcSkipBreachCaptureRule();
  rule.minigameController = this;
  ArrayPush(data.rules,rule);
  board.SetVariant(GetAllBlackboardDefs().HackingMinigame.MinigameDefaults,data,true);
  M1Arc_BreachTrace("Passive capture rule installed on initial board defaults");
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

@addMethod(HackingMinigameGameController)
protected cb func OnM1ArcSkipBreachPulseEvent(evt: ref<M1ArcSkipBreachPulseEvent>) -> Bool {
  let player: wref<GameObject> = this.GetPlayerControlledObject();
  if !IsDefined(player) || !IsDefined(evt) {
    return false;
  };
  let sys: ref<M1ArcSkipBreachSystem> = M1ArcSkipBreachSystem.Get(player.GetGame());
  if IsDefined(sys) {
    sys.ReceivePulse(this,evt.session,evt.captureSerial);
  };
  return true;
}
