/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <thread>
#include <mutex>

class TaskController_t {
public:
    virtual ~TaskController_t()
    {
    }

    std::mutex mutex;

    void sendKillSignal()
    {
        std::lock_guard<std::mutex> lock(mutex);
        _kill_signal = true;
    }

    bool checkKillSignal()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return _kill_signal;
    }

    bool isTaskDeleted()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return _task_deleted = true;
    }

    void setTaskDeleted()
    {
        std::lock_guard<std::mutex> lock(mutex);
        _task_deleted = true;
    }

    void sendKillSignalAndWaitDelete(const TickType_t xTicksToDelay = pdMS_TO_TICKS(100))
    {
        sendKillSignal();
        while (1) {
            vTaskDelay(xTicksToDelay);
            if (isTaskDeleted()) break;
        }
    }

protected:
    bool _kill_signal  = false;
    bool _task_deleted = false;
};
