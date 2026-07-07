#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

namespace spectra5::platform {

// Flash the ESP32-C6 with a firmware image read from the microSD, over the
// esp_hosted OTA RPC (no cable, no HTTP). Drives rpc_ota_begin/write/end with the
// file bytes; the C6 reboots into the new image (dual-slot A/B, auto-rollback on
// boot failure). Use this to deploy our custom C6 radio-engine firmware.
//
// Requires the esp_hosted transport/RPC to be up (call after Wi-Fi has been
// initialised at least once). Returns ESP_OK on a verified flash.
esp_err_t flash_c6_from_file(const char* path);

// Same, but from an in-memory image (e.g. the C6 firmware embedded in the P4
// deploy build) — no microSD needed.
esp_err_t flash_c6_from_buffer(const uint8_t* data, std::size_t len);

// --- First-boot auto-provisioning -------------------------------------------
// The C6 radio firmware is embedded in the P4 app. On boot the P4 compares that
// image's CRC32 against what NVS says it last flashed to the C6; if they differ
// (fresh unit, or the C6 image changed with a firmware update) the C6 needs
// (re)provisioning. Cheap — no transport required.
bool c6_needs_provision();

// Bring up the minimal hosted stack, OTA-push the embedded C6 image, record its
// CRC in NVS on success, then esp_restart() for a clean normal boot. On failure it
// logs and RETURNS so the normal app still starts (the radio reports unprovisioned).
// Mirrors the proven deploy path, so it must run after app::Init().
void provision_c6_and_reboot();

}  // namespace spectra5::platform
