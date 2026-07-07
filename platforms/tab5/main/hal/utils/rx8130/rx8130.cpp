/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "rx8130.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

// RX-8130 Register definitions
#define RX8130_REG_SEC   0x10
#define RX8130_REG_MIN   0x11
#define RX8130_REG_HOUR  0x12
#define RX8130_REG_WDAY  0x13
#define RX8130_REG_MDAY  0x14
#define RX8130_REG_MONTH 0x15
#define RX8130_REG_YEAR  0x16

#define RX8130_REG_ALMIN   0x17
#define RX8130_REG_ALHOUR  0x18
#define RX8130_REG_ALWDAY  0x19
#define RX8130_REG_TCOUNT0 0x1A
#define RX8130_REG_TCOUNT1 0x1B
#define RX8130_REG_EXT     0x1C
#define RX8130_REG_FLAG    0x1D
#define RX8130_REG_CTRL0   0x1E
#define RX8130_REG_CTRL1   0x1F

#define RX8130_REG_END 0x23

// Extension Register (1Ch) bit positions
#define RX8130_BIT_EXT_TSEL (7 << 0)
#define RX8130_BIT_EXT_WADA (1 << 3)
#define RX8130_BIT_EXT_TE   (1 << 4)
#define RX8130_BIT_EXT_USEL (1 << 5)
#define RX8130_BIT_EXT_FSEL (3 << 6)

// Flag Register (1Dh) bit positions
#define RX8130_BIT_FLAG_VLF (1 << 1)
#define RX8130_BIT_FLAG_AF  (1 << 3)
#define RX8130_BIT_FLAG_TF  (1 << 4)
#define RX8130_BIT_FLAG_UF  (1 << 5)

// Control 0 Register (1Еh) bit positions
#define RX8130_BIT_CTRL_TSTP (1 << 2)
#define RX8130_BIT_CTRL_AIE  (1 << 3)
#define RX8130_BIT_CTRL_TIE  (1 << 4)
#define RX8130_BIT_CTRL_UIE  (1 << 5)
#define RX8130_BIT_CTRL_STOP (1 << 6)
#define RX8130_BIT_CTRL_TEST (1 << 7)

#define setbit(x, y)     x |= (0x01 << y)
#define clrbit(x, y)     x &= ~(0x01 << y)
#define reversebit(x, y) x ^= (0x01 << y)
#define getbit(x, y)     ((x) >> (y)&0x01)

static uint8_t bcd2dec(uint8_t val)
{
    return (val >> 4) * 10 + (val & 0x0f);
}

static uint8_t dec2bcd(uint8_t val)
{
    return ((val / 10) << 4) + (val % 10);
}

bool RX8130_Class::begin(i2c_master_bus_handle_t busHandle, uint8_t addr)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(busHandle, &dev_cfg, &_i2c_device_handle));

    if (_i2c_device_handle == NULL) {
        return false;
    }

    return true;
}

void RX8130_Class::initBat()
{
    auto data = readRegister8(0x1F);
    setbit(data, 4);
    setbit(data, 5);
    writeRegister8(0x1F, data);
    data = readRegister8(0x1F);
    printf("rtc bat init: 0x1F: %02X\n", data);
}

void RX8130_Class::setTime(struct tm* time)
{
    uint8_t rbuf = 0;

    time->tm_year -= 100;

    // set STOP bit before changing clock/calendar
    rbuf = readRegister8(RX8130_REG_CTRL0);
    rbuf = rbuf | RX8130_BIT_CTRL_STOP;
    writeRegister8(RX8130_REG_CTRL0, rbuf);

    uint8_t date[7] = {dec2bcd(time->tm_sec),       dec2bcd(time->tm_min),  dec2bcd(time->tm_hour),
                       dec2bcd(time->tm_wday),      dec2bcd(time->tm_mday), dec2bcd(time->tm_mon),
                       dec2bcd(time->tm_year % 100)};

    writeRegister(RX8130_REG_SEC, date, 7);

    // clear STOP bit after changing clock/calendar
    rbuf = readRegister8(RX8130_REG_CTRL0);
    rbuf = rbuf & ~RX8130_BIT_CTRL_STOP;
    writeRegister8(RX8130_REG_CTRL0, rbuf);
}

void RX8130_Class::getTime(struct tm* time)
{
    uint8_t date[7];
    readRegister(RX8130_REG_SEC, date, 7);

    time->tm_sec  = bcd2dec(date[RX8130_REG_SEC - 0x10] & 0x7f);
    time->tm_min  = bcd2dec(date[RX8130_REG_MIN - 0x10] & 0x7f);
    time->tm_hour = bcd2dec(date[RX8130_REG_HOUR - 0x10] & 0x3f);  // only 24-hour clock
    time->tm_mday = bcd2dec(date[RX8130_REG_MDAY - 0x10] & 0x3f);
    time->tm_mon  = bcd2dec(date[RX8130_REG_MONTH - 0x10] & 0x1f);
    time->tm_year = bcd2dec(date[RX8130_REG_YEAR - 0x10]);
    time->tm_wday = bcd2dec(date[RX8130_REG_WDAY - 0x10] & 0x7f);

    time->tm_year += 100;
}

