/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal/hal_esp32.h"
#include "core/diagnostics/log.hpp"
#include <vector>
#include <memory>
#include <string.h>
#include <bsp/m5stack_tab5.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <thread>
#include <mutex>
#include <audio_player.h>

static const char* TAG = "audio";

static uint8_t _current_speaker_volume = 60;

void HalEsp32::setSpeakerVolume(uint8_t volume)
{
    _current_speaker_volume = std::clamp((int)volume, 0, 100);
    spectra5::log::tagInfo(TAG, "set speaker volume: {}%", _current_speaker_volume);
}

uint8_t HalEsp32::getSpeakerVolume()
{
    return _current_speaker_volume;
}

void HalEsp32::audioRecord(std::vector<int16_t>& data, uint16_t durationMs, float gain)
{
    data.resize(48000 * 4 * durationMs / 1000);

    // ESP_LOGI(TAG, "start record");
    bsp_codec_config_t* codec_handle = bsp_get_codec_handle();
    codec_handle->set_in_gain(gain);
    size_t bytes_read = 0;
    codec_handle->i2s_read((char*)data.data(), (48000 * 4 * durationMs / 1000) * sizeof(uint16_t), &bytes_read,
                           portMAX_DELAY);
    // ESP_LOGI(TAG, "record done, %d bytes", bytes_read);
}

struct AudioTaskData_t {
    std::mutex mutex;
    bool is_task_running  = false;
    bool is_audio_ready   = false;
    bool is_audio_playing = false;
    std::vector<int16_t> audio_data;
};
static AudioTaskData_t _audio_task_data;

