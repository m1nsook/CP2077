#include "keybind_actions.hpp"

#include <limits>

namespace m1core
{
KeybindActions& KeybindActions::Get()
{
    static KeybindActions instance;
    return instance;
}

bool KeybindActions::Poll(std::uint64_t mod, std::uint64_t setting, int vk,
                          bool down, bool allowed)
{
    std::lock_guard lock(mutex_);
    const auto id = std::pair{mod, setting};

    // F11 is m1core's reserved toggle. Do not ever dispatch it as a mod
    // action (VK_F11 = 0x7A); zero is an unbound key.
    if (!mod || !setting || vk <= 0 || vk > 255 || vk == 0x7A)
    {
        keys_.erase(id);
        return false;
    }

    // Bound memory even if a broken mod polls with unbounded fake IDs.
    constexpr std::size_t kMaxActions = 1024;
    auto found = keys_.find(id);
    if (found == keys_.end())
    {
        if (keys_.size() >= kMaxActions)
            return false;
        keys_.emplace(id, KeyState{vk, generation_, down, down});
        return false; // First poll primes the current physical key state.
    }

    auto& state = found->second;
    if (state.vk != vk || state.generation != generation_)
    {
        state = KeyState{vk, generation_, down, down};
        return false;
    }

    if (!allowed)
    {
        state.down = down;
        state.suppressUntilRelease = down;
        return false;
    }

    if (state.suppressUntilRelease)
    {
        state.down = down;
        if (!down)
            state.suppressUntilRelease = false;
        return false;
    }

    const bool pressed = down && !state.down;
    state.down = down;
    return pressed;
}

void KeybindActions::Invalidate()
{
    std::lock_guard lock(mutex_);
    if (generation_ == std::numeric_limits<std::uint64_t>::max())
    {
        keys_.clear();
        generation_ = 1;
    }
    else
        ++generation_;
}

void KeybindActions::Reset()
{
    std::lock_guard lock(mutex_);
    keys_.clear();
    generation_ = 1;
}
} // namespace m1core
