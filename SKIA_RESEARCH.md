# Skia Rendering Backend Research

## Summary

A Skia backend would fit into the newer drawables architecture. It would not be a separate high-level map painter or a small adapter around individual style layers. It would be a full `gfx` backend beside OpenGL, Metal, Vulkan, and WebGPU.

The most plausible target is modern Skia with `SkMesh`, SkSL mesh programs, `SkCanvas::drawMesh`, `SkImage`/`SkShader` textures, and GPU-backed `SkSurface` render targets. `SkRuntimeEffect` is useful for fragment-style effects and child shader composition, but the core drawable path needs mesh vertex programs because MapLibre drawables depend on custom vertex attributes, indexed geometry, uniforms, textures, and per-layer shader logic.

The first practical milestone should be complete 2D map rendering plus the current mesh-backed fill extrusion path. Fill extrusion is supported as projected mesh geometry, with depth-buffer semantics intentionally degraded until a dedicated Skia 3D policy exists.

## Existing MapLibre Architecture

The relevant split is:

- `src/mbgl/renderer`: render orchestration, style/source/layer diffing, render tree creation, pass ordering.
- `include/mbgl/gfx`: backend-neutral graphics interfaces.
- `src/mbgl/gl`, `src/mbgl/mtl`, `src/mbgl/vulkan`, `src/mbgl/webgpu`: concrete backend implementations.
- `include/mbgl/shaders` and `src/mbgl/shaders/*`: backend-specific shader program plumbing.

The main frame path is:

1. `Renderer::render()` receives `UpdateParameters`.
2. `RenderOrchestrator::createRenderTree()` diffs style/source/layer state and prepares render data.
3. `Renderer::Impl::render()` gets the active `gfx::Context` from `gfx::RendererBackend`.
4. Shaders are registered once via `RendererBackend::initShaders()`.
5. Upload passes prepare buffers, textures, and drawable resources.
6. Layer groups are updated and rendered in 3D, offscreen target, opaque, translucent, and debug passes.
7. `CommandEncoder::present()` submits or presents the default renderable.

Key MapLibre files:

- `src/mbgl/renderer/renderer.cpp`
- `src/mbgl/renderer/renderer_impl.cpp`
- `src/mbgl/renderer/render_orchestrator.cpp`
- `include/mbgl/gfx/renderer_backend.hpp`
- `include/mbgl/gfx/context.hpp`
- `include/mbgl/gfx/command_encoder.hpp`
- `include/mbgl/gfx/renderable.hpp`
- `include/mbgl/gfx/drawable.hpp`
- `include/mbgl/gfx/drawable_builder.hpp`
- `include/mbgl/renderer/layer_group.hpp`

## Backend Contract

A Skia backend would add a new backend type, build flag, and implementation namespace, for example `mbgl::skia`.

Expected backend files:

```text
include/mbgl/skia/renderer_backend.hpp
include/mbgl/skia/context.hpp
include/mbgl/skia/command_encoder.hpp
include/mbgl/skia/render_pass.hpp
include/mbgl/skia/upload_pass.hpp
include/mbgl/skia/drawable.hpp
include/mbgl/skia/drawable_builder.hpp
include/mbgl/skia/texture2d.hpp
include/mbgl/skia/offscreen_texture.hpp
include/mbgl/skia/uniform_buffer.hpp
include/mbgl/skia/layer_group.hpp
include/mbgl/skia/tile_layer_group.hpp
src/mbgl/skia/...
include/mbgl/shaders/skia/...
src/mbgl/shaders/skia/...
```

The high-level class mapping is:

| MapLibre contract | Skia backend implementation | Skia concept |
| --- | --- | --- |
| `gfx::RendererBackend` | `skia::RendererBackend` | owns or references Skia render context/surface provider |
| `gfx::Context` | `skia::Context` | frame lifecycle and resource factory |
| `gfx::CommandEncoder` | `skia::CommandEncoder` | creates upload/render passes and flushes/presents |
| `gfx::RenderPass` | `skia::RenderPass` | selected `SkCanvas` plus pass state |
| `gfx::Renderable` | `skia::Renderable`/resource | default `SkSurface`/`SkCanvas` target |
| `gfx::OffscreenTexture` | `skia::OffscreenTexture` | offscreen `SkSurface` and snapshot `SkImage` |
| `gfx::Texture2D` | `skia::Texture2D` | `SkImage`, `SkShader`, sampling/tile modes |
| `gfx::UniformBuffer` | `skia::UniformBuffer` | raw UBO bytes copied to SkSL uniform data |
| `gfx::DrawableBuilder` | `skia::DrawableBuilder` | creates Skia drawables from MapLibre geometry state |
| `gfx::Drawable` | `skia::Drawable` | packed vertex/index buffers, `SkMesh`, SkSL programs |
| `LayerGroup`/`TileLayerGroup` | `skia` subclasses | draw drawable collections, apply tile clips |
| `ShaderProgramBase` | `skia::ShaderProgram` | `SkMeshSpecification`, SkSL source, attribute layout |

