# Examples

These are small, deliberately direct programs built against the single Gecko
shared library:

- `core`: headless initialization, jobs, logging, and memory statistics.
- `window`: native Wayland/X11/Win32 window and event loop.
- `triangle`: Vulkan rendering with HLSL compiled to SPIR-V before the C++ link.

Build all examples with `./build.sh debug examples` or
`build.bat debug examples`. Run them from the generated `bin` directory so the
shared library and the triangle's `shaders` directory are beside the program.

The examples are learning references, not framework layers. Copy useful code
into an experiment and change it freely.
