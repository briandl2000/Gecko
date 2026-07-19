# Gecko

Gecko is a handmade C++26 game engine for learning, experiments, and games. It builds as one shared library used by executables and optional plugins.

## Build

Gecko uses one Python 3.10+ build driver with no third-party Python packages:

```sh
python3 build.py engine
python3 build.py sandbox
python3 build.py sdk-test
python3 build.py sdk
```

The target comes first and Debug is the default; use `--config release` when needed. `python3 build.py --help` lists the complete interface, and `python3 build.py graph` prints the engine module order.

On Windows, run the same commands with `python` from an MSVC Developer Command Prompt. Linux uses GCC. Building Gecko from source also requires the Vulkan development files and the Wayland/X11 development packages; projects with shaders require `glslc`.

`sandbox` builds the launcher and its shader-capable plugin against the current engine. `sdk-test` stages an SDK and rebuilds the same launcher/plugin exclusively through that public package. `sdk` produces the downloadable Debug and Release SDK under `out/sdk`.

Every project or plugin has a small `module.py`:

```python
from build import module

module(
    name="my_project",
    output="executable",
    unity="src/main.cpp",
    requires=["gecko"],
)
```

Engine areas use the same description. Dependencies form a checked directed graph, and each source module contributes one unity object to the single Gecko shared library.

Build an external module with either a source checkout or an unpacked SDK:

```sh
python3 ../Gecko/build.py ../MyProject
python3 ../GeckoSDK/build.py ../MyProject --config release
```

See the [build-system guide](docs/build-system.md), [development workflow](docs/development.md), [architecture](docs/architecture.md), and [coding style](docs/coding-style.md).

Development releases use `v0.0.0-alpha.N`. Pull requests and pushes to `dev` or `main` build and test Linux and Windows SDKs; pushes publish the tested packages.

MIT licensed.