## Rendering Target

The backend should target a real Skia `SkSurface` or `SkCanvas`, preferably GPU-backed. A CPU/raster fallback can be considered later, but it should not shape the initial design because MapLibre's renderer is shader and mesh heavy.

For owned offscreen rendering, the backend can create `SkSurface` instances and expose snapshots as `SkImage`-backed `Texture2D` objects. For host-provided rendering, the platform integration layer can provide the active `SkCanvas`/`SkSurface` for the frame. This is a platform integration concern, not a core renderer design requirement.

## GPU Context Policy

The first Skia backend uses Ganesh. Graphite should remain a follow-up backend decision after Ganesh reaches useful render-test coverage because the current implementation depends on `GrDirectContext` and Ganesh surface creation.

Runtime GPU initialization is best-effort. `skia::RendererBackend` owns an optional `GrDirectContext`; Apple builds create it from the default Metal device and command queue when `MLN_SKIA_ENABLE_GPU` is enabled. If GPU initialization fails or the platform has no Skia GPU context implementation yet, renderable and offscreen resources fall back to raster `SkSurface`s.

Platform selection policy:

- Apple platforms use Metal through Ganesh.
- Linux and Android Vulkan support is not implemented yet.
- GL is reserved for a fallback GPU path after the Metal path is stable.

Owned default renderables and offscreen textures use GPU-backed `SkSurface`s when a `GrDirectContext` is available. The raster path remains a compatibility fallback until non-Apple GPU paths and host-provided surface ownership are defined.

## Drawables With SkMesh

The concrete drawable path should be:

```text
MapLibre bucket data
  -> gfx::VertexAttributeArray / segments / textures / uniforms
  -> skia::DrawableBuilder
  -> packed vertex and index buffers
  -> SkMeshSpecification
  -> SkMesh
  -> SkCanvas::drawMesh(...)
```

`SkMeshSpecification` is the important Skia primitive because it defines vertex attributes, stride, varyings, a vertex SkSL program, a mesh-fragment SkSL program, uniforms, and child shader declarations. This maps much more directly to MapLibre's existing GPU-style drawables than `SkVertices` or path drawing would.

MapLibre's `gfx::AttributeDataType` values should be packed into Skia mesh attributes. The backend should normalize or repack attributes as needed to satisfy Skia alignment and attribute-type requirements. Shader definitions should own the canonical mapping from MapLibre attribute IDs to SkSL attribute names.

The backend should not assume every layer maps to simple paths. Layers such as line, symbol, heatmap, and hillshade rely on shader behavior and data-driven attributes. Keeping the mesh pipeline preserves the existing renderer architecture and avoids reimplementing layer semantics at a higher level.

## SkSL Shader Strategy

The Skia shader inventory should mirror the existing backend shader groups. `skia::RendererBackend::initShaders()` would register `ShaderGroup` instances for built-in shaders such as:

- `BackgroundShader`
- `BackgroundPatternShader`
- `FillShader`
- `FillOutlineShader`
- `FillPatternShader`
- `LineShader`
- `LineGradientShader`
- `LinePatternShader`
- `LineSDFShader`
- `RasterShader`
- `SymbolIconShader`
- `SymbolSDFShader`
- `SymbolTextAndIconShader`
- `CircleShader`
- `HeatmapShader`
- `HeatmapTextureShader`
- `HillshadeShader`
- `HillshadePrepareShader`
- `ColorReliefShader`

Each shader group would provide:

- vertex attribute declarations and default values for `ShaderProgramBase::getVertexAttributes()`.
- instance attribute declarations if needed.
- SkSL mesh vertex program source.
- SkSL mesh fragment program source.
- uniform layout metadata.
- child shader declarations for texture inputs.

