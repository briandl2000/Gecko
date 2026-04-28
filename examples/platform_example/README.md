# platform_example

Interactive showcase for `gecko::platform`: window creation, monitor
info, the input service, and the full main-loop pump (`PumpEvents` →
`DispatchEvents` → `IInput` poll → frame mark).

> Requires a graphical session (X11 or Wayland on Linux, native on
> Windows). It is a feature test — `gk test debug --feature` only.

## Run

```bash
gk run platform_example debug
```

## Keyboard

| Key  | Action                                     |
|------|--------------------------------------------|
| 1-4  | Spawn extra window (small / large / borderless / fullscreen) |
| W    | Close last spawned window                  |
| D    | Toggle decorations on the main window      |
| R    | Toggle resizable                           |
| F    | Toggle borderless fullscreen               |
| T    | Toggle always-on-top                       |
| M    | Cycle Normal -> Maximized -> Minimized     |
| 5/6/7| Toggle close/minimize/maximize button      |
| 8    | Restore all title-bar buttons              |
| 9    | Set min/max size constraints               |
| 0    | Clear constraints                          |
| H    | Re-print help                              |
| I    | Print full window info                     |
| P    | Print input snapshot (mouse, modifiers)    |
| Esc  | Quit                                       |

The example also subscribes to `WindowFocusChanged`, `WindowMoved`,
`WindowResized`, `WindowStateChanged` and `WindowKey`, and polls
`gecko::platform::GetInput()` every frame for mouse-button edges and
focus/hover transitions.

## Files

| File | Role |
|---|---|
| [src/App.h](src/App.h) / [src/App.cpp](src/App.cpp) | Owns services, modules, the main window, event subscriptions, and the keyboard dispatch table. |
| [src/main.cpp](src/main.cpp) | Constructs `App`, calls `Run`, prints the exit message. |

The keyboard switch is split into grouped methods on `App` (`HandleSpawnKey`,
`HandleMainWindowKey`, `HandleTitleBarButtonsKey`,
`HandleSizeConstraintsKey`) plus three info printers
(`PrintHelp`, `PrintWindowInfo`, `PrintInputSnapshot`).

## See also

- Boot pattern: [app_skeleton](../app_skeleton)
- Windowing design notes: [docs/windowing.md](../../docs/windowing.md)
