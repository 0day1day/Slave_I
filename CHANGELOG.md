# Changelog

All notable changes to Slave I are documented here.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

## [0.1.0] — first private cut

### Fixed
- RTC (RX8130) no longer syncs a garbage/implausible time (e.g. `08:75` or a wrong
  year) as the system clock on boot. It's validated against the firmware build year;
  if implausible, the build date is seeded and written back so the clock stays sane
  until the user sets the real time in Settings (which persists to the RTC).

### Changed (KPI honesty pass)
- Dashboard: wired `Radio Activity`, `Devices` (Wi-Fi+BLE seen) and `Tasks`
  (FreeRTOS) to real data; C6 status reports Connected only with real evidence
  (scan in progress / results received). Removed dead cards that never populated
  (`Active Session`, `Alerts`, `Ext. Peripherals`).
- Removed synthetic/fake observations from the (nav-disabled) Workflows demo.
- RF / NFC / IR clearly labelled "PLANNED — ULTRA board (gauging community interest)".

### Added (single-flash install)
- **C6 auto-provisioning**: the ESP32-C6 radio firmware is embedded in the P4 app
  and flashed over the air automatically on first boot (or after a C6 image change),
  gated by an NVS CRC marker so normal boots skip it. Collapses install into a
  single flash — no microSD, no separate deploy firmware. A "ARMING RADIO" overlay
  with a spinner shows during the one-time ~15 s provisioning.

First consolidated build of the offensive-security firmware for the M5Stack Tab5
(ESP32-P4 UI + ESP32-C6 radio over `esp_hosted`).

### Added
- Wi-Fi: 2.4/5 GHz scan, Evil Twin, deauth, captive portals (built-in + SD
  templates), handshake / EAPOL capture to `.pcap`.
- BLE: scan with vendor resolution (company-ID + OUI database).
- Nearby: unified Wi-Fi + BLE radar.
- 802.15.4 / Zigbee: energy scan, device sniffer (PAN / EUI-64 / vendor / LQI),
  frame capture to `.pcap`, all-channel hop mode.
- File manager with create/delete/copy/cut/paste and a text editor.
- RTC date/time (persistent), structured SD logging, live KPIs.
- Physical-keyboard navigation (M5 I2C smart keyboard @0x6D + USB HID).
- IBM Plex Mono UI (Field Armor palette), boot splash, About screen.
- Desktop LVGL/SDL emulator for the full UI.

### Known limitations
- RF (CC1101) / NFC / IR / ULTRA external module: planned, hardware not built.
- ESP-NOW: deferred.
