#include "startup_gate.hpp"

#include <cstdlib>
#include <iostream>

#define CHECK(x) do { if (!(x)) { \
  std::cerr << "startup gate check failed on line " << __LINE__ << ": " #x "\n"; \
  std::exit(EXIT_FAILURE); } } while (false)

int main()
{
    m1core::StartupGate gate;
    // Before V is spawned, do not install a window subclass, DXGI hooks,
    // cursor detours, or intercept the title screen's "Press to Continue".
    CHECK(!gate.MayInstallOverlayHooks());
    CHECK(gate.MarkPlayerAttached());
    CHECK(gate.MayInstallOverlayHooks());
    CHECK(!gate.MarkPlayerAttached());
    CHECK(gate.MayInstallOverlayHooks());

    // A new process/game startup must begin in the safe, passive state.
    gate.Reset();
    CHECK(!gate.MayInstallOverlayHooks());
    CHECK(gate.MarkPlayerAttached());
    CHECK(gate.MayInstallOverlayHooks());
}