Textures should be passed as child `SkShader`s, usually produced from `SkImage::makeShader(...)`. In SkSL these are sampled with `child.eval(...)`, consistent with Skia runtime effect conventions.

Uniform buffers can remain raw bytes at the MapLibre boundary. The Skia backend can copy those bytes into the uniform data expected by each `SkMeshSpecification` or runtime effect. The existing UBO headers in `include/mbgl/shaders/*_ubo.hpp` are useful because they already define the packed data that layer tweakers produce.

## Uniform Buffer Policy

`skia::UniformBuffer` owns a byte copy of the MapLibre UBO payload. Drawables read typed UBO structs from those bytes and write the fields needed by the selected `SkMeshSpecification` into Skia uniform data immediately before drawing.

Layer tweakers may provide consolidated per-layer UBO arrays. Skia drawables use their UBO index to read the matching entry from the layer uniform array, falling back to drawable-local UBOs where the shared renderer still writes them directly.

Global uniform buffers are not bound as backend state in the Skia path. Current SkSL mesh programs consume per-drawable and per-layer values directly, so `Context::bindGlobalUniformBuffers()` and `unbindGlobalUniformBuffers()` are no-ops until a shader path needs shared global state.

## Render Passes And Targets

MapLibre render targets map naturally to offscreen `SkSurface`s:

- `Context::createOffscreenTexture()` creates a GPU-backed `SkSurface` where possible.
- `RenderTarget::render()` creates a `skia::RenderPass` against that surface.
- `OffscreenTexture::getTexture()` returns a `Texture2D` wrapping `surface->makeImageSnapshot()`.
- `OffscreenTexture::readStillImage()` uses `readPixels(...)`.

This should support heatmap and hillshade-style intermediate passes without changing the shared renderer flow.

## Memory Ownership And Cleanup

Skia backend resources use RAII ownership at the MapLibre boundary. `RendererBackend` owns the optional Ganesh `GrDirectContext`, `RenderableResource` owns each default or offscreen `SkSurface`, and `Texture2D` owns its CPU pixel staging plus the current `SkImage` snapshot.

Offscreen-derived textures retain their source `SkSurface` with `sk_sp<SkSurface>` while they need live snapshots. This avoids dangling references when a texture outlives the `OffscreenTexture` that created it. Pixel uploads and explicit image snapshots clear any old surface snapshot source before replacing image contents.

Buffer and uniform resources own byte copies in `std::vector<std::uint8_t>`, so drawable resource lifetime does not depend on caller-owned staging memory. Skia GPU resources are released by dropping `sk_sp` and `std::unique_ptr` owners; `Context::reduceMemoryUsage()` is still a no-op and can later call Ganesh cache-purge APIs once the backend has a clear memory-pressure policy.

## Tile Clipping

Existing GPU backends use stencil for tile clipping. Public Skia canvas APIs do not expose GL-style stencil writes/tests as a portable rendering primitive. The Skia backend should implement tile clipping in `skia::TileLayerGroup::render()` using the canvas clip stack.

The practical approach is:

```cpp
canvas.save();
canvas.clipPath(tilePathOrRect, SkClipOp::kIntersect, true);
drawable.draw(parameters);
canvas.restore();
```

This is a deliberate replacement for stencil-based clipping, not a missing backend feature. It should be adequate for 2D map content and keeps the implementation on public Skia APIs. It may have different performance characteristics than stencil clipping, so render tests and profiling should compare tile-heavy styles.

## Tile-Heavy Profiling

Tile-heavy profiling currently uses the render-test runner against local macOS Debug builds. The comparison is a smoke-level backend check, not a production benchmark, because both presets use `CMAKE_BUILD_TYPE=Debug` and the Skia run still has known tile LOD rendering failures.

The measured subset is:

- `render-tests/tile-lod/*`
- `render-tests/tile-mode/streets-v11`
- `render-tests/sparse-tileset/overdraw`
- `render-tests/real-world/*`

Local command pattern:

```sh
/usr/bin/time -p build-macos-skia/mbgl-render-test-runner --manifestPath metrics/macos-skia.json --filter 'render-tests/(tile-lod/|tile-mode/streets-v11|sparse-tileset/overdraw|real-world/)'
/usr/bin/time -p build-macos-metal/mbgl-render-test-runner --manifestPath metrics/macos-xcode11-release-style.json --filter 'render-tests/(tile-lod/|tile-mode/streets-v11|sparse-tileset/overdraw|real-world/)'
```

