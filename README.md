# WorldForge

**Windows-first 3D World Runtime & Asset Infrastructure.**

WorldForge is being built around a native editor for composing Mesh and Gaussian
worlds. This **0.0.2-texture-dev preview** adds embedded PNG base-color textures and
independent selection overlays to static GLB editing. Full PBR, project
serialization, Gaussian rendering, CLI/Agent tools and other platforms remain planned.
This is an increment toward 0.0.3, not the completed 0.0.3 milestone.

## Available in this preview

- SDL3 + Dear ImGui docking, saved panel layout and DPI-aware styling.
- OpenGL 4.5 offscreen viewport with shared indexed Mesh geometry and a ground grid.
- Background import of self-contained static GLB, preserving node hierarchy and instances.
- Add/select/delete model roots and cubes; edit transforms/tints/visibility.
- Read-only source-node tree, bounds-based framing and explicit import diagnostics.
- Orbit, pan, zoom and frame selected; independent grid, wireframe and selection overlay.
- Embedded PNG base-color textures with UV0, source samplers, mipmaps and linear-light shading.
- Clear selection through the Scene panel/menu, Scene background or focused Esc.
- Session console, CPU frame interval graph, viewport size and draw counts.
- Parser/worker/editor tests, real GPU pixel/lifetime checks and ImGui interaction tests.
- Separate geometry/image cache estimates, including texture mipmaps.

Only static TRIANGLES in glTF 2.0 GLB are supported. Animation, skins, morphs,
sparse/compressed/quantized geometry, transparency and external URIs are rejected.
Files and decoded upload geometry are each limited to 256 MiB. PNG dimensions
are at most 4096 per axis, decoded RGBA8 at most 64 MiB/image and 128 MiB/import.
Decoder allocations are capped at 192 MiB per worker thread; this is not a total
process-memory limit. JPEG, UV1 and texture transforms are rejected when used
by a base-color texture. Other PBR channels remain preview limitations.

This is an in-memory preview: object changes are not saved on exit. Only the
window/panel layout persists. The current shader is a diffuse preview, not PBR.

## Build on Windows

Requires Visual Studio 2026 or 2022 with Desktop C++, Windows SDK, CMake 3.25+
(4.2+ for VS 2026), and Python 3 with Jinja2. First configure downloads pinned dependencies.

```powershell
py -3 -m venv build/python
build/python/Scripts/python.exe -m pip install -r tools/build-requirements.txt
cmake --preset windows -DPython_EXECUTABLE="$PWD/build/python/Scripts/python.exe"
cmake --build --preset windows-release --parallel
ctest --preset windows-release
build/windows/Release/worldforge-studio.exe
```

For Visual Studio 2022, use configure preset `windows-vs2022`, build preset
`vs2022-release`, and `ctest --test-dir build/vs2022 -C Release --output-on-failure`.

## Controls

| Action | Control |
| --- | --- |
| Orbit / pan / zoom | Right drag / middle drag / mouse wheel inside viewport |
| Frame selected | F while hovering the viewport, or Frame button |
| Import GLB | File → Import GLB..., or Assets panel |
| Select | Model root in Scene; expand for read-only source nodes |
| Clear selection | Scene button/menu, Scene background, or Esc in focused Scene/Viewport (outside text/dialogs) |
| Display | Ground grid / Wireframe / Selection overlay (overlay defaults off) |
| Edit object | Inspector; values are in world units / degrees |
| Add / delete | Scene menu or panel / Inspector |
| Restore layout | View → Reset layout |

For packaging, GPU validation and architecture notes, see
[Development](docs/DEVELOPMENT.md) and [Documentation index](docs/README.md).

MIT license for project-owned code; see [third-party notices](THIRD_PARTY_NOTICES.md).

## C++ coding standard

All project-owned C++ **strictly follows the
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)**,
with one project filename exception: source files use `.cpp` instead of `.cc`.
Read [Coding standard](docs/CODING_STYLE.md) before contributing. Formatting and
cpplint checks are mandatory in Windows CI:

```powershell
build/python/Scripts/python.exe -m pip install -r tools/style-requirements.txt
build/python/Scripts/python.exe tools/check_style.py --fix
build/python/Scripts/python.exe tools/check_style.py
```
