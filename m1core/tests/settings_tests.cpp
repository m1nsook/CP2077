#include "settings.hpp"

#include <cstdlib>
#include <iostream>

// Do not use assert: Release builds define NDEBUG and would skip the tests.
#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << "Check failed: " #expression << " (line " << __LINE__ << ")\\n"; \
    std::exit(EXIT_FAILURE); \
} } while (false)
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

int main()
{
    const auto file = std::filesystem::temp_directory_path() / "m1core-registry-test.json";
    std::error_code ignored;
    std::filesystem::remove(file, ignored);

    {
        m1core::Registry registry(file);
        CHECK(registry.Load());
        CHECK(registry.RegisterBool(1, 1, true));
        CHECK(registry.RegisterBool(2, 1, false));
        CHECK(registry.RegisterInt(1, 2, 5, 0, 10));
        CHECK(registry.RegisterFloat(1, 3, 1.0F, 0.5F, 3.0F));
        CHECK(!registry.RegisterBool(1, 2, false)); // Type collision.
        CHECK(!registry.RegisterFloat(1, 4, std::numeric_limits<float>::quiet_NaN(), 0, 1));
        CHECK(registry.SetBool(1, 1, false));
        CHECK(registry.SetInt(1, 2, 999));
        CHECK(registry.SetFloat(1, 3, -100.0F));
        CHECK(!registry.SetBool(1, 999, true));
        CHECK(!registry.SetFloat(1, 3, std::numeric_limits<float>::infinity()));
        CHECK(registry.GetBool(1, 1) == false);
        CHECK(registry.GetBool(2, 1) == false);
        CHECK(registry.GetInt(1, 2) == 10);
        CHECK(registry.GetFloat(1, 3) == 0.5F);
        const auto snapshot = registry.Snapshot();
        CHECK(snapshot.size() == 2);
        CHECK(snapshot[0].id == 1);
        CHECK(snapshot[0].settings.size() == 3);
        CHECK(snapshot[0].settings[0].id == 1);
        CHECK(snapshot[0].settings[2].kind == m1core::Kind::Float);
        CHECK(std::get<float>(snapshot[0].settings[2].value) == 0.5F);
        CHECK(registry.Save());
    }
    {
        m1core::Registry registry(file);
        CHECK(registry.Load());
        CHECK(registry.RegisterFloat(1, 3, 2.0F, 0.0F, 5.0F));
        CHECK(registry.RegisterBool(1, 1, true));
        CHECK(registry.RegisterInt(1, 2, 0, 0, 20));
        CHECK(registry.GetBool(1, 1) == false);
        CHECK(registry.GetInt(1, 2) == 10);
        CHECK(registry.GetFloat(1, 3) == 0.5F);
        CHECK(registry.Reset(1, 1));
        CHECK(registry.GetBool(1, 1) == true);
        CHECK(registry.Save());
    }
    {
        // An inactive module must not lose its saved values.
        m1core::Registry registry(file);
        CHECK(registry.Load());
        CHECK(registry.RegisterBool(2, 1, true));
        CHECK(registry.GetBool(2, 1) == false);
    }

    {
        std::ofstream output(file, std::ios::trunc);
        output << "{ corrupt json";
    }
    {
        m1core::Registry registry(file);
        CHECK(!registry.Load());
        CHECK(registry.RegisterBool(1, 1, true));
        CHECK(registry.GetBool(1, 1) == true);
    }
    std::filesystem::remove(file, ignored);
}
