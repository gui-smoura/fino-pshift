#include "fino/audio_engine.hpp"
#include "fino/spsc_ring_buffer.hpp"

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MA_NO_GENERATION
#include "miniaudio.h"

#include <iostream>
#include <atomic>
#include <cstring>
#include <algorithm>

namespace fino {

struct AudioEngine::Impl {
    ma_context context{};
    bool context_initialized{false};

    ma_device capture_device{};
    bool capture_device_initialized{false};

    ma_device playback_device{};
    bool playback_device_initialized{false};

    std::vector<AudioDeviceInfo> capture_devices_list;
    std::vector<AudioDeviceInfo> playback_devices_list;
    std::vector<ma_device_id> capture_device_ids;
    std::vector<ma_device_id> playback_device_ids;

    std::atomic<bool> is_running{false};
    uint32_t active_sample_rate{48000};
    uint32_t active_buffer_frames{512};

    PitchProcessor processor;
    SpscRingBuffer<float> ring_buffer{65536};
    std::vector<float> playback_scratch;

    static void capture_callback(ma_device* pDevice, void* /*pOutput*/, const void* pInput, ma_uint32 frameCount) {
        auto* impl = static_cast<AudioEngine::Impl*>(pDevice->pUserData);
        if (impl == nullptr || pInput == nullptr || frameCount == 0) {
            return;
        }

        const size_t sample_count = static_cast<size_t>(frameCount) * 2; // Stereo float
        impl->ring_buffer.push(static_cast<const float*>(pInput), sample_count);
    }

