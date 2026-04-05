#pragma once

// Initializes ESP-Hosted transport + hosted BLE controller on ESP32-P4/ESP32-C6.
// Returns true when BLE host transport is ready for NimBLEDevice::init().
bool hostedBlePrepare();
