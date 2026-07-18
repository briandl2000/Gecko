#pragma once

#include "gecko/core/api.h"
#include "gecko/platform/monitors_interface.h"
#include "gecko/platform/windows_interface.h"

namespace gecko::platform {

[[nodiscard]] GECKO_API IWindowsBackend* GetWindows() noexcept;
[[nodiscard]] GECKO_API IMonitorsBackend* GetMonitors() noexcept;
GECKO_API void PumpEvents() noexcept;
GECKO_API void SetModalFrameCallback(IWindowsBackend::ModalFrameFn callback, void* userData) noexcept;

}  // namespace gecko::platform