    static void playback_callback(ma_device* pDevice, void* pOutput, const void* /*pInput*/, ma_uint32 frameCount) {
        auto* impl = static_cast<AudioEngine::Impl*>(pDevice->pUserData);
        if (impl == nullptr || pOutput == nullptr || frameCount == 0) {
            return;
        }

        const size_t sample_count = static_cast<size_t>(frameCount) * 2; // Stereo float
        float* out = static_cast<float*>(pOutput);

        if (impl->playback_scratch.size() < sample_count) {
            std::fill_n(out, sample_count, 0.0f);
            return;
        }

        const size_t popped = impl->ring_buffer.pop(impl->playback_scratch.data(), sample_count);
        if (popped < sample_count) {
            // Buffer underflow protection: zero out missing tail
            std::fill_n(impl->playback_scratch.data() + popped, sample_count - popped, 0.0f);
        }

        impl->processor.process(impl->playback_scratch.data(), out, frameCount);
    }
};

AudioEngine::AudioEngine()
    : m_impl(std::make_unique<Impl>())
{
    // Pre-allocate playback scratch buffer up front
    m_impl->playback_scratch.assign(16384, 0.0f);
}

AudioEngine::~AudioEngine() {
    stop();
    if (m_impl->context_initialized) {
        ma_context_uninit(&m_impl->context);
    }
}

AudioEngine::AudioEngine(AudioEngine&&) noexcept = default;
AudioEngine& AudioEngine::operator=(AudioEngine&&) noexcept = default;

bool AudioEngine::initialize() {
    if (m_impl->context_initialized) {
        return true;
    }

    ma_result result = ma_context_init(nullptr, 0, nullptr, &m_impl->context);
    if (result != MA_SUCCESS) {
        return false;
    }

    m_impl->context_initialized = true;
    refresh_devices();
    return true;
}

void AudioEngine::refresh_devices() {
    if (!m_impl->context_initialized) {
        return;
    }

    m_impl->capture_devices_list.clear();
    m_impl->playback_devices_list.clear();
    m_impl->capture_device_ids.clear();
    m_impl->playback_device_ids.clear();

    ma_device_info* pPlaybackInfos = nullptr;
    ma_uint32 playbackCount = 0;
    ma_device_info* pCaptureInfos = nullptr;
    ma_uint32 captureCount = 0;

    ma_result result = ma_context_get_devices(
        &m_impl->context,
        &pPlaybackInfos, &playbackCount,
        &pCaptureInfos, &captureCount
    );

    if (result != MA_SUCCESS) {
        return;
    }

    // Enumerate Capture Devices (e.g., VB-Audio Virtual Cable, Microphones)
    for (ma_uint32 i = 0; i < captureCount; ++i) {
        AudioDeviceInfo info;
        info.index = static_cast<int>(i);
        info.name = pCaptureInfos[i].name;
        info.is_default = (pCaptureInfos[i].isDefault != 0);

        m_impl->capture_devices_list.push_back(info);
        m_impl->capture_device_ids.push_back(pCaptureInfos[i].id);
    }

    // Enumerate Playback Devices (e.g., Headphones, Speakers)
    for (ma_uint32 i = 0; i < playbackCount; ++i) {
        AudioDeviceInfo info;
        info.index = static_cast<int>(i);
        info.name = pPlaybackInfos[i].name;
        info.is_default = (pPlaybackInfos[i].isDefault != 0);

        m_impl->playback_devices_list.push_back(info);
        m_impl->playback_device_ids.push_back(pPlaybackInfos[i].id);
    }
}

const std::vector<AudioDeviceInfo>& AudioEngine::capture_devices() const noexcept {
    return m_impl->capture_devices_list;
}

const std::vector<AudioDeviceInfo>& AudioEngine::playback_devices() const noexcept {
    return m_impl->playback_devices_list;
}

bool AudioEngine::start(int capture_index, int playback_index, uint32_t buffer_frames, uint32_t sample_rate) {
    stop();

    if (!m_impl->context_initialized) {
        if (!initialize()) {
            return false;
        }
    }

    m_impl->active_sample_rate = sample_rate;
    m_impl->active_buffer_frames = buffer_frames;

    // Configure DSP processor with the chosen sample rate and buffer dimensions
    m_impl->processor.configure(2, static_cast<float>(sample_rate), buffer_frames * 2);

    // Reset ring buffer and prime with silence to establish safety headroom
    m_impl->ring_buffer.clear();
    const size_t prime_samples = static_cast<size_t>(buffer_frames) * 4; // ~2 blocks pre-roll
    std::vector<float> prime_silence(prime_samples, 0.0f);
    m_impl->ring_buffer.push(prime_silence.data(), prime_samples);

    // 1. Configure Capture Device
    ma_device_config capture_config = ma_device_config_init(ma_device_type_capture);
    capture_config.capture.format = ma_format_f32;
    capture_config.capture.channels = 2;
    capture_config.sampleRate = sample_rate;
    capture_config.periodSizeInFrames = buffer_frames;
    capture_config.dataCallback = Impl::capture_callback;
    capture_config.pUserData = m_impl.get();

    if (capture_index >= 0 && capture_index < static_cast<int>(m_impl->capture_device_ids.size())) {
        capture_config.capture.pDeviceID = &m_impl->capture_device_ids[capture_index];
    } else {
        capture_config.capture.pDeviceID = nullptr; // Default capture device
    }

    ma_result res_capture = ma_device_init(&m_impl->context, &capture_config, &m_impl->capture_device);
    if (res_capture != MA_SUCCESS) {
        return false;
    }
    m_impl->capture_device_initialized = true;

    // 2. Configure Playback Device
    ma_device_config playback_config = ma_device_config_init(ma_device_type_playback);
    playback_config.playback.format = ma_format_f32;
    playback_config.playback.channels = 2;
    playback_config.sampleRate = sample_rate;
    playback_config.periodSizeInFrames = buffer_frames;
    playback_config.dataCallback = Impl::playback_callback;
    playback_config.pUserData = m_impl.get();

    if (playback_index >= 0 && playback_index < static_cast<int>(m_impl->playback_device_ids.size())) {
        playback_config.playback.pDeviceID = &m_impl->playback_device_ids[playback_index];
    } else {
        playback_config.playback.pDeviceID = nullptr; // Default playback device
    }

    ma_result res_playback = ma_device_init(&m_impl->context, &playback_config, &m_impl->playback_device);
    if (res_playback != MA_SUCCESS) {
        ma_device_uninit(&m_impl->capture_device);
        m_impl->capture_device_initialized = false;
        return false;
    }
    m_impl->playback_device_initialized = true;

    // Start playback stream
    if (ma_device_start(&m_impl->playback_device) != MA_SUCCESS) {
        stop();
        return false;
    }

    // Start capture stream
    if (ma_device_start(&m_impl->capture_device) != MA_SUCCESS) {
        stop();
        return false;
    }

    m_impl->is_running.store(true, std::memory_order_release);
    return true;
}

void AudioEngine::stop() {
    m_impl->is_running.store(false, std::memory_order_release);

    if (m_impl->capture_device_initialized) {
        ma_device_stop(&m_impl->capture_device);
        ma_device_uninit(&m_impl->capture_device);
        m_impl->capture_device_initialized = false;
    }

    if (m_impl->playback_device_initialized) {
        ma_device_stop(&m_impl->playback_device);
        ma_device_uninit(&m_impl->playback_device);
        m_impl->playback_device_initialized = false;
    }
}

bool AudioEngine::is_running() const noexcept {
    return m_impl->is_running.load(std::memory_order_acquire);
}

PitchProcessor& AudioEngine::processor() noexcept {
    return m_impl->processor;
}

const PitchProcessor& AudioEngine::processor() const noexcept {
    return m_impl->processor;
}

uint32_t AudioEngine::active_sample_rate() const noexcept {
    return m_impl->active_sample_rate;
}

uint32_t AudioEngine::active_buffer_frames() const noexcept {
    return m_impl->active_buffer_frames;
}

float AudioEngine::buffer_fill_ratio() const noexcept {
    const size_t readable = m_impl->ring_buffer.available_read();
    const size_t cap = m_impl->ring_buffer.capacity();
    return (cap > 0) ? (static_cast<float>(readable) / static_cast<float>(cap)) : 0.0f;
}

} // namespace fino
