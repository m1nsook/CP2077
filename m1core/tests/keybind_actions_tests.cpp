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
    events.Reset();
    CHECK(!events.Poll(1, 10, 0x75, true, true));
}
