#include "keybind_actions.hpp"
#include <cstdlib>
#include <iostream>

#define CHECK(e) do { if (!(e)) { \
  std::cerr << "keybind check failed: " #e << " on line " << __LINE__ << "\n"; \
  std::exit(EXIT_FAILURE); } } while (false)

int main()
{
    auto& events = m1core::KeybindActions::Get();
    events.Reset();
    // First poll never triggers a key already held before registration.
    CHECK(!events.Poll(1, 10, 0x75, true, true));
    CHECK(!events.Poll(1, 10, 0x75, true, true));
    CHECK(!events.Poll(1, 10, 0x75, false, true));
    CHECK(events.Poll(1, 10, 0x75, true, true));
    CHECK(!events.Poll(1, 10, 0x75, true, true)); // Repeat blocked.
    CHECK(!events.Poll(1, 10, 0x75, false, true));
    CHECK(events.Poll(1, 10, 0x75, true, true));

    // Same key can be consumed independently by different mod actions.
    CHECK(!events.Poll(2, 10, 0x75, false, true));
    CHECK(events.Poll(2, 10, 0x75, true, true));
    CHECK(!events.Poll(2, 10, 0x75, true, true));

    // Changing a binding primes the newly assigned key.
    CHECK(!events.Poll(1, 10, 0x76, true, true));
    CHECK(!events.Poll(1, 10, 0x76, false, true));
    CHECK(events.Poll(1, 10, 0x76, true, true));

    // Menu open or focus lost: neither produce action nor replay on return.
    CHECK(!events.Poll(1, 10, 0x76, true, false));
    CHECK(!events.Poll(1, 10, 0x76, true, true));
    CHECK(!events.Poll(1, 10, 0x76, false, true));
    CHECK(events.Poll(1, 10, 0x76, true, true));
    events.Invalidate();
    CHECK(!events.Poll(1, 10, 0x76, true, true));
    CHECK(!events.Poll(1, 10, 0x76, false, true));
    CHECK(events.Poll(1, 10, 0x76, true, true));

    // Reserved key and unbound settings never dispatch.
    CHECK(!events.Poll(1, 10, 0, true, true));
    CHECK(!events.Poll(1, 10, 0x7A, true, true));
    CHECK(!events.Poll(0, 10, 0x75, true, true));
    CHECK(!events.Poll(1, 0, 0x75, true, true));
    CHECK(!events.Poll(1, 10, 256, true, true));
    CHECK(!events.Poll(1, 10, 0x75, true, true)); // Seed after unbind.
    CHECK(!events.Poll(1, 10, 0x75, false, true));
    CHECK(events.Poll(1, 10, 0x75, true, true));
    // M5 buffered tap: press+release between two mod polls is not lost.
    events.Reset();
    CHECK(!events.Poll(11, 12, 0x77, false, true));
    events.ObserveKeyEvent(0x77, true);
    events.ObserveKeyEvent(0x77, true); // Win32 and RAWINPUT duplicate down.
    events.ObserveKeyEvent(0x77, true); // Keyboard auto-repeat.
    events.ObserveKeyEvent(0x77, false);
    CHECK(events.Poll(11, 12, 0x77, false, true));
    CHECK(!events.Poll(11, 12, 0x77, false, true));
    // A second mod watching the same VK gets its own event, after priming.
    CHECK(!events.Poll(22, 12, 0x77, false, true));
    events.ObserveKeyEvent(0x77, true);
    events.ObserveKeyEvent(0x77, false);
    CHECK(events.Poll(11, 12, 0x77, false, true));
    CHECK(events.Poll(22, 12, 0x77, false, true));
    CHECK(!events.Poll(22, 12, 0x77, false, true));

    // Held key may be seen by both polling and the queued edge; no double fire.
    events.ObserveKeyEvent(0x77, true);
    CHECK(events.Poll(11, 12, 0x77, true, true));
    CHECK(!events.Poll(11, 12, 0x77, true, true));
    events.ObserveKeyEvent(0x77, false);
    CHECK(!events.Poll(11, 12, 0x77, false, true));

    // Invalidate clears queued events and suppresses replay on alt-tab/F11.
    events.ObserveKeyEvent(0x77, true);
    events.ObserveKeyEvent(0x77, false);
    events.Invalidate();
    CHECK(!events.Poll(11, 12, 0x77, false, true));
    events.ObserveKeyEvent(0x77, true);
    events.ObserveKeyEvent(0x77, false);
    CHECK(events.Poll(11, 12, 0x77, false, true));
    CHECK(!events.Poll(11, 12, 0x77, false, false));
    events.ObserveKeyEvent(0x77, true);
    events.ObserveKeyEvent(0x77, false);
    CHECK(events.Poll(11, 12, 0x77, false, false));
    CHECK(!events.Poll(11, 12, 0x77, false, true)); // Forbidden event was dropped.

    // Rebinding and unbinding invalidates old event history.
    events.ObserveKeyEvent(0x77, true);
    events.ObserveKeyEvent(0x77, false);
    CHECK(!events.Poll(11, 12, 0, false, true));
    CHECK(!events.Poll(11, 12, 0x77, false, true));
    events.ObserveKeyEvent(0x77, true);
    events.ObserveKeyEvent(0x77, false);
    CHECK(events.Poll(11, 12, 0x77, false, true));
    // F11 remains protected even when input is observed.
    events.ObserveKeyEvent(0x7A, true);
    events.ObserveKeyEvent(0x7A, false);
    CHECK(!events.Poll(11, 12, 0x7A, false, true));
    events.Reset();
    CHECK(!events.Poll(1, 10, 0x75, true, true));
}
