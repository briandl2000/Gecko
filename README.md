# Gecko

Gecko is a handmade C++26 game engine for learning, experimentation, and shipping games. It builds as one shared engine library. Executables and game/plugin shared libraries all link that same library, so one process has one engine state and one allocation authority.

The development host is Linux with Wayland preferred and X11 retained as a fallback. Windows is the primary shipping target. Vulkan is the hardware graphics backend; a software renderer can live beside it later.

## Build

Linux with GCC 14 or newer:

```sh
bash build.sh debug
cd out/Linux-x86_64/Debug/bin
./gecko_launcher
```

Windows, from a Visual Studio Developer Command Prompt:

```bat
build.bat debug
out\Windows-x86_64\Debug\bin\gecko_launcher.exe
```

Use `release` instead of `debug`, or pass `clean` as the second argument. Linux builds are incremental; an unchanged build is effectively immediate.

Build the original learning examples with `./build.sh debug examples`. The
graphics example compiles its HLSL to SPIR-V with `glslc`, then the small
`gecko_embed` build utility emits a generated C++26 header so the SPIR-V lives
inside the executable. Set `GLSLC` or install the Vulkan SDK if it is not on
`PATH`; no shader files are needed at runtime. See the [examples](examples/README.md)
for the recommended learning order.

In Zed, `Ctrl+Shift+R` opens the project task picker. Choose **Gecko: Build
Debug** for the normal incremental build or **Gecko: Build Examples** for all
five examples. The debug panel contains **Gecko: Build & Debug Launcher** and
builds before starting CodeLLDB, so `F4` is the quick build-and-debug path.

For a display/GPU-independent smoke run, use `gecko_launcher --backend=null --graphics=null --frames=2`.

Outputs are the Gecko shared library, the launcher, and the sandbox game shared library. The game exports `GeckoGame_GetApi`; it also links Gecko normally and therefore resolves engine calls through the same `Gecko.dll` or `libGecko.so` already loaded by the launcher.

See [architecture](docs/architecture.md) and [coding style](docs/coding-style.md) for the deliberately small set of project rules.

## Versioning

Development releases continue as `v0.0.0-alpha.N`. A game records the engine version and ABI it was built against. Compatible engine releases keep the ABI number; breaking public C++ changes increment it and fail loading explicitly. The dynamic game function table has its own version for changes to that narrow contract.

MIT licensed.

## Continuous builds

Pull requests and pushes to `dev` or `main` build release binaries and all five
examples on Linux and Windows. Pushes also publish zipped engine artifacts and
a versioned GitHub release; `dev` releases receive a timestamp and remain
prereleases.
