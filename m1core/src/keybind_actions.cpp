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
        keys_.emplace(id, KeyState{vk, generation_, down, down,
                                   observed_[vk].presses});
        return false; // First poll primes the current physical key state.
    }

    auto& state = found->second;
    if (state.vk != vk || state.generation != generation_)
    {
        state = KeyState{vk, generation_, down, down,
                         observed_[vk].presses};
        return false;
    }

    if (!allowed)
    {
        state.down = down;
        state.suppressUntilRelease = down;
        state.lastObservedPress = observed_[vk].presses;
        return false;
    }

    if (state.suppressUntilRelease)
    {
        state.down = down;
        state.lastObservedPress = observed_[vk].presses;
        if (!down)
            state.suppressUntilRelease = false;
        return false;
    }

    // If a key went down and up entirely between REDscript ticks, the
    // Win32 input stream still carries a press edge. Consume it once per
    // (mod,setting) even when the key is no longer physically held.
    const auto count = observed_[vk].presses;
    const bool buffered = count != state.lastObservedPress;
    state.lastObservedPress = count;
    const bool pressed = buffered || (down && !state.down);
    state.down = down;
    return pressed;
}

void KeybindActions::ObserveKeyEvent(int vk, bool down)
{
    if (vk <= 0 || vk > 255 || vk == 0x7A)
        return;

    std::lock_guard lock(mutex_);
    auto& key = observed_[static_cast<std::size_t>(vk)];
    if (down && !key.down)
    {
        if (++key.presses == 0)
            key.presses = 1;
    }
    key.down = down;
}

void KeybindActions::Invalidate()
{
    std::lock_guard lock(mutex_);
    // Do not replay presses collected before focus or overlay transition.
    // An incoming down is a fresh edge only after this reset.
    observed_.fill({});
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
    observed_.fill({});
    generation_ = 1;
}
} // namespace m1core
