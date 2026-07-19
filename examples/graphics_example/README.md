# graphics_example

The full original two-window Vulkan demonstration, ported to the single Gecko
library:

- a vertex-buffer triangle rendered to an offscreen target;
- indirect drawing and explicit timestamp queries;
- a compute-generated plasma texture;
- fullscreen blits into two swapchains;
- GPU profiler zones, resize handling, and a periodic performance HUD.

Its HLSL files are compiled to SPIR-V by the direct build script and embedded in
the executable through a generated C++ header. No shader directory is needed at
runtime. Press `Escape` to quit and `F4` to dump current profiler statistics.
