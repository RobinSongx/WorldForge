# WorldForge

**Windows-first 3D World Runtime & Asset Infrastructure.**

WorldForge is being built around a native editor for composing Mesh and Gaussian
worlds. This **0.0.2 Mesh development preview** adds static GLB import to the
desktop shell and OpenGL viewport. PBR, textures, project serialization,
Gaussian rendering, CLI/Agent tools and other platforms remain planned.

## Available in this preview

- SDL3 + Dear ImGui docking, saved panel layout and DPI-aware styling.
- OpenGL 4.5 offscreen viewport with shared indexed Mesh geometry and a ground grid.
- Background import of self-contained static GLB, preserving node hierarchy and instances.
- Add/select/delete model roots and cubes; edit transforms/tints/visibility.
- Read-only source-node tree, bounds-based framing and explicit import diagnostics.
- Orbit, pan, zoom and frame selected; wireframe and grid toggles.
- Session console, CPU frame interval graph, viewport size and draw counts.
- Parser/worker/editor tests and real GPU checks for imported meshes and resource lifetimes.

Only static TRIANGLES in glTF 2.0 GLB are supported. Animation, skins, morphs,
sparse/compressed/quantized geometry, transparency and external URIs are rejected.
Files and decoded upload geometry are each limited to 256 MiB. Textures/PBR
are reported as preview limitations.

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