void RX8130_Class::clearIrqFlags()
{
    writeRegister8(RX8130_REG_FLAG, 0);
}

void RX8130_Class::disableIrq()
{
    writeRegister8(RX8130_REG_CTRL0, 0);
}

void RX8130_Class::setAlarmIrq(struct tm* time)
{
    uint8_t buf = 0;

    // Write 0 to AIE
    buf = readRegister8(RX8130_REG_CTRL0);
    clrbit(buf, 3);
    clrbit(buf, 5);
    writeRegister8(RX8130_REG_CTRL0, buf);

    buf = readRegister8(RX8130_REG_CTRL0);
    // debug_print_reg(0x1E, buf);

    // Hour AE, week AE day AE
    buf = 0x80;
    writeRegister8(RX8130_REG_ALWDAY, buf);
    buf = readRegister8(RX8130_REG_ALWDAY);
    // debug_print_reg(0x19, buf);

    buf = 0x80;
    writeRegister8(RX8130_REG_ALHOUR, buf);
    buf = readRegister8(RX8130_REG_ALHOUR);
    // debug_print_reg(0x18, buf);

    buf = 0x80;
    writeRegister8(RX8130_REG_ALMIN, buf);
    buf = readRegister8(RX8130_REG_ALMIN);
    // debug_print_reg(0x17, buf);

    // Write 1 to AIE
    buf = readRegister8(RX8130_REG_CTRL0);
    setbit(buf, 3);
    writeRegister8(RX8130_REG_CTRL0, buf);

    buf = readRegister8(RX8130_REG_CTRL0);
    // debug_print_reg(0x1E, buf);
}

// RX8130 寄存器地址
#define RX8130_REG_SEC                0x10
#define RX8130_REG_MIN                0x11
#define RX8130_REG_HOUR               0x12
#define RX8130_REG_WEEK               0x13
#define RX8130_REG_DAY                0x14
#define RX8130_REG_MONTH              0x15
#define RX8130_REG_YEAR               0x16
#define RX8130_REG_ALARM_MINUTE       0x17
#define RX8130_REG_ALARM_HOUR         0x18
#define RX8130_REG_ALARM_WEEKDAY      0x19
#define RX8130_REG_TIMER_COUNTER_LOW  0x1A
#define RX8130_REG_TIMER_COUNTER_HIGH 0x1B
#define RX8130_REG_EXTENSION          0x1C
#define RX8130_REG_FLAG               0x1D
#define RX8130_REG_CONTROL0           0x1E
#define RX8130_REG_CONTROL1           0x1F

void RX8130_Class::setTimerIrq(uint16_t seconds)
{
    uint8_t flag_register = 0;
    uint8_t buffer[2]     = {0};
    buffer[0]             = seconds & 0xFF;         // 定时器低字节
    buffer[1]             = (seconds >> 8) & 0xFF;  // 定时器高字节

    // Step 1: Disable Timer
    flag_register = readRegister8(RX8130_REG_EXTENSION);

    flag_register &= ~(1 << 4);  // 禁用定时器 (TE = 0)
    writeRegister8(RX8130_REG_EXTENSION, flag_register);

    // Setp 2: Write Timer Counter Register (1Ah, 1Bh)
    writeRegister(RX8130_REG_TIMER_COUNTER_LOW, buffer, 2);

    // Step 3: Enable Timer
    flag_register = readRegister8(RX8130_REG_EXTENSION);

    setbit(flag_register, 4);  // 启用定时器 (TE = 1)
    clrbit(flag_register, 2);
    setbit(flag_register, 1);
    clrbit(flag_register, 0);
    writeRegister8(RX8130_REG_EXTENSION, flag_register);

    // Step 4: Enable Timer Interrupt
    flag_register = readRegister8(RX8130_REG_CONTROL0);
    flag_register |= (1 << 4);  // 启用定时器中断 (TIE = 1)
    writeRegister8(RX8130_REG_CONTROL0, flag_register);
}

uint8_t RX8130_Class::readRegister8(uint8_t reg)
{
    uint8_t value;
    readRegister(reg, &value, 1);
    return value;
}

void RX8130_Class::writeRegister8(uint8_t reg, uint8_t value)
{
    uint8_t buf[1] = {value};
    writeRegister(reg, buf, 1);
}

void RX8130_Class::readRegister(uint8_t reg, uint8_t* buf, uint8_t len)
{
    uint8_t w_buffer[1] = {0};
    w_buffer[0]         = reg;
    i2c_master_transmit_receive(_i2c_device_handle, w_buffer, 1, buf, len, portMAX_DELAY);
}

void RX8130_Class::writeRegister(uint8_t reg, uint8_t* buf, uint8_t len)
{
    uint8_t w_buffer[1 + len];
    w_buffer[0] = reg;
    memcpy(w_buffer + 1, buf, len);
    i2c_master_transmit(_i2c_device_handle, w_buffer, 1 + len, portMAX_DELAY);
}