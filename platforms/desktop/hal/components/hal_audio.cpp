/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "../hal_desktop.h"
#include "hal/hal.h"
#include <cmath>
#include "core/diagnostics/log.hpp"
#include <SDL2/SDL.h>
#include <thread>
#include <iostream>

static const std::string _tag = "audio";

void HalDesktop::setSpeakerVolume(uint8_t volume)
{
    _current_speaker_volume = std::clamp((int)volume, 0, 100);
    spectra5::log::tagInfo(_tag, "set speaker volume: {}%", _current_speaker_volume);
}

uint8_t HalDesktop::getSpeakerVolume()
{
    return _current_speaker_volume;
}

void HalDesktop::audioPlay(std::vector<int16_t>& data, bool async)
{
    static std::once_flag initFlag;
    static SDL_AudioDeviceID deviceId = 0;
    static std::mutex audioMutex;

    // 音频初始化 & 打开设备（只执行一次）
    std::call_once(initFlag, []() {
        if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO)) {
            if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
                std::cerr << "Failed to init SDL audio: " << SDL_GetError() << std::endl;
                return;
            }
        }

        SDL_AudioSpec want, have;
        SDL_memset(&want, 0, sizeof(want));
        want.freq     = 48000;
        want.format   = AUDIO_S16SYS;
        want.channels = 2;
        want.samples  = 4096;
        want.callback = nullptr;

        deviceId = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
        if (deviceId == 0) {
            std::cerr << "SDL_OpenAudioDevice failed: " << SDL_GetError() << std::endl;
        } else {
            SDL_PauseAudioDevice(deviceId, 0);  // 开启播放
        }
    });

    // 若设备打开失败，直接返回
    if (deviceId == 0) return;

    // 异步线程提交音频数据
    auto volume = getSpeakerVolume();
    std::thread([data, volume]() {
        std::vector<int16_t> adjustedData = data;

        // 音量缩放
        float scale = volume / 100.0f;
        for (size_t i = 0; i < adjustedData.size(); ++i) {
            int sample = static_cast<int>(adjustedData[i] * scale);
            if (sample > INT16_MAX) sample = INT16_MAX;
            if (sample < INT16_MIN) sample = INT16_MIN;
            adjustedData[i] = static_cast<int16_t>(sample);
        }

        // 加锁提交数据（线程安全）
        {
            std::lock_guard<std::mutex> lock(audioMutex);
            int byteSize = adjustedData.size() * sizeof(int16_t);
            if (SDL_QueueAudio(deviceId, adjustedData.data(), byteSize) < 0) {
                std::cerr << "SDL_QueueAudio failed: " << SDL_GetError() << std::endl;
            }
        }
    }).detach();
}

void HalDesktop::audioRecord(std::vector<int16_t>& data, uint16_t durationMs, float gain)
{
    const uint32_t sampleRate   = 48000;
    const uint32_t channels     = 4;
    const float frequency       = 500.0f;
    const uint32_t totalSamples = (sampleRate * durationMs) / 1000;

    data.clear();
    data.reserve(totalSamples * channels);

    for (uint32_t i = 0; i < totalSamples; ++i) {
        float t             = static_cast<float>(i) / sampleRate;
        float sample        = std::sin(2.0f * M_PI * frequency * t);
        int16_t sampleInt16 = static_cast<int16_t>(sample * 32767);
        for (int ch = 0; ch < channels; ++ch) {
            data.push_back(sampleInt16);
        }
    }
}

struct DualMicRecordTestData_t {
    std::mutex mutex;
    hal::HalBase::MicTestState_t state = hal::HalBase::MIC_TEST_IDLE;
};
static DualMicRecordTestData_t _dual_mic_record_test_data;

void HalDesktop::startDualMicRecordTest()
{
    std::lock_guard<std::mutex> lock(_dual_mic_record_test_data.mutex);
    if (_dual_mic_record_test_data.state == hal::HalBase::MIC_TEST_IDLE) {
        _dual_mic_record_test_data.state = hal::HalBase::MIC_TEST_RECORDING;
        std::thread([this]() {
            GetHAL()->delay(3000);
            _dual_mic_record_test_data.mutex.lock();
            _dual_mic_record_test_data.state = hal::HalBase::MIC_TEST_PLAYING;
            _dual_mic_record_test_data.mutex.unlock();
            GetHAL()->delay(3000);
            _dual_mic_record_test_data.mutex.lock();
            _dual_mic_record_test_data.state = hal::HalBase::MIC_TEST_IDLE;
            _dual_mic_record_test_data.mutex.unlock();
        }).detach();
    }
}

hal::HalBase::MicTestState_t HalDesktop::getDualMicRecordTestState()
{
    std::lock_guard<std::mutex> lock(_dual_mic_record_test_data.mutex);
    return _dual_mic_record_test_data.state;
}

void HalDesktop::startHeadphoneMicRecordTest()
{
    startDualMicRecordTest();
}

hal::HalBase::MicTestState_t HalDesktop::getHeadphoneMicRecordTestState()
{
    return getDualMicRecordTestState();
}

struct MusicPlayTestData_t {
    std::mutex mutex;
    bool killSignal                      = false;
    hal::HalBase::MusicPlayState_t state = hal::HalBase::MUSIC_PLAY_IDLE;
};
static MusicPlayTestData_t _music_play_test_data;

void HalDesktop::startPlayMusicTest()
{
    std::lock_guard<std::mutex> lock(_music_play_test_data.mutex);
    if (_music_play_test_data.state == hal::HalBase::MUSIC_PLAY_IDLE) {
        _music_play_test_data.state      = hal::HalBase::MUSIC_PLAY_PLAYING;
        _music_play_test_data.killSignal = false;
        std::thread([this]() {
            spectra5::log::tagInfo(_tag, "start playing music");
            int count = 0;
            while (count < 5 * 1000 / 100) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::lock_guard<std::mutex> lock(_music_play_test_data.mutex);
                if (_music_play_test_data.killSignal) {
                    break;
                }
                count++;
            }
            _music_play_test_data.mutex.lock();
            _music_play_test_data.state = hal::HalBase::MUSIC_PLAY_IDLE;
            _music_play_test_data.mutex.unlock();
            spectra5::log::tagInfo(_tag, "stop playing music");
        }).detach();
    } else {
        spectra5::log::tagInfo(_tag, "already playing");
    }
}

hal::HalBase::MusicPlayState_t HalDesktop::getMusicPlayTestState()
{
    std::lock_guard<std::mutex> lock(_music_play_test_data.mutex);
    return _music_play_test_data.state;
}

void HalDesktop::stopPlayMusicTest()
{
    std::lock_guard<std::mutex> lock(_music_play_test_data.mutex);
    _music_play_test_data.killSignal = true;
}
