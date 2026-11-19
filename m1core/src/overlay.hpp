#pragma once
#include <filesystem>

namespace RED4ext::v1 { struct Sdk; struct Logger; }
namespace RED4ext { namespace v1 { struct PluginHandle; } }
namespace m1core {
class Registry;

// Optional, fail-closed development overlay. Only enabled if the explicit
// red4ext/plugins/m1core/overlay.enable file exists at process startup.
// Never installs hooks or intercepts input in the normal production package.
void StartOverlay(Registry& registry, const std::filesystem::path& directory,
                  RED4ext::v1::PluginHandle handle, const RED4ext::v1::Sdk* sdk);
void StopOverlay();
} // namespace m1core
