#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include "pitch_processor.hpp"

namespace fino {

struct AudioDeviceInfo {
    int index{-1};
    std::string name;
    bool is_default{false};
};

/**
 * @brief Multiplatform real-time audio engine managing capture & playback via miniaudio.
 *
 * Implements strict audio thread safety:
 * - Thread-safe SPSC ring buffer bridges capture callback to playback callback.
 * - Pitch shifting processing executed directly in playback callback without heap allocations.
 * - PImpl idiom cleanly encapsulates miniaudio OS handles.
 */
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    AudioEngine(AudioEngine&&) noexcept;
    AudioEngine& operator=(AudioEngine&&) noexcept;

    /**
     * @brief Initializes miniaudio context and discovers devices.
     */
    bool initialize();

    /**
     * @brief Refreshes list of connected input and output devices.
     */
    void refresh_devices();

    [[nodiscard]] const std::vector<AudioDeviceInfo>& capture_devices() const noexcept;
    [[nodiscard]] const std::vector<AudioDeviceInfo>& playback_devices() const noexcept;

    /**
     * @brief Starts real-time capture and playback streams.
     * @param capture_index Device index from capture_devices() (-1 for system default).
     * @param playback_index Device index from playback_devices() (-1 for system default).
     * @param buffer_frames Requested period size in frames (e.g., 128, 256, 512, 1024).
     * @param sample_rate Target sample rate (default 48000 Hz).
     */
    bool start(int capture_index = -1, int playback_index = -1, uint32_t buffer_frames = 512, uint32_t sample_rate = 48000);

    /**
     * @brief Stops active audio streams safely.
     */
    void stop();

    [[nodiscard]] bool is_running() const noexcept;
    [[nodiscard]] PitchProcessor& processor() noexcept;
    [[nodiscard]] const PitchProcessor& processor() const noexcept;

    [[nodiscard]] uint32_t active_sample_rate() const noexcept;
    [[nodiscard]] uint32_t active_buffer_frames() const noexcept;
    [[nodiscard]] float buffer_fill_ratio() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace fino
