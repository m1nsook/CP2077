#include "overlay_ui_policy.hpp"
#include <cstdlib>
#include <iostream>

#define CHECK(x) do { if (!(x)) { \
    std::cerr << "overlay UI policy failed: " #x " line " << __LINE__ << "\n"; \
    std::exit(EXIT_FAILURE); \
} } while (false)

int main()
{
    CHECK(!m1core::ShouldDrawOverlayCursor(false, false));
    CHECK(!m1core::ShouldDrawOverlayCursor(false, true));
    CHECK(!m1core::ShouldDrawOverlayCursor(true, false));
    CHECK(m1core::ShouldDrawOverlayCursor(true, true));
    CHECK(m1core::NeedsCompactSetup(0));
    CHECK(!m1core::NeedsCompactSetup('Q'));
}
