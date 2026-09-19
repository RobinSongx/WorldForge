# Third-party dependencies

Dependencies are downloaded at configure time, not vendored into this repository.
Exact source commits are pinned in CMakeLists.txt. Installed packages include
the upstream license texts in `licenses/`.

| Dependency | Version / commit | License |
| --- | --- | --- |
| SDL3 | release-3.2.28 / 7f3ae3d57459e59943a4ecfefc8f6277ec6bf540 | zlib |
| Dear ImGui | v1.92.5-docking / af84778eeaab75bee0b54420f3157cf3ee00618f | MIT |
| GLM | 1.0.1 / 0af55ccecd98d4e5a8d1fad7de25ba429d60e863 | MIT or Happy Bunny, use MIT |
| glad | v2.0.8 / 73db193f853e2ee079bf3ca8a64aa2eaf6459043 | MIT; generated Khronos declarations carry their own notices |

Build-only Python dependencies: Jinja2 3.1.6 (BSD-3-Clause), MarkupSafe 3.0.3
(BSD-3-Clause). They are not distributed with Studio. Generated glad sources
retain their Khronos/loader notices; the glad license included in the package
describes generated-code licensing. Microsoft runtime redistribution is governed
by the Visual Studio license, not the WorldForge MIT license.
