#pragma once

#include <atomic>
#include <vector>
#include <memory>
#include <cstddef>
#include <cstring>
#include <complex>
#include "signalsmith-stretch.h"
#include "pitch_math.hpp"

namespace fino {

/**
 * @brief Real-time pitch shifter processor wrapping Signalsmith Stretch.
 *
 * Adheres strictly to GEMINI.md:
 * - Completely zero-allocation in process() callback.
 * - Lock-free parameter updates with linear smoothing to eliminate clicks/pops.
 * - Thread-safe atomic peak metering for UI VU meters.
 * - Hardware denormal flush (FTZ/DAZ) protection.
 */
class PitchProcessor {
public:
    PitchProcessor();
    ~PitchProcessor() = default;

    PitchProcessor(const PitchProcessor&) = delete;
    PitchProcessor& operator=(const PitchProcessor&) = delete;
    PitchProcessor(PitchProcessor&&) = delete;
    PitchProcessor& operator=(PitchProcessor&&) = delete;

    /**
     * @brief Setup and allocate buffers. Must be called from host/UI thread prior to processing.
     */
    void configure(int channels, float sample_rate, size_t max_block_frames);

    /**
     * @brief Reset internal DSP states. Call when stream restarts.
     */
    void reset();

    /**
     * @brief Thread-safe pitch ratio target update (called by UI thread).
     */
    void set_pitch_ratio(float ratio) noexcept;

    /**
     * @brief Thread-safe bypass toggle (called by UI thread).
     */
    void set_bypass(bool bypass) noexcept;

    /**
     * @brief Thread-safe output gain multiplier (called by UI thread).
     */
    void set_gain(float gain) noexcept;

    /**
     * @brief Audio thread real-time processing callback.
     * @param interleaved_input Pointer to input interleaved audio frames (float).
     * @param interleaved_output Pointer to output interleaved audio frames (float).
     * @param frames Number of frames to process.
     */
    void process(const float* interleaved_input, float* interleaved_output, size_t frames) noexcept;

    /**
     * @brief Read current input peak levels (0.0 to 1.0+) for VU meters.
     */
    void get_input_peaks(float& left, float& right) const noexcept;

    /**
     * @brief Read current output peak levels (0.0 to 1.0+) for VU meters.
     */
    void get_output_peaks(float& left, float& right) const noexcept;

    [[nodiscard]] float current_pitch_ratio() const noexcept {
        return m_current_ratio;
    }

    [[nodiscard]] float sample_rate() const noexcept {
        return m_sample_rate;
    }

    [[nodiscard]] int channels() const noexcept {
        return m_channels;
    }

private:
    void enable_denormal_flush() noexcept;

    int m_channels{2};
    float m_sample_rate{48000.0f};
    size_t m_max_block_frames{2048};

    signalsmith::stretch::SignalsmithStretch<float> m_stretch;

    // Parameter smoothing & atomic controls
    std::atomic<float> m_target_ratio{1.0f};
    std::atomic<bool> m_bypass{false};
    std::atomic<float> m_gain{1.0f};

    float m_current_ratio{1.0f};
    float m_smoothing_factor{0.05f}; // Low-pass filter coefficient for pitch transition

    // Pre-allocated non-interleaved scratch channels (no heap allocation in audio thread)
    std::vector<float> m_in_left;
    std::vector<float> m_in_right;
    std::vector<float> m_out_left;
    std::vector<float> m_out_right;

    // Atomic VU meter peaks (relaxed loads/stores)
    std::atomic<float> m_in_peak_l{0.0f};
    std::atomic<float> m_in_peak_r{0.0f};
    std::atomic<float> m_out_peak_l{0.0f};
    std::atomic<float> m_out_peak_r{0.0f};
};

} // namespace fino
