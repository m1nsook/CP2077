#include "workspace_layout.hpp"
#include <cstdlib>
#include <iostream>

#define CHECK(x) do { if(!(x)) { \
    std::cerr << "workspace layout regression: " #x " line " << __LINE__ << "\n"; \
    std::exit(EXIT_FAILURE); \
} } while(false)

int main()
{
    using m1core::workspace::Calculate;
    const auto full = Calculate(1920, 1080, true);
    CHECK(full.screen.width == 1920);
    CHECK(full.header.height == 56);
    CHECK(full.tabs.y == 56);
    CHECK(full.footer.y == 1024);
    CHECK(full.showSidebar && full.sidebar.width == 240);
    CHECK(full.content.x == 240);
    CHECK(full.body.height == 924);
    CHECK(full.content.height == full.sidebar.height);
    CHECK(full.content.x + full.content.width == full.screen.width);
    CHECK(full.body.y + full.body.height == full.footer.y);
    const auto profiles = Calculate(1920, 1080, false);
    CHECK(!profiles.showSidebar && profiles.content.width == 1920);
    const auto compact = Calculate(640, 480, true);
    CHECK(!compact.showSidebar && compact.content.x == 0);
    CHECK(compact.footer.y == 424);
    const auto scaled = Calculate(1920, 1080, true, 1.25f);
    CHECK(scaled.sidebar.width == 300);
    CHECK(scaled.header.height == 70);
    CHECK(Calculate(-1, 1080, true).screen.width == 0);
    CHECK(Calculate(0, 1080, true).body.height == 0);
}
