#pragma once

#include <cmath>
#include <algorithm>

namespace fino {

/**
 * @brief Pure math functions for pitch and frequency calculations.
 * Suitable for constexpr and zero-allocation DSP logic.
 */
class PitchMath {
public:
    static constexpr float MIN_PITCH_RATIO = 0.25f;  // -24 semitones (-2 octaves)
    static constexpr float MAX_PITCH_RATIO = 4.0f;   // +24 semitones (+2 octaves)
    static constexpr float DEFAULT_BASE_HZ = 440.0f; // Concert pitch A4

    /**
     * @brief Converts semitones and cents offset into a frequency multiplier ratio.
     * Ratio = 2 ^ ((semitones + cents / 100) / 12)
     */
    [[nodiscard]] static inline float semitones_to_ratio(float semitones, float cents = 0.0f) noexcept {
        const float total_semitones = semitones + (cents / 100.0f);
        const float ratio = std::pow(2.0f, total_semitones / 12.0f);
        return clamp_ratio(ratio);
    }

    /**
     * @brief Converts a frequency multiplier ratio into musical semitones.
     * Semitones = 12 * log2(ratio)
     */
    [[nodiscard]] static inline float ratio_to_semitones(float ratio) noexcept {
        if (ratio <= 0.0f) {
            return 0.0f;
        }
        return 12.0f * std::log2(ratio);
    }

    /**
     * @brief Converts source frequency (Hz) and target frequency (Hz) into a pitch multiplier ratio.
     * Ratio = target_hz / source_hz
     */
    [[nodiscard]] static inline float frequency_to_ratio(float source_hz, float target_hz) noexcept {
        if (source_hz <= 0.001f || target_hz <= 0.001f) {
            return 1.0f;
        }
        return clamp_ratio(target_hz / source_hz);
    }

    /**
     * @brief Clamps a pitch ratio within safe audible bounds [0.25, 4.0].
     */
    [[nodiscard]] static constexpr float clamp_ratio(float ratio) noexcept {
        return std::clamp(ratio, MIN_PITCH_RATIO, MAX_PITCH_RATIO);
    }

    /**
     * @brief Linear decibel to linear amplitude conversion.
     */
    [[nodiscard]] static inline float db_to_linear(float db) noexcept {
        return std::pow(10.0f, db / 20.0f);
    }

    /**
     * @brief Linear amplitude to decibel conversion.
     */
    [[nodiscard]] static inline float linear_to_db(float lin) noexcept {
        if (lin <= 1e-5f) {
            return -100.0f;
        }
        return 20.0f * std::log10(lin);
    }
};

} // namespace fino
