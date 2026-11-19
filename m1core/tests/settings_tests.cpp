#include "settings.hpp"

#include <cassert>
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
        assert(registry.Load());
        assert(registry.RegisterBool(1, 1, true));
        assert(registry.RegisterBool(2, 1, false));
        assert(registry.RegisterInt(1, 2, 5, 0, 10));
        assert(registry.RegisterFloat(1, 3, 1.0F, 0.5F, 3.0F));
        assert(!registry.RegisterBool(1, 2, false)); // Type collision.
        assert(!registry.RegisterFloat(1, 4, std::numeric_limits<float>::quiet_NaN(), 0, 1));
        assert(registry.SetBool(1, 1, false));
        assert(registry.SetInt(1, 2, 999));
        assert(registry.SetFloat(1, 3, -100.0F));
        assert(!registry.SetBool(1, 999, true));
        assert(!registry.SetFloat(1, 3, std::numeric_limits<float>::infinity()));
        assert(registry.GetBool(1, 1) == false);
        assert(registry.GetBool(2, 1) == false);
        assert(registry.GetInt(1, 2) == 10);
        assert(registry.GetFloat(1, 3) == 0.5F);
        assert(registry.Save());
    }
    {
        m1core::Registry registry(file);
        assert(registry.Load());
        assert(registry.RegisterFloat(1, 3, 2.0F, 0.0F, 5.0F));
        assert(registry.RegisterBool(1, 1, true));
        assert(registry.RegisterInt(1, 2, 0, 0, 20));
        assert(registry.GetBool(1, 1) == false);
        assert(registry.GetInt(1, 2) == 10);
        assert(registry.GetFloat(1, 3) == 0.5F);
        assert(registry.Reset(1, 1));
        assert(registry.GetBool(1, 1) == true);
        assert(registry.Save());
    }
    {
        // An inactive module must not lose its saved values.
        m1core::Registry registry(file);
        assert(registry.Load());
        assert(registry.RegisterBool(2, 1, true));
        assert(registry.GetBool(2, 1) == false);
    }

    {
        std::ofstream output(file, std::ios::trunc);
        output << "{ corrupt json";
    }
    {
        m1core::Registry registry(file);
        assert(!registry.Load());
        assert(registry.RegisterBool(1, 1, true));
        assert(registry.GetBool(1, 1) == true);
    }
    std::filesystem::remove(file, ignored);
}
