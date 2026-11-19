#include "input_gate.hpp"

#include <cstdlib>
#include <iostream>

#define CHECK(expr) do { if (!(expr)) { \
    std::cerr << "Input gate check failed: " #expr << " at line " << __LINE__ << "\n"; \
    std::exit(EXIT_FAILURE); \
} } while (false)

int main()
{
    m1core::InputGate input;

    CHECK(!input.IsOpen());
    CHECK(!input.MayCaptureInput());
    CHECK(!input.Toggle()); // No renderer or focus: cannot steal input.
    CHECK(!input.Escape());

    input.SetRendererReady(true);
    CHECK(!input.Toggle()); // A ready renderer is not enough.
    input.SetGameFocused(true);
    CHECK(input.Toggle());
    CHECK(input.IsOpen());
    CHECK(input.MayCaptureInput());
    CHECK(input.Escape()); // ESC must close regardless of UI state.
    CHECK(!input.IsOpen());
    CHECK(!input.MayCaptureInput());
    CHECK(!input.Escape());

    CHECK(input.Toggle());
    CHECK(!input.Toggle()); // Hotkey toggles closed.
    CHECK(!input.MayCaptureInput());

    CHECK(input.Toggle());
    input.SetGameFocused(false); // Alt-tab / focus loss.
    CHECK(!input.IsOpen());
    CHECK(!input.MayCaptureInput());
    CHECK(!input.Toggle());

    input.SetGameFocused(true);
    CHECK(input.Toggle());
    input.SetOtherOverlayActive(true); // E.g. CET overlay.
    CHECK(!input.IsOpen());
    CHECK(!input.MayCaptureInput());
    CHECK(!input.Toggle());

    input.SetOtherOverlayActive(false);
    CHECK(input.Toggle());
    input.SetRendererReady(false); // Device loss / resize.
    CHECK(!input.IsOpen());
    CHECK(!input.MayCaptureInput());
    CHECK(!input.Toggle());

    return 0;
}
