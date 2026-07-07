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
#include <mutex>
#include <lvgl.h>
#include <usb/usb_host.h>
#include <usb/hid_host.h>
#include <usb/hid_usage_keyboard.h>
#include <usb/hid_usage_mouse.h>
#include <esp_log.h>
#include <cstring>
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "bsp/m5stack_tab5.h"

#define TAG "usba"

static std::mutex _usba_detect_mutex;
static bool _is_usba_connected = false;

QueueHandle_t app_event_queue = NULL;
typedef enum { APP_EVENT = 0, APP_EVENT_HID_HOST } app_event_group_t;

// Shared keypad plumbing (fed by the USB HID keyboard and/or the I2C keyboard).
static QueueHandle_t kb_queue       = nullptr;
static volatile bool g_kb_connected = false;

typedef struct {
    app_event_group_t event_group;
    struct {
        hid_host_device_handle_t handle;
        hid_host_driver_event_t event;
        void* arg;
    } hid_host_device;
} app_event_queue_t;

static const char* hid_proto_name_str[] = {"NONE", "KEYBOARD", "MOUSE"};

typedef struct {
    enum key_state { KEY_STATE_PRESSED = 0x00, KEY_STATE_RELEASED = 0x01 } state;
    uint8_t modifier;
    uint8_t key_code;
} key_event_t;

#define KEYBOARD_ENTER_MAIN_CHAR '\r'
#define KEYBOARD_ENTER_LF_EXTEND 1

static void hid_print_new_device_report_header(hid_protocol_t proto)
{
    static hid_protocol_t prev_proto_output;

    if (prev_proto_output != proto) {
        prev_proto_output = proto;
        printf("\r\n");
        if (proto == HID_PROTOCOL_MOUSE) {
            printf("Mouse\r\n");
        } else if (proto == HID_PROTOCOL_KEYBOARD) {
            printf("Keyboard\r\n");
        } else {
            printf("Generic\r\n");
        }
        fflush(stdout);
    }
}

static void hid_host_mouse_report_callback(const uint8_t* const data, const int length)
{
    hid_mouse_input_report_boot_t* mouse_report = (hid_mouse_input_report_boot_t*)data;

    if (length < sizeof(hid_mouse_input_report_boot_t)) {
        return;
    }

    static int x_pos = 720 / 2;
    static int y_pos = 1280 / 2;

    // Calculate absolute position from displacement
    x_pos += mouse_report->y_displacement;
    y_pos -= mouse_report->x_displacement;

    x_pos = std::clamp(x_pos, 0, 720);
    y_pos = std::clamp(y_pos, 0, 1280);

    hid_print_new_device_report_header(HID_PROTOCOL_MOUSE);

    // printf("X: %06d\tY: %06d\t|%c|%c|\r", x_pos, y_pos, (mouse_report->buttons.button1 ? 'o' : ' '),
    //        (mouse_report->buttons.button2 ? 'o' : ' '));

    GetHAL()->hidMouseData.mutex.lock();
    GetHAL()->hidMouseData.x        = x_pos;
    GetHAL()->hidMouseData.y        = y_pos;
    GetHAL()->hidMouseData.btnLeft  = mouse_report->buttons.button1;
    GetHAL()->hidMouseData.btnRight = mouse_report->buttons.button2;
    GetHAL()->hidMouseData.mutex.unlock();

    fflush(stdout);
}

// --- USB HID boot keyboard -> LVGL keys (fed into the same keypad queue) -------
static uint32_t usb_hid_to_lvkey(uint8_t code, bool shift)
{
    if (code >= 0x04 && code <= 0x1d) {  // a..z
        char c = static_cast<char>('a' + (code - 0x04));
        return shift ? static_cast<uint32_t>(c - 32) : static_cast<uint32_t>(c);
    }
    if (code >= 0x1e && code <= 0x26) {  // 1..9
        static const char* sh = ")!@#$%^&*(";
        return shift ? static_cast<uint32_t>(sh[code - 0x1d])
                     : static_cast<uint32_t>('1' + (code - 0x1e));
    }
    switch (code) {
        case 0x27: return shift ? ')' : '0';
        case 0x28: return LV_KEY_ENTER;
        case 0x29: return LV_KEY_ESC;
        case 0x2a: return LV_KEY_BACKSPACE;
        case 0x2b: return LV_KEY_NEXT;   // Tab
        case 0x2c: return ' ';
        case 0x2d: return shift ? '_' : '-';
        case 0x2e: return shift ? '+' : '=';
        case 0x2f: return shift ? '{' : '[';
        case 0x30: return shift ? '}' : ']';
        case 0x33: return shift ? ':' : ';';
        case 0x34: return shift ? '"' : '\'';
        case 0x36: return shift ? '<' : ',';
        case 0x37: return shift ? '>' : '.';
        case 0x38: return shift ? '?' : '/';
        case 0x4f: return LV_KEY_NEXT;   // Right
        case 0x50: return LV_KEY_PREV;   // Left
        case 0x51: return LV_KEY_NEXT;   // Down
        case 0x52: return LV_KEY_PREV;   // Up
        default:   return 0;
    }
}

