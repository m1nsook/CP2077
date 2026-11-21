#include "../native/input.hpp"
#include "check.hpp"
#include <iostream>

int main()
{
    using m1core::MustPassThroughWindows;
    using m1core::OwnsInput;
    REQUIRE(MustPassThroughWindows(WM_CLOSE,0,0));
    REQUIRE(MustPassThroughWindows(WM_DESTROY,0,0));
    REQUIRE(MustPassThroughWindows(WM_NCDESTROY,0,0));
    REQUIRE(MustPassThroughWindows(WM_SYSCOMMAND,SC_CLOSE,0));
    REQUIRE(MustPassThroughWindows(WM_SYSCOMMAND,SC_CLOSE|1,0));
    REQUIRE(!MustPassThroughWindows(WM_SYSCOMMAND,SC_MINIMIZE,0));
    REQUIRE(MustPassThroughWindows(WM_SYSKEYDOWN,VK_F4,
                                   static_cast<LPARAM>(1ull<<29)));
    REQUIRE(!MustPassThroughWindows(WM_KEYDOWN,VK_F4,
                                    static_cast<LPARAM>(1ull<<29)));
    REQUIRE(!MustPassThroughWindows(WM_SYSKEYDOWN,VK_F4,0));
    REQUIRE(!MustPassThroughWindows(WM_SYSKEYDOWN,VK_F3,
                                    static_cast<LPARAM>(1ull<<29)));
    REQUIRE(!OwnsInput(false,false));
    REQUIRE(!OwnsInput(false,true));
    REQUIRE(!OwnsInput(true,false));
    REQUIRE(OwnsInput(true,true));
    using m1core::CanCaptureInput;
    REQUIRE(!CanCaptureInput(true,true,false)); // GPU backend unavailable
    REQUIRE(!CanCaptureInput(false,true,true));
    REQUIRE(!CanCaptureInput(true,false,true)); // game lost foreground
    REQUIRE(CanCaptureInput(true,true,true));
    std::cout<<"Native input ownership/OS shutdown policy passed\n";
}
