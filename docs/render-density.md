# Integer rendering density

SrvrUI supports explicit 1x and 2x backing density. Widget geometry, input,
text measurement, clipping and damage stay in logical pixels. Existing
`desk_canvas_init` remains 1x; a legacy zero canvas `scale` also means 1x.
Rebuild consumers with the matched header and library after this source API
change. No desktop wire protocol is defined here.

```c
desk_canvas_init_scaled(&canvas, pixels, logical_width, logical_height,
                        physical_stride, 2);
```

The caller owns `logical_width * scale` by `logical_height * scale` packed RGB
pixels. Stride is a physical row length in uint32_t pixels, including padding.
Physical width, height and stride are limited to 8192; maximum logical dimensions
at 2x are 4096. Unsupported scale, bad dimensions, NULL storage or insufficient
stride fail without changing the destination. The caller supplies enough storage.

- `desk_canvas_scale(c)` returns 1 or 2, normalizing legacy zero.
- `desk_canvas_pixel_width(c)` and `desk_canvas_pixel_height(c)` return physical
  dimensions of a valid initialized canvas.
- `desk_canvas_physical_rect(c, r)` clips a logical rectangle to canvas bounds
  and multiplies by density. It does not apply the current draw clip.
- `canvas.clip` and `desk_damage` remain logical half-open rectangles. Convert
  damage exactly once at the upload boundary.
- `pixel_writes` counts physical writes, including repeated writes. A logical
  pixel fill contributes four at 2x.

Direct pixel indexing must use physical coordinates and stride. A presenter may
round up backing for odd scanout sizes and clip uploads to the actual output.
Canvas density does not choose a monitor mode or logical layout dimensions.

## Fonts and controls

Built-in fonts retain the original glyph order, advances, ascent and line height;
text width, caret placement and line breaks remain logical. Their optional
`density2` raster supplies native alpha coverage/bearings from the bundled fonts
at twice the size. The renderer advances and positions the baseline using the
original logical metrics. Custom rasters must have the same glyph count/order;
their glyph dimensions, offsets and bearings are physical, and their advances
are unused. A NULL raster uses nearest replication as a compatibility fallback.

Gradient/glass interpolation, rounded spans, radio circles, checkmarks and image
sampling use physical pixels at 2x. Source image dimensions/zoom are independent
of canvas density, so fit-down images can use the additional physical samples.
Tiles sample from the original shape/image origin. Accepted 1x output remains
byte-identical in the regression scenes.

Added 2x alpha tables occupy 427,339 bytes plus glyph metadata. Existing 1x alpha
tables occupy 110,512 bytes and are unchanged. A 2x backing needs four times the
pixel storage; consumers must budget surfaces, caches and resize candidates.

```sh
python3 tools/make_desktop_font.py --density2-only
```

The added raster was generated with Pillow 12.0.0 and FreeType 2.14.1 using the
bundled DejaVu fonts. Omitting the flag also regenerates 1x: review that diff,
since rasterizer versions can affect coverage. `render/desk_font.c` includes
`render/desk_font_2x.inc`; external build rules must depend on both. Runtime font
parsing, shaping, allocation and host font lookup are not introduced.

## SDL opt-in

`srvrui_sdl3_set_density(host, 2)` reallocates backing transactionally, preserves
logical dimensions/input conversion and cancels pointer capture on success.
The caller redraws afterward. Failure retains the old canvas. Initialization
defaults to 1x; the application continues to own its window and event loop.
Presentation uses physical canvas dimensions. A 2x canvas matches a Retina 2x
surface directly; nearest scaling remains when source and target sizes differ.
Automatic monitor policy and fractional density are separate work.

```sh
build-pinned/showcase --density 2
build-pinned/showcase --density 2 --smoke build/review/density-native
```

Smoke PPMs contain physical backing resolution. Existing invocations retain their
behavior. Density switching does not change document or widget model state.

## Verification and integration

`test_density` covers stride/guards, accounting, rejection, all-theme full/tiled
equality, image sampling, raster/fallback and native glyph detail. Three 1x hashes
come from independently compiling the same scene against merged `76deeb30`.
Original atlas/metrics/license bytes are preserved. SDL tests cover density
changes, unchanged input geometry/model state, capture cancellation, invalid
density/oversize refusal and presentation. CTest runs showcase smoke at both
densities; sanitizer checks exclude the separate ordinary archive boundary test.

SrvrOS integration must allow `desk_canvas_scale` and
`desk_canvas_physical_rect` in its widget checker's external `RENDERER` set,
track the new atlas include, and adapt raw pixel copies, backing budgets and
upload conversion. This child changes no OS source or pin. Matched OS/native
output acceptance belongs to the integration owner; macOS smoke does not accept
physical SrvrOS scanout. Shared checkouts and review VMs remain untouched.