static void hid_host_keyboard_report_callback(const uint8_t* const data, const int length)
{
    if (length < 8) {
        return;
    }
    static uint8_t prev[6] = {0};
    const bool shift       = (data[0] & 0x22) != 0;  // L/R Shift
    const uint8_t* keys    = &data[2];
    for (int i = 0; i < 6; ++i) {
        const uint8_t code = keys[i];
        if (code == 0) continue;
        bool was_down = false;
        for (int j = 0; j < 6; ++j) {
            if (prev[j] == code) { was_down = true; break; }
        }
        if (!was_down) {
            const uint32_t lk = usb_hid_to_lvkey(code, shift);
            if (lk && kb_queue) {
                xQueueSend(kb_queue, &lk, 0);
            }
        }
    }
    for (int i = 0; i < 6; ++i) prev[i] = keys[i];
    g_kb_connected = true;
}

void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle, const hid_host_interface_event_t event,
                                 void* arg)
{
    uint8_t data[64]   = {0};
    size_t data_length = 0;
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
        case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
            ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(hid_device_handle, data, 64, &data_length));

            if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
                if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                    hid_host_keyboard_report_callback(data, data_length);
                } else if (HID_PROTOCOL_MOUSE == dev_params.proto) {
                    hid_host_mouse_report_callback(data, data_length);
                }
            } else {
                // hid_host_generic_report_callback(data, data_length);
            }

            break;
        case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HID Device, protocol '%s' DISCONNECTED", hid_proto_name_str[dev_params.proto]);
            ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));

            if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                g_kb_connected = false;
            }
            _usba_detect_mutex.lock();
            _is_usba_connected = false;
            _usba_detect_mutex.unlock();

            break;
        case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
            ESP_LOGI(TAG, "HID Device, protocol '%s' TRANSFER_ERROR", hid_proto_name_str[dev_params.proto]);
            break;
        default:
            ESP_LOGE(TAG, "HID Device, protocol '%s' Unhandled event", hid_proto_name_str[dev_params.proto]);
            break;
    }
}

void hid_host_device_event(hid_host_device_handle_t hid_device_handle, const hid_host_driver_event_t event, void* arg)
{
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
        case HID_HOST_DRIVER_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "HID Device, protocol '%s' CONNECTED", hid_proto_name_str[dev_params.proto]);

            const hid_host_device_config_t dev_config = {.callback = hid_host_interface_callback, .callback_arg = NULL};

            ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
            if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
                ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
                if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                    ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
                }
            }
            ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));

            _usba_detect_mutex.lock();
            _is_usba_connected = true;
            _usba_detect_mutex.unlock();

            break;
        }
        default:
            break;
    }
}

void hid_host_device_callback(hid_host_device_handle_t hid_device_handle, const hid_host_driver_event_t event,
                              void* arg)
{
    const app_event_queue_t evt_queue = {.event_group = APP_EVENT_HID_HOST,
                                         // HID Host Device related info
                                         .hid_host_device = {.handle = hid_device_handle, .event = event, .arg = arg}};

    if (app_event_queue) {
        xQueueSend(app_event_queue, &evt_queue, 0);
    }
}

static void tab5_usb_host_task(void* pvParameters)
{
    // BaseType_t task_created;
    app_event_queue_t evt_queue;
    // ESP_LOGI(TAG, "HID Host example");
    // task_created = xTaskCreatePinnedToCore(usb_lib_task, "usb_events", 4096, xTaskGetCurrentTaskHandle(), 2, NULL,
    // 0); assert(task_created == pdTRUE);

    ulTaskNotifyTake(false, 1000);
    const hid_host_driver_config_t hid_host_driver_config = {.create_background_task = true,
                                                             .task_priority          = 5,
                                                             .stack_size             = 4096,
                                                             .core_id                = 0,
                                                             .callback               = hid_host_device_callback,
                                                             .callback_arg           = NULL};

    ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));

    // Create queue
    app_event_queue = xQueueCreate(10, sizeof(app_event_queue_t));

    ESP_LOGI(TAG, "Waiting for HID Device to be connected");

    while (1) {
        // Wait queue
        if (xQueueReceive(app_event_queue, &evt_queue, portMAX_DELAY)) {
            if (APP_EVENT == evt_queue.event_group) {
                // User pressed button
                usb_host_lib_info_t lib_info;
                ESP_ERROR_CHECK(usb_host_lib_info(&lib_info));
                if (lib_info.num_devices == 0) {
                    // End while cycle
                    break;
                } else {
                    ESP_LOGW(TAG, "To shutdown example, remove all USB devices and press button again.");
                    // Keep polling
                }
            }

            if (APP_EVENT_HID_HOST == evt_queue.event_group) {
                hid_host_device_event(evt_queue.hid_host_device.handle, evt_queue.hid_host_device.event,
                                      evt_queue.hid_host_device.arg);
            }
        }
    }
}

