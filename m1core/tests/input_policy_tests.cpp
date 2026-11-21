#include "../native/input.hpp"
#include <cassert>
#include <iostream>

int main()
{
    using m1core::MustPassThroughWindows;
    using m1core::OwnsInput;
    assert(MustPassThroughWindows(WM_CLOSE,0,0));
    assert(MustPassThroughWindows(WM_DESTROY,0,0));
    assert(MustPassThroughWindows(WM_NCDESTROY,0,0));
    assert(MustPassThroughWindows(WM_SYSCOMMAND,SC_CLOSE,0));
    assert(MustPassThroughWindows(WM_SYSCOMMAND,SC_CLOSE|1,0));
    assert(!MustPassThroughWindows(WM_SYSCOMMAND,SC_MINIMIZE,0));
    assert(MustPassThroughWindows(WM_SYSKEYDOWN,VK_F4,
                                   static_cast<LPARAM>(1ull<<29)));
    assert(!MustPassThroughWindows(WM_KEYDOWN,VK_F4,
                                    static_cast<LPARAM>(1ull<<29)));
    assert(!MustPassThroughWindows(WM_SYSKEYDOWN,VK_F4,0));
    assert(!MustPassThroughWindows(WM_SYSKEYDOWN,VK_F3,
                                    static_cast<LPARAM>(1ull<<29)));
    assert(!OwnsInput(false,false));
    assert(!OwnsInput(false,true));
    assert(!OwnsInput(true,false));
    assert(OwnsInput(true,true));
    std::cout<<"Native input ownership/OS shutdown policy passed\n";
}
