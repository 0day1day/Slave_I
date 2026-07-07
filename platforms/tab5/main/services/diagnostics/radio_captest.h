#pragma once

// Spectra5 radio capability probe.
//
// Empirically validates, on real hardware, which offensive-radio primitives the
// ESP32-C6 exposes to the ESP32-P4 through the current esp_hosted / esp_wifi_remote
// transport. This is the go/no-go check for "1:1 with Marauder/Bruce/oink": those
// firmwares depend on monitor mode (promiscuous RX) and raw 802.11 injection
// (esp_wifi_80211_tx). The probe logs a PASS / UNSUPPORTED / FAIL matrix to serial.
//
// Compiled only when SPECTRA5_RADIO_CAPTEST is defined (see main/CMakeLists.txt
// and scripts/captest-tab5.sh). It runs once at boot and then halts, so it does
// not interfere with the normal firmware.

namespace spectra5::diagnostics {

void run_radio_captest();

}  // namespace spectra5::diagnostics
