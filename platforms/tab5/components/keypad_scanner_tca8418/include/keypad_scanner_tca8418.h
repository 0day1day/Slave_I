#pragma once

#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// #define TAB5_EXT_I2C_SCL_PIN 1  //
// #define TAB5_EXT_I2C_SDA_PIN 0  //
// #define TAB5_TCA8418_INT_PIN 50 // 中断输入

void app_keypad_scanner_test(void *pvParam);

esp_err_t keypad_scanner_tca8418_init(i2c_master_bus_handle_t bus_handle);
bool keypad_scanner_tca8418_matrix(uint8_t rows, uint8_t columns);
uint8_t keypad_scanner_tca8418_flush();
uint8_t keypad_scanner_tca8418_available();
uint8_t keypad_scanner_tca8418_get_event();
esp_err_t keypad_scanner_tca8418_irq_init(int int_pin);

// configuration
void keypad_scanner_tca8418_enable_int();
void keypad_scanner_tca8418_disable_int();
void keypad_scanner_tca8418_clear_irq();

extern const char key_value_map[];
extern const char key_value_map_str[][10];

esp_err_t bsp_rgb_led_init(void);
void bsp_rgb_led_set(uint8_t led_num, uint8_t r, uint8_t g, uint8_t b);
void bsp_rgb_led_display();
void bsp_rgb_led_clear();

#ifdef __cplusplus
}
#endif
