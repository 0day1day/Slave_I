/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal/hal_esp32.h"
#include "core/diagnostics/log.hpp"
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <bsp/m5stack_tab5.h>
#include <esp_sleep.h>
#include <esp_check.h>
#include <esp_pm.h>

static const std::string _tag = "power";

void HalEsp32::updatePowerMonitorData()
{
    // spectra5::log::tagInfo(_tag, "update power monitor");
    powerMonitorData.busVoltage   = ina226.readBusVoltage();
    powerMonitorData.shuntVoltage = ina226.readShuntVoltage();
    powerMonitorData.busPower     = ina226.readBusPower();
    powerMonitorData.shuntCurrent = ina226.readShuntCurrent();
}

void HalEsp32::setChargeQcEnable(bool enable)
{
    _charge_qc_enable = enable;
    spectra5::log::tagInfo(_tag, "set charge qc enable: {}", _charge_qc_enable);
    bsp_set_charge_qc_en(_charge_qc_enable);
}

bool HalEsp32::getChargeQcEnable()
{
    return _charge_qc_enable;
}

void HalEsp32::setChargeEnable(bool enable)
{
    _charge_enable = enable;
    spectra5::log::tagInfo(_tag, "set charge enable: {}", _charge_enable);
    bsp_set_charge_en(_charge_enable);
}

bool HalEsp32::getChargeEnable()
{
    return _charge_enable;
}

void HalEsp32::setUsb5vEnable(bool enable)
{
    _usba_5v_enable = enable;
    spectra5::log::tagInfo(_tag, "set usb 5v enable: {}", _usba_5v_enable);
    bsp_set_usb_5v_en(_usba_5v_enable);
}

bool HalEsp32::getUsb5vEnable()
{
    return _usba_5v_enable;
}

void HalEsp32::setExt5vEnable(bool enable)
{
    _ext_5v_enable = enable;
    spectra5::log::tagInfo(_tag, "set ext 5v enable: {}", _ext_5v_enable);
    bsp_set_ext_5v_en(_ext_5v_enable);
}

bool HalEsp32::getExt5vEnable()
{
    return _ext_5v_enable;
}

void HalEsp32::powerOff()
{
    spectra5::log::tagInfo(_tag, "power off");

    playShutdownSfx();
    setDisplayBrightness(0);

    delay(100);
    while (1) {
        if (getMusicPlayTestState() == hal::HalBase::MUSIC_PLAY_IDLE) {
            break;
        }
        delay(100);
    }

    bsp_generate_poweroff_signal();
}

extern esp_lcd_touch_handle_t _lcd_touch_handle;

void HalEsp32::sleepAndTouchWakeup()
{
    spectra5::log::tagInfo(_tag, "sleep and touch wakeup");

    lvglLock();
    auto brightness = getDisplayBrightness();
    setDisplayBrightness(0);

    uint16_t touch_x[1];
    uint16_t touch_y[1];
    uint16_t touch_strength[1];
    uint8_t touch_cnt = 0;

    while (1) {
        esp_lcd_touch_read_data(_lcd_touch_handle);
        bool touchpad_pressed =
            esp_lcd_touch_get_coordinates(_lcd_touch_handle, touch_x, touch_y, touch_strength, &touch_cnt, 1);
        // spectra5::log::tagInfo(_tag, "touchpad pressed: {}", touchpad_pressed);
        if (!touchpad_pressed) {
            break;
        }
        delay(100);
    }

    while (1) {
        esp_lcd_touch_read_data(_lcd_touch_handle);
        bool touchpad_pressed =
            esp_lcd_touch_get_coordinates(_lcd_touch_handle, touch_x, touch_y, touch_strength, &touch_cnt, 1);
        // spectra5::log::tagInfo(_tag, "touchpad pressed: {}", touchpad_pressed);
        if (touchpad_pressed) {
            break;
        }
        delay(100);
    }

    // esp_restart();

    lvglUnlock();
    delay(500);
    setDisplayBrightness(brightness);
}

void HalEsp32::sleepAndRtcWakeup()
{
    spectra5::log::tagInfo(_tag, "start sleep and rtc wakeup");

    clearRtcIrq();
    clearImuIrq();

    delay(200);

    spectra5::log::tagInfo(_tag, "set rtc alarm");
    struct tm time;
    rx8130.getTime(&time);
    time.tm_hour = 0;
    time.tm_min  = 1;
    time.tm_sec  = 49;
    rx8130.setTime(&time);
    time.tm_hour = 0;
    time.tm_min  = 2;
    time.tm_sec  = 0;
    rx8130.setAlarmIrq(&time);

    // delay(800);
    powerOff();
    while (1) {
        delay(100);
    }
}
