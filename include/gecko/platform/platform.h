#pragma once

#include "gecko/api.h"
#include "gecko/platform/clipboard.h"
#include "gecko/platform/input.h"
#include "gecko/platform/input_codes.h"
#include "gecko/platform/monitor.h"
#include "gecko/platform/monitors_interface.h"
#include "gecko/platform/path_view.h"
#include "gecko/platform/platform_config.h"
#include "gecko/platform/platform_events.h"
#include "gecko/platform/platform_io.h"
#include "gecko/platform/shared_library.h"
#include "gecko/platform/terminal.h"
#include "gecko/platform/threading.h"
#include "gecko/platform/window.h"
#include "gecko/platform/windows_interface.h"

namespace gecko::platform {

[[nodiscard]] GECKO_API IWindowsBackend* GetWindows() noexcept;
[[nodiscard]] GECKO_API IMonitorsBackend* GetMonitors() noexcept;
GECKO_API void PumpEvents() noexcept;
GECKO_API void SetModalFrameCallback(IWindowsBackend::ModalFrameFn callback, void* userData) noexcept;

}  // namespace gecko::platform
