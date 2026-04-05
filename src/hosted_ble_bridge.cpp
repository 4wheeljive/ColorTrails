#include "hosted_ble_bridge.h"

#if defined(ARDUINO_ARCH_ESP32) && __has_include("esp_hosted.h") && __has_include("esp_hosted_misc.h")

#include <Arduino.h>
#include <esp_event.h>
#include <esp_hosted.h>
#include <esp_hosted_misc.h>
#include <nvs_flash.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

constexpr int kConnectAttempts = 5;
constexpr int kConnectDelayMs = 250;
constexpr int kBleBringupAttempts = 2;
constexpr int kBleRetryDelayMs = 700;

esp_err_t initNvsIfNeeded() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            return err;
        }
        err = nvs_flash_init();
    }
    return err;
}

void ensureDefaultEventLoop() {
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        Serial.printf("[hosted-ble] esp_event_loop_create_default failed: %s\n", esp_err_to_name(err));
    }
}

bool ensureHostedTransportReady() {
    int hostRet = esp_hosted_init();
    if (hostRet != ESP_OK && hostRet != ESP_ERR_INVALID_STATE) {
        Serial.printf("[hosted-ble] esp_hosted_init failed: %s\n", esp_err_to_name(static_cast<esp_err_t>(hostRet)));
        return false;
    }

    for (int attempt = 1; attempt <= kConnectAttempts; ++attempt) {
        hostRet = esp_hosted_connect_to_slave();
        if (hostRet == ESP_OK || hostRet == ESP_ERR_INVALID_STATE) {
            return true;
        }

        Serial.printf("[hosted-ble] esp_hosted_connect_to_slave attempt %d/%d failed: %s\n",
                      attempt,
                      kConnectAttempts,
                      esp_err_to_name(static_cast<esp_err_t>(hostRet)));
        vTaskDelay(pdMS_TO_TICKS(kConnectDelayMs));
    }

    return false;
}

bool bringUpHostedBleController() {
    esp_err_t initErr = esp_hosted_bt_controller_init();
    if (initErr != ESP_OK && initErr != ESP_ERR_INVALID_STATE) {
        Serial.printf("[hosted-ble] esp_hosted_bt_controller_init failed: %s\n", esp_err_to_name(initErr));
        return false;
    }

    esp_err_t enableErr = esp_hosted_bt_controller_enable();
    if (enableErr != ESP_OK && enableErr != ESP_ERR_INVALID_STATE) {
        Serial.printf("[hosted-ble] esp_hosted_bt_controller_enable failed: %s\n", esp_err_to_name(enableErr));
        return false;
    }

    return true;
}

void resetHostedStackForRetry() {
    esp_hosted_bt_controller_disable();
    esp_hosted_bt_controller_deinit(false);
    esp_hosted_deinit();
    vTaskDelay(pdMS_TO_TICKS(kBleRetryDelayMs));
}

}  // namespace

bool hostedBlePrepare() {
    esp_err_t nvsErr = initNvsIfNeeded();
    if (nvsErr != ESP_OK) {
        Serial.printf("[hosted-ble] NVS init failed: %s\n", esp_err_to_name(nvsErr));
        return false;
    }

    ensureDefaultEventLoop();

    for (int attempt = 1; attempt <= kBleBringupAttempts; ++attempt) {
        if (!ensureHostedTransportReady()) {
            if (attempt < kBleBringupAttempts) {
                Serial.println("[hosted-ble] Transport not ready, retrying BLE bring-up");
                resetHostedStackForRetry();
                continue;
            }
            return false;
        }

        if (bringUpHostedBleController()) {
            Serial.println("[hosted-ble] Hosted BLE controller ready");
            return true;
        }

        if (attempt < kBleBringupAttempts) {
            Serial.println("[hosted-ble] BLE controller init failed, retrying once");
            resetHostedStackForRetry();
        }
    }

    return false;
}

#else

bool hostedBlePrepare() {
    // Non-ESP-Hosted builds can continue with regular NimBLE init.
    return true;
}

#endif
