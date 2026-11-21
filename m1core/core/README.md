# m1core — active native runtime

This directory builds **m1core.dll**, a standalone Cyberpunk 2077 RED4ext
framework. It contains the integrated game-window/DX12/ImGui input lifecycle,
native settings registry and LuaJIT scripting runtime.

The current implementation includes directly authorized donated source code,
but it is maintained, named, packaged and released as **m1core**, with its
independent public API and visual design. It does **not** require any separate
CET installation. Historical donor-source analysis and earlier prototypes
remain available in Git history and the project's development notes.

- Build instructions and independent third-party licenses: [BUILD.md](BUILD.md)
- API documentation: [../DEVELOPERS.md](../DEVELOPERS.md)
- Current verification: [../PROJECT_STATUS.md](../PROJECT_STATUS.md)

Source/bytecode sample consumers live under `../examples/`.
