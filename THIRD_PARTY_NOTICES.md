# Third-party dependencies

Dependencies are downloaded at configure time, not vendored into this repository.
Exact source commits are pinned in CMakeLists.txt. Installed packages include
the upstream license texts in `licenses/`.

| Dependency | Version / commit | License |
| --- | --- | --- |
| SDL3 | release-3.2.28 / 7f3ae3d57459e59943a4ecfefc8f6277ec6bf540 | zlib |
| Dear ImGui | v1.92.5-docking / af84778eeaab75bee0b54420f3157cf3ee00618f | MIT |
| GLM | 1.0.1 / 0af55ccecd98d4e5a8d1fad7de25ba429d60e863 | MIT or Happy Bunny, use MIT |
| tinygltf | v3.0.0-alpha C API / 0d9ce1a9561688c6c6e7471430d15a50a0b15f1a | MIT (including bundled tinygltf_json_c.h) |
| LodePNG | 20190210, bundled in the pinned tinygltf archive above | zlib-style LodePNG license |
| glad | v2.0.8 / 73db193f853e2ee079bf3ca8a64aa2eaf6459043 | MIT; generated Khronos declarations carry their own notices |

Build-only Python dependencies: Jinja2 3.1.6 (BSD-3-Clause), MarkupSafe 3.0.3
(BSD-3-Clause). They are not distributed with Studio. Generated glad sources
retain their Khronos/loader notices; the glad license included in the package
describes generated-code licensing. Microsoft runtime redistribution is governed
by the Visual Studio license, not the WorldForge MIT license.

tinygltf is compiled as C11 with filesystem and image-decoder integrations
disabled. The C++ adapter owns parsed-model cleanup and copies validated data
into project types. PNG decoding uses the bundled LodePNG C API, compiled without
its C++ wrappers, encoder, filesystem or ancillary metadata processing. The source
is unmodified. WorldForge supplies bounded allocation hooks and validates PNG
chunk ranges/CRCs, dimensions and aggregate decoded image sizes. The allocator
hook names retain the exact identifiers required by the upstream interface.
The optional real-model acceptance test uses the Khronos
Duck sample, copyright Sony 2006, under the SCEA Shared Source License 1.0;
the model is downloaded separately and is not included in source or packages.
See docs/benchmarks/2026-09-20-mesh-import.md for source, hash and attribution.
