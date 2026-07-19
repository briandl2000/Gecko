from build import build_options, module

module(
    name="graphics",
    unity="gecko_graphics.cpp",
    requires=["../core", "../math", "../platform"],
    windows=build_options(
        defines=["GECKO_GRAPHICS_VULKAN=1", "GECKO_GRAPHICS_VULKAN_WIN32=1"],
        libraries=["vulkan-1.lib"],
    ),
    linux=build_options(
        defines=[
            "GECKO_GRAPHICS_VULKAN=1",
            "GECKO_GRAPHICS_VULKAN_XLIB=1",
            "GECKO_GRAPHICS_VULKAN_WAYLAND=1",
        ],
        packages=["vulkan"],
    ),
)
