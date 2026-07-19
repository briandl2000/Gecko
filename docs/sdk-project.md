# Start a project from the Gecko SDK

The downloaded SDK contains Gecko's headers, shared library, and build driver.
Use Python 3.10 or newer and the compiler matching the SDK package: GCC on
Linux or `cl` from an MSVC Developer Command Prompt on Windows. The platform
and Vulkan runtime libraries must be installed normally; the SDK does not
bundle them. `glslc` is only required after the project declares shaders.

## 1. Create the project

Unpack the SDK anywhere, then create a separate directory with two files:

```text
HelloGecko/
  module.py
  main.cpp
```

`module.py` describes the executable and its engine dependency:

```python
from build import module

module(
    name="hello_gecko",
    output="executable",
    unity="main.cpp",
    requires=["gecko"],
)
```

`main.cpp` starts with the headless backends so the first build tests only the
SDK boundary:

```cpp
#include "gecko/gecko.h"

namespace {

constexpr gecko::Label AppLabel = gecko::MakeLabel("game.hello");

}  // namespace

int main()
{
  gecko::GeckoConfig config {};
  config.AppName = "Hello Gecko";
  config.Platform.Backend = gecko::platform::DisplayBackendKind::Null;
  config.Graphics.Backend = gecko::graphics::GraphicsBackend::Null;

  if (gecko::Initialize(config) != gecko::InitializeResult::Success)
    return 1;

  GECKO_INFO(AppLabel, "Hello from Gecko {}", gecko::VersionFullString());
  gecko::Shutdown();
  return 0;
}
```

## 2. Build and run

From `HelloGecko`, point at the SDK's build driver:

```sh
python3 /path/to/GeckoSDK/build.py .
```

On Windows, use `python` instead of `python3`. The Debug output is:

```text
out/Linux-x86_64/Debug/bin/hello_gecko
out/Windows-x86_64/Debug/bin/hello_gecko.exe
```

Run the program from that `bin` directory. The build copies `libGecko.so` or
`Gecko.dll` beside it. Build the Release configuration with:

```sh
python3 /path/to/GeckoSDK/build.py . --config release
```

Every successful build also writes `compile_commands.json` in the project
directory so Zed/clangd can navigate the real compiler configuration.
C++ completion and navigation therefore work without copying editor flags.
For Python completion in `module.py`, add the SDK directory to Zed's workspace
or Python analysis paths so it can resolve the SDK's `build.py` module.

## 3. Grow from the working boundary

Remove the two Null-backend assignments when the project is ready to create a
real platform window and Vulkan resources. Keep one `module.py` per subsystem,
add its directory to `requires`, and declare shaders or platform libraries in
the module that owns them. The [build-system guide](build-system.md) documents
those fields.

Use `output="plugin"` instead of `output="executable"` for a game shared
library. A plugin is not standalone: it needs a host that initializes Gecko,
loads the library, and resolves `GeckoPlugin_GetApi`. Gecko's launcher and
sandbox are the reference host/plugin pair.

When shipping a game with the Gecko shared library, place `LICENSE` and
`THIRD_PARTY_NOTICES.txt` in the distribution. The MIT license applies to
Gecko, not to the game's own code or assets.
