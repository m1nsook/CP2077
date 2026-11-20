#include "menu_shortcut.hpp"
#include <cstdlib>
#include <iostream>

#define CHECK(x) do { if (!(x)) { \
  std::cerr << "menu shortcut test failed at line " << __LINE__ << ": " #x "\n"; \
  std::exit(EXIT_FAILURE); } } while (false)

int main()
{
    constexpr int chosen = VK_F4;
    CHECK(m1core::ConsumeMenuKey(WM_KEYDOWN, VK_F4, chosen, false, false));
    CHECK(m1core::ConsumeMenuKey(WM_SYSKEYDOWN, VK_F4, chosen, false, false));
    CHECK(!m1core::ConsumeMenuKey(WM_KEYUP, VK_F4, chosen, false, false));
    CHECK(!m1core::ConsumeMenuKey(WM_SYSKEYUP, VK_F4, chosen, false, false));
    CHECK(!m1core::ConsumeMenuKey(WM_CHAR, VK_F4, chosen, false, false));
    CHECK(!m1core::ConsumeMenuKey(WM_KEYDOWN, VK_F4, chosen, true, false));
    CHECK(!m1core::ConsumeMenuKey(WM_SYSKEYDOWN, VK_F4, chosen, false, true)); // Alt+F4
    CHECK(!m1core::ConsumeMenuKey(WM_KEYDOWN, VK_F4, chosen, false, true)); // Ctrl/Shift
    CHECK(!m1core::ConsumeMenuKey(WM_KEYDOWN, VK_F11, chosen, false, false));
    CHECK(!m1core::ConsumeMenuKey(WM_KEYDOWN, VK_F4, 0, false, false));
    CHECK(!m1core::MayToggleMenu(false, false, true, false, false));
    CHECK(!m1core::MayToggleMenu(true, true, true, false, false));
    CHECK(!m1core::MayToggleMenu(true, false, true, true, false));
    CHECK(!m1core::MayToggleMenu(true, false, true, false, true)); // OS chords
    CHECK(m1core::MayToggleMenu(true, false, true, false, false));
    CHECK(m1core::IsWindowCloseMessage(WM_CLOSE));
    CHECK(m1core::IsWindowCloseMessage(WM_DESTROY));
    CHECK(m1core::IsWindowCloseMessage(WM_QUERYENDSESSION));
    CHECK(m1core::IsWindowCloseMessage(WM_ENDSESSION));
    CHECK(!m1core::IsWindowCloseMessage(WM_KEYDOWN));
    CHECK(!m1core::IsWindowCloseMessage(WM_SYSCOMMAND));
}
