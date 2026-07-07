# Credits & Third-Party Licenses

Slave I stands on the shoulders of open-source work. Thanks to:

| Component | License | Use in Slave I |
|-----------|---------|----------------|
| [M5Stack **M5Tab5-UserDemo**](https://github.com/m5stack/M5Tab5-UserDemo) | MIT | Project base: BSP, board bring-up, desktop LVGL/SDL harness. |
| [Espressif **ESP-IDF** + **esp_hosted**](https://github.com/espressif/esp-idf) | Apache-2.0 | Toolchain and the P4↔C6 hosted-radio transport (used and modified). |
| [**LVGL**](https://github.com/lvgl/lvgl) | MIT | UI framework. |
| [**IBM Plex Mono/Sans**](https://github.com/IBM/plex) | OFL-1.1 | Interface typeface. |
| [M5PORKCHOP **wsl_bypasser**](https://github.com/) — © 2025 0ct0 | MIT | Injection sanity-check bypass technique (`ieee80211_raw_frame_sanity_check`). |

The raw-frame injection bypass is a well-known technique (Marauder / PORKCHOP /
`esp32_wifi_penetration_tool` lineage); the one-line credit above covers the small
snippet our C6 firmware notes as ported. Everything else is original work.

If you believe an attribution is missing or incorrect, open an issue —
we'll fix it.
