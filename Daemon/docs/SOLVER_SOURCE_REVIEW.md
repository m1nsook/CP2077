# What the supplied working AutoBreach solver actually does

Reviewed the user-uploaded **AutoBreach 0.4.0** archive directly,
especially `bin/x64/plugins/cyber_engine_tweaks/mods/AutoBreach/modules/solver.lua`
(1,020 lines), plus `modules/coordinates.lua` and `r6/scripts/AutoBreach/autobreach.reds`.

This reference is **not just a simple DFS**:

- It normalizes the column-major Lua grid, while the actual game
  `GridCell` matrix and `OnPositionSelected(Vector2)` are (row,column).
- It builds an Aho-Corasick finite automaton of up to **four** daemon
  sequences; overlapping matches accrue in a four-bit A/B/C/D mask.
- The primary optimization ranks a subset by the count of completed
  daemons, then the identity of the completed subset.
- It prunes with a relaxed (geometry-free) upper bound and a dominance
  memo keyed by visited positions, last position and automaton state.
- It separately searches for the earliest achievable completion depth
  and then reconstructs a deterministic path.
- The executor is a separate runtime that monitors controller events,
  generation and selection serials; its successful path selection does
  **not** prove that the separate initial game controller will always
  register in time.

The earlier Skip Breach REDscript solver ignored almost all this
structure; it exhausted 50,000 depth-first states in input order and
could prefer an arbitrary starting cell over a short solution.

## Current native REDscript adaptation

We have now implemented **source-informed ranked daemon masks, overlap
matching, longest suffix/prefix progress, optimistic buffer upper bounds,
best-first candidate ordering, and iterative depth limits** directly
inside the REDscript consumer. No CET, Lua, OCR or `AutoBreachInput.dll`
is required by this adaptation.

**This is not a literal/full port of the reference's Aho-Corasick +
64-bit visited-set memo or its three-stage optimality proof.** It retains
a 50,000-state guard; on budget exhaustion a legal best-found path is
returned but optimality is not guaranteed. Do not claim equivalence
to the supplied Lua solver. The new candidate ranking and heuristics
require actual REDscript compilation and gameplay evaluation against
repeatable puzzles.

The first-undetected-attempt activation issue is **separate** from solver
quality. The user's cancel/reentry generates a *new* matrix and can
irreversibly fail a mission. Never recommend that as a way to activate
Skip Breach.