Local result from 2026-05-02:

| Backend | Result | Wall time |
| --- | --- | --- |
| Skia | 3 passed, 4 ignored, 9 failed | 3.05s |
| Metal | 12 passed, 4 ignored | 2.64s |

The Skia failures are concentrated in `tile-lod/*` plus `real-world/nepal`. This closes the initial tile-heavy profiling milestone while leaving tile LOD parity and performance as follow-up work. The run confirms the Skia backend can execute the tile-heavy suite locally without crashes or resource lifetime failures.

## Text-Heavy Profiling

Text-heavy profiling uses a focused render-test subset that exercises glyph atlas uploads, SDF text drawing, formatted text, CJK shaping, vertical writing, variable anchors, symbol placement, symbol spacing, and text sort/z-order paths.

The measured subset is selected with this filter:

```sh
render-tests/(text-(field/(literal|formatted|formatted-images|formatted-line|formatted-arabic)|font/(literal|chinese|devanagari)|size/(literal|composite-function-line-placement)|color/(literal|property-function)|halo-color/literal|opacity/literal|writing-mode/(point_label/cjk-vertical-mode|line_label/chinese)|variable-anchor/(top-bottom-left-right|all-anchors-tile-map-mode))|symbol-(placement/(point|line|line-center)|spacing/(line-close|line-far)|sort-key/(text-placement|text-expression)|z-order/icon-with-text))/style.json$
```

Local result from 2026-05-02:

| Backend | Result | Wall time |
| --- | --- | --- |
| Skia | 17 passed, 9 failed | 2.12s |
| Metal | 25 passed, 1 ignored passed | 1.99s |

The Skia failures are concentrated in line-label placement/spacing, line-placement text size, formatted line text, tile-map variable anchors, and CJK/vertical writing modes. The timing is close to Metal for this Debug smoke subset, but the parity gaps mean text-heavy performance should be revisited after the remaining symbol/text placement work lands.

## Raster-Heavy Profiling

Raster-heavy profiling uses raster property families, masking, rotation, zoomed/retina raster, and image-backed raster cases. These cases exercise raster texture upload, sampling, color adjustment, opacity, tile clipping, and projected image placement.

The measured subset is selected with this filter:

```sh
render-tests/(raster-(brightness|contrast|hue-rotate|opacity|resampling|saturation)/(default|literal|function)|raster-(masking/(overlapping|overlapping-zoom|overlapping-vector)|rotation/(0|45|90|180|270)|extent/(minzoom|maxzoom)|alpha/default|visibility/visible)|zoomed-raster/(overzoom|underzoom|fractional)|retina-raster/default|image/(default|pitched|raster-(brightness|contrast|hue-rotate|opacity|resampling|saturation|visibility)))/style.json$
```

Local result from 2026-05-02:

| Backend | Result | Wall time |
| --- | --- | --- |
| Skia | 41 passed, 1 ignored, 1 failed | 3.53s |
| Metal | 42 passed, 1 ignored | 3.19s |

The only Skia failure in this subset is `image/pitched`. Raster-heavy timing is close to Metal for local Debug builds, and the pass rate indicates the raster shader path is not a major parity risk compared with symbol/text and tile LOD work.

## Fixed-Function State

The Skia backend maps MapLibre's fixed-function state only where Skia exposes an equivalent canvas or paint concept.

Cull-face state is a no-op for the current 2D mesh path. MapLibre buckets already provide front-facing triangle order for the layers Skia renders, and Skia mesh drawing does not expose portable GPU-style face culling through `SkCanvas`.

Depth and stencil state are intentionally degraded. Layer order, render passes, tile clips, and drawable order provide ordering for 2D content. Tile clipping uses the canvas clip stack instead of stencil. Render pass depth and stencil clears are therefore no-ops until a future Skia 3D path needs explicit depth semantics.

Debug groups are currently no-ops. They are safe to add later using Skia tracing or platform signposts, but rendering correctness must not depend on them.

## Depth And 3D

Most 2D ordering is already expressed through MapLibre's layer order, opaque/translucent passes, and drawable sort order. The Skia backend can handle that without a depth buffer.

Fill extrusion uses the existing extrusion bucket geometry and SkSL lighting logic. The Skia path renders solid and patterned extrusion meshes through `SkCanvas::drawMesh`, including height/base attributes, side normals, and light uniforms.

