# app_skeleton

The minimal starting point for a standalone Gecko application. It keeps the
original command-line options and demonstrates explicit engine initialization,
headless work, a native window/event loop, and explicit shutdown.

```sh
gecko_example_app_skeleton --no-window
gecko_example_app_skeleton --frames=120
gecko_example_app_skeleton --backend=wayland
gecko_example_app_skeleton --backend=xlib
```

Options are `--no-window`, `--frames=N`, `--title=TEXT`, and
`--backend=auto|null|wayland|xlib`.
