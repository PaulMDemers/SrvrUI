# Standalone SrvrUI and the macOS showcase

The first host target is macOS ARM64. Linux and Windows remain planned and
untested. This ports the widget library, not the SrvrOS kernel, compositor,
desktop services or applications. Silver, Aero and Aqua use the same controls,
software drawing and font atlas as SrvrOS.

## Build and run

Requirements: C11 compiler, CMake (3.16+ for SrvrUI; use a current version for
building SDL), Python 3 for tests, and SDL3 3.2+ with its CMake package. The tested
host uses Xcode Clang 21, CMake 4.4.0 and SDL3 3.4.12 on macOS 26.6.2 ARM64.
No OS checkout, cross compiler, VM, system fonts or runtime font parser is needed.

From a standalone SrvrUI checkout:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/showcase
```

On macOS, `build/SrvrUIShowcase.app` is also a launchable application. A build
against Homebrew's shared SDL depends on that installation. For a local bundle
without an external SDL dylib, use the pinned static dependency recipe:

```sh
sh tools/bootstrap-sdl3.sh build/deps
cmake -S . -B build-pinned -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$PWD/build/deps/install"
cmake --build build-pinned --parallel 4
ctest --test-dir build-pinned --output-on-failure
./build-pinned/showcase
```

The explicit bootstrap downloads SDL3 3.4.12 from its official GitHub release,
checks SHA-256 `f07b958a9ac5020fb7a44cadb957f658b2149c3c8abb4f63145fac9303249db7`,
and builds/installs under the given directory. Configuration never downloads
dependencies. Normal compilation uses the checked-in atlas; regeneration alone
requires the Pillow/FreeType versions recorded in [font provenance](../assets/fonts/README.md).

## Try the showcase

Select an appearance, toggle the option, drag or arrow-key the slider, edit Notes,
and scroll or select the collection. Tab/Shift-Tab follows the controls; Enter
or Space activates buttons. Control/Command-A/C/X/V/Z and Shift-Command-Z select,
copy, cut, paste, undo and redo. Native committed UTF-8 and dead keys are separate
from physical key events. Type in a focused collection to use its typeahead.

Resize between the default 900×650 and minimum 420×500 layout. The narrower
layout stacks Notes above the collection. Themes retain text/selection, control
values and scroll position; resizing retains model state while clamping viewports.
Focus loss, resize and cancellation disarm capture. Failed inserts retain text,
selection and undo history. Notes have a 2047-byte UTF-8 capacity.

```sh
./build/showcase --smoke build/review
./build/showcase --trace build/native-events.log
```

Smoke uses synthetic SDL events and writes real rendered PPM captures of all
three profiles plus compact layout. It checks theme/state retention, text/undo,
pointer activation, held dragging, outside release, scrolling and resizing.
Trace is optional local diagnostic output: pointer events, capture, geometry and
text length, never text contents. Neither mode establishes human input acceptance.

## Embed the toolkit

`srvrui` contains allocation-free controls/models, renderer and font data.
`srvrui_sdl3` is an optional adapter that may allocate host resources. Disable it
with `-DSRVRUI_BUILD_SDL3=OFF` for a core-only build; SDL is then unnecessary.
Use `add_subdirectory` and link those targets, or install the CMake package:

```sh
cmake --install build-pinned --prefix "$PWD/build/install"
```

```cmake
find_package(SrvrUI CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE SrvrUI::srvrui_sdl3)
```

Point `CMAKE_PREFIX_PATH` at the installation and SDL's installation. Link
`SrvrUI::srvrui` for core-only usage. The installed portable headers exclude
OS-specific battery, listing, file-search, menu and popup interfaces.

Applications own the main-thread event loop, SDL initialization, window, UI,
control instances, models and theme. Initialize the adapter with a borrowed window
and `desk_ui`; dispatch relevant SDL events through `srvrui_sdl3_event`. On its
LAYOUT flag, reconfigure control frames and call `desk_ui_layout_changed`.
Draw into `host.canvas` and call `srvrui_sdl3_present` when dirty. Call
`srvrui_sdl3_sync_text_input` after application-driven focus/layout changes.
Destroy the adapter before its borrowed UI/window. There is no hidden event loop,
worker, global theme, filesystem service or transport inside the core.

The adapter owns an RGB canvas and converts its format when presenting to SDL's
window surface. It reacquires that surface after resizing. On Retina the existing
logical raster is scaled with nearest-neighbor sampling: pixel and input geometry
match, but this is not a new high-resolution font rasterizer. Display content scale
and pixel density are treated separately. SDL's native clipboard lives outside
controls; Cut deletes only after clipboard publication succeeds.

The current `srv_key_event`, calendar helpers and image limits are small value
contract snapshots. Matched OS tests compare them byte-for-byte to catch drift;
they do not import OS calls, device queues or IPC. The OS key ABI is unchanged.
Future contract changes must update both copies and the comparison tests.

## Verification and limits

Standalone CTest covers actual widget/edit/text/UI/theme regressions, an archive
dependency boundary check, and SDL adapter event/clipboard/resize/format checks.
In a matched OS checkout it additionally compares the three shared headers.
Tests keep assertions enabled in Release. Dummy-backend tests are deterministic
adapter checks; native Cocoa operation is a separate acceptance activity.

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DSRVRUI_SANITIZE=ON
cmake --build build-sanitize --parallel 4
ctest --test-dir build-sanitize --output-on-failure -E '^widget_boundary$'
```

Run the archive boundary check in the ordinary build: sanitizer runtime imports
are intentionally outside its pure-function allowlist. The original OS continues
to run `test-widgets`, `test-desktop`, `test-desktop-io`, `widget-theme-preview`,
a matched full image build and its representative desktop/theme VM check.

This milestone retains atlas glyph coverage and replacement-glyph fallback.
Full IME preedit presentation, shaping, bidirectional/grapheme editing, native
accessibility semantics, native menus/dialogs, additional host OS acceptance and
notarized distribution are separate work. A successful macOS host app or x86 VM
does not imply native SrvrOS hardware acceptance.
