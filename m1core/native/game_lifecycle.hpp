#pragma once

namespace m1core {
// RED4ext's game-state callback sets this after base initialization,
// independently from DX12 backend initialization on Present. The first
// visible settings UI may be armed only after this callback fires.
[[nodiscard]] bool GameBaseInitialized() noexcept;
void SetGameBaseInitialized() noexcept;
}
