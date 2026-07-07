file(GLOB_RECURSE APP_LAYER_SRCS
    ../../../app/*.c
    ../../../app/*.cc
    ../../../app/*.cpp
)

set(APP_LAYER_INCS
    ../../../app
    ../../../dependencies/json
)

file(GLOB_RECURSE MY_HAL_SRCS
    ./hal/*.c
    ./hal/*.cc
    ./hal/*.cpp
)

file(GLOB_RECURSE MY_SERVICES_SRCS
    ./services/*.c
    ./services/*.cc
    ./services/*.cpp
)

idf_component_register(SRCS "app_main.cpp" "boot_splash.cpp" "about_overlay.cpp" "power_control.cpp" ${APP_LAYER_SRCS} ${MY_HAL_SRCS} ${MY_SERVICES_SRCS}
                    INCLUDE_DIRS "." ${APP_LAYER_INCS}
                    EMBED_TXTFILES "../audio/canon_in_d.mp3" "../audio/startup_sfx.mp3" "../audio/shutdown_sfx.mp3"
                    EMBED_FILES "assets/helmet.png" "assets/crest.png"
                    PRIV_REQUIRES spectra5_ble m5stack_tab5 power_monitor_ina226 esp_lvgl_port lvgl
                                sensor_bmi270 esp_video imlib usb esp_http_server espressif__usb_host_hid
                                esp_wifi espressif__esp_wifi_remote nvs_flash fatfs spiffs)

# Diagnostic-only build: `idf.py -DSPECTRA5_RADIO_CAPTEST=1 build` (see
# scripts/captest-tab5.sh) compiles the radio capability probe into app_main.
if(SPECTRA5_RADIO_CAPTEST)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE SPECTRA5_RADIO_CAPTEST=1)
    message(STATUS "Spectra5: radio capability probe ENABLED (diagnostic build)")
endif()

# The ESP32-C6 radio firmware is embedded in the P4 app so the P4 can auto-provision
# the C6 over OTA on first boot (no microSD, no separate deploy step). The normal
# firmware checks an NVS CRC marker and only (re)flashes the C6 when the image differs.
# Build the C6 image first:  ./scripts/build-c6.sh
set(C6_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components/espressif__esp_hosted/examples/slave/build/network_adapter.bin")
if(NOT EXISTS "${C6_IMAGE}")
    message(FATAL_ERROR "C6 image not found: ${C6_IMAGE}\n  Run ./scripts/build-c6.sh first.")
endif()
target_add_binary_data(${COMPONENT_LIB} "${C6_IMAGE}" BINARY)
message(STATUS "Slave I: embedding C6 image for auto-provision (${C6_IMAGE})")

# Deploy-only build: provision the C6 then halt (legacy manual path, still works).
# `idf.py -DSPECTRA5_C6_OTA=1 build`.
if(SPECTRA5_C6_OTA)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE SPECTRA5_C6_OTA=1)
endif()
