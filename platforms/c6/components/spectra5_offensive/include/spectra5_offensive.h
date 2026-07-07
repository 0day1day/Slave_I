#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Spectra5 offensive engine for the ESP32-C6 (radio side).
//
// The P4 sends a RadioCommand (opcode byte + little-endian params, matching
// app/domain/radio/offensive.hpp + offensive_codec) over the esp_hosted
// side-channel; the slave hands the bytes here. This module owns native raw
// injection on the C6. Returns 0 on success, negative on error.
//
// Frame layout + the ieee80211_raw_frame_sanity_check bypass are ported from
// M5PORKCHOP core/wsl_bypasser.cpp (MIT, (c) 2025 0ct0).
int spectra5_offensive_dispatch(const uint8_t *data, uint16_t len);

// Custom esp_priv event_type carrying a RadioCommand (distinct from
// ESP_PRIV_EVENT_INIT). The slave's process_priv_pkt routes this here.
#define SPECTRA5_PRIV_EVENT_COMMAND 0x55

#ifdef __cplusplus
}
#endif
