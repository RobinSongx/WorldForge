# WorldForge

**Windows-first 3D World Runtime & Asset Infrastructure.**

WorldForge is being built around a native editor for composing Mesh and Gaussian
worlds. This first **framework preview** implements the desktop shell and a real
OpenGL viewport. GLB import, PBR, project serialization, Gaussian rendering,
CLI/Agent tools and other platforms are planned, not implemented yet.

## Available in this preview

- SDL3 + Dear ImGui docking, saved panel layout and DPI-aware styling.
- OpenGL 4.5 offscreen viewport with a procedural cube and ground grid.
- Add/select/delete cubes, edit transforms/tints/visibility in the Inspector.
- Orbit, pan, zoom and frame selected; wireframe and grid toggles.
- Session console, CPU frame interval graph, viewport size and draw counts.
- CPU tests and a real GPU smoke run with a framebuffer capture and JSON report.

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
| Select | Scene panel |
| Edit object | Inspector; values are in world units / degrees |
| Add / delete | Scene menu or panel / Inspector |
| Restore layout | View → Reset layout |

For packaging, GPU validation and architecture notes, see
[Development](docs/DEVELOPMENT.md) and [Documentation index](docs/README.md).

MIT license for project-owned code; see [third-party notices](THIRD_PARTY_NOTICES.md).