void _audio_play_task(void* param)
{
    size_t bytes_written = 0;

    while (true) {
        _audio_task_data.mutex.lock();

        if (_audio_task_data.is_audio_ready) {
            _audio_task_data.is_audio_playing = true;
            _audio_task_data.mutex.unlock();

            bsp_codec_config_t* codec_handle = bsp_get_codec_handle();
            codec_handle->set_volume(_current_speaker_volume);
            codec_handle->i2s_reconfig_clk_fn(48000, 16, I2S_SLOT_MODE_STEREO);
            codec_handle->i2s_write(_audio_task_data.audio_data.data(),
                                    _audio_task_data.audio_data.size() * sizeof(uint16_t), &bytes_written,
                                    portMAX_DELAY);

            _audio_task_data.mutex.lock();
            _audio_task_data.is_audio_playing = false;
            _audio_task_data.is_audio_ready   = false;
            _audio_task_data.mutex.unlock();

            continue;
        }

        _audio_task_data.mutex.unlock();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void HalEsp32::audioPlay(std::vector<int16_t>& data, bool async)
{
    if (async) {
        std::lock_guard<std::mutex> lock(_audio_task_data.mutex);

        if (_audio_task_data.is_audio_playing) {
            spectra5::log::tagWarn(TAG, "audio is playing");
            return;
        }

        if (!_audio_task_data.is_task_running) {
            _audio_task_data.is_task_running = true;
            xTaskCreate(_audio_play_task, "audio", 4096, nullptr, 5, nullptr);
        }

        _audio_task_data.audio_data     = data;
        _audio_task_data.is_audio_ready = true;
    } else {
        bsp_codec_config_t* codec_handle = bsp_get_codec_handle();
        codec_handle->set_volume(_current_speaker_volume);
        size_t bytes_written = 0;
        codec_handle->i2s_reconfig_clk_fn(48000, 16, I2S_SLOT_MODE_STEREO);
        codec_handle->i2s_write(data.data(), data.size() * sizeof(uint16_t), &bytes_written, portMAX_DELAY);
    }
}

/* -------------------------------------------------------------------------- */
/*                            Record and play test                            */
/* -------------------------------------------------------------------------- */
struct RecTestData_t {
    std::mutex mutex;
    bool isDualMic                     = true;
    hal::HalBase::MicTestState_t state = hal::HalBase::MIC_TEST_IDLE;
    int16_t* audio_buffer              = nullptr;
    int16_t* read_buffer               = nullptr;
};
static RecTestData_t _rec_test_data;

static void _rec_test_task(void* param)
{
    spectra5::log::tagInfo(TAG, "start record test");

    const size_t read_buffer_size  = 48000 * 4 * 3;
    const size_t audio_buffer_size = 48000 * 2 * 3;

    // Create buffers
    if (_rec_test_data.audio_buffer == nullptr) {
        _rec_test_data.audio_buffer = new int16_t[audio_buffer_size](0);
    }
    if (_rec_test_data.read_buffer == nullptr) {
        _rec_test_data.read_buffer = new int16_t[read_buffer_size](0);
    }

    const size_t sample_rate   = 48000;
    const size_t total_samples = sample_rate * 4 * 3;  // 4通道，3秒
    const size_t chunk_samples = 4096 * 4;             // 4通道一帧
    const size_t chunk_bytes   = chunk_samples * sizeof(int16_t);

    size_t total_read_samples = 0;
    size_t total_read_bytes   = 0;

    bsp_codec_config_t* codec_handle = bsp_get_codec_handle();
    codec_handle->set_in_gain(240);

    int16_t* read_buf = _rec_test_data.read_buffer;
    memset(read_buf, 0, total_samples * sizeof(int16_t));  // 清零

    spectra5::log::tagInfo(TAG, "start record");

    while (total_read_samples < total_samples) {
        size_t bytes_to_read = chunk_bytes;
        if (total_read_samples + chunk_samples > total_samples) {
            bytes_to_read = (total_samples - total_read_samples) * sizeof(int16_t);
        }

        size_t bytes_read = 0;
        codec_handle->i2s_read((char*)(read_buf + total_read_samples), bytes_to_read, &bytes_read, portMAX_DELAY);

        total_read_samples += bytes_read / sizeof(int16_t);
        total_read_bytes += bytes_read;

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    spectra5::log::tagInfo(TAG, "record done");

    // Create audio data  [MIC-L, AEC, MIC-R, MIC-HP]
    int num_frames = audio_buffer_size / 2;  // 每帧一组 stereo 输出
    for (int i = 0; i < num_frames; ++i) {
        int16_t* in  = _rec_test_data.read_buffer;
        int16_t* out = _rec_test_data.audio_buffer;

        if (_rec_test_data.isDualMic) {
            out[i * 2 + 0] = in[i * 4 + 0];  // MIC-L
            out[i * 2 + 1] = in[i * 4 + 2];  // MIC-R
        } else {
            out[i * 2 + 0] = in[i * 4 + 3];  // MIC-HP
            out[i * 2 + 1] = in[i * 4 + 3];  // MIC-HP (duplicate for stereo)
        }
    }

    _rec_test_data.mutex.lock();
    _rec_test_data.state = hal::HalBase::MIC_TEST_PLAYING;
    _rec_test_data.mutex.unlock();

    size_t bytes_written = 0;
    codec_handle->set_volume(_current_speaker_volume);
    codec_handle->i2s_reconfig_clk_fn(48000, 16, I2S_SLOT_MODE_STEREO);

    spectra5::log::tagInfo(TAG, "start playback");
    codec_handle->i2s_write(_rec_test_data.audio_buffer, (48000 * 2 * 3) * sizeof(uint16_t), &bytes_written,
                            portMAX_DELAY);
    spectra5::log::tagInfo(TAG, "playback done");

    _rec_test_data.mutex.lock();
    _rec_test_data.state = hal::HalBase::MIC_TEST_IDLE;
    _rec_test_data.mutex.unlock();

    vTaskDelete(NULL);
}

static void try_create_rec_test_task(bool isDualMic)
{
    _rec_test_data.mutex.lock();

    if (_rec_test_data.state == hal::HalBase::MIC_TEST_IDLE) {
        _rec_test_data.mutex.unlock();
        vTaskDelay(pdMS_TO_TICKS(20));
        _rec_test_data.mutex.lock();
        if (_rec_test_data.state == hal::HalBase::MIC_TEST_IDLE) {
            _rec_test_data.isDualMic = isDualMic;
            _rec_test_data.state     = hal::HalBase::MIC_TEST_RECORDING;
            xTaskCreate(_rec_test_task, "rec", 4096, nullptr, 5, nullptr);
            _rec_test_data.mutex.unlock();
            return;
        }
    }

    _rec_test_data.mutex.unlock();
    spectra5::log::tagWarn(TAG, "rec test is running");
}

void HalEsp32::startDualMicRecordTest()
{
    try_create_rec_test_task(true);
}

hal::HalBase::MicTestState_t HalEsp32::getDualMicRecordTestState()
{
    std::lock_guard<std::mutex> lock(_rec_test_data.mutex);
    return _rec_test_data.state;
}

void HalEsp32::startHeadphoneMicRecordTest()
{
    try_create_rec_test_task(false);
}

hal::HalBase::MicTestState_t HalEsp32::getHeadphoneMicRecordTestState()
{
    std::lock_guard<std::mutex> lock(_rec_test_data.mutex);
    return _rec_test_data.state;
}

/* -------------------------------------------------------------------------- */
/*                               Music play test                              */
/* -------------------------------------------------------------------------- */
extern const uint8_t canon_in_d_mp3_start[] asm("_binary_canon_in_d_mp3_start");
extern const uint8_t canon_in_d_mp3_end[] asm("_binary_canon_in_d_mp3_end");
extern const uint8_t startup_sfx_mp3_start[] asm("_binary_startup_sfx_mp3_start");
extern const uint8_t startup_sfx_mp3_end[] asm("_binary_startup_sfx_mp3_end");
extern const uint8_t shutdown_sfx_mp3_start[] asm("_binary_shutdown_sfx_mp3_start");
extern const uint8_t shutdown_sfx_mp3_end[] asm("_binary_shutdown_sfx_mp3_end");

enum Mp3PlayTarget_t {
    MP3_PLAY_TARGET_CANON_IN_D,
    MP3_PLAY_TARGET_STARTUP_SFX,
    MP3_PLAY_TARGET_SHUTDOWN_SFX,
};

struct MusicTestData_t {
    std::mutex mutex;
    bool killSignal                      = false;
    hal::HalBase::MusicPlayState_t state = hal::HalBase::MUSIC_PLAY_IDLE;
    Mp3PlayTarget_t target               = MP3_PLAY_TARGET_CANON_IN_D;
};
static MusicTestData_t _music_test_data;

static esp_err_t audio_mute_function(AUDIO_PLAYER_MUTE_SETTING setting)
{
    bsp_codec_config_t* codec_handle = bsp_get_codec_handle();
    codec_handle->set_mute(setting == AUDIO_PLAYER_MUTE ? true : false);
    return ESP_OK;
}

static void audio_player_callback(audio_player_cb_ctx_t* ctx)
{
    spectra5::log::tagInfo(TAG, "audio event: {}", (int)ctx->audio_event);

    audio_player_state_t state = audio_player_get_state();
    spectra5::log::tagInfo(TAG, "audio state: {}", (int)state);

    if (state == AUDIO_PLAYER_STATE_IDLE) {
        GetHAL()->stopPlayMusicTest();
    }
}

// Persistent music worker: created once, then loops forever waiting for play
// requests. It NEVER self-deletes -- deleting a task that has touched the audio
// decoder's TLS aborts under SPIRAM XIP (see the PSRAM-XIP task-delete gotcha).
static volatile bool _music_task_started = false;
static volatile bool _music_request      = false;

static void _music_play_task(void* param)
{
    bsp_codec_config_t* codec_handle = bsp_get_codec_handle();
    codec_handle->i2s_reconfig_clk_fn(48000, 16, I2S_SLOT_MODE_STEREO);
    audio_player_config_t config = {
        .mute_fn    = audio_mute_function,
        .clk_set_fn = codec_handle->i2s_reconfig_clk_fn,
        .write_fn   = codec_handle->i2s_write,
        .priority   = 8,
        .coreID     = 1,
    };
    ESP_ERROR_CHECK(audio_player_new(config));
    audio_player_callback_register(audio_player_callback, NULL);

    for (;;) {
        Mp3PlayTarget_t target = MP3_PLAY_TARGET_STARTUP_SFX;
        bool go                = false;
        {
            std::lock_guard<std::mutex> lock(_music_test_data.mutex);
            if (_music_request) {
                _music_request = false;
                target         = _music_test_data.target;
                go             = true;
            }
        }
        if (!go) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        codec_handle->set_volume(_current_speaker_volume);
        size_t mp3_size = 0;
        FILE* fp        = nullptr;
        switch (target) {
            case MP3_PLAY_TARGET_CANON_IN_D:
                mp3_size = (canon_in_d_mp3_end - canon_in_d_mp3_start) - 1;
                fp       = fmemopen((void*)canon_in_d_mp3_start, mp3_size, "rb");
                break;
            case MP3_PLAY_TARGET_STARTUP_SFX:
                mp3_size = (startup_sfx_mp3_end - startup_sfx_mp3_start) - 1;
                fp       = fmemopen((void*)startup_sfx_mp3_start, mp3_size, "rb");
                break;
            case MP3_PLAY_TARGET_SHUTDOWN_SFX:
                mp3_size = (shutdown_sfx_mp3_end - shutdown_sfx_mp3_start) - 1;
                fp       = fmemopen((void*)shutdown_sfx_mp3_start, mp3_size, "rb");
                break;
        }

        if (fp != nullptr && audio_player_play(fp) == ESP_OK) {
            while (1) {
                vTaskDelay(pdMS_TO_TICKS(100));
                std::lock_guard<std::mutex> lock(_music_test_data.mutex);
                if (_music_test_data.killSignal) {
                    break;
                }
            }
        } else {
            spectra5::log::tagError(TAG, "audio play failed");
        }

        std::lock_guard<std::mutex> lock(_music_test_data.mutex);
        _music_test_data.state      = hal::HalBase::MUSIC_PLAY_IDLE;
        _music_test_data.killSignal = false;
        // Loop back and wait for the next request -- never vTaskDelete.
    }
}

void try_create_music_play_task(Mp3PlayTarget_t target)
{
    // Caller holds _music_test_data.mutex.
    if (_music_test_data.state == hal::HalBase::MUSIC_PLAY_IDLE) {
        _music_test_data.state      = hal::HalBase::MUSIC_PLAY_PLAYING;
        _music_test_data.target     = target;
        _music_test_data.killSignal = false;
        _music_request              = true;
        if (!_music_task_started) {
            _music_task_started = true;
            xTaskCreate(_music_play_task, "music", 4096, nullptr, 5, nullptr);
        }
    } else {
        spectra5::log::tagWarn(TAG, "music play is running");
    }
}

void HalEsp32::startPlayMusicTest()
{
    std::lock_guard<std::mutex> lock(_music_test_data.mutex);
    try_create_music_play_task(MP3_PLAY_TARGET_CANON_IN_D);
}

hal::HalBase::MusicPlayState_t HalEsp32::getMusicPlayTestState()
{
    std::lock_guard<std::mutex> lock(_music_test_data.mutex);
    return _music_test_data.state;
}

void HalEsp32::stopPlayMusicTest()
{
    std::lock_guard<std::mutex> lock(_music_test_data.mutex);
    _music_test_data.killSignal = true;
}

/* -------------------------------------------------------------------------- */
/*                                     SFX                                    */
/* -------------------------------------------------------------------------- */
void HalEsp32::playStartupSfx()
{
    std::lock_guard<std::mutex> lock(_music_test_data.mutex);
    try_create_music_play_task(MP3_PLAY_TARGET_STARTUP_SFX);
}

void HalEsp32::playShutdownSfx()
{
    std::lock_guard<std::mutex> lock(_music_test_data.mutex);
    try_create_music_play_task(MP3_PLAY_TARGET_SHUTDOWN_SFX);
}
