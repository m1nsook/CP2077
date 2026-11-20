#include "pointer_transform.hpp"

#include <cstdlib>
#include <iostream>

#define CHECK(x) do { if (!(x)) { \
    std::cerr << "pointer transform test failed: " #x " line " << __LINE__ << "\n"; \
    std::exit(EXIT_FAILURE); \
} } while(false)

int main()
{
    using m1core::MapClientPointer;
    const auto native = MapClientPointer(400, 200, 1920, 1080, 1920, 1080);
    CHECK(native.valid && native.x == 400 && native.y == 200);
    // 1280x720 window rendering at 1920x1080; the pointer remains aligned.
    const auto scaled = MapClientPointer(400, 200, 1280, 720, 1920, 1080);
    CHECK(scaled.valid && scaled.x == 600 && scaled.y == 300);
    const auto downscaled = MapClientPointer(600, 300, 1920, 1080, 1280, 720);
    CHECK(downscaled.valid && downscaled.x == 400 && downscaled.y == 200);
    CHECK(MapClientPointer(0, 0, 1920, 1080, 1920, 1080).valid);
    CHECK(!MapClientPointer(-1, 0, 1920, 1080, 1920, 1080).valid);
    CHECK(!MapClientPointer(1920, 0, 1920, 1080, 1920, 1080).valid);
    CHECK(!MapClientPointer(0, 1080, 1920, 1080, 1920, 1080).valid);
    CHECK(!MapClientPointer(10, 10, 0, 1080, 1920, 1080).valid);
    CHECK(!MapClientPointer(10, 10, 1920, 0, 1920, 1080).valid);
    CHECK(!MapClientPointer(10, 10, 1920, 1080, 0, 1080).valid);
}
