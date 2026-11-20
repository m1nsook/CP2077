#include "startup_gate.hpp"

#include <cstdlib>
#include <iostream>

#define CHECK(x) do { if (!(x)) { \
  std::cerr << "startup gate check failed on line " << __LINE__ << ": " #x "\n"; \
  std::exit(EXIT_FAILURE); } } while (false)

int main()
{
    m1core::StartupGate gate;
    // Before V is spawned, the early DXGI/window bridge is passive; no
    // gameplay input interception, visible menu or cursor detours.
    CHECK(!gate.GameplayReady());
    CHECK(gate.MarkPlayerAttached());
    CHECK(gate.GameplayReady());
    CHECK(!gate.MarkPlayerAttached());
    CHECK(gate.GameplayReady());

    // A new process/game startup must begin in the safe, passive state.
    gate.Reset();
    CHECK(!gate.GameplayReady());
    CHECK(gate.MarkPlayerAttached());
    CHECK(gate.GameplayReady());
}
