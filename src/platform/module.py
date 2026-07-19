from build import build_options, module, wayland_protocol

module(
    name="platform",
    unity="gecko_platform.cpp",
    requires=["../core", "../math"],
    windows=build_options(
        libraries=["user32.lib", "shell32.lib", "shcore.lib", "ole32.lib", "winmm.lib"],
    ),
    linux=build_options(
        defines=[
            "GECKO_PLATFORM_LINUX_X11=1",
            "GECKO_PLATFORM_LINUX_WAYLAND=1",
            "GECKO_HAS_XKBCOMMON=1",
            "GECKO_HAVE_XDG_DECORATION=1",
        ],
        libraries=["-pthread", "-ldl", "-lm"],
        packages=["wayland-client", "wayland-cursor", "xkbcommon", "x11", "xrandr"],
        wayland_protocols=[
            wayland_protocol("xdg-shell", "stable/xdg-shell/xdg-shell.xml"),
            wayland_protocol("xdg-decoration", "unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"),
        ],
    ),
)
