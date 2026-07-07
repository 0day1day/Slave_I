/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal/hal_esp32.h"
#include "core/diagnostics/log.hpp"
#include <vector>
#include <driver/gpio.h>
#include <memory>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "accel_gyro_bmi270.h"

static const std::string _tag = "imu";

void HalEsp32::clearImuIrq()
{
    spectra5::log::tagInfo(_tag, "clear imu irq");
    accel_gyro_bmi270_init(bsp_i2c_get_handle());
    if (accel_gyro_bmi270_check_irq()) {
        accel_gyro_bmi270_clear_irq_int();
        spectra5::log::tagInfo(_tag, "imu irq detected! clear it!");
    }
}

void HalEsp32::imu_init()
{
    spectra5::log::tagInfo(_tag, "imu init");

    accel_gyro_bmi270_init(bsp_i2c_get_handle());
    if (accel_gyro_bmi270_check_irq()) {
        accel_gyro_bmi270_clear_irq_int();
        spectra5::log::tagInfo(_tag, "imu irq detected! clear it!");
    }
    accel_gyro_bmi270_enable_sensor();
}

void HalEsp32::updateImuData()
{
    static struct bmi2_sens_data bmi_sensor_data;
    accel_gyro_bmi270_get_data(&bmi_sensor_data);

    /* 根据设置量程转换 */
    imuData.accelX = bmi_sensor_data.acc.y / 835.92 / 10.0f;  // m/s^2
    imuData.accelY = -bmi_sensor_data.acc.x / 835.92 / 10.0f;
    imuData.accelZ = -bmi_sensor_data.acc.z / 835.92 / 10.0f;
    imuData.gyroX  = bmi_sensor_data.gyr.y / 32.768 / 10.0f;  // °/s   gyro_raw*2*1000/2^16 --> 0.0305
    imuData.gyroY  = bmi_sensor_data.gyr.x / 32.768 / 10.0f;
    imuData.gyroZ  = -bmi_sensor_data.gyr.z / 32.768 / 10.0f;
}

void HalEsp32::sleepAndShakeWakeup()
{
    spectra5::log::tagInfo(_tag, "start aleep and shake wakeup");

    clearRtcIrq();
    clearImuIrq();

    delay(200);

    spectra5::log::tagInfo(_tag, "set motion irq");
    accel_gyro_bmi270_motion_irq();

    // delay(800);
    powerOff();
    while (1) {
        delay(100);
    }
}
