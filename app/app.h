/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 * SPDX-FileCopyrightText: 2026 Slave I contributors
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <functional>

namespace app {

struct InitCallback_t {
    std::function<void()> onHalInjection = nullptr;
};

void Init(InitCallback_t callback);
void Update();
bool IsDone();
void Destroy();

}  // namespace app
