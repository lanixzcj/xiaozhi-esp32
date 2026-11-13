#include <esp_log.h>
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application.h"
#include "controlled_trial.h"

#define TAG "main"

extern "C" void app_main(void)
{
    // Initialize NVS flash for WiFi configuration
    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "STARTUP STOP: NVS init failed (%s); NVS was not erased",
                 esp_err_to_name(ret));
        return;
    }

    if (ControlledTrial::Enabled()) {
        ESP_LOGW(TAG,
                 "CONTROLLED TRIAL: offline; settings read-only; no OTA validity confirmation");
    }

    if (!ControlledTrial::Enabled() && ControlledTrial::FirmwareProtected()) {
        ESP_LOGW(TAG,
                 "CUSTOM NETWORK: normal chat/game/volume; remote firmware upgrades and automatic "
                 "OTA confirmation disabled");
    }
    // Initialize and run the application
    auto& app = Application::GetInstance();
    app.Initialize();
    app.Run();  // This function runs the main event loop and never returns
}
