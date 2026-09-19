# WorldForge repository instructions

All project-owned C++ must strictly follow the
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
Read [docs/CODING_STYLE.md](docs/CODING_STYLE.md) before changing C++.

- Use C++20, lowercase snake_case `.cc` / `.h` filenames, Google names,
  self-contained headers and `WORLDFORGE_<REPOSITORY_PATH>_H_` guards.
- Use the pinned clang-format Google preset and cpplint. Do not weaken rules
  to make a change pass. Third-party source and generated code retain upstream
  conventions and are excluded from our source scan.
- Do not introduce C++ exception handling or prohibited library facilities
  such as `<filesystem>`. Return explicit errors; use RAII for cleanup.
- Run `build/python/Scripts/python.exe tools/check_style.py`,
  the Windows build and relevant tests before completing a C++ change.
  Use `--fix` to apply formatting. Report checks that could not be run.
- Keep architecture, decisions, review findings and development notes in docs/.
- Preserve the Windows-first roadmap; CLI/Agent, plugins and other platforms
  remain later work.
