#include "raw_input.hpp"

#include <cstdlib>
#include <iostream>

#define CHECK(x) do { if (!(x)) { \
    std::cerr << "raw input test failed: " #x << " at " << __LINE__ << "\n"; \
    std::exit(EXIT_FAILURE); \
} } while (false)

int main()
{
    m1core::RawSample key{};
    key.type = RIM_TYPEKEYBOARD;
    key.vkey = 'A';
    CHECK(!m1core::ForwardToGame(key));
    key.keyFlags = RI_KEY_BREAK;
    CHECK(m1core::ForwardToGame(key));

    m1core::RawSample mouse{};
    mouse.type = RIM_TYPEMOUSE;
    mouse.dx = 10;
    mouse.dy = -3;
    CHECK(!m1core::ForwardToGame(mouse));
    mouse.buttonFlags = RI_MOUSE_LEFT_BUTTON_DOWN;
    CHECK(!m1core::ForwardToGame(mouse));
    mouse.buttonFlags = RI_MOUSE_LEFT_BUTTON_UP;
    CHECK(m1core::ForwardToGame(mouse));
    mouse.buttonFlags = RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_LEFT_BUTTON_UP;
    CHECK(!m1core::ForwardToGame(mouse));
    mouse.buttonFlags = RI_MOUSE_WHEEL;
    CHECK(!m1core::ForwardToGame(mouse));

    m1core::RawSample hid{};
    hid.type = RIM_TYPEHID;
    CHECK(m1core::ForwardToGame(hid));
}
