#include "../core/src/input/MouseClick.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>

#define REQUIRE(test) do { if (!(test)) throw std::runtime_error("MouseClick test failed: " #test); } while (false)

using m1core::input::ClickClient;
using m1core::input::ClickResult;
using m1core::input::ClientPoint;
using m1core::input::MouseBackend;
using m1core::input::MouseButton;

namespace {
class FakeBackend final : public MouseBackend
{
public:
    ClickResult check{ClickResult::Sent};
    bool cursorOK{true};
    bool inputsOK{true};
    bool partial{false};
    int resolves{};
    int moves{};
    int injects{};
    POINT at{};
    std::vector<DWORD> flags;

    ClickResult Resolve(HWND, ClientPoint p, POINT& point) override
    {
        ++resolves;
        point = {p.x + 100, p.y + 200};
        return check;
    }

    bool MoveCursor(POINT point) override
    {
        ++moves;
        at = point;
        return cursorOK;
    }

    UINT Inject(const INPUT* input, UINT count) override
    {
        ++injects;
        for (UINT i=0; i<count; ++i)
        {
            REQUIRE(input[i].type == INPUT_MOUSE);
            flags.push_back(input[i].mi.dwFlags);
        }
        if (partial && count == 2) return 1;
        return inputsOK ? count : 0;
    }
};
}

int main()
{
    const auto target = reinterpret_cast<HWND>(0x1234);
    const ClientPoint location{40, 60};
    {
        FakeBackend fake;
        REQUIRE(ClickClient(fake, target, location, MouseButton::Left, false)
                == ClickResult::NotAllowed);
        REQUIRE(fake.resolves == 0 && fake.moves == 0 && fake.injects == 0);
    }
    {
        FakeBackend fake;
        fake.check = ClickResult::NotForeground;
        REQUIRE(ClickClient(fake, target, location, MouseButton::Left, true)
                == ClickResult::NotForeground);
        REQUIRE(fake.resolves == 1 && fake.moves == 0 && fake.injects == 0);
    }
    {
        FakeBackend fake;
        fake.check = ClickResult::OutsideClient;
        REQUIRE(ClickClient(fake, target, location, MouseButton::Left, true)
                == ClickResult::OutsideClient);
        REQUIRE(fake.moves == 0 && fake.injects == 0);
    }
    {
        FakeBackend fake;
        fake.cursorOK = false;
        REQUIRE(ClickClient(fake, target, location, MouseButton::Left, true)
                == ClickResult::CursorFailed);
        REQUIRE(fake.resolves == 1 && fake.moves == 1 && fake.injects == 0);
    }
    {
        FakeBackend fake;
        REQUIRE(ClickClient(fake, target, location, MouseButton::Left, true)
                == ClickResult::Sent);
        REQUIRE(fake.at.x == 140 && fake.at.y == 260);
        REQUIRE(fake.injects == 1 && fake.flags.size() == 2);
        REQUIRE(fake.flags[0] == MOUSEEVENTF_LEFTDOWN);
        REQUIRE(fake.flags[1] == MOUSEEVENTF_LEFTUP);
    }
    {
        FakeBackend fake;
        REQUIRE(ClickClient(fake, target, location, MouseButton::Right, true)
                == ClickResult::Sent);
        REQUIRE(fake.flags[0] == MOUSEEVENTF_RIGHTDOWN);
        REQUIRE(fake.flags[1] == MOUSEEVENTF_RIGHTUP);
    }
    {
        FakeBackend fake;
        fake.partial = true;
        REQUIRE(ClickClient(fake, target, location, MouseButton::Left, true)
                == ClickResult::InjectionFailed);
        REQUIRE(fake.injects == 2);
        REQUIRE(fake.flags.back() == MOUSEEVENTF_LEFTUP);
    }
    {
        FakeBackend fake;
        fake.inputsOK = false;
        REQUIRE(ClickClient(fake, target, location, MouseButton::Left, true)
                == ClickResult::InjectionFailed);
        REQUIRE(fake.injects == 2);
    }
    std::cout << "m1core internal mouse click: safety gate, focus, button sequences and release fallback passed\n";
}
