-dev equivalent).
+5. “Could not find a package configuration file provided by "glfw3"” (glfw3Config.cmake / glfw3-config.cmake) — supply GLFW development package (libglfw3-dev equivalent).
+6. “Could not find a package configuration file provided by "SDL2"” (SDL2Config.cmake / sdl2-config.cmake) — supply libsdl2-dev only if choosing SDL2 instead of GLFW.
+7. “Could NOT find PkgConfig (missing: PKG_CONFIG_EXECUTABLE)” — supply pkg-config; mpg123 discovery treats it as optional, so this alone is not an engine blocker.
+Dependencies present: native pthreads and OpenGL development files; native OpenAL, mpg123 and SDL2 runtime libraries; vendored OpenAL/mpg123/SDL2 headers with Windows-oriented libraries.
+Dependencies missing: librw source/install; Linux OpenAL/mpg123 development discovery; GLFW headers/library/config; SDL2 system development config; pkg-config; AArch64 toolchain/sysroot.
+Optional ogg/opus/opusfile submodules are empty; leave REVC_WITH_OPUS=OFF for M0.
+M0 fixes: (1) supply pinned librw externally under /tmp; (2) supply native development dependencies and achieve a native executable; (3) supply AArch64 GCC/G++ and matching sysroot/dependencies, never host libraries.
+Then use a /tmp CMake toolchain with CMAKE_SYSTEM_NAME=Linux, CMAKE_SYSTEM_PROCESSOR=aarch64, cross compilers, sysroot and target-only library/include/package searches; rebuild and verify ELF Machine=AArch64.
+All generated files are under /tmp/revc-build; source-tree git status stayed clean. Logs: configure.log, build.log, probes/*.log.

tokens used
47,189
Native configuration failed before any engine objects compiled. ARM64 compatibility remains untested.

Build systems inspected: CMake, Premake, Android NDK configuration, and `autoconf/` (a Premake Lua module). Chosen path: out-of-tree CMake.
Host tools: GCC/G++ 11.4, CMake 3.22.1, make; no AArch64 GCC/G++ found.
Configure command: `cmake -S /home/pavliukov/ipodwork/NanoRework/games/reVC -B /tmp/revc-build/native -DCMAKE_BUILD_TYPE=Release -DLIBRW_PLATFORM=GL3 -DLIBRW_GL3_GFXLIB=GLFW`
Build command: `cmake --build /tmp/revc-build/native --parallel 2`
Compiler and pthread checks passed. Seven distinct failures were observed; there are no ten engine compiler errors to report.

1. `does not contain a CMakeLists.txt file.` — `vendor/librw` is empty; supply the pinned librw externally under `/tmp`, then use `REVC_VENDORED_LIBRW=OFF`.
2. `Could NOT find OpenAL (missing: OPENAL_LIBRARY OPENAL_INCLUDE_DIR)` — supply Linux OpenAL development headers/library.
3. `gmake: Makefile: No such file or directory` / `No rule to make target 'Makefile'. Stop.` — resolve configure failures and regenerate.
4. Probe: `Could NOT find mpg123 (missing: mpg123_LIBRARIES mpg123_INCLUDE_DIR)` — supply Linux mpg123 development headers/library.
5. Probe: `Could not find a package configuration file provided by "glfw3"` — supply GLFW headers/library/CMake config.
6. Probe: `Could not find a package configuration file provided by "SDL2"` — supply SDL2 development files only if selecting SDL2 instead of GLFW.
7. Probe: `Could NOT find PkgConfig (missing: PKG_CONFIG_EXECUTABLE)` — supply pkg-config; it is optional for this mpg123 finder.

Present: native pthreads/OpenGL development files; OpenAL, mpg123 and SDL2 runtime libraries; vendored headers with Windows-oriented libraries.
Missing: librw source/install, Linux OpenAL/mpg123 development files, GLFW, SDL2 system development config, pkg-config, AArch64 toolchain/sysroot.
M0 fixes: supply pinned librw and native dependencies under `/tmp`; achieve a native executable; then cross-build with AArch64 GCC/G++, a matching sysroot and target libraries. Verify the resulting ELF is AArch64.
Keep optional Opus disabled. No source changes were made; git status remained clean.
[Saved report](/tmp/revc-build/report.txt), [configure log](/tmp/revc-build/configure.log), [build log](/tmp/revc-build/build.log); dependency logs are under `/tmp/revc-build/probes/`.
