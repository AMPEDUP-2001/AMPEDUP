# Build from source on Windows

The [Windows installers](https://github.com/AMPEDUP-2001/AMPEDUP/releases/latest)
compile the game during installation. Choose the online download to fetch the
build tools during setup or the all-in-one download to install offline.
Follow this guide to work on the source or reproduce the release.

Build on 64-bit Windows. Install Python 3.11 or newer, CMake 3.20 or newer and Ninja. Make them available on PATH. Use a 64-bit Python environment for guest disassembly and visual asset generation. The release installers use Python 3.11.9 with these dependencies.

```powershell
python -m pip install capstone==5.0.9 numpy==2.2.6 Pillow==11.3.0
```

Use short paths for the game image, extracted files and install directory. The installer accepts ASCII paths and supports spaces.

## Dependencies

Make a dependency directory with the following layout. The compiler, SDL2, Vulkan headers and glslang versions below match the development build.

| Relative path | Contents |
|---|---|
| `windows-deps/llvm-mingw/` | [LLVM-MinGW 20260922 x86_64 UCRT](https://github.com/mstorsjo/llvm-mingw/releases/tag/20260922), with `bin/` at this level |
| `windows-deps/SDL2/` | [SDL2 2.32.10 MinGW development archive](https://github.com/libsdl-org/SDL/releases/tag/release-2.32.10), with `x86_64-w64-mingw32/` and `LICENSE.txt` |
| `windows-deps/glslang/` | The installed CMake package from [glslang 16.6.0](https://github.com/KhronosGroup/glslang/releases/tag/16.6.0), compiled for the same target |
| `windows-deps/glslang-src/` | The glslang source directory, which provides the licence files used by setup |
| `windows-deps/vulkan/` | [Vulkan headers 1.4.363.0](https://github.com/KhronosGroup/Vulkan-Headers) under `include/` and `lib/libvulkan-1.dll.a` |

For glslang, use the included Windows toolchain and set `CMAKE_BUILD_TYPE=Release`, `ENABLE_OPT=OFF` and `BUILD_SHARED_LIBS=OFF`. Set `CMAKE_INSTALL_PREFIX` to the glslang install directory and run `cmake --build <glslang-build> --target install`. The standalone installer builds its disc reader from the vendored toolkit.

## Generate and compile

Extract your own supported XISO with `extract-xiso.exe -x -d <disc-folder> <image.iso>` and pass the extracted `default.xbe` to the build. Run from the repository root.

```powershell
./Amped-Host/packaging/release/build-local.ps1 `
  -Xbe C:/Games/AmpedDisc/default.xbe `
  -DependencyRoot C:/AmpedDeps -Installer
```

Regeneration verifies the executable and the course archives. Keep the extracted Resource folder beside default.xbe. The build derives the matching mesh data and HD Texture Pack from those local files. It builds the game and packages `Amped-Host/dist/AMPEDUP-Private/Setup-AMPEDUP.exe`. The `-ReuseGenerated` switch resumes compilation after successful regeneration. The `Analysis/` and `Amped-Host/build/` directories hold local build output.

Use a public installer from the section below for the full native menu setup. Select your XISO, installation folder and desktop shortcut option. The updated standalone setup accepts a repack with the same 610 files. To create one from a verified extracted folder, run `extract-xiso.exe -m -c <disc-folder> <new-image.iso>`. The -m option preserves the executable bytes. The install keeps saves and settings in its `profile/` subdirectory. Windows may show a publisher prompt because this preview is unsigned.

## Package the public installers

From a clean, committed checkout, run the package builder with the dependency
directory above.

```powershell
./Amped-Host/packaging/windows/build-package.ps1 -PackageName AMPEDUP-Online -DependencyRoot C:/AmpedDeps -Online
./Amped-Host/packaging/windows/build-package.ps1 -PackageName AMPEDUP-Offline -DependencyRoot C:/AmpedDeps -ToolArchives C:/AmpedToolArchives
```

The download URLs and SHA-256 hashes are listed in
[`build-tool-archives.json`](../Amped-Host/packaging/windows/build-tool-archives.json).
For the all-in-one build, put the LLVM-MinGW zip in `windows-deps/dl/` and
the other listed files in the directory passed to `-ToolArchives`.

## Package for RecompLauncher

```powershell
./Amped-Host/packaging/windows/build-package.ps1 -PackageName AMPEDUP-0.7.1-RecompLauncher -DependencyRoot C:/AmpedDeps -RecompLauncher
```

This produces a Windows x64 ZIP with `recomplauncher.json` beside
`AMPEDUP!.exe`. Easy Setup calls `--prepare --game <path>` with the supported
XISO or extracted game folder. Setup verifies the player's copy and compiles
the game. `--play` starts it directly. The prepared build is in `runtime/`,
and settings, saves, and logs are in `profile/` at the top of the installation.

RecompLauncher identifies platform downloads by filename. Include `source`
in the filename of any developer handoff attached to a player release (for
example, `AMPEDUP-Linux-source-handoff.zip`). Reserve platform build names
such as `AMPEDUP-Linux-x64.zip` for tested, playable builds.

The XISO listed for verification in the manifest has CRC32 `922d8f9c` and
size 2,016,346,112 bytes. Both values were computed from the verified
European multilingual XISO that matches the project's documented SHA-256.
