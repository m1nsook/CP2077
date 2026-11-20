#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <mutex>
#include <utility>

namespace m1core
{
// One observer per (mod, setting). A REDscript mod polls its own binding
// during updates; m1core samples key state in the native API without hooking
// or intercepting keyboard messages. Never runs callbacks on a Win32 thread.
class KeybindActions final
{
public:
    static KeybindActions& Get();

    // vk: 1..255; down: high bit from GetAsyncKeyState; allowed: foreground,
    // game active and m1core closed. First observation primes the key state.
    // Fires once for each up->down transition, never during repeats/holds.
    bool Poll(std::uint64_t mod, std::uint64_t setting, int vk,
              bool down, bool allowed);

    // Called from Cyberpunk's existing Win32 subclass for keyboard messages
    // while game is foreground and our overlay is closed. Events are
    // observational: never intercept input or invoke mod code on WndProc.
    // Deduplicate RAWINPUT + legacy messages and keyboard auto-repeat.
    void ObserveKeyEvent(int vk, bool down);

    // Call when the overlay opens/closes or the window loses/regains focus,
    // so a pre-held key cannot trigger gameplay when returning to the game.
    void Invalidate();
    void Reset();

private:
    struct KeyState
    {
        int vk = 0;
        std::uint64_t generation = 0;
        bool down = false;
        bool suppressUntilRelease = false;
        std::uint64_t lastObservedPress = 0;
    };

    struct ObservedKey
    {
        bool down = false;
        std::uint64_t presses = 0;
    };

    std::array<ObservedKey, 256> observed_{};
    std::mutex mutex_;
    std::map<std::pair<std::uint64_t, std::uint64_t>, KeyState> keys_;
    std::uint64_t generation_ = 1;
};
} // namespace m1core
