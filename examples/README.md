# Gecko examples

The examples are small tours of the public Gecko API. Build all of them with:

```sh
./build.sh debug examples
```

On Windows, use `build.bat debug examples` from a Visual Studio Developer
Command Prompt.

- [`app_skeleton`](app_skeleton/README.md) is the minimal standalone
  application and the best starting point for a new experiment.
- [`core_example`](core_example/README.md) demonstrates memory, events,
  threading, jobs, logging, and profiling.
- [`math_example`](math_example/README.md) tours the header-only math API.
- [`platform_example`](platform_example/README.md) is the interactive window,
  monitor, and input showcase.
- [`graphics_example`](graphics_example/README.md) is the full Vulkan example,
  including embedded shaders, compute, multiple swapchains, and GPU profiling.

Start with `app_skeleton`, then read only the example for the subsystem you are
currently learning. The examples link the same shared Gecko library as the
launcher and game library; they do not contain a private engine copy.