static void lvgl_mouse_read_cb(lv_indev_t* indev, lv_indev_data_t* data)
{
    _usba_detect_mutex.lock();
    if (!_is_usba_connected) {
        _usba_detect_mutex.unlock();
        data->state = LV_INDEV_STATE_REL;
        return;
    }
    _usba_detect_mutex.unlock();

    std::lock_guard<std::mutex> lock(GetHAL()->hidMouseData.mutex);
    data->point.x = GetHAL()->hidMouseData.x;
    data->point.y = GetHAL()->hidMouseData.y;
    data->state   = GetHAL()->hidMouseData.btnLeft ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/* ---------------------------------------------------------------------------
 * M5Stack official Tab5 keyboard (I2C "smart" keyboard, STRING mode).
 * Ported from M5Tab5-UserDemo. Feeds an LVGL KEYPAD indev + a navigation group
 * so the UI is operable with the arrow keys (sidebar / cards / views) and text
 * can be typed into focused textareas (Files naming, etc.).
 * ------------------------------------------------------------------------- */
static i2c_master_dev_handle_t s_smart_dev = nullptr;
static lv_group_t*          g_kb_group  = nullptr;

#define KB_SMART_ADDR 0x6D
#define KB_TCA_ADDR   0x34

static bool smart_read(uint8_t reg, uint8_t* out, size_t n)
{
    if (!s_smart_dev) return false;
    return i2c_master_transmit_receive(s_smart_dev, &reg, 1, out, n, 50) == ESP_OK;
}
static bool smart_write(uint8_t reg, uint8_t val)
{
    if (!s_smart_dev) return false;
    uint8_t b[2] = {reg, val};
    return i2c_master_transmit(s_smart_dev, b, 2, 50) == ESP_OK;
}

static uint32_t char_to_lvkey(uint8_t c)
{
    if (c == '\r' || c == '\n') return LV_KEY_ENTER;
    if (c == 0x08 || c == 0x7F) return LV_KEY_BACKSPACE;
    if (c == '\t')              return LV_KEY_NEXT;
    if (c == 27)                return LV_KEY_ESC;
    if (c == 0x11)              return LV_KEY_PREV;   // UP
    if (c == 0x14)              return LV_KEY_PREV;   // LEFT
    if (c == 0x12)              return LV_KEY_NEXT;   // DOWN
    if (c == 0x13)              return LV_KEY_NEXT;   // RIGHT
    if (c >= 0x20 && c < 0x7F)  return (uint32_t)c;
    return 0;
}

static uint32_t word_to_lvkey(const char* in)
{
    char s[18];
    int i = 0;
    for (; in[i] && i < 16; i++) s[i] = (in[i] >= 'A' && in[i] <= 'Z') ? in[i] + 32 : in[i];
    s[i] = 0;
    if (!strcmp(s, "left"))                          return LV_KEY_PREV;
    if (!strcmp(s, "right"))                         return LV_KEY_NEXT;
    if (!strcmp(s, "up"))                            return LV_KEY_PREV;
    if (!strcmp(s, "down"))                          return LV_KEY_NEXT;
    if (!strcmp(s, "enter") || !strcmp(s, "return")) return LV_KEY_ENTER;
    if (!strcmp(s, "esc")   || !strcmp(s, "escape")) return LV_KEY_ESC;
    if (!strcmp(s, "tab"))                           return LV_KEY_NEXT;
    if (!strcmp(s, "space"))                         return (uint32_t)' ';
    if (!strcmp(s, "home"))                          return LV_KEY_HOME;
    if (!strcmp(s, "end"))                           return LV_KEY_END;
    if (!strcmp(s, "del") || !strcmp(s, "delete") ||
        !strcmp(s, "backspace") || !strcmp(s, "bksp"))
        return LV_KEY_BACKSPACE;
    return 0;
}

static void lvgl_kb_read_cb(lv_indev_t*, lv_indev_data_t* data)
{
    static uint32_t last = 0;
    uint32_t k;
    if (kb_queue && xQueueReceive(kb_queue, &k, 0) == pdTRUE) {
        last        = k;
        data->key   = k;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->key   = last;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void tab5_keyboard_task(void*)
{
    // The keyboard NACKs noisily until it is plugged in; keep that out of the log.
    esp_log_level_set("i2c.master", ESP_LOG_NONE);

    // Power the external + USB 5V rails before probing (the keyboard on the side
    // external port needs them), then give the keyboard MCU time to boot.
    bsp_set_usb_5v_en(true);
    bsp_set_ext_5v_en(true);
    vTaskDelay(pdMS_TO_TICKS(800));

    // Probe the external, Grove and internal buses (the M5Stack keyboard reports at
    // 0x34 or 0x6D depending on unit/port). NACKs to those addresses don't disturb
    // the GT911 touch controller, the i2c log is silenced, and the idle probe is
    // slow -- so no spam.
    // EXT_I2C is now GPIO0/1 (the keyboard bus) -- see the BSP header. The smart
    // keyboard is at 0x6D (0x34 was a false lead: that's the PMIC).
    bsp_ext_i2c_init();
    i2c_master_bus_handle_t ext_bus = bsp_ext_i2c_get_handle();

    struct Cand { i2c_master_bus_handle_t bus; uint8_t addr; };
    Cand cands[] = {{ext_bus, KB_SMART_ADDR}, {ext_bus, KB_TCA_ADDR}};
    bool connected = false;
    int  fails     = 0;

    for (;;) {
        if (!connected) {
            for (auto& c : cands) {
                if (c.bus == nullptr) continue;
                if (s_smart_dev) { i2c_master_bus_rm_device(s_smart_dev); s_smart_dev = nullptr; }
                i2c_device_config_t cfg = {};
                cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
                cfg.device_address  = c.addr;
                cfg.scl_speed_hz    = 100000;
                if (i2c_master_bus_add_device(c.bus, &cfg, &s_smart_dev) != ESP_OK) {
                    s_smart_dev = nullptr;
                    continue;
                }
                uint8_t tmp = 0;
                if (smart_read(0x02, &tmp, 1)) {  // EVENT_NUM ACK => present
                    smart_write(0x10, 2);         // STRING mode -> ASCII
                    connected = true; fails = 0;
                    g_kb_connected = true;
                    spectra5::log::tagInfo(TAG, "M5 keyboard detected");
                    break;
                }
                i2c_master_bus_rm_device(s_smart_dev);
                s_smart_dev = nullptr;
            }
            if (!connected) { vTaskDelay(pdMS_TO_TICKS(3000)); continue; }  // slow idle probe
        }

        uint8_t count = 0;
        if (!smart_read(0x02, &count, 1)) {
            if (++fails > 3) { connected = false; g_kb_connected = false; }
            vTaskDelay(pdMS_TO_TICKS(300));
            continue;
        }
        fails = 0;
        for (int i = 0; i < count && i < 32; i++) {
            uint8_t len = 0;
            if (smart_read(0x40, &len, 1) && len > 0 && len <= 15) {
                uint8_t buf[17] = {0};
                if (smart_read(0x50, buf, len + 1)) {
                    char s[18];
                    int n = 0;
                    for (int j = 1; j <= len && n < 16 && buf[j] != 0; j++) s[n++] = (char)buf[j];
                    s[n] = 0;
                    uint32_t lk = 0;
                    if (n == 1)      lk = char_to_lvkey((uint8_t)s[0]);
                    else if (n > 1)  lk = word_to_lvkey(s);
                    if (lk && kb_queue) xQueueSend(kb_queue, &lk, 0);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void HalEsp32::hid_init()
{
    spectra5::log::tagInfo(TAG, "hid init");
    xTaskCreatePinnedToCore(tab5_usb_host_task, "usba", 4096 * 2, NULL, 5, NULL, 0);

    auto lvMouse = lv_indev_create();
    lv_indev_set_type(lvMouse, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(lvMouse, lvgl_mouse_read_cb);
    lv_indev_set_display(lvMouse, lvDisp);

    // I2C keyboard -> KEYPAD indev bound to a navigation group.
    g_kb_group = lv_group_create();
    lv_group_set_wrap(g_kb_group, true);
    kb_queue       = xQueueCreate(32, sizeof(uint32_t));
    lv_indev_t* kb = lv_indev_create();
    lv_indev_set_type(kb, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(kb, lvgl_kb_read_cb);
    lv_indev_set_display(kb, lvDisp);
    lv_indev_set_group(kb, g_kb_group);
    xTaskCreatePinnedToCore(tab5_keyboard_task, "kbd", 4096, NULL, 5, NULL, 0);
}

// Exposed to the UI (weak-overridable hook) so screens can register focusable
// widgets for arrow-key navigation.
extern "C" lv_group_t* spectra5_nav_group()
{
    return g_kb_group;
}

extern "C" bool spectra5_keyboard_connected()
{
    return g_kb_connected;
}

bool HalEsp32::usbADetect()
{
    std::lock_guard<std::mutex> lock(_usba_detect_mutex);
    return _is_usba_connected;
}
