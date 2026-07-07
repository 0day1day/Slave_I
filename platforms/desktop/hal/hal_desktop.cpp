/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal_desktop.h"
#include <SDL2/SDL.h>
#include "core/diagnostics/log.hpp"
#include <random>
#include <filesystem>
#include <thread>

static const std::string _tag = "hal";

void HalDesktop::init()
{
    spectra5::log::tagInfo(_tag, "init");
    lvgl_init();
}

/* -------------------------------------------------------------------------- */
/*                                   System                                   */
/* -------------------------------------------------------------------------- */
void HalDesktop::delay(uint32_t ms)
{
    SDL_Delay(ms);
}

uint32_t HalDesktop::millis()
{
    return SDL_GetTicks();
}

int HalDesktop::getCpuTemp()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(0.99, 1.01);

    return dis(gen) * 45.0f;
}

/* -------------------------------------------------------------------------- */
/*                                   Display                                  */
/* -------------------------------------------------------------------------- */
void HalDesktop::setDisplayBrightness(uint8_t brightness)
{
    _current_lcd_brightness = std::clamp((int)brightness, 0, 100);
    spectra5::log::tagInfo(_tag, "set display brightness: {}%", _current_lcd_brightness);
}

uint8_t HalDesktop::getDisplayBrightness()
{
    return _current_lcd_brightness;
}

/* -------------------------------------------------------------------------- */
/*                                Power monitor                               */
/* -------------------------------------------------------------------------- */
void HalDesktop::updatePowerMonitorData()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(0.95, 1.0);

    powerMonitorData.busVoltage   = dis(gen) * 8.0f;
    powerMonitorData.shuntCurrent = -dis(gen) * 0.5f;
}

/* -------------------------------------------------------------------------- */
/*                                     IMU                                    */
/* -------------------------------------------------------------------------- */
void HalDesktop::updateImuData()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(-0.1, 0.1);

    imuData.accelX = dis(gen);
    imuData.accelY = dis(gen);
    imuData.accelZ = dis(gen);
}

/* -------------------------------------------------------------------------- */
/*                                    Power                                   */
/* -------------------------------------------------------------------------- */
void HalDesktop::setChargeQcEnable(bool enable)
{
    _charge_qc_enable = enable;
    spectra5::log::tagInfo(_tag, "set charge qc enable: {}", _charge_qc_enable);
}

bool HalDesktop::getChargeQcEnable()
{
    return _charge_qc_enable;
}

void HalDesktop::setChargeEnable(bool enable)
{
    _charge_enable = enable;
    spectra5::log::tagInfo(_tag, "set charge enable: {}", _charge_enable);
}

bool HalDesktop::getChargeEnable()
{
    return _charge_enable;
}

void HalDesktop::setUsb5vEnable(bool enable)
{
    _usba_5v_enable = enable;
    spectra5::log::tagInfo(_tag, "set usb5v enable: {}", _usba_5v_enable);
}

bool HalDesktop::getUsb5vEnable()
{
    return _usba_5v_enable;
}

void HalDesktop::setExt5vEnable(bool enable)
{
    _ext_5v_enable = enable;
    spectra5::log::tagInfo(_tag, "set ext5v enable: {}", _ext_5v_enable);
}

bool HalDesktop::getExt5vEnable()
{
    return _ext_5v_enable;
}

void HalDesktop::setExtAntennaEnable(bool enable)
{
    _ext_antenna_enable = enable;
    spectra5::log::tagInfo(_tag, "set ext antenna enable: {}", _ext_antenna_enable);
}

bool HalDesktop::getExtAntennaEnable()
{
    return _ext_antenna_enable;
}

/* -------------------------------------------------------------------------- */
/*                                   SD card                                  */
/* -------------------------------------------------------------------------- */
bool HalDesktop::isSdCardMounted()
{
    return true;
}

std::vector<hal::HalBase::FileEntry_t> HalDesktop::scanSdCard(const std::string& dirPath)
{
    std::filesystem::path path(dirPath);
    std::vector<hal::HalBase::FileEntry_t> file_entries;
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        file_entries.push_back({entry.path().filename().string(), entry.is_directory()});
    }
    return file_entries;
}

/* -------------------------------------------------------------------------- */
/*                                  Interface                                 */
/* -------------------------------------------------------------------------- */
bool HalDesktop::usbCDetect()
{
    // return rand() % 2 == 0;
    return false;
}

bool HalDesktop::usbADetect()
{
    // return rand() % 2 == 0;
    return false;
}

bool HalDesktop::headPhoneDetect()
{
    // return rand() % 2 == 0;
    return false;
}

std::vector<uint8_t> HalDesktop::i2cScan(bool isInternal)
{
    // Random 6 addrs
    std::vector<uint8_t> addrs;
    for (int i = 0; i < 6; i++) {
        addrs.push_back(rand() % 128);
    }
    return addrs;
}

/* -------------------------------------------------------------------------- */
/*                                UART monitor                                */
/* -------------------------------------------------------------------------- */
void HalDesktop::uartMonitorSend(std::string msg, bool newLine)
{
    static bool is_test_thread_running = false;
    if (is_test_thread_running) {
        return;
    }

    is_test_thread_running = true;
    std::thread([&]() {
        for (int i = 0; i < 6; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::lock_guard<std::mutex> lock(GetHAL()->uartMonitorData.mutex);
            std::string send_msg = "[2025-04-24 14:32:38.111] [info] [panel-com] recv msg: 32\n";
            spectra5::log::tagInfo(_tag, "send msg: {}", send_msg);
            for (auto c : send_msg) {
                GetHAL()->uartMonitorData.rxQueue.push(c);
            }
        }
        is_test_thread_running = false;
    }).detach();
}