The unsupported boundary is fixed-function 3D state. Skia does not expose a portable `SkCanvas` depth buffer or face-culling contract for this path, so extrusion ordering relies on MapLibre's layer/drawable order rather than depth tests. Future work that needs full 3D interleaving should define that policy separately instead of adding ad hoc depth behavior to 2D drawables.

## Layer Mapping

Initial layer mapping:

| MapLibre layer | Initial Skia implementation |
| --- | --- |
| Background solid | `SkCanvas::drawRect` or mesh with solid SkSL color |
| Background pattern | `SkImage` child shader with repeat sampling |
| Fill solid | indexed `SkMesh` triangles with solid color SkSL |
| Fill pattern | indexed `SkMesh` plus image child shader |
| Fill outline | existing outline geometry as mesh or stroked path where equivalent |
| Raster | image shader or image rect mesh for projected tile quads |
| Line | `SkMesh` using existing line layout attributes and ported line SkSL |
| Line gradient | `SkMesh` plus gradient texture/child shader |
| Line pattern | `SkMesh` plus sprite atlas child shader |
| Circle | quad/point mesh with distance-field SkSL |
| Symbol icon | quad mesh plus sprite atlas child shader |
| Symbol SDF text | quad mesh plus glyph atlas child shader and SDF SkSL |
| Collision debug | Skia strokes/fills or simple meshes |
| Heatmap | offscreen `SkSurface` accumulation and colorization SkSL |
| Hillshade | DEM texture sampling in SkSL, likely offscreen prepare pass |
| Color relief | DEM/color ramp texture sampling in SkSL |
| Fill extrusion | solid and patterned `SkMesh` extrusion path with degraded depth semantics |

## Suggested Milestones

1. Add `gfx::Backend::Type::Skia`, build flags, and empty backend registration.
2. Implement `skia::RendererBackend`, `Context`, `CommandEncoder`, `RenderPass`, and `Renderable` against a host-provided or owned `SkSurface`.
3. Implement `Texture2D`, `UniformBuffer`, `UniformBufferArray`, and `OffscreenTexture`.
4. Implement `DrawableBuilder` and `Drawable` using `SkMesh`.
5. Register and port background, fill, fill outline, and raster shaders.
6. Pass basic render tests for background/fill/raster styles.
7. Add line shaders and line render tests.
8. Add symbol icon and SDF text shaders.
9. Add offscreen-pass-dependent layers such as heatmap, hillshade, and color relief.
10. Document full-depth 3D limitations for fill extrusion and defer explicit depth semantics to a separate Skia 3D effort.

## Risks And Open Questions

- Shader parity is the main body of work. Each built-in shader family needs a SkSL equivalent.
- Tile clipping via canvas clips may differ from stencil clipping in performance or edge behavior.
- `SkMesh` support should be treated as a requirement for the first backend version.
- The backend should initially target GPU-backed Skia surfaces. CPU-only rendering can be evaluated after the mesh/shader path works.
- Platform integration needs a way to supply or own a compatible `SkSurface`/`SkCanvas`. Compose-style environments are examples of hosts that may make this attractive, but the backend itself should be designed around Skia, not around a specific UI toolkit.

## References

- MapLibre rendering modularization proposal: `design-proposals/2022-10-27-rendering-modularization.md`
- MapLibre renderer implementation: `src/mbgl/renderer/renderer_impl.cpp`
- MapLibre graphics backend interface: `include/mbgl/gfx/renderer_backend.hpp`
- MapLibre graphics context interface: `include/mbgl/gfx/context.hpp`
- MapLibre drawable interface: `include/mbgl/gfx/drawable.hpp`
- MapLibre drawable builder interface: `include/mbgl/gfx/drawable_builder.hpp`
- MapLibre layer group interface: `include/mbgl/renderer/layer_group.hpp`
- Skia `SkMeshSpecification` API: https://api.skia.org/classSkMeshSpecification.html
- Skia `SkMesh.h` API reference: https://api.skia.org/SkMesh_8h.html
- Skia `SkMesh.h` source: https://skia.googlesource.com/skia/+/main/include/core/SkMesh.h
- Skia SkSL and runtime effects docs: https://docs.skia.org/docs/user/sksl/
- Skia `SkRuntimeEffect` API: https://api.skia.org/classSkRuntimeEffect.html
