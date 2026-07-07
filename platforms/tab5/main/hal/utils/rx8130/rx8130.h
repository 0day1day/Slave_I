/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <driver/i2c_master.h>
#include <driver/gpio.h>
#include <time.h>
// https://download.epsondevice.com/td/pdf/app/RX8130CE_en.pdf
// https://github.com/alexreinert/piVCCU/blob/master/kernel/rtc-rx8130.c

class RX8130_Class {
public:
    RX8130_Class()
    {
    }

    bool begin(i2c_master_bus_handle_t busHandle, uint8_t addr = 0x32);
    void initBat();
    void setTime(struct tm* time);
    void getTime(struct tm* time);
    void clearIrqFlags();
    void disableIrq();
    void setAlarmIrq(struct tm* time);
    void setTimerIrq(uint16_t seconds);

protected:
    uint8_t readRegister8(uint8_t reg);
    void writeRegister8(uint8_t reg, uint8_t value);
    void readRegister(uint8_t reg, uint8_t* buf, uint8_t len);
    void writeRegister(uint8_t reg, uint8_t* buf, uint8_t len);

private:
    i2c_master_dev_handle_t _i2c_device_handle;
};