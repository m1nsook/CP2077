#include "ui_components_math.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>

#define CHECK(x) do { if (!(x)) { \
    std::cerr << "ui components test failed: " #x " at " << __LINE__ << "\n"; \
    std::exit(EXIT_FAILURE); \
} } while (false)

int main()
{
    using m1core::ui::SnapSliderValue;
    CHECK(SnapSliderValue(-3, 0, 20, 1) == 0);
    CHECK(SnapSliderValue(3, 0, 20, 1) == 20);
    CHECK(SnapSliderValue(0.50, 0, 20, 1) == 10);
    CHECK(SnapSliderValue(0.01, 0, 20, 1) == 0);
    CHECK(SnapSliderValue(0.076, 0, 20, 1) == 2);
    CHECK(std::abs(SnapSliderValue(0.5, 0.5, 5.0, 0.01) - 2.75) < 0.0001);
    CHECK(SnapSliderValue(0.5, 10, 10, 1) == 10);
    CHECK(SnapSliderValue(0.5, 10, 2, 1) == 10);
    CHECK(SnapSliderValue(NAN, 0, 20, 1) == 0);
    CHECK(SnapSliderValue(0.5, 0, 20, 0) == 10);
}
