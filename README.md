# Gecko

Gecko is a handmade C++23 game engine for learning, experimentation, and shipping games. It builds as one shared engine library. Executables and game/plugin shared libraries all link that same library, so one process has one engine state and one allocation authority.

The development host is Linux with Wayland preferred and X11 retained as a fallback. Windows is the primary shipping target. Vulkan is the hardware graphics backend; a software renderer can live beside it later.

## Build

Linux:

```sh
bash build.sh debug
cd out/Linux-x86_64/handmade/Debug/bin
./gecko_launcher
```

Windows, from a Visual Studio Developer Command Prompt:

```bat
build.bat debug
out\Windows-x86_64\handmade\Debug\bin\gecko_launcher.exe
```

Use `release` instead of `debug`, or pass `clean` as the second argument. Linux builds are incremental; an unchanged build is effectively immediate. Pass `monolithic` as the second argument to produce one executable containing Gecko and the sandbox game while preserving the same game API (`bash build.sh release monolithic` or `build.bat release monolithic`).

For a display/GPU-independent smoke run, use `gecko_launcher --backend=null --graphics=null --frames=2`.

Outputs are the Gecko shared library, the launcher, and the sandbox game shared library. The game exports `GeckoGame_GetApi`; it also links Gecko normally and therefore resolves engine calls through the same `Gecko.dll` or `libGecko.so` already loaded by the launcher.

See [architecture](docs/architecture.md) and [coding style](docs/coding-style.md) for the deliberately small set of project rules.

## Versioning

Development releases continue as `v0.0.0-alpha.N`. A game records the engine version and ABI it was built against. Compatible engine releases keep the ABI number; breaking public C++ changes increment it and fail loading explicitly. The dynamic game function table has its own version for changes to that narrow contract.

MIT licensed.
