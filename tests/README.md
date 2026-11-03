# Standalone regression provenance

The widget, text, editor, UI-router and theme fixtures and archive-boundary
checker originate in SrvrOS `e376669c` under `tools/`. They run the actual core
sources in this standalone package, not mock controls. Keep matching behavioral
fixes synchronized with the OS fixtures; shared key/calendar/image contracts are
also compared automatically when the OS tree is present.

`test_sdl3.c` is the adapter-specific fixture. It creates a real SDL window and
injects typed SDL events into the same dispatch function used by the showcase.
CTest uses SDL's dummy backend. Cocoa/manual checks remain separate evidence.
Clipboard text is saved/restored by this fixture, and all allocations/window
resources are retired on the successful path. Assertions are enabled in Release.
