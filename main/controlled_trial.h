#pragma once
#include "sdkconfig.h"

// One build-time switch, default off. No NVS flag can enable/disable this mode.
namespace ControlledTrial {
constexpr bool Enabled() {
#if defined(CONFIG_CONTROLLED_TRIAL) && CONFIG_CONTROLLED_TRIAL
    return true;
#else
    return false;
#endif
}
constexpr bool FirmwareProtected() {
#if defined(CONFIG_CUSTOM_FIRMWARE_PROTECTION) && CONFIG_CUSTOM_FIRMWARE_PROTECTION
    return true;
#else
    return Enabled();
#endif
}
constexpr bool PreserveSharedAssets() {
#if defined(CONFIG_CUSTOM_PRESERVE_SHARED_ASSETS) && CONFIG_CUSTOM_PRESERVE_SHARED_ASSETS
    return true;
#else
    return Enabled();
#endif
}
// Logical codec volume only; this is not a calibrated sound-pressure limit.
constexpr int ClampOutputVolume(int volume) {
    return Enabled() ? (volume < 0 ? 0 : (volume > 5 ? 5 : volume)) : volume;
}
}  // namespace ControlledTrial
