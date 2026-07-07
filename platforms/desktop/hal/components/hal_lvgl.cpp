/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "../hal_desktop.h"
#include "../hal_config.h"
#include "hal/hal.h"
#include "core/diagnostics/log.hpp"
#include <lvgl.h>
#include <mutex>
#include <thread>
// https://github.com/lvgl/lv_port_pc_vscode/blob/master/main/src/main.c

static const std::string _tag = "lvgl";
static std::mutex _lvgl_mutex;

void HalDesktop::lvgl_init()
{
    spectra5::log::tagInfo(_tag, "lvgl init");

    lv_init();

    lv_group_set_default(lv_group_create());

    auto display = lv_sdl_window_create(HAL_SCREEN_WIDTH, HAL_SCREEN_HEIGHT);
    lv_display_set_default(display);

    lvTouchpad = lv_sdl_mouse_create();
    lv_indev_set_group(lvTouchpad, lv_group_get_default());
    lv_indev_set_display(lvTouchpad, display);

    // // LV_IMAGE_DECLARE(mouse_cursor_icon); /*Declare the image file.*/
    // lv_obj_t* cursor_obj;
    // cursor_obj = lv_image_create(lv_screen_active()); /*Create an image object for the cursor */
    // lv_image_set_src(cursor_obj, &mouse_cursor);      /*Set the image source*/
    // lv_indev_set_cursor(lvTouchpad, cursor_obj);      /*Connect the image  object to the driver*/

    auto mouse_wheel = lv_sdl_mousewheel_create();
    lv_indev_set_display(mouse_wheel, display);
    lv_indev_set_group(mouse_wheel, lv_group_get_default());

    auto keyboard = lv_sdl_keyboard_create();
    lv_indev_set_display(keyboard, display);
    lv_indev_set_group(keyboard, lv_group_get_default());

#if not defined(__APPLE__) && not defined(__MACH__)
    std::thread([]() {
        while (!hal::Check()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        while (true) {
            GetHAL()->lvglLock();
            auto time_till_next = lv_timer_handler();
            GetHAL()->lvglUnlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(time_till_next));
        }
    }).detach();
#endif
}

void HalDesktop::lvglLock()
{
    _lvgl_mutex.lock();
}

void HalDesktop::lvglUnlock()
{
    _lvgl_mutex.unlock();
}
