#include "keybind_conflicts.hpp"

#include <cstdlib>
#include <iostream>

#define CHECK(x) do { if (!(x)) { \
    std::cerr << "keybind conflict test failed: " #x " at " << __LINE__ << "\n"; \
    std::exit(EXIT_FAILURE); \
} } while (false)

int main()
{
    m1core::Registry registry("unused_conflict_test_settings.json");

    // The framework owns no preset key, and unassigned keys never conflict.
    CHECK(registry.RegisterKeybind(1, 10, 0));
    CHECK(registry.RegisterKeybind(2, 20, 0));
    CHECK(registry.RegisterBool(2, 21, true));
    CHECK(m1core::FindKeybindConflicts(registry.Snapshot()).empty());

    // Two separate mods can legitimately share the player's chosen key,
    // but the UI must warn; the framework must not rebind either one.
    CHECK(registry.SetKeybind(1, 10, 'Q'));
    CHECK(registry.SetKeybind(2, 20, 'Q'));
    const auto duplicate = m1core::FindKeybindConflicts(registry.Snapshot());
    CHECK(duplicate.size() == 1);
    CHECK(duplicate[0].virtualKey == 'Q');
    CHECK(duplicate[0].owners.size() == 2);
    CHECK(duplicate[0].owners[0].mod == 1);
    CHECK(duplicate[0].owners[1].mod == 2);
    CHECK(registry.GetKeybind(1, 10).value() == 'Q');
    CHECK(registry.GetKeybind(2, 20).value() == 'Q');

    // Moving, clearing and resetting a binding removes the warning.
    CHECK(registry.SetKeybind(2, 20, 'W'));
    CHECK(m1core::FindKeybindConflicts(registry.Snapshot()).empty());
    CHECK(registry.SetKeybind(1, 10, 0));
    CHECK(m1core::FindKeybindConflicts(registry.Snapshot()).empty());
    CHECK(registry.SetKeybind(1, 10, 'W'));
    CHECK(m1core::FindKeybindConflicts(registry.Snapshot()).size() == 1);
    CHECK(registry.Reset(2, 20));
    CHECK(m1core::FindKeybindConflicts(registry.Snapshot()).empty());

    // The same mod can bind two actions to one VK; it must also be detected.
    CHECK(registry.RegisterKeybind(1, 11, 0));
    CHECK(registry.SetKeybind(1, 11, 'W'));
    CHECK(m1core::FindKeybindConflicts(registry.Snapshot()).size() == 1);
}
