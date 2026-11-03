# SrvrUI integration

Work against a matched SrvrOS checkout; read its AGENTS.md and coordination
guide. Keep widgets allocation-free and free of OS, I/O and hidden event loops.
Preserve caller-owned themes, state and clipped/padded-stride drawing. Put
application behavior in SrvrApps and runtime/transport policy in SrvrOS.
Build from the OS root, run test-widgets and affected control checks, and inspect
widget-theme-preview plus a representative desktop. Do not run concurrent
build/VM harnesses in the same checkout. Push this repository's commit before
updating the OS pin, and record the exact combined tested revisions.

For standalone host work, use the CMake build and docs/host-showcase.md. Keep
SDL/native resources in adapters/, separate from src/ and render/. Run standalone
CTest and affected adapter checks; renderer/core changes also require the matched
OS regressions above. Linux/Windows remain unaccepted until exercised there.
